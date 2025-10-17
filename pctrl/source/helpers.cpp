#include "helpers.h"
#include <sstream>
#include <iomanip>
#include <cinttypes>
#include <iostream>
#include <vector>
#include <string_view>
#include <ranges>
#include <codecvt>
#include <stratosphere.hpp>
#include "logger.h"
#include "ams_bpc.h"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::structs;
using namespace ams::fs;
using std::operator""sv;

namespace alefbet::pctrl::helpers {
    std::string titleIdToString(u64 titleId) {
        if(titleId == 0) {
            return std::string("None");        
        }
        
        std::stringstream ss;
        ss << std::uppercase << std::setfill('0') << std::setw(16) << std::hex << titleId;
        return ss.str();
    }

    UserUid accountUidToString(AccountUid uid) {
        return std::to_string(uid.uid[0]) + ":" + std::to_string(uid.uid[1]);
    }

    AccountUid accountUidFromString(const UserUid& uid_str) {
        AccountUid uid;

        std::vector<std::string> parts;
        for (const auto part : std::views::split(uid_str, ":"sv)) {
            parts.push_back(std::string(part.begin(), part.end()));
        }
        
        if(parts.size() != 2) {
            logToFile("[Helpers] Incorrect split of AccountUid %s.\n", uid_str.c_str());
            return uid;
        }

        uid.uid[0] = std::stoull(parts[0]);
        uid.uid[1] = std::stoull(parts[1]);

        return uid;
    }

    UserData getCurrentUser() {
        UserData user;

        ::Result rc = accountInitialize(AccountServiceType_Administrator);
        if(rc != 0) {
            logToFile("[Helpers] Could not initialize account service: %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            user.nickname = UserNickname("ERR#003");
            return user;
        }

        AccountUid uid;
        rc = accountGetPreselectedUser(&uid);
        //logToFile("rc=%i, Uid=%i.%i\n", rc, uid.uid[0], uid.uid[1]);
        rc = accountGetLastOpenedUser(&uid);            
        if(rc != 0) {
            logToFile("[Helpers] Could not get preselected user: %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
            accountExit();
            user.nickname = UserNickname("ERR#004");
            return user;
        }

        logToFile("[Helpers] user uid: %i.%i\n", uid.uid[0], uid.uid[1]);
        user.uid = uid;

        AccountProfile profile;
        AccountUserData user_data;
        AccountProfileBase base;
        rc = accountGetProfile(&profile, uid); 
        if(rc != 0) {
            logToFile("[Helpers] Could not get account profile: %i\n", rc);
            accountExit();
            user.nickname = UserNickname("ERR#005");
            return user;
        } else {
            logToFile("[Helpers] accountGetProfile() ok\n");
        }

        rc = accountProfileGet(&profile, &user_data, &base);
        if(rc != 0) {
            logToFile("[Helpers] Could not get user data: %i\n", rc);
            accountProfileClose(&profile);
            accountExit();
            user.nickname = UserNickname("ERR#006");
            return user;
        } else {
            logToFile("[Helpers] accountProfileGet() ok\n");
        }
        
        user.nickname = UserNickname(base.nickname);
        logToFile("[Helpers] uid=%i:%i, Nickname=%s\n", user.uid.uid[0], user.uid.uid[1], user.nickname.c_str());

        accountProfileClose(&profile);
        accountExit();

        return user;
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
        ::Result rc = timeInitialize();
        if(R_FAILED(rc)) {
            logToFile("[Helpers] Could not connect to time service\n");
            return "";
        }

        u64 ts;
        TimeCalendarTime time;
        TimeCalendarAdditionalInfo info;
        rc = timeGetCurrentTime(TimeType_LocalSystemClock, &ts);        
        if(R_FAILED(rc)) {
            logToFile("[Helpers] Could not get current time (err %i:%i)\n", R_MODULE(rc), R_DESCRIPTION(rc));
            return "";
        }

        rc = timeToCalendarTimeWithMyRule(ts, &time, &info);
        if(R_FAILED(rc)) {
            logToFile("[Helpers] Could not convert timestamp\n");
            return "";
        }
        
        char buffer[9];
        std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d",
            time.year,
            time.month,
            time.day
        );

        return std::string(buffer);
    }

    /*bool shutdown() {
        logToFile("[Helpers] Try to shutdown...\n");

        ams::pmic::ShutdownSystem(false);        
    }*/

    #define IRAM_PAYLOAD_MAX_SIZE 0x24000
    //static u8 g_reboot_payload[IRAM_PAYLOAD_MAX_SIZE];
    bool rebootToPayload() {
        logToFile("[Helpers] Try to reboot to payload\n");

        bpcRebootSystem();
        return true;

        /*::Result rc = spsmInitialize();
        if(rc != 0) {
            logToFile("[Helpers] Failed to initialize SPSM\n");
            bpcShutdownSystem();
            return false;
        }

        smExit(); //Required to connect to ams:bpc            
        rc = amsBpcInitialize();
        if (rc != 0) {
            logToFile("[Helpers] Failed to initialize ams:bpc: %i\n", rc);
            logToFile("[Helpers] Shutting down.\n");
            bpcShutdownSystem();
            return false;
        }

        static FileHandle handle;
        bool opened = OpenFile(std::addressof(handle), "sdmc:/atmosphere/reboot_payload.bin", OpenMode_Read).IsSuccess();
        //FILE *f = fopen("sdmc:/atmosphere/reboot_payload.bin", "rb");
        if (!opened) {
            logToFile("[Helpers] Failed to load /atmosphere/reboot_payload.bin!\n");
            logToFile("[Helpers] Shutting down.\n");
            bpcShutdownSystem();
            return false;
        } else {
            logToFile("[Helpers] Payload file opened\n");
            s64 file_size = 0;
            if(GetFileSize(&file_size, handle).IsFailure()) {
                logToFile("[Helpers] Failed to get payload file size\n");
                logToFile("[Helpers] Shutting down.\n");
                bpcShutdownSystem();
                return false;
            }

            logToFile("[Helpers] Paylaod file read\n");
            if(ReadFile(handle, 0, g_reboot_payload, file_size).IsFailure()) {
                logToFile("[Helpers] Could not read the payload\n");
                CloseFile(handle);
                logToFile("[Helpers] Shutting down.\n");
                bpcShutdownSystem();
                return false;
            }

            CloseFile(handle);            
            //fread(g_reboot_payload, 1, sizeof(g_reboot_payload), f);
            //fclose(f);
        }

        logToFile("[Helpers] Calling amsBpcSetRebootPayload\n");
        rc = amsBpcSetRebootPayload(g_reboot_payload, IRAM_PAYLOAD_MAX_SIZE);
        if (rc != 0) {
            logToFile("[Helpers] Failed to set reboot to payload: %i\n", rc);
            logToFile("[Helpers] Shutting down.\n");
            bpcShutdownSystem();
        } else {
            logToFile("[Helpers] Rebooting to payload\n");
            spsmShutdown(true);            
        }

        return true;*/
    }
}