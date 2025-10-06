#include <fstream>
#include <sys/stat.h>
#include <vector>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <switch.h>
#include <filesystem>
#include "database.h"
//#include "json.hpp"
#include "../logger.h"
#include "../helpers.h"

//using json = nlohmann::json;
using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;

const std::string DATA_DIR = "sdmc:/switch/parental_control";
const std::string DB_FILENAME = DATA_DIR + "/sessions.json";
const std::string SETTINGS_FILENAME = DATA_DIR + "/settings.json";

namespace alefbet::pctrl::database {
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

        std::ifstream in(DB_FILENAME, std::ios::in); 
        if (!in) return history;

        /*if (in.is_open()) {
            json j_data;
            in >> j_data;

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
        }*/

        in.close();

        return history;
    }

    void saveDatabase(const History&) {
        std::ofstream of(DB_FILENAME, std::ios::out);

        /*json j_entries;
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
        };*/

        of.close();
    }    

    std::vector<HistoryEntry> getHistory(AccountUid uid, std::string date) {
        //Get current history
        History history = loadDatabase();

        return history.entries(uid, date);
    }

    HistoryEntry addToHistory(AccountUid uid, u64 titleId, u32 duration_in_minutes) 
    {
        HistoryEntry result;

        std::string uidToString = accountUidToString(uid);

        //Get current history
        History history = loadDatabase();

        //Search existing entry
        auto entries = history.entries(uid, today(), titleId);

        if(!entries.empty()) {
            auto entry = entries[0];
            logToFile("[Database] entry found for user %s with title ID %i\n", accountUidToString(uid), titleId);
            entry.setDurationInMinutes(duration_in_minutes + entry.durationInMinutes());
            logToFile("[Database] new duration is %i", entry.durationInMinutes());
            result = entry;
        } else {
            //Or create a new one
            logToFile("[Database] no entry found for user %s with title ID %i\n", accountUidToString(uid), titleId);
            HistoryEntry entry = HistoryEntry(uid, today(), titleId, duration_in_minutes);
            history.addEntry(entry);
            result = entry;
        }

        saveDatabase(history);
        return result;
    }

    Settings loadSettings() {
        std::ifstream ifs(SETTINGS_FILENAME, std::ios::in);    
        Settings settings;

        /*if(!ifs) {
            logToFile("Could not open settings file. Initialise default settings.\n");
            settings[SETTING_DAILY_LIMIT_GAME].int_value = 1*3600; //Default: 1 hour
            settings[SETTING_DAILY_LIMIT_GLOBAL].int_value = 1*3600; //Default: 1 hour
        } else {
            json j_settings;
            ifs >> j_settings;

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

        }*/

        ifs.close();
        return settings;
    }

    void saveSetting(Settings& settings, Setting& setting) {
        //Update settings internal structure
        settings[setting.key] = setting;

        //Update database file
        std::ofstream ofs(SETTINGS_FILENAME, std::ios::out);    

        /*json j_settings({});

        for(const auto& [key, value] : settings) {
            switch(value.type) {
                case INTEGER: j_settings[key] = value.int_value; break;
                case DOUBLE: j_settings[key] = value.double_value; break;
                case STRING: j_settings[key] = value.string_value; break;
            }
        }

        ofs << j_settings;*/
        ofs.close();
    }
    
}