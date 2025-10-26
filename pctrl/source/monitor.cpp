#include "monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include <chrono>
//#include "pctrl_screen.hpp"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;
using namespace std::chrono_literals;

constexpr std::chrono::minutes MainLoopDelayInMinutes = 1min;
constexpr std::chrono::nanoseconds MainLoopDelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(MainLoopDelayInMinutes); 
constexpr std::chrono::milliseconds SubLoopDelayInMillis = 1000ms;
constexpr std::chrono::nanoseconds SubLoopDelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(SubLoopDelayInMillis); 

namespace alefbet::pctrl::srv {

    /*! \brief Returns the remaining time for the current user in minutes
    The remaining time can be negative when the user outpass his limit.
    */
    /*s16 Monitor::remainingTimeInMinutes(const HistoryEntry& entry, const u16& daily_limit) {        
        logToFile("[Monitor] daily limit=%i\n", daily_limit);
        logToFile("[Monitor] duration=%i\n", entry.durationInMinutes());

                 
    }*/

    void Monitor::start() {
        if(running_) return;
        running_ = true;
    }

    void Monitor::loop() {        
        logToFile("[Monitor] Starting monitoring loop\n");        
        //WorkingMode workingMode = WorkingModeInfo;        
        u64 pid = 0;

        while(true) {
            if(!running_) {
                //Do nothing
                svcSleepThread(1000*500); // Wait for 500ms
                continue;
            }
            
            logToFile("[Monitor] Monitoring loop has started\n");
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
            pid = getRunningApplicationPid();            
            
            const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;

            if(pid != 0) { 
                const auto title_id = getRunningApplicationTitleId(pid);
                const auto user = getCurrentUser();

                if(user.isValid()) {
                    logToFile("[Monitor] Title %i is currently in used by %s. Updating history.\n", title_id, user.nickname.c_str());                    

                    /*for(const auto& [key, value]: settings) {
                        std::string _key(key);
                        logToFile("[Monitor] setting key: %s\n", _key.c_str());
                    }*/

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

                    const auto entry = addToHistory(user.uid, title_id, MainLoopDelayInMinutes.count());                    

                    if(!entry.isValid()) {
                        continue;
                    }                    

                    //const auto uid_str = accountUidToString(user.uid);
                    u16 remainingTimeInMinutes = daily_limit > entry.durationInMinutes() ? daily_limit - entry.durationInMinutes() : 0;
                    logToFile("[Monitor] Remaining time for user %s is %i minutes. Daily limit=%i\n", user.nickname.c_str(), remainingTimeInMinutes, daily_limit);

                    if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
                        const auto& showRemainingTime = settings[SETTING_SHOW_REMAINING_TIME].int_value > 0;                                                
                        if(showRemainingTime) {
                            if(!service_->gui().isRemainingTimePanelVisible()) {
                                logToFile("[Monitor] Show the remaining time panel\n");
                                service_->gui().showRemainingTimePanel();
                            }

                            logToFile("[Monitor] Update the remaining time panel\n");
                            service_->gui().updateRemainingTimePanel(remainingTimeInMinutes, daily_limit); 
                        }
                    }
                    
                    // After database update we need to verify the limits
                    //const auto remaining_time = remainingTimeInMinutes(entry);                                    

                    if(remainingTimeInMinutes <= 0) {
                        logToFile("[Monitor] Timeout for the user %s\n", user.nickname.c_str());
                        //service_->gui().ShowScreenTimeout();
                    } /*else {
                        logToFile("[Monitor] Remaining time for user %s is %i minutes.\n", user.nickname.c_str(), remainingTimeInMinutes);
                        service_->gui().updateRemainingTimePanel(remainingTimeInMinutes, daily_limit);
                    }*/
                } else {
                    logToFile("[Monitor] No user found\n");
                }
            } else {
                logToFile("[Monitor] No title running\n");
                if(service_->gui().isRemainingTimePanelVisible()) {
                    service_->gui().hideRemainingTimePanel();
                }
                logToFile("[Monitor] ok\n");
            }

            // Sub-loop to monitor games usage and show/hide the remaining time panel
            for(int i = 0 ; i < MainLoopDelayInNanos.count() / SubLoopDelayInNanos.count() ; ++i) {
                pid = getRunningApplicationPid();   
                if(pid == 0 && service_->gui().isRemainingTimePanelVisible()) {
                    // If no app is running we hide the remaining time panel
                    service_->gui().hideRemainingTimePanel();
                }

                svcSleepThread(SubLoopDelayInNanos.count()); // Wait a little
            }

            logToFile("[Monitoring] loop\n");
            //svcSleepThread(MainLoopDelayInNanos.count()); //Wait 1 minute
        }

        logToFile("[Monitor] Stopped monitoring.\n");
    }

    void Monitor::stop() {
        logToFile("[Monitor] Stopping monitor\n");
        running_ = false;
    }

}