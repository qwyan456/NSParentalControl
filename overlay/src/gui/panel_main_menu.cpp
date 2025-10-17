#include "panel_main_menu.h"
#include <tesla.hpp>
#include <switch.h>
#include <chrono>
#include "version.h"
#include "logger.h"
#include "Command.hpp"
#include "panel_debug_menu.h"
#include "panel_setup_menu.h"
#include "panel_verifypin.h"
#include "panel_setup_limits.h"
#include "AppContext.h"
#include "helpers/ipc_helpers.h"

using namespace alefbet::pctrl;
using namespace std::chrono;

MainMenuPanel::MainMenuPanel() {    
}

MainMenuPanel::~MainMenuPanel() {      
}

void MainMenuPanel::closeAndClean() {
}

bool MainMenuPanel::isParentalControlEnabled() {
    getAppContext().is_enabled = ipc::isEnabled();
    return getAppContext().is_enabled;
}

std::list<std::string> MainMenuPanel::getUsersList() {
    std::list<std::string> users;

    return users;
}

tsl::elm::Element* MainMenuPanel::createUI() {
    std::string subTitle = isParentalControlEnabled() ? "Parental Control is Enabled" : "Parental Control is Disabled";    

    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", subTitle);
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}


void MainMenuPanel::rebuildUI() {

    // Current User
    auto currentTitle = ipc::getCurrentTitle();
    logToFile("title=");
    logToFile(currentTitle.c_str());

    if(!currentTitle.empty() && !currentTitle.starts_with("Err#")) {
        auto currentUserUid = ipc::getCurrentUserUid();
        logToFile("user id=");
        logToFile(currentUserUid.c_str());

        auto currentUserNickname = ipc::getCurrentUserNickname();        
        logToFile("user name=");
        logToFile(currentUserNickname.c_str());        

        auto entryUser = new tsl::elm::ListItem("User: " +currentUserNickname);
        rootList_->addItem(entryUser);
        
        // Current game        
        auto entryTitle = new tsl::elm::ListItem("Playing: " +currentTitle);
        rootList_->addItem(entryTitle);

        // Usage time        
        auto usageTimeInMinutes = ipc::getUserUsageTime();
        auto durationInMinutes = minutes{usageTimeInMinutes};
        auto hoursPart = duration_cast<hours>(durationInMinutes);
        auto minutesPart = duration_cast<minutes>(durationInMinutes - hoursPart);
        auto entryUsageTime = new tsl::elm::ListItem("Usage: " +(hoursPart.count() > 0 ? std::to_string(hoursPart.count()) + "h " : "") +std::to_string(minutesPart.count()) +" mn");
        rootList_->addItem(entryUsageTime);

        // Remaining time
        auto remainingTime = ipc::getUserRemainingTime();
        durationInMinutes = minutes{remainingTime};
        hoursPart = duration_cast<hours>(durationInMinutes);
        minutesPart = duration_cast<minutes>(durationInMinutes - hoursPart);
        auto entryRemainingTime = new tsl::elm::ListItem("Remaining: " +(hoursPart.count() > 0 ? std::to_string(hoursPart.count()) + "h " : "") +std::to_string(minutesPart.count()) +" mn");
        rootList_->addItem(entryRemainingTime);

    } else {
        rootList_->addItem(new tsl::elm::ListItem("No user / app started"));
    }        
    
    // Debug menu
    if(getAppContext().is_debug) {
        auto entryDebugMenu = new tsl::elm::ListItem("Debug");
        rootList_->addItem(entryDebugMenu);        
        entryDebugMenu->setClickListener([this](u64 keys) {
            if(keys & HidNpadButton_A) {
                tsl::changeTo<DebugMenu>();
                return true;
            }

            return false;
        });
    }

    // Admin menu
    auto entrySetupMenu = new tsl::elm::ListItem("Admin");
    rootList_->addItem(entrySetupMenu);        
    entrySetupMenu->setClickListener([this](u64 keys) {
        if(keys & HidNpadButton_A) {
            tsl::changeTo<VerifyPinPanel, NextPanel>(PanelSetupMenu);
            //tsl::changeTo<SetupLimitsPanel>();
            return true;
        }

        return false;
    });

    rootFrame_->setContent(rootList_);
}

void MainMenuPanel::update() {
    
}

// Called once every frame to handle inputs not handled by other UI elements
bool MainMenuPanel::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::Overlay::get()->close();
        return true;
    }

    return false;
}