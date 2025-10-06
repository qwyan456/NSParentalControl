#pragma once
#include <string>
#include <switch.h>

namespace alefbet::pctrl {

    namespace structs {

        typedef struct {
            AccountUid uid;
            std::string nickname;

            bool isValid() const {
                return uid.uid[0] > 0 && uid.uid[0] > 1 && !nickname.empty() && nickname.substr(0, 4) != "ERR#";
            }
        } UserData;

    }

    namespace helpers {
        std::string titleIdToString(u64 titleId);
        std::string accountUidToString(AccountUid uid);
        AccountUid accountUidFromString(const std::string& uid_str);

        structs::UserData getCurrentUser();
        u64 getRunningApplicationPid();
        u64 getRunningApplicationTitleId(u64 process_id);
        std::string getApplicationName(u64 title_id);

        std::string today();
    }
}   
