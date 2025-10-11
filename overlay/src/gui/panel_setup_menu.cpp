#include "panel_setup_menu.h"
#include "logger.h"
#include <tesla.hpp>
#include <switch.h>
#include "Command.hpp"
#include "AppContext.h"
#include "helpers/ipc_helpers.h"
#include "panel_setuppin.h"
#include "panel_main_menu.h"

using namespace alefbet::pctrl;

SetupMenuPanel::SetupMenuPanel() {    
}

SetupMenuPanel::~SetupMenuPanel() {      
}

void SetupMenuPanel::closeAndClean() {
}

tsl::elm::Element* SetupMenuPanel::createUI() {
    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", "Setup");
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}


void SetupMenuPanel::rebuildUI() {

    // Enable parental control
    auto entryEnabled = new tsl::elm::ToggleListItem("Enabled", AppContext().is_ready);
    rootList_->addItem(entryEnabled);        
    entryEnabled->setStateChangedListener([] (bool on) {
        logToFile("[Overlay] enable/disable not implemented;");
        return true;
    });

    // Setup PIN
    auto entrySetPin = new tsl::elm::ListItem("Set PIN");
    rootList_->addItem(entrySetPin);
    entrySetPin->setClickListener([](u64 keys) -> bool {
        if(keys & HidNpadButton_A) {
            tsl::changeTo<SetupPinPanel>();
            return true;
        }

        return false;
    });

    // Set working mode
    const auto& workingMode = ipc::getWorkingMode();

    auto entrySetWorkingMode = new tsl::elm::ToggleListItem("Working mode", workingMode, "Blocking", "Information");
    rootList_->addItem(entrySetWorkingMode);
    entrySetWorkingMode->setClickListener([entrySetWorkingMode](u64 keys) -> bool {
        if(keys & HidNpadButton_A) {            
            bool res = ipc::setWorkingMode(entrySetWorkingMode->getState() ? ipc::WorkingModeBlocking : ipc::WorkingModeInfo);
            if(!res) {
                entrySetWorkingMode->setState(!entrySetWorkingMode->getState());
            }
            return true;
        }

        return false;
    });

    const auto& remainingTimeVisibility = ipc::getShowRemainingTime();
    auto entryShowRemainingTime = new tsl::elm::ToggleListItem("Working mode", remainingTimeVisibility);
    rootList_->addItem(entryShowRemainingTime);
    entryShowRemainingTime->setClickListener([entryShowRemainingTime](u64 keys) -> bool {
        if(keys & HidNpadButton_A) {            
            bool res = ipc::setShowRemainingTime(entryShowRemainingTime->getState());
            if(!res) {
                entryShowRemainingTime->setState(!entryShowRemainingTime->getState());
            }
            return true;
        }

        return false;
    });

    // Show remaining time
    

    // Setup global limit
    /*auto entrySetPin = new tsl::elm::("Set PIN");
    rootList_->addItem(entrySetPin);
    entrySetPin->setClickListener([](u64 keys) -> bool {
        if(keys & HidNpadButton_A) {
            tsl::changeTo<SetupPinPanel>();
            return true;
        }

        return false;
    });*/

    rootFrame_->setContent(rootList_);
}

void SetupMenuPanel::update() {
    
}

// Called once every frame to handle inputs not handled by other UI elements
bool SetupMenuPanel::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::changeTo<MainMenuPanel>();
        return true;
    }

    return false;
}