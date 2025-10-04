#pragma once

#include "switch.h"
#include "tesla.hpp"

class SetupMenu : public tsl::Gui {
public:
    SetupMenu();
    ~SetupMenu();

    tsl::elm::Element* createUI() override;
    void rebuildUI();
    void update() override;
    void closeAndClean();

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override;

private:
    

private:    
    tsl::elm::OverlayFrame* rootFrame_ = nullptr;
    tsl::elm::List* rootList_ = nullptr;
};