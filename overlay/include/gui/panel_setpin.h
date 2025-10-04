#pragma once

#include "switch.h"
#include "tesla.hpp"

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
    std::vector<u64> currentPin_;
    std::vector<u64> newPin_;
    std::vector<u64> verifyPin_;
    bool updateOvl_ = false;
};