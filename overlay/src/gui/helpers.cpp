#include "helpers.h"
#include "logger.h"
#include "AppContext.h"
#include "Command.hpp"

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

    std::string getCurrentUser() {
        logToFile("[IPC] Get current used");

        Service service = getAppContext().pctrl_service;
        char currentUser[20] = {0};
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetCurrentUser, currentUser);
        if(R_FAILED(res)) {
            return std::string("Err#102");
        }

        return std::string(currentUser);
    }

    std::string getCurrentTitle() {
        logToFile("[IPC] Get current title");

        Service service = getAppContext().pctrl_service;
        char cAppName[512] = {0};
        Result res = serviceDispatchOut(&service, (u32)Ipc::Command::GetRunningApplication, cAppName);
        if(R_FAILED(res)) {
            return std::string("Err#103");
        }

        return std::string(cAppName);
    }

    u16 getUserUsageTime(std::string uid) {
        logToFile("[IPC] Get usage time for the user");
        
        Service service = getAppContext().pctrl_service;
        u16 usageTime = 0;
        Result res = serviceDispatchInOut(&service, (u32)Ipc::Command::GetUserUsageTime, uid, usageTime);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while queriying the usage time for the user");
            logToFile(uid.c_str());
        }

        return usageTime;
    }

    u16 getUserRemainingTime(std::string uid) {
        logToFile("[IPC] Get remaining time for the user");
        
        Service service = getAppContext().pctrl_service;
        u16 remainingTime = 0;
        Result res = serviceDispatchInOut(&service, (u32)Ipc::Command::GetUserRemainingTime, uid, remainingTime);

        if(R_FAILED(res)) {
            logToFile("[IPC] An error occured while queriying the remaining time for the user");
            logToFile(uid.c_str());
        }

        return remainingTime;
    }

}