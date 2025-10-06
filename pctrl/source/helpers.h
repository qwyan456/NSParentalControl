#pragma once
#include <string>
#include <switch.h>

namespace alefbet::pctrl::helpers {
    std::string titleIdToString(u64 titleId);
    std::string accountUidToString(AccountUid uid);
    AccountUid accountUidFromString(const std::string& uid_str);

    std::string getCurrentUser();
    u64 getRunningApplicationPid();
    u64 getRunningApplicationTitleId(u64 process_id);
    std::string getApplicationName(u64 title_id);


    std::string today();
}   
