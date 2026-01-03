#pragma once
#include <switch.h>

struct AppContext {
    bool is_available = false;
    bool is_enabled = false;    
    Service pctrl_service;
    bool is_debug = true;
    bool needs_refresh = false;
};

AppContext& getAppContext();