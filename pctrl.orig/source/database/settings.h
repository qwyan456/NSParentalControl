#pragma once

#include <map>
#include <string>
#include <string_view>
#include <switch.h>

using SettingKey = std::string_view;

typedef enum {
    INTEGER = 0,
    DOUBLE = 1,
    STRING = 2
} SettingType;

struct Setting {
    SettingKey key;
    SettingType type = INTEGER;
    u64 int_value = 0;
    double double_value = 0.0;
    std::string string_value;
};

using Settings = std::map<SettingKey, Setting>;

constexpr std::string_view SETTING_DAILY_LIMIT_GAME = "daily_limit_game";
constexpr std::string_view SETTING_DAILY_LIMIT_GLOBAL = "daily_limit_global";
constexpr std::string_view SETTING_ADMIN_PIN = "admin_pin";

