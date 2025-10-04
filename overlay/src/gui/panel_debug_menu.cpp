#include "panel_debug_menu.h"
#include "logger.h"
#include <tesla.hpp>
#include <switch.h>
#include "Command.hpp"
#include "AppContext.h"
#include "helpers.h"

using namespace alefbet::pctrl;

DebugMenu::DebugMenu() {}

DebugMenu::~DebugMenu() {  
    closeAndClean();
}

void DebugMenu::closeAndClean() {
}


tsl::elm::Element* DebugMenu::createUI() {
    std::string subTitle = "Debug options";

    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", subTitle);
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}


void DebugMenu::rebuildUI() {
   
    // Run Test
    auto entryTest = new tsl::elm::ListItem("Run test");
    rootList_->addItem(entryTest);
    entryTest->setClickListener([this](u64 keys) {
        if(keys & HidNpadButton_A) {
            ipc::startTest();
            return true;
        }

        return false;
    });

    // Current User
    auto currentUser = ipc::getCurrentUser();
    auto entryUser = new tsl::elm::ListItem("Current user: " +currentUser);
    rootList_->addItem(entryUser);
    
    // Current game
    auto currentTitle = ipc::getCurrentTitle();
    auto entryTitle = new tsl::elm::ListItem("Current title: " +currentTitle);
    rootList_->addItem(entryTitle);    
    
    rootFrame_->setContent(rootList_);
}

void DebugMenu::update() {
    
}

// Called once every frame to handle inputs not handled by other UI elements
bool DebugMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    return false;
}