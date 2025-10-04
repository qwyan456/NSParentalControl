#pragma once
#include <string>
#include <switch.h>

namespace alefbet::pctrl::ipc {

    void startTest();
    std::string getCurrentUser();
    std::string getCurrentTitle();
    u16 getUserUsageTime(std::string uid);
    u16 getUserRemainingTime(std::string uid);

}