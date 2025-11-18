#pragma once

#include "switch.h"
#include <tesla.hpp>

class MainMenuPanel : public tsl::Gui {
public:
    MainMenuPanel();
    ~MainMenuPanel();

    tsl::elm::Element* createUI() override;
    void rebuildUI();
    void update() override;    

    // Called once every frame to handle inputs not handled by other UI elements
    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) override;

private:
    void closeAndClean();    
    std::list<std::string> getUsersList();
    bool isParentalControlEnabled();    
    bool isParentalControlInstalled();

private:    
    bool dirty_ = false;
    tsl::elm::OverlayFrame* rootFrame_ = nullptr;
    tsl::elm::List* rootList_ = nullptr;
};