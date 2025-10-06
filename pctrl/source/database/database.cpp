#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
//#include <switch.h>
#include <stratosphere.hpp>
#include <filesystem>
#include <mutex>
#include "database.h"
#include "json.hpp"
#include "../logger.h"
#include "../helpers.h"

using json = nlohmann::json;
using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace ams;
using namespace ams::fs;

const std::string DATA_DIR = "sdmc:/switch/parental_control";
const std::string DB_FILENAME = DATA_DIR + "/sessions.json";
const std::string SETTINGS_FILENAME = DATA_DIR + "/settings.json";

namespace alefbet::pctrl::database {
    static FileHandle handle_settings;
    static std::mutex mutex_settings;
    //static FileHandle handle_database;

    bool dataDirectoryExists()
    {
        fsdevMountSdmc();

        bool exists = std::filesystem::exists(DATA_DIR) && std::filesystem::is_directory(DATA_DIR);
        fsdevUnmountDevice("sdmc");

        return exists;
    }

    bool dataFileExists() 
    {
        fsdevMountSdmc();

        bool exists = std::filesystem::exists(DB_FILENAME) && std::filesystem::is_regular_file(DB_FILENAME);
        fsdevUnmountDevice("sdmc");

        return exists;
    }

    bool settingsFileExists()
    {
        fsdevMountSdmc();

        bool exists = std::filesystem::exists(SETTINGS_FILENAME) && std::filesystem::is_regular_file(SETTINGS_FILENAME);
        fsdevUnmountDevice("sdmc");

        return exists;
    }

    bool createDataDirectory() 
    {
        fsdevMountSdmc();

        bool result = std::filesystem::create_directory(DATA_DIR);
        fsdevUnmountDevice("sdmc");

        return result;
    }

    History loadDatabase()
    {    
        History history;

        logToFile("[Database] Opening database at %s\n", DB_FILENAME.c_str());
        std::ifstream ifs(DB_FILENAME, std::ios::in); 
        //if (!in) return history;

        if (ifs.is_open()) {
            json j_data = json::parse(ifs);

            std::vector<json> j_entries = j_data["history"].get<std::vector<json>>();

            for(const auto& j_entry: j_entries) {
                auto uidAsString = j_entry["uid"].get<std::string>();
                auto date = j_entry["date"].get<std::string>();
                auto titleId = j_entry["title_id"].get<u64>();
                auto durationInMinutes = j_entry["duration_in_min"].get<u16>();
                auto uid = accountUidFromString(uidAsString);

                HistoryEntry entry(uid, date, titleId, durationInMinutes);
            }
        } else {
            logToFile("[Database] Error: Could not load database.\n");
        }

        ifs.close();

        return history;
    }

    void saveDatabase(const History& history) {
        std::ofstream ofs(DB_FILENAME, std::ios::out);

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

        ofs << j_history.dump();

        ofs.close();
    }    

    std::vector<HistoryEntry> getHistory(AccountUid uid, std::string date) {
        //Get current history
        History history = loadDatabase();

        return history.entries(uid, date);
    }

    HistoryEntry addToHistory(AccountUid uid, u64 titleId, u16 duration_in_minutes) 
    {
        HistoryEntry result;

        std::string uidToString = accountUidToString(uid);

        //Get current history
        History history = loadDatabase();

        //Search existing entry
        auto entries = history.entries(uid, today(), titleId);

        if(!entries.empty()) {
            auto entry = entries[0];
            logToFile("[Database] entry found for user %s with title ID %i\n", uidToString.c_str(), titleId);
            entry.setDurationInMinutes(duration_in_minutes + entry.durationInMinutes());
            logToFile("[Database] new duration is %i", entry.durationInMinutes());
            result = entry;
        } else {
            //Or create a new one
            logToFile("[Database] no entry found for user %s with title ID %i\n", uidToString.c_str(), titleId);
            HistoryEntry entry = HistoryEntry(uid, today(), titleId, duration_in_minutes);
            history.addEntry(entry);
            result = entry;
        }

        saveDatabase(history);
        return result;
    }

