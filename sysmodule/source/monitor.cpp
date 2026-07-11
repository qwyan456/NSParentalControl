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

            // —— 强制休息守卫（全局冷却）——
            // 休息倒计时内，任何游戏一启动就被立即强制关闭；
            // 倒计时结束且当日额度未耗尽后，恢复正常游玩（单次计时归零，可再次循环）。
            if(inRest_) {
                if(pid != 0) {
                    logInfo("[Monitor] Still in forced rest, terminating launched application\n");
                    terminateCurrentApplication();
                    NotificationsController::notifyRestActive(restRemainingMin_); // 提示被关原因与剩余休息时长
                }
                if(--restRemainingMin_ <= 0) {
                    inRest_ = false;
                    sessionElapsedMin_ = 0;
                    logInfo("[Monitor] Forced rest finished, play allowed again\n");
                    NotificationsController::notifyRestOver();
                }
                svcSleepThread(MainLoopDelayInNanos.count());
                continue; // 已 sleep，安全返回循环顶部
            }

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

                        // —— 新增：单次最长可玩时间累计 ——
                        const auto sessionLimit = settings.contains(SETTING_SESSION_LIMIT)
                            ? (u16)settings[SETTING_SESSION_LIMIT].int_value : 0u;
                        const auto restMin = settings.contains(SETTING_REST_DURATION)
                            ? (u16)settings[SETTING_REST_DURATION].int_value : 0u;

                        if(sessionLimit > 0) {
                            sessionElapsedMin_ += MainLoopDelayInMinutes.count(); // 每次循环 +1 分钟
                            logDebug("[Monitor] Session elapsed %i/%i min for user %s\n", sessionElapsedMin_, sessionLimit, user.nickname.c_str());
                        }

                        if(remainingTimeInMinutes <= 0) {
                            // 每日额度耗尽 → 永久封锁（优先于单次休息）
                            logInfo("[Monitor] Timeout for the user %s\n", user.nickname.c_str());
                            // v1.3.5: 不再渲染全屏超时界面。sysmodule 内存受限，1.9MB 帧缓冲
                            // 无法分配（aligned_alloc/tmemCreate 在 512KB 进程堆上失败；
                            // svcSetHeapSize 在 AMS 1.10.3 下被内核拒绝 rc=1:101）。
                            // 改为：弹轻量系统提示 + 直接终止前台游戏，回到 HOME Menu，强制限玩。
                            NotificationsController::notifyTimeExpired();
                            terminateCurrentApplication();
                        } else if(sessionLimit > 0 && sessionElapsedMin_ >= (int)sessionLimit) {
                            // 单次时长达到 → 终止并进入强制休息（当日仍有额度时才进入休息）
                            logInfo("[Monitor] Session limit reached for user %s, forcing rest\n", user.nickname.c_str());
                            NotificationsController::notifySessionExpired(restMin);
                            terminateCurrentApplication();
                            inRest_ = true;
                            restRemainingMin_ = (restMin > 0) ? (int)restMin : 0; // 0=无冷却，直到当日额度耗尽才解封
                            sessionElapsedMin_ = 0;
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
                    sessionElapsedMin_ = 0; // 离开游戏即结束当前单次会话
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