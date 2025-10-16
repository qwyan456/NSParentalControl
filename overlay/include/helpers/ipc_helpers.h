#pragma once
#include <string>
#include <vector>
#include <switch.h>

namespace alefbet::pctrl::ipc {
    typedef enum {
        WorkingModeInfo = 0,
        WorkingModeBlocking = 1
    } WorkingMode;

    using UserUid = std::string;
    using UserNickname = std::string;
    constexpr int UserUidMaxLength = 39;

    void startTest();
    UserUid getCurrentUserUid();
    UserNickname getCurrentUserNickname();
    std::string getCurrentTitle();
    u16 getUserUsageTime();
    u16 getUserRemainingTime();

    std::string encodeAdminPin(const std::vector<u64>&);
    //std::vector<u64> decodeAdminPin(const std::string&);
    bool verifyPin(const std::vector<u64>&);
    bool setupPin(const std::vector<u64>&);

    bool setWorkingMode(const WorkingMode&);
    WorkingMode getWorkingMode();
    bool setShowRemainingTime(const bool&);
    bool getShowRemainingTime();
    bool isEnabled();
    bool setEnabled(const bool&);

    std::string getVersion();
}