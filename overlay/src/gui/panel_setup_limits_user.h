#pragma once

#include "switch_helpers.h"
#include <tesla.hpp>
#include <switch.h>

typedef enum {
    NoItem = 0,
    DailyLimitHours = 1,
    DailyLimitMinutes = 2,
    SessionLimitHours = 3,
    SessionLimitMinutes = 4,
    RestDurationHours = 5,
    RestDurationMinutes = 6
} SelectedItem;

using namespace alefbet::pctrl::helpers;

class SetupLimitsUserPanel : public tsl::Gui {
public:
    SetupLimitsUserPanel(const UserData& user);
    ~SetupLimitsUserPanel();

    tsl::elm::Element* createUI() override;
    void rebuildUI();
    void update() override;
    void closeAndClean();

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override;

private:
    void selectNextItem();
    void selectBeforeItem();
    void decreaseValue();
    void increaseValue();
    u16 valueRanged(int value, int diff, int min, int max, bool cycle = true);

private:    
    UserData user_;
    tsl::elm::OverlayFrame* rootFrame_ = nullptr;
    tsl::elm::List* rootList_ = nullptr;
    bool error_ = false;
    SelectedItem selectedItem_ = DailyLimitHours;
    u16 dailyLimitHours_ = 0;
    u16 dailyLimitMinutes_ = 0;
    u16 sessionLimitHours_ = 0;
    u16 sessionLimitMinutes_ = 0;
    u16 restDurationHours_ = 0;
    u16 restDurationMinutes_ = 0;
};
