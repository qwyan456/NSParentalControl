#include "pctrl_monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include "pctrl_screen.hpp"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;

constexpr std::chrono::minutes DelayInMinutes(1);
constexpr std::chrono::nanoseconds DelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(DelayInMinutes); 

namespace alefbet::pctrl::srv {

    /*! \brief Returns the remaining time for the current user in minutes
    The remaining time can be negative when the user outpass his limit.
    */
    s16 Monitor::remainingTimeInMinutes(const HistoryEntry& entry) {
        auto settings = loadSettings();
        const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;

        logToFile("[Monitor] daily limit=%i\n", daily_limit);
        logToFile("[Monitor] duration=%i\n", entry.durationInMinutes());

        return daily_limit - entry.durationInMinutes();            
    }

    void Monitor::start() {
        if(running_) return;
        running_ = true;
    }

    void Monitor::loop() {        
        logToFile("[Monitor] Starting monitoring loop\n");        
        //WorkingMode workingMode = WorkingModeInfo;

        while(true) {
            if(!running_) {
                //Do nothing
                svcSleepThread(1000*500); // Wait for 500ms
                continue;
            }

            auto settings = loadSettings();

            // Verify whether the service is enabled
            if(settings.contains(SETTING_ENABLED)) {
                const auto setting = settings[SETTING_ENABLED];
                if(setting.int_value == 0) {
                    logToFile("[Monitor] parental control is now disabled.\n");
                    running_ = false;
                    return;
                }
            }            

            logToFile("[Monitor] Update usages\n");
            // Query the active application and user
            // and update the database
            const auto pid = getRunningApplicationPid();
            const auto title_id = getRunningApplicationTitleId(pid);

            if(title_id != 0) { 
                const auto user = getCurrentUser();

                if(user.isValid()) {
                    logToFile("[Monitor] Title %i is currently in used by %s. Updating history.\n", title_id, user.nickname.c_str());                    

                    for(const auto& [key, value]: settings) {
                        std::string _key(key);
                        logToFile("[Monitor] setting key: %s\n", _key.c_str());
                    }

                    //Update working mode
                    //Show remaining time is disabled
                    /*if(settings.contains(SETTING_WORKING_MODE)) {
                        workingMode = (WorkingMode)settings[SETTING_WORKING_MODE].int_value;
                        logToFile("[Monitor] Working mode is %i\n", workingMode);
                    }

                    if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
                        if(settings[SETTING_SHOW_REMAINING_TIME].int_value == 1) {
                            logToFile("[Monitor] Show remaining time.\n");
                            service_->gui().ShowRemainingTime();
                        }
                    } else {
                        logToFile("[Monitor] Force show remaining time (TEST)\n");
                        service_->gui().ShowRemainingTime();
                    }*/

                    const auto entry = addToHistory(user.uid, title_id, DelayInMinutes.count());

                    if(!entry.isValid()) {
                        continue;
                    }
                    
                    // After database update we need to verify the limits
                    const auto remaining_time = remainingTimeInMinutes(entry);
                    const auto uid_str = accountUidToString(user.uid);                    

                    //service_->gui().UpdateRemainingTime(remaining_time);

                    if(remaining_time <= 0) {
                        logToFile("[Monitor] Timeout for the user %s\n", user.nickname.c_str());
                        service_->gui().ShowScreenTimeout();
                    } else if(remaining_time <= 5) {
                        logToFile("[Monitor] Remaining time for user %i is exactly 5 minutes. Warn the user.\n", uid_str);
                        //service_->gui().ShowScreenWarning();
                    } else {
                        logToFile("[Monitor] Remaining time for user %s is %i minutes.\n", user.nickname.c_str(), remaining_time);
                    }
                } else {
                    logToFile("[Monitor] No user found\n");
                }
            } else {
                logToFile("[Monitor] No title running\n");
            }

            svcSleepThread(DelayInNanos.count()); //Wait for a minute
        }

        logToFile("[Monitor] Stopped monitoring.\n");
    }

    void Monitor::stop() {
        logToFile("[Monitor] Stopping monitor\n");
        running_ = false;
    }

}