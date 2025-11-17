#include "switch_helpers.h"
#include "logger.h"
#include <string_view>
#include <string_view>
#include <ranges>
#include <vector>

namespace alefbet {
    namespace pctrl {
        namespace helpers {        

            using std::operator""sv;

            std::list<UserData> getUsersList() {
                std::list<UserData> users;

                Result rc = accountInitialize(AccountServiceType_System);
                if(R_FAILED(rc)) {
                    logToFile("[Service] Could not connect to service account\n");
                    return std::list<UserData>{};
                }

                AccountUid _users[ACC_USER_LIST_SIZE];
                s32 count = 0;

                rc = accountListAllUsers(_users, ACC_USER_LIST_SIZE, &count);
                if(R_FAILED(rc)) {
                    logToFile("[Service] Could not enumerate users\n");
                    return std::list<UserData>{};
                }

                AccountProfile profile;
                AccountUserData user_data;
                AccountProfileBase base;

                for(int i = 0 ; i < count ; i++) {
                    const auto& uid = _users[i];
                    rc = accountGetProfile(&profile, uid); 
                    if(rc != 0) {
                        logToFile("[Helpers] Could not get account profile");
                        accountExit();                        
                    } else {
                        logToFile("[Helpers] accountGetProfile() ok\n");
                    }

                    rc = accountProfileGet(&profile, &user_data, &base);
                    if(rc != 0) {
                        logToFile("[Helpers] Could not get user data");
                        accountProfileClose(&profile);
                        accountExit();                        
                    } else {
                        logToFile("[Helpers] accountProfileGet() ok\n");
                    }

                    UserData user;
                    user.uid = uid;
                    user.nickname = base.nickname;

                    users.push_back(user);
                }

                return users;
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
                    logToFile("[Helpers] Incorrect split of AccountUid");
                    logToFile(uid_str.c_str());
                    return uid;
                }

                uid.uid[0] = std::stoull(parts[0]);
                uid.uid[1] = std::stoull(parts[1]);

                return uid;
            }
        }
    }
}