#include "panel_history_user.h"
#include <switch.h>
#include <chrono>
#include <string>
#include "version.h"
#include "logger.h"
#include "Command.hpp"
#include "AppContext.h"
#include "helpers/ipc_helpers.h"

using namespace alefbet::pctrl;
using namespace alefbet::pctrl::logger;
using namespace std::chrono;

HistoryUserPanel::HistoryUserPanel(const UserData& user) {    
    user_ = user;
}

HistoryUserPanel::~HistoryUserPanel() {      
}

tsl::elm::Element* HistoryUserPanel::createUI() {
    rootFrame_ = new tsl::elm::OverlayFrame("Parental Control", "Usage history");
    rootList_ = new tsl::elm::List();
    
    rebuildUI();        

    return rootFrame_;
}

void HistoryUserPanel::rebuildUI() {
    rootList_->addItem(new tsl::elm::CategoryHeader("Today"));    

    duration today_usage = ipc::getUserUsageTime(user_.uid);
    hours today_usage_h = duration_cast<hours>(today_usage);
    duration today_remaining = ipc::getUserRemainingTime(user_.uid);
    hours today_remaining_h = duration_cast<hours>(today_remaining);

    logDebug("Usage: %i, remaining:%i\n", today_usage.count(), today_remaining.count());

    std::string strUsage = std::to_string(today_usage_h.count()) +"h " +std::to_string( (today_usage - today_usage_h).count() ) +"mn";
    std::string strRemaining = std::to_string(today_remaining_h.count()) +"h " +std::to_string( (today_remaining - today_remaining_h).count() ) +"mn";

    rootList_->addItem(new tsl::elm::ListItem("Usage", strUsage));
    rootList_->addItem(new tsl::elm::ListItem("Remaining", strRemaining));

    rootFrame_->setContent(rootList_);
}

void HistoryUserPanel::update() {    
}

// Called once every frame to handle inputs not handled by other UI elements
bool HistoryUserPanel::handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchPos, HidAnalogStickState joyStickPosLeft, HidAnalogStickState joyStickPosRight) {
    if (keysDown & HidNpadButton_B) {
        tsl::goBack();
        return true;
    }

    return false;
}