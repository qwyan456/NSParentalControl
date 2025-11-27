#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <switch.h>
#include <filesystem>
#include <mutex>
#include "database.h"
#include "json.hpp"
#include "../logger.h"
#include "../helpers.h"
#include "aes.hpp"

using json = nlohmann::json;
using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;

static const char* DATA_DIR = "/config/parental_control";
static const char* DB_FILENAME = "/config/parental_control/sessions.json";
static const char* SETTINGS_FILENAME = "/config/parental_control/settings.json";

namespace alefbet::pctrl::database {
    static FsFileSystem sdmc;
    static FsFile handle_settings;
    static std::mutex mutex_settings;
    static FsFile handle_database;
    static std::mutex mutex_database;
    static bool ready = false;
    static bool data_synchronized = false; // The data are synchronized between the cache and the file
    static bool settings_synchronized = false; 
    static bool cipher_ready = false;
    static History history;
    static Settings settings;

    namespace cipher {
        static struct AES_ctx aes_ctx;

        void initCipher() {
            Result rc = setcalInitialize();
            if(R_FAILED(rc)) {
                logError("[Database] Could not connect to SetSys to initialize key\n");
                return;
            }

            // Get the device ID
            u64 deviceId = 0;
            rc = setcalGetDeviceId(&deviceId);
            if(R_FAILED(rc)) {
                logError("[Database] Failed to get Device ID for ciphering\n");
                return;
            }

            // Derivate a hash
            u8 hash[32] = {0};
            sha256CalculateHash(hash, (u8*)&deviceId, sizeof(deviceId));

            // Create a 128 bit key
            u8 cipher_key[16];
            memcpy(cipher_key, hash, 16);

            // Initialize AES context
            AES_init_ctx(&aes_ctx, cipher_key);            

            setcalExit();
            cipher_ready = true;
        }

        void cipherBuffer(u8* buffer, u64 size) {
            AES_CBC_encrypt_buffer(&aes_ctx, buffer, size);
        }

        void decipherBuffer(u8* buffer, u64 size) {
            AES_CBC_decrypt_buffer(&aes_ctx, buffer, size);
        }
    }

    bool prepare() {
        if(ready) return true;

        ready = R_SUCCEEDED(fsOpenSdCardFileSystem(&sdmc));
        if(!ready) {
            logError("[Database] Could not get access to SD card\n");
        }

        // Initialize ciphering
        cipher::initCipher();
        logInfo("[Database] Data ciphering is %s\n", cipher_ready ? "ready" : "not ready");

        return ready;
    }

    void freeFs() {
        if(!ready) return;

        fsFsClose(&sdmc);
    }

    bool createDataDirectory() 
    {
        if(!prepare()) return false;

        bool result = R_SUCCEEDED(fsFsCreateDirectory(&sdmc, DATA_DIR));

        return result;
    }

    void loadDatabase()
    {   
        // We load the file only when the data are not up to date                 
        if(data_synchronized) return;

        std::lock_guard<std::mutex> lock(mutex_database);

        logDebug("[Database] Loading database at %s\n", DB_FILENAME);

        if(!prepare()) return;

        bool opened = R_SUCCEEDED(fsFsOpenFile(&sdmc, DB_FILENAME, FsOpenMode_Read, &handle_database));
        History history;

        if(!opened) {
            logError("Could not open database file. Try to create a new file.\n");

            // Verify whether data directory exists
            FsDirEntryType type;
            if(R_FAILED(fsFsGetEntryType(&sdmc, DATA_DIR, &type))) {
                // The directory does not exist
                if(!createDataDirectory()) {
                    logError("[Database] The data directory could not be created.\n");
                    return;
                }
            }

            if(R_FAILED(fsFsCreateFile(&sdmc, DB_FILENAME, 0, 0))) {
                logError("[Database] Could not create a new database file.\n");
                return;
            }
        } else {
            s64 fileSize = 0;
            if(R_FAILED(fsFileGetSize(&handle_database, &fileSize))) {
                logError("[Database] Could not get database file size\n");
                fsFileClose(&handle_database);
                return ;
            } else {
                logDebug("[Database] Database file size is %i\n", fileSize);
            }

            if(fileSize == 0) {
                logError("[Database] Database file is empty\n");
                fsFileClose(&handle_database);
                return;
            }

            u8* data_sessions = new u8[fileSize+1];            
            if(data_sessions == nullptr) {
                logError("[Database] Could not create a buffer for sessions\n");
                fsFileClose(&handle_database);
                return;
            } else {
                logDebug("[Database] sessions buffer ready at @%p\n", (void*)data_sessions);
            }

            u64 dataRead = 0;
            if(R_FAILED(fsFileRead(&handle_database, 0, data_sessions, fileSize, FsReadOption_None, &dataRead))) {
                logError("[Database] Could not read the database file\n");
                fsFileClose(&handle_database);
                return;
            } else {
                logDebug("[Database] Sessions database read\n");
            }

            data_sessions[fileSize] = '\0';

            // Decipher if needed
            if(cipher_ready) {
                cipher::decipherBuffer(data_sessions, fileSize);
            }

            logDebug("[Database] Sessions data: %s\n", data_sessions);
            logDebug("[Database] Parse sessions file\n");
            json j_settings = json::parse(data_sessions);

            std::vector<json> j_entries = j_settings["history"].get<std::vector<json>>();

            for(const auto& j_entry: j_entries) {
                auto uidAsString = j_entry["uid"].get<std::string>();
                auto date = j_entry["date"].get<std::string>();
                auto titleId = j_entry["title_id"].get<u64>();
                auto durationInMinutes = j_entry["duration_in_min"].get<u16>();
                auto uid = accountUidFromString(uidAsString);

                HistoryEntry entry(uid, date, titleId, durationInMinutes);
                history.addEntry(entry);
            }

            fsFileClose(&handle_database);
            delete[] data_sessions;
        }

        data_synchronized = true;
    }

