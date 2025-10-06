#include "helpers.h"
#include <sstream>
#include <iomanip>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <string_view>
#include <ranges>
#include "logger.h"

using namespace alefbet::pctrl::logger;

namespace alefbet::pctrl::helpers {
    std::string titleIdToString(u64 titleId) {
        if(titleId == 0) {
            return std::string("None");        
        }
        
        std::stringstream ss;
        ss << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << titleId;
        return ss.str();
    }

    std::string accountUidToString(AccountUid uid) {
        return std::to_string(uid.uid[0]) + ":" + std::to_string(uid.uid[1]);
    }

    AccountUid accountUidFromString(const std::string& uid_str) {
        AccountUid uid;

        std::vector<std::string> parts;
        for (const auto part : std::views::split(uid_str, ":")) {
            parts.push_back(std::string(part.begin(), part.end()));
        }
        
        if(parts.size() != 2) {
            logToFile("[Helpers] Incorrect split of AccountUid %s.", uid_str);
            return uid;
        }

        uid.uid[0] = std::stoull(parts[0]);
        uid.uid[1] = std::stoull(parts[1]);

        return uid;
    }

    std::string getCurrentUser() {
        ::Result rc = accountInitialize(AccountServiceType_Administrator);
        if(rc != 0) {
            logToFile("[Helpers] Could not initialize account service: %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            return std::string("ERR#003");
        }

        AccountUid uid;
        rc = accountGetPreselectedUser(&uid);
        //logToFile("rc=%i, Uid=%i.%i\n", rc, uid.uid[0], uid.uid[1]);
        rc = accountGetLastOpenedUser(&uid);            
        if(rc != 0) {
            logToFile("[Helpers] Could not get preselected user: %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            accountExit();
            return std::string("ERR#004");
        }

        logToFile("[Helpers] user uid: %i.%i\n", uid.uid[0], uid.uid[1]);

        AccountProfile profile;
        AccountUserData user_data;
        AccountProfileBase base;
        rc = accountGetProfile(&profile, uid); 
        if(rc != 0) {
            logToFile("[Helpers] Could not get account profile: %i\n", rc);
            accountExit();
            return std::string("ERR#005");
        }

        rc = accountProfileGet(&profile, &user_data, &base);
        if(rc != 0) {
            logToFile("[Helpers] Could not get user data: %i\n", rc);
            accountProfileClose(&profile);
            accountExit();
            return std::string("ERR#006");
        }
        
        accountProfileClose(&profile);
        accountExit();

        return std::string(base.nickname);
    }

    u64 getRunningApplicationPid() {
        u64 process_id = 0;                        

        ::Result rc = pmdmntGetApplicationProcessId(&process_id);
        if(rc != 0) {
            logToFile("[Helpers] Could not get application process ID: %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            return 0;
        }

        logToFile("[Helpers] process ID=%i\n", process_id);

        return process_id;
    }

    u64 getRunningApplicationTitleId(u64 process_id) {
        u64 title_id = 0;

        ::Result rc = pmdmntGetProgramId(&title_id, process_id);
        if(rc != 0) {
            logToFile("[Helpers] Could not get the title ID for the process %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            return 0;
        }
        
        auto formatted_title_id = titleIdToString(title_id);
        logToFile("[Helpers] title formatted ID=%s, ID=%i, pid=%i\n", formatted_title_id.c_str(), title_id, process_id);

        return title_id;
    }

    std::string getApplicationName(u64 title_id) {
        NsApplicationControlData buffer;
        u64 actual_size = 0;
        
        ::Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, title_id, std::addressof(buffer), sizeof(buffer), &actual_size);
        if(R_FAILED(rc)) {
            logToFile("[Helpers] Could not get application control data for title %s\n", titleIdToString(title_id));
            return "Unknown";
        }

        return std::string(buffer.nacp.lang[0].name);
    }

    std::string today() {
        std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_r(&t, &tm);
        std::ostringstream oss;
        oss << std::put_time(&tm, "%Y%m%d");
        return oss.str();
    }
}