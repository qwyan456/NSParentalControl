#pragma once

#include <string>

namespace Ipc {
    enum class Command {
        Version,            /*! Queries the server version. Result: the version as a string. */
        Test,               /*! Triggers a test of the service. Result: none. */
        Initialize,         /*! Triggers the initialization of the system. */
        GetUsersList,       /*! Queries the current users list. Returns a list of strings containing the users names registered on the system. */
        GetUserUsageTime,   /*! Queries a user's usage data. Returns a data structure with the information of the user. */
        GetUserRemainingTime,
        GetCurrentUserUid,     /*! Returns the current profile. */
        GetCurrentUserNickname,     /*! Returns the current profile. */
        GetRunningApplication,
        SetUserLimits,      /*! Sets a users limits. */
        SetAdminPin,        /*! Sets the administrator PIN. */
        VerifyAdminPin,     /*! Verifies the PIN entered by the admin. */  
        SetWorkingMode,     /*! Sets the working mode (information, blocking) */   
        GetWorkingMode,
        SetShowRemainingTime,  /*! Activate the remaining time badge in the upper right corner*/
        GetShowRemainingTime,
        IsEnabled,
        SetEnabled,
        GetDailyLimit,
        SetDailyLimit
    };


    std::string commandToString(const Command& cmd) {
        switch(cmd) {
            case Command::Version: return "Version";
            case Command::Test: return "Test";
            case Command::Initialize: return "Initialize";
            case Command::GetUsersList: return "GetUsersList";
            case Command::GetUserUsageTime: return "GetUserUsageTime";
            case Command::GetUserRemainingTime: return "GetUserRemainingTime";
            case Command::GetCurrentUserUid: return "GetCurrentUserUid";
            case Command::GetCurrentUserNickname: return "GetCurrentUserNickname";
            case Command::GetRunningApplication: return "GetRunningApplication";
            case Command::SetUserLimits: return "SetUserLimits";
            case Command::SetAdminPin: return "SetAdminPin";
            case Command::VerifyAdminPin: return "VerifyAdminPin";
            case Command::SetWorkingMode: return "SetWorkingMode";
            case Command::GetWorkingMode: return "GetWorkingMode";
            case Command::SetShowRemainingTime: return "SetShowRemainingTime";
            case Command::GetShowRemainingTime: return "GetShowRemainingTime";
            case Command::IsEnabled: return "IsEnabled";
            case Command::SetEnabled: return "SetEnabled";
            case Command::GetDailyLimit: return "GetDailyLimit";
            case Command::SetDailyLimit: return "SetDailyLimit";
            default: return "Unknown";
        }
    };
};
