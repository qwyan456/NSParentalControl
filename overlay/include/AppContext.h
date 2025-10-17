#pragma once
#include <switch.h>

struct AppContext {
    bool is_enabled = false;    
    Service pctrl_service;
    bool is_debug = false;
};

AppContext& getAppContext();