    void saveDatabase(const History& history) {
        std::lock_guard<std::mutex> lock(mutex_database);

        if(!prepare()) return;

        json j_entries;
        for(const auto& entry: history.entries()) {
            json j_entry = json::object( {
                { "uid", entry.uidAsString() },
                { "date", entry.date() },
                { "title_id", entry.titleId() },
                { "duration_in_min", entry.durationInMinutes() }
            });

            j_entries.push_back(j_entry);
        }

        json j_history = json{
            { "history", j_entries }
        };

        if(R_FAILED(fsFsDeleteFile(&sdmc, DB_FILENAME))) {
            logError("[Database] Could not delete the current database file\n");            
        }

        if(R_FAILED(fsFsCreateFile(&sdmc, DB_FILENAME, 0, 0))) {
            logError("[Database] Could not create the database file\n");
            return;
        } else {
            logInfo("[Database] New database file created\n");
        }

        if(R_FAILED(fsFsOpenFile(&sdmc, DB_FILENAME, FsOpenMode_Write | FsOpenMode_Append, &handle_database))) {
            logError("[Database] The database file could not be opened for writing\n");
            return;
        }

        const auto stdstr_data = j_history.dump();
        const auto str_data = stdstr_data.c_str();
        u64 lenstr = std::strlen(str_data);
        void* s_data = malloc(lenstr+1);
        memset(s_data, 0, lenstr);
        memcpy(s_data, str_data, lenstr);

        logDebug("[Database] Writing sessions data %s (size=%i)\n", s_data, lenstr);    

        // Cipher the data if needed
        if(cipher_ready) {
            cipher::cipherBuffer((u8*)s_data, lenstr);
        }

        if(R_FAILED(fsFileWrite(&handle_database, 0, s_data, lenstr, FsWriteOption_Flush))) {
            logError("[Database] Could not write into database file\n");
        }

        fsFileClose(&handle_database);
    }    

    std::vector<HistoryEntry> getHistory(AccountUid uid, std::string date) {
        //Get current history
        loadDatabase();

        return history.entries(uid, date);
    }

    HistoryEntry addToHistory(AccountUid uid, u64 titleId, u16 duration_in_minutes) 
    {
        HistoryEntry result;

        if(uid.uid[0] == 0 || uid.uid[1] == 0 || titleId == 0) {
            logError("[Database] Wrong parameters\n");
            return result;
        }

        std::string uidToString = accountUidToString(uid);

        //Get current history
        loadDatabase();

        const auto& date = today();
        if(date.empty()) {
            return result;
        }

        //Search existing entry
        auto entries = history.entries(uid, date, titleId);

        if(!entries.empty()) {
            auto entry = entries[0];
            logDebug("[Database] entry found for user %s with title ID %i\n", uidToString.c_str(), titleId);
            entry.setDurationInMinutes(duration_in_minutes + entry.durationInMinutes());
            logDebug("[Database] new duration is %i\n", entry.durationInMinutes());
            result = entry;
        } else {
            //Or create a new one
            logDebug("[Database] no entry found for user %s with title ID %i\n", uidToString.c_str(), titleId);
            HistoryEntry entry = HistoryEntry(uid, date, titleId, duration_in_minutes);
            history.addEntry(entry);
            result = entry;
        }

        saveDatabase(history);
        return result;
    }

