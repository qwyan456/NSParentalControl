#include "panel_main_menu.h"
#include "version.h"
#include "logger.h"
#include <tesla.hpp>
#include <switch.h>
#include "Command.hpp"
#include "panel_debug_menu.h"
#include "panel_setup_menu.h"
#include "AppContext.h"
#include "helpers.h"

using namespace alefbet::pctrl;

MainMenu::MainMenu() {    
}

MainMenu::~MainMenu() {      
}

void MainMenu::closeAndClean() {
}

bool MainMenu::isParentalControlInitialized() { 
    return true;
}

bool MainMenu::isParentalControlActive() {
    return getAppContext().is_ready;
}

std::list<std::string> MainMenu::getUsersList() {
    std::list<std::string> users;

    return users;
}

tsl::elm::Element* MainMenu::createUI() {
    std::string subTitle = "Parental Control is not initialized";
    if(isParentalControlInitialized()) {
        subTitle = isParentalControlActive() ? "Parental Control is Ready" : "Parental Control is not Ready";
    } else {
        subTitle = "Parental Control is not initialized";
    }

    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", subTitle);
    rootList_ = new tsl::elm::List();
    
    rebuildUI();

    return rootFrame_;
}


void MainMenu::rebuildUI() {

    if(!isParentalControlInitialized()) {
        auto entryInitialize = new tsl::elm::ListItem("Initialize Parental Control");        
        rootList_->addItem(entryInitialize);

        entryInitialize->setClickListener([this, entryInitialize](u64 keys) {
            if(keys & HidNpadButton_A) {
                //initializeParentalControl();
                entryInitialize->setText("Initialization finished");
                tsl::goBack();
                return true;
            }

            return false;
        });
    } else {
        /*const auto users = getUsersList();

        rootList_->addItem(new tsl::elm::CategoryHeader("Choose user", true));
        if(users.empty()) {
            rootList_->addItem(new tsl::elm::ListItem("No user found"));
        } else {
            for(const auto& nick_name: users) {
                rootList_->addItem(new tsl::elm::ListItem(nick_name));
            }
        }*/

        // Current User
        auto currentUser = ipc::getCurrentUser();        
        auto currentTitle = ipc::getCurrentTitle();

        if(!currentUser.empty() && !currentTitle.empty()) {
            auto entryUser = new tsl::elm::ListItem("User: " +currentUser);
            rootList_->addItem(entryUser);
            
            // Current game        
            auto entryTitle = new tsl::elm::ListItem("Playing: " +currentTitle);
            rootList_->addItem(entryTitle);

            // Usage time        
            auto usageTime = ipc::getUserUsageTime(currentUser);        
            auto entryUsageTime = new tsl::elm::ListItem("Usage: " +std::to_string(usageTime) +" mn");
            rootList_->addItem(entryUsageTime);

            // Remaining time
            auto remainingTime = ipc::getUserRemainingTime(currentUser);
            auto entryRemainingTime = new tsl::elm::ListItem("Remaining: " +std::to_string(remainingTime) +" mn");
            rootList_->addItem(entryRemainingTime);

        } else {
            rootList_->addItem(new tsl::elm::ListItem("No user / app started"));
        }        
        
        // Debug menu
        auto entryDebugMenu = new tsl::elm::ListItem("Debug");
        rootList_->addItem(entryDebugMenu);        
        entryDebugMenu->setClickListener([this](u64 keys) {
            if(keys & HidNpadButton_A) {
                tsl::changeTo<DebugMenu>();
                return true;
            }

            return false;
        });

        // Admin menu
        auto entrySetupMenu = new tsl::elm::ListItem("Setup");
        rootList_->addItem(entrySetupMenu);        
        entrySetupMenu->setClickListener([this](u64 keys) {
            if(keys & HidNpadButton_A) {
                tsl::changeTo<SetupMenu>();
                return true;
            }

            return false;
        });

    }

    rootFrame_->setContent(rootList_);
}

void MainMenu::update() {
    
}

// Called once every frame to handle inputs not handled by other UI elements
bool MainMenu::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    return false;
}