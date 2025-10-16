#pragma once

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
        GetShowRemainingTime
    };
};
