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

namespace {
    // 当前墙钟时间（纳秒），基于 ARM 系统节拍计数器(cntvct_el0, 内核 syscall)。
    // 关键：该计数器由常驻(always-on)硬件计时器驱动，
    //   1) 系统灭屏睡眠时也持续前进（RTC/常驻时钟不随进程挂起而停止）；
    //   2) 不依赖 time 服务，避免 sleep/wake 后 timeGetCurrentTime 偶发返回 0 / 冻结，
    //      导致 `now < restEndTs_` 永远成立、休息倒计时卡在 30 分钟无法结束（坑5 二次修复）。
    // Tegra X1/X2 节拍频率为 19.2MHz：ns = ticks * 1e9 / 19_200_000。
    u64 nowNs() {
        const u64 ticks = armGetSystemTick();
        return (ticks * 1000000000ULL) / 19200000ULL;
    }
}

constexpr std::chrono::minutes MainLoopDelayInMinutes = 1min;
constexpr std::chrono::nanoseconds MainLoopDelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(MainLoopDelayInMinutes); 
// 休息守卫内的轮询间隔更短：让“休息中”提示更频繁出现（避免亮屏瞬间错过 2.5s 弹窗），
// 同时让休息到点后更快解封（原 1 分钟粒度太粗）。
constexpr std::chrono::seconds RestLoopDelayInSeconds = 15s;
constexpr std::chrono::nanoseconds RestLoopDelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(RestLoopDelayInSeconds);

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
                }
                // FIX(坑5): 用绝对墙钟结束时间判断，而非“每循环 -1 分钟”计数器。
                // 系统灭屏睡眠时监控线程被挂起，循环不跑，旧计数器冻结 → 真实时间已超设定休息时长，
                // 亮屏后打开游戏仍被判“休息未到”反复关闭。restEndTs_ 基于 RTC 墙钟（睡眠也前进）。
                const u64 now = nowNs();
                int remainingMin = 0;
                if(now < restEndTs_) {
                    const u64 remainNs = restEndTs_ - now;
                    remainingMin = (int)((remainNs + 59999999999ULL) / 60000000000ULL); // 向上取整到分钟
                    if(remainingMin < 1) remainingMin = 1;
                }
                logInfo("[Monitor] Forced rest: %i min left (now=%llu restEnd=%llu)\n",
                        remainingMin, (unsigned long long)now, (unsigned long long)restEndTs_);
                NotificationsController::notifyRestActive(remainingMin); // 提示被关原因与剩余休息时长
                if(now >= restEndTs_) {
                    inRest_ = false;
                    sessionElapsedMin_ = 0;
                    logInfo("[Monitor] Forced rest finished, play allowed again\n");
                    NotificationsController::notifyRestOver();
                }
                svcSleepThread(RestLoopDelayInNanos.count()); // 休息守卫内 15s 轮询：提示更频繁、到点更快解封
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
                            // FIX(坑5): 用绝对墙钟结束时间，而非“每循环 -1 分钟”计数器。
                            // 系统灭屏睡眠时监控线程被挂起，计数器冻结，真实时间已超设定休息时长，
                            // 亮屏后打开游戏仍被判“休息未到”反复关闭。
                            // restEndTs_ 基于 ARM 系统节拍计数器(nowNs)：睡眠也前进、且不依赖 time 服务，
                            // 亮屏后 nowNs() >= restEndTs_ 即判定休息结束。
                            restEndTs_ = nowNs() + (u64)restMin * 60ULL * 1000000000ULL;
                            logInfo("[Monitor] Forced rest started: %u min, restEndTs_=%llu\n",
                                    restMin, (unsigned long long)restEndTs_);
                            NotificationsController::notifyRestActive(restMin); // 立即提示，避免亮屏瞬间错过
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