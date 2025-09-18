#include "panel_main_menu.h"
#include "database.h"
#include "console.h"
#include "version.h"
#include "functions.h"
#include "gui_helpers.h"
#include "logger.h"
#include "pctrl_protocol.h"
#include <tesla.hpp>
#include <switch.h>

MainMenu::MainMenu() {
}

tsl::elm::Element* MainMenu::createUI() {
    std::string subTitle = "Parental Controle is not initialized";
    if(isParentalControlInitialized()) {
        subTitle = isParentalControlActive() ? "Parental Controle is Active" : "Parental Controle is Inactive";
    } else {
        subTitle = "Parental Controle is not initialized";
    }

    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", subTitle);
    rootList_ = new tsl::elm::List();

    /*user_ = getCurrentUser();
    game_ = getCurrentGame(user_);

    loadDatabase(sessions_);*/

    rebuildUI();

    return rootFrame_;
}

void MainMenu::startTest() {
    Service pctrl_service;
    TipcService service;
    Result res = smGetService(&pctrl_service, "pctrl:u");
    if(R_FAILED(res)) {
        logToFile("Overlay: could not open service");
        return;
    }
        
    tipcCreate(&service, pctrl_service.own_handle);
    if(service.session == 0) {
        logToFile("Overlay: could not create Tipc session");
        return;
    }

    /* Start listening */

    /* Send a test command */
    tipcDispatch(&service, CmdTestTimeout);
}

void MainMenu::rebuildUI() {
    //if(!active_) return;

    // The content is layed out using a List containing panels than will
    // be visible or not depending on the needs    

    if(!isParentalControlInitialized()) {
        auto entryInitialize = new tsl::elm::ListItem("Initialize Parental Control");        
        rootList_->addItem(entryInitialize);

        entryInitialize->setClickListener([this, entryInitialize](u64 keys) {
            if(keys & HidNpadButton_A) {
                initializeParentalControl();
                entryInitialize->setText("Initialization finished");
                tsl::goBack();
                return true;
            }

            return false;
        });
    } else {
        const auto users = getUsersList();

        rootList_->addItem(new tsl::elm::CategoryHeader("Choose user", true));
        if(users.empty()) {
            rootList_->addItem(new tsl::elm::ListItem("No user found"));
        } else {
            for(const auto& [uid, nick_name]: users) {
                rootList_->addItem(new tsl::elm::ListItem(nick_name));
            }
        }

        auto entryTest = new tsl::elm::ListItem("Run test");
        rootList_->addItem(entryTest);
        entryTest->setClickListener([this, entryTest](u64 keys) {
            if(keys & HidNpadButton_A) {
                startTest();
                return true;
            }

            return false;
        });

    }

    /*list->addItem(new tsl::elm::ListItem(makeUserProfileLabel(user_)));
    list->addItem(new tsl::elm::ListItem(makeDailyLimitLabel(settings_)));
    list->addItem(new tsl::elm::ListItem(makeRemainingDurationLabel(settings_, sessions_)));*/
    
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