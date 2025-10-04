#pragma once
#include <switch.h>

struct AppContext {
    bool is_ready = false;
    Service pctrl_service;
};

AppContext& getAppContext();