    Settings loadSettings() {
        // We load the file only when the settings are not synchronized
        if(settings_synchronized) return settings;

        logDebug("[Database] Loading settings at %s\n", SETTINGS_FILENAME);

        if(!prepare()) return Settings{};

        bool opened = R_SUCCEEDED(fsFsOpenFile(&sdmc, SETTINGS_FILENAME, FsOpenMode_Read, &handle_settings));

        if(!opened) {
            logError("Could not open settings file. Initialise default settings.\n");

            // Verify whether data directory exists
            FsDirEntryType type;
            if(R_FAILED(fsFsGetEntryType(&sdmc, DATA_DIR, &type))) {
                // The directory does not exist
                if(!createDataDirectory()) {
                    logError("[Database] The data directory could not be created.\n");
                    return settings;
                }
            }

            saveSetting(settings, Setting {
                .key = SETTING_DAILY_LIMIT_GAME,
                .type = INTEGER,
                .int_value = 1*60 //Default: 1 hour
            });
                        
            saveSetting(settings, Setting {
                .key = SETTING_DAILY_LIMIT_GLOBAL,
                .type = INTEGER,
                .int_value = 1*60 //Default: 1 hour
            });
        } else {
            s64 fileSize = 0;
            if(R_FAILED(fsFileGetSize(&handle_settings, &fileSize))) {
                logError("[Database] Could not get settings file size\n");
                fsFileClose(&handle_settings);
                return settings;
            } else {
                logDebug("[Database] Settings file size is %i\n", fileSize);
            }

            if(fileSize == 0) {
                logError("[Database] Settings file is empty\n");
                fsFileClose(&handle_settings);
                return settings;
            }

            u8* data_settings = new u8[fileSize+1];            
            if(data_settings == nullptr) {
                logError("[Database] Could not create a buffer for settings\n");
                fsFileClose(&handle_settings);
                return settings;
            } else {
                logDebug("[Database] settings buffer ready at @%p\n", (void*)data_settings);
            }

            u64 dataRead = 0;
            if(R_FAILED(fsFileRead(&handle_settings, 0, data_settings, fileSize, FsReadOption_None, &dataRead))) {
                logError("[Database] Could not read the settings file\n");
                fsFileClose(&handle_settings);
                return settings;
            } else {
                logDebug("[Database] Settings database read\n");
            }

            data_settings[fileSize] = '\0';
            logDebug("[Database] Settings data: %s\n", data_settings);
            logDebug("[Database] Parse settings file\n");
            json j_settings = json::parse(data_settings);

            if(j_settings.contains("settings")) {
                for(const auto& j_setting: j_settings["settings"]) {
                    if(j_setting.is_object() && j_setting.contains("key") && j_setting.contains("type")) {
                        Setting setting;

                        setting.key = j_setting["key"].get<std::string>();
                        setting.type = j_setting["type"].get<SettingType>();

                        switch(setting.type) {
                            case INTEGER: setting.int_value = j_setting["value"].get<u64>(); break;
                            case DOUBLE: setting.double_value = j_setting["value"].get<double>(); break;
                            case STRING: setting.string_value = j_setting["value"].get<std::string>(); break;
                        }

                        settings[setting.key] = setting;
                    } else {
                        logError("[Database] setting is malformed\n");
                    }
                }
            }

            fsFileClose(&handle_settings);
            delete[] data_settings;
            settings_synchronized = true;
        }
        
        return settings;
    }

    void saveSetting(Settings& settings, Setting setting) {        
        //Update settings internal structure
        settings[setting.key] = setting;  
        saveSettings(settings);      
    }
    
    void saveSettings(Settings& settings) {
        logDebug("[Database] Saving settings\n");
        std::lock_guard<std::mutex> lock(mutex_settings);        

        if(!prepare()) return;

        //Update database file
        json j_entries;
        
        auto&& values = settings | std::views::values;
        for(const auto& value : values) {
            json j_entry = json::object( {
                { "type", value.type },
                { "key", value.key }
            });
            
            switch(value.type) {
                case INTEGER: j_entry["value"] = value.int_value; break;
                case DOUBLE: j_entry["value"] = std::to_string(value.double_value); break;
                case STRING: j_entry["value"] = value.string_value; break;            
                default: j_entry["value"] = "";
            }

            j_entries.push_back(j_entry);
        }

        json j_settings = json{
            { "settings", j_entries }
        };

   
        if(R_FAILED(fsFsDeleteFile(&sdmc, SETTINGS_FILENAME))) {
            logError("[Database] Could not delete the settings file\n"); //Error 514?            
        } else {
            logDebug("[Database] Successfully removed current settings\n");
        }

        if(R_FAILED(fsFsCreateFile(&sdmc, SETTINGS_FILENAME, 0, 0))) {
            logError("[Database] Could not create the settings file\n");
            return;
        } else {
            logDebug("[Database] New settings file created\n");
        }

        if(R_FAILED(fsFsOpenFile(&sdmc, SETTINGS_FILENAME, FsOpenMode_Write | FsOpenMode_Append, &handle_settings))) {
            logError("[Database] The settings file could not be opened for writing\n");
            return;
        } else {
            logDebug("[Database] Settings file successfully opened\n");
        }

        const auto data = j_settings.dump();
        const auto s_data = data.c_str();
        logDebug("[Database] Writing settings %s (size=%i)\n", s_data, std::strlen(s_data));

        if(R_FAILED(fsFileWrite(&handle_settings, 0, s_data, std::strlen(s_data), FsWriteOption_Flush))) {
            logError("[Database] Could not write into settings file\n");
        }

        fsFileClose(&handle_settings);
    }

}