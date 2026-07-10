#include "monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include "notifications_controller.h"
#include <chrono>
#include <list>
//#include "pctrl_screen.hpp"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;
using namespace std::chrono_literals;

constexpr std::chrono::minutes MainLoopDelayInMinutes = 1min;
constexpr std::chrono::nanoseconds MainLoopDelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(MainLoopDelayInMinutes); 

namespace alefbet::pctrl::srv {   

    void Monitor::start() {
        if(running_) return;
        running_ = true;
    }

    void Monitor::loop() {        
        logDebug("[Monitor] Starting monitoring loop\n");        
        structs::UserData user;
        u64 pid = 0;

        while(true) {
            if(!running_) {
                //Do nothing
                svcSleepThread(500'000'000); // Wait for 500ms
                continue;
            }

            if(!notified_) {
                logInfo("[Monitoring] Parental control is now enabled.\n");
                NotificationsController::notifyMonitoringStarted();
                notified_ = true;
            }
            
            logDebug("[Monitor] Monitoring loop has started\n");

            // 心跳：每 10 分钟（10 次循环）输出一条 INFO 日志，
            // 便于确认 sysmodule 仍在运行（而非静默停止/崩溃）
            {
                static int heartbeatCounter = 0;
                if((++heartbeatCounter % 10) == 0) {
                    logInfo("[Monitor] Heartbeat: monitoring still active (loop #%i)\n", heartbeatCounter);
                }
            }

            auto settings = loadSettings();

            // Verify whether the service is enabled
            if(settings.contains(SETTING_ENABLED)) {
                const auto setting = settings[SETTING_ENABLED];
                if(setting.int_value == 0) {
                    logInfo("[Monitor] Parental Control is now disabled.\n");
                    running_ = false;
                    return;
                }
            }            

            logDebug("[Monitor] Update usages\n");
            // Query the active application and user
            // and update the database
            pid = getRunningApplicationPid();
            u16 daily_limit = 0;            
            
            if(pid != 0) { 
                const auto title_id = getRunningApplicationTitleId(pid);

                // FIX: 不监控 HOME Menu (qlaunch) 和其他系统应用
                // HOME Menu 的 title_id 是 0x0100000000000023
                // 系统应用 title_id 以 0x01000000000010xx 开头
                if(title_id == 0x0100000000000023ULL || 
                   (title_id >= 0x0100000000001000ULL && title_id <= 0x010000000000101FULL)) {
                    logDebug("[Monitor] System app (0x%016llx) running, skipping\n", title_id);
                    currentTitle_ = 0;
                } else {
                    user = getCurrentUser();

                    if(user.isValid()) {
                        logDebug("[Monitor] Title %i is currently in used by %s. Updating history.\n", title_id, user.nickname.c_str());                    

                        const auto& entry = addToHistory(user.uid, title_id, MainLoopDelayInMinutes.count());

                        if(!entry.isValid()) {
                            logError("[Monitor] The database entry is corrupted\n");
                            continue;
                        }

                        const auto totalDuration = getUserUsageTimeForToday(user.uid);                    

                        const auto& userId = accountUidToString(user.uid);
                        daily_limit = getDailyLimitForUser(userId);

                        u16 remainingTimeInMinutes = daily_limit > totalDuration ? daily_limit - totalDuration : 0;
                        logDebug("[Monitor] Remaining time for user %s is %i minutes. Daily limit=%i\n", user.nickname.c_str(), remainingTimeInMinutes, daily_limit);

                        if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
                            const auto& showRemainingTime = settings[SETTING_SHOW_REMAINING_TIME].int_value > 0;                                                
                            if(showRemainingTime) {                            
                                if(shouldSendNotification(remainingTimeInMinutes)) {
                                    NotificationsController::notifyRemainingTime(remainingTimeInMinutes);
                                }
                            }
                        }

                        if(remainingTimeInMinutes <= 0) {
                            logInfo("[Monitor] Timeout for the user %s\n", user.nickname.c_str());
                            service_->showScreenTimeout();
                        }
                    } else {
                        logDebug("[Monitor] No user found\n");
                    }
                }
            } else {
                logDebug("[Monitor] No title running\n");

                if(currentTitle_ > 0) {
                    logDebug("[Monitor] The game has been closed\n");

                    currentTitle_ = 0;
                }
            }

            logDebug("[Monitoring] loop\n");
            svcSleepThread(MainLoopDelayInNanos.count()); //Wait 1 minute
        }

        logInfo("[Monitor] Stopped monitoring.\n");
    }

    void Monitor::stop() {
        logInfo("[Monitor] Stopping monitor\n");
        running_ = false;
        notified_ = false;
    }

    bool Monitor::shouldSendNotification(int remainingTimeInMinutes) {
        // Remaining time notitications are sent every 15 minutes and every minute during the last 5 minutes
        return remainingTimeInMinutes % 15 == 0 || remainingTimeInMinutes <= 5;
    }
}