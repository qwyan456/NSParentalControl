#include "panel_setup_menu.h"
#include "logger.h"
#include <tesla.hpp>
#include <switch.h>
#include "Command.hpp"
#include "AppContext.h"
#include "helpers.h"
#include "panel_setpin.h"

using namespace alefbet::pctrl;

SetupMenu::SetupMenu() {    
}

SetupMenu::~SetupMenu() {      
}

void SetupMenu::closeAndClean() {
}

tsl::elm::Element* SetupMenu::createUI() {
    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", "Setup");
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}


void SetupMenu::rebuildUI() {

    // Admin menu
    auto entryEnabled = new tsl::elm::ToggleListItem("Enabled", AppContext().is_ready);
    rootList_->addItem(entryEnabled);        
    entryEnabled->setStateChangedListener([] (bool on) {
        logToFile("[Overlay] enable/disable not implemented;");
        return true;
    });

    // Set PIN
    auto entrySetPin = new tsl::elm::ListItem("Set PIN");
    rootList_->addItem(entrySetPin);
    entrySetPin->setClickListener([](u64 keys) -> bool {
        if(keys & HidNpadButton_A) {
            tsl::changeTo<SetupPinPanel>();
            return true;
        }

        return false;
    });

    rootFrame_->setContent(rootList_);
}

void SetupMenu::update() {
    
}

// Called once every frame to handle inputs not handled by other UI elements
bool SetupMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    return false;
}