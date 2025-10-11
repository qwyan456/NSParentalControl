#pragma once

#include "switch.h"
#include "tesla.hpp"

typedef enum {
    NO_ERROR = 0,
    VERIFY_ERROR = 1,
    SERVICE_ERROR = 2
} PinError;

class SetupPinPanel : public tsl::Gui {
public:
    SetupPinPanel();
    ~SetupPinPanel();

    tsl::elm::Element* createUI() override;
    void rebuildUI();
    void update() override;
    void closeAndClean();

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override;

private:    
    tsl::elm::OverlayFrame* rootFrame_ = nullptr;
    tsl::elm::List* rootList_ = nullptr;
    //std::vector<u64> currentPin_;
    std::vector<u64> newPin_;
    std::vector<u64> verifyPin_;
    PinError error_ = NO_ERROR;
};