#include "helpers/ipc_helpers.h"
#include "logger.h"
#include "AppContext.h"
#include "Command.hpp"
#include <string_view>
#include <ranges>
#include <string>
#include <cstring>

namespace alefbet::pctrl::ipc {

    constexpr u8 PIN_LEN_MAX = 20*4+3;

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

    /*std::vector<u64> decodeAdminPin(const std::string& pin) {
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
    }*/

    bool verifyPin(const std::vector<u64>& pin) {
        logToFile("[IPC] Verify Admin PIN");

        Service service = getAppContext().pctrl_service;
        bool verified = false;
        u64 a_pin[4] = { 
            pin[0], pin[1], pin[2], pin[3]
        };        
        //std::strncpy(pin_str, pin.c_str(), pin.size());
        logToFile("PIN");
        logToFile(encodeAdminPin(pin).c_str());
        Result res = serviceDispatchInOut(&service, (u32)Ipc::Command::VerifyAdminPin, a_pin, verified);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured during the Admin PIN verification.");
            return false;
        }

        return verified;
    }

    bool setupPin(const std::vector<u64>& pin) {
        logToFile("[IPC] Setup new PIN");

        Service service = getAppContext().pctrl_service;
        //char pin_str[PIN_LEN_MAX+1] = {0};
        //std::strncpy(pin_str, pin.c_str(), pin.size());
        u64 a_pin[4] = { 
            pin[0], pin[1], pin[2], pin[3]
        }; 
        logToFile("PIN");
        logToFile(encodeAdminPin(pin).c_str());
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::SetAdminPin, a_pin);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured during the setup of the Admin PIN.");
            return false;
        }

        return true;
    }

    bool setWorkingMode(const WorkingMode& mode) {
        logToFile("[IPC] Setting working mode");

        Service service = getAppContext().pctrl_service;
        int _mode = (u8)mode;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::SetWorkingMode, _mode);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while setting up the working mode.");
            return false;
        }

        return true;
    }

    WorkingMode getWorkingMode() {
        logToFile("[IPC] Getting working mode");

        WorkingMode workingMode = WorkingModeInfo;

        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::GetWorkingMode, workingMode);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while getting the working mode.");
        }

        return workingMode;
    }
    
    bool setShowRemainingTime(const bool& active) {
        logToFile("[IPC] Setting remaining time visibility");

        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::SetShowRemainingTime, active);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while setting up remaining time visibility.");
            return false;
        }

        return true;
    }

    bool getShowRemainingTime() {
        logToFile("[IPC] Getting remaining time visibility");

        u8 visible = 0;

        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::GetShowRemainingTime, visible);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while getting the remaining time visibility.");
        }

        return visible == 1 ? true : false;
    }

    bool setEnabled(const bool& enabled) {
        if(enabled)
            logToFile("[IPC] Setting service enabled");
        else 
            logToFile("[IPC] Setting service disabled");

        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::SetEnabled, enabled);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while change the state of the service.");
            return false;
        }

        return true;
    }

    bool isEnabled() {
        logToFile("[IPC] Getting current state of the service");

        u8 enabled = 0;

        Service service = getAppContext().pctrl_service;
        Result res = serviceDispatchIn(&service, (u32)Ipc::Command::IsEnabled, enabled);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while getting the current service state.");
        } else {
            logToFile("[IPC] Service state is");
            logIntToFile(enabled);
        }

        return enabled == 1 ? true : false;
    }
}