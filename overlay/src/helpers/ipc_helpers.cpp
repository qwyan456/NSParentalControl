#include "helpers/ipc_helpers.h"
#include "logger.h"
#include "AppContext.h"
#include "Command.hpp"
#include <string_view>
#include <ranges>
#include <string>
#include <cstring>

namespace alefbet::pctrl::ipc {

    void startTest() {
        logToFile("[IPC] starting test");
            
        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatch(&service, (u32)Ipc::Command::Test); 
        if(R_FAILED(res)) {
            logToFile("[IPC] serviceDispatch failed");
            logIntToFile(R_MODULE(res));
            logIntToFile(R_DESCRIPTION(res));
            return;
        }

        logToFile("[IPC] service dispatch OK");
    }

    UserUid getCurrentUserUid() {
        logToFile("[IPC] Get current user Uid");

        Service service = getAppContext().pctrl_service;
        char currentUser[50] = {0};
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetCurrentUserUid, currentUser);
        if(R_FAILED(res)) {
            return UserUid("Err#102");
        }

        UserUid user_id = UserUid(currentUser);
        logToFile("Recv uid = ");
        logToFile(user_id.c_str());
        if(user_id == "(NULL)") {
            return "";
        }

        return user_id;
    }

    UserNickname getCurrentUserNickname() {
        logToFile("[IPC] Get current user Nickname");

        Service service = getAppContext().pctrl_service;
        char currentUser[21] = {0};
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetCurrentUserNickname, currentUser);
        if(R_FAILED(res)) {
            return UserNickname("Err#102");
        }

        UserNickname nickname = currentUser;
        if(nickname == "(NULL)") {
            return "";
        }

        return nickname;
    }

    std::string getCurrentTitle() {
        logToFile("[IPC] Get current title");

        Service service = getAppContext().pctrl_service;
        char cAppName[512] = {0};
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetRunningApplication, cAppName);
        if(R_FAILED(res)) {
            return std::string("Err#103");
        }

        std::string title = cAppName;
        if(title == "(NULL)") {
            return "";
        }

        return title;
    }

    u16 getUserUsageTime() {
        logToFile("[IPC] Get usage time for the user");
                
        Service service = getAppContext().pctrl_service;
        u16 usageTime = 0;
        
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetUserUsageTime, usageTime);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while queriying the usage time for the user");
        }

        return usageTime;
    }

    u16 getUserRemainingTime() {
        logToFile("[IPC] Get remaining time for the user");
        
        Service service = getAppContext().pctrl_service;
        u16 remainingTime = 0;
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetUserRemainingTime, remainingTime);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while queriying the remaining time for the user");
        }

        return remainingTime;
    }

    std::string encodeAdminPin(const std::vector<u64>& keys) {
        std::string pin;

        if(keys.size() != 4) {
            logToFile("[IPC] The PIN size is wrong.");
            return pin;
        }

        pin = std::to_string(keys[0]) +"," +std::to_string(keys[1]) +"," +std::to_string(keys[2]) +"," +std::to_string(keys[3]);
        logToFile("[IPC] Encoded PIN is");
        logToFile(pin.c_str());

        return pin;
    }

    std::vector<u64> decodeAdminPin(const std::string& pin) {
        std::vector<u64> keys;

        for (const auto part : std::views::split(pin, ",")) {
            std::string val = std::string(part.begin(), part.end());
            keys.push_back(std::stoull(val));
        }

        if(keys.size() != 4) {
            logToFile("[IPC] The decoded PIN is not 4 digits long.");
            return keys;
        }

        logToFile("[IPC] Decoded Admin PIN:");
        logIntToFile(keys[0]);
        logIntToFile(keys[1]);
        logIntToFile(keys[2]);
        logIntToFile(keys[3]);

        return keys;
    }

    bool verifyPin(const std::string& pin) {
        logToFile("[IPC] Verify Admin PIN");

        Service service = getAppContext().pctrl_service;
        bool verified = false;
        char pin_str[8] = {0};
        std::strncpy(pin_str, pin.c_str(), pin.size());
        logToFile("PIN");
        logToFile(pin_str);
        Result res = serviceDispatchInOut(&service, (u32)Ipc::Command::VerifyAdminPin, pin_str, verified);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured during the Admin PIN verification.");
            return false;
        }

        return verified;
    }

    /*IpcData ipcBufferToData(u8* buffer, size_t length) {
        IpcData data;

        // We parse the buffer
        for(size_t i = 0 ; i < length-1 ; i++) {
            // First IpcDataElement
            IpcDataType type = static_cast<IpcDataType>(buffer[i]);
            if(type < IpcString) {
                logToFile("[IPC] The IPC type is unknown.");
                logIntToFile(buffer[i]);
                return data;
            }

            IpcDataElement elm {
                .type = type
            };

            switch(type) {
                case IpcInteger: elm.value = buffer[i+1];
            }
        }

        return data;
    }*/

}