    Settings loadSettings() {
        logToFile("[Database] Loading settings at %s\n", SETTINGS_FILENAME.c_str());

        bool opened = OpenFile(std::addressof(handle_settings), SETTINGS_FILENAME.c_str(), OpenMode_Read).IsSuccess();
        Settings settings;

        if(!opened) {
            logToFile("Could not open settings file. Initialise default settings.\n");

            saveSetting(settings, Setting {
                .key = SETTING_DAILY_LIMIT_GAME,
                .int_value = 1*3600 //Default: 1 hour
            });
                        
            saveSetting(settings, Setting {
                .key = SETTING_DAILY_LIMIT_GLOBAL,
                .int_value = 1*3600 //Default: 1 hour
            });
        } else {
            s64 fileSize = 0;
            if(GetFileSize(&fileSize, handle_settings).IsFailure()) {
                logToFile("[Database] Could not get settings file size\n");
                CloseFile(handle_settings);
                return settings;
            } else {
                logToFile("[Database] Settings file size is %i\n", fileSize);
            }

            if(fileSize == 0) {
                logToFile("[Database] Settings file is empty\n");
                CloseFile(handle_settings);
                return settings;
            }

            u8* data_settings = new u8[fileSize+1];            
            if(data_settings == nullptr) {
                logToFile("[Database] Could not create a buffer for settings\n");
                CloseFile(handle_settings);
                return settings;
            } else {
                logToFile("[Database] settings buffer ready at @%p\n", (void*)data_settings);
            }

            if(ReadFile(handle_settings, 0, data_settings, fileSize).IsFailure()) {
                logToFile("[Database] Could not read the settings file\n");
                CloseFile(handle_settings);
                return settings;
            } else {
                logToFile("[Database] Settings database read\n");
            }

            data_settings[fileSize] = '\0';
            logToFile("[Database] Settings data: %s\n", data_settings);
            logToFile("[Database] Parse settings file\n");
            json j_settings = json::parse(data_settings);            

            if(j_settings.contains(SETTING_DAILY_LIMIT_GAME)) {
                Setting setting;
                setting.key = SETTING_DAILY_LIMIT_GAME;
                setting.int_value = j_settings[SETTING_DAILY_LIMIT_GAME].get<int>();
                settings[SETTING_DAILY_LIMIT_GAME] = setting;
            }

            if(j_settings.contains(SETTING_DAILY_LIMIT_GLOBAL)) {
                Setting setting;
                setting.key = SETTING_DAILY_LIMIT_GLOBAL;
                setting.int_value = j_settings[SETTING_DAILY_LIMIT_GLOBAL].get<int>();
                settings[SETTING_DAILY_LIMIT_GLOBAL] = setting;
            }
            
            logToFile("[Database] End");

            CloseFile(handle_settings);
            delete[] data_settings;
        }        

        return settings;
    }

    void saveSetting(Settings& settings, Setting setting) {
        //Update settings internal structure
        settings[setting.key] = setting;  
        saveSettings(settings);      
    }
    
    void saveSettings(Settings& settings) {
        std::lock_guard<std::mutex> lock(mutex_settings);

        //Update database file
        json j_settings({});

        for(const auto& [key, value] : settings) {
            switch(value.type) {
                case INTEGER: j_settings[key] = value.int_value; break;
                case DOUBLE: j_settings[key] = value.double_value; break;
                case STRING: j_settings[key] = value.string_value; break;
            }
        }

        DeleteFile(SETTINGS_FILENAME.c_str());
        bool ok = CreateFile(SETTINGS_FILENAME.c_str(), 0).IsSuccess();
        if(!ok) {
            logToFile("[Database] Could not create the settings file\n");
            return;
        } else {
            logToFile("[Database] New settings file created\n");
        }

        bool opened = OpenFile(std::addressof(handle_settings), SETTINGS_FILENAME.c_str(), OpenMode_Write | OpenMode_AllowAppend).IsSuccess();

        if(!opened) {
            logToFile("[Database] The settings file could not be opened for writing\n");
            return;
        }

        const auto data = j_settings.dump();
        const auto s_data = data.c_str();
        logToFile("[Database] Writing settings %s (size=%i)\n", s_data, std::strlen(s_data));
        if(WriteFile(handle_settings, 0, s_data, std::strlen(s_data), WriteOption::Flush).IsFailure()) {
            logToFile("[Database] Could not write into settings file\n");
        }

        CloseFile(handle_settings);
    }

}