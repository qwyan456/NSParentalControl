#include "notifications_controller.h"
#include "ultrahand_interface.h"
#include <switch.h>
#include <chrono>

using namespace std::chrono;

// 醒目通知参数：比默认(28/1)更大的字号与更高的优先级，让 UltraHand toast 更显眼
constexpr int kNoticeFontSize = 44;
constexpr int kNoticePriority = 5;

// 以「重复发送 + 间隔」的方式延长通知在屏幕上的可见时间。
// UltraHand 的通知 .notify 文件不支持直接设置显示时长，重复写入可使其持续重新显示，
// 从而把可见时间从默认的约 2 秒拉长到约 (repeats-1)*interval + 0.8 秒。
void NotificationsController::notifyWithEmphasis(const std::string& message, int repeats, int intervalMs)
{
    for(int i = 0; i < repeats; ++i) {
        UltraHandInterface::writeNotification(message, kNoticeFontSize, kNoticePriority);
        if(i + 1 < repeats) {
            svcSleepThread((s64)intervalMs * 1'000'000ULL);
        }
    }
    // 让最后一条通知被 UltraHand 读取并显示后再继续（如随后会终止游戏）
    svcSleepThread(800'000'000ULL);
}

void NotificationsController::notifyMonitoringStarted() 
{
    UltraHandInterface::writeNotification("Parental Control enabled");
}

void NotificationsController::notifyRemainingTime(int remainingTimeInMinutes)
{
    //std::string message = std::to_string(remainingTimeInMinutes) + " minute" +(remainingTimeInMinutes > 1 ? "s" : "") +" left";
    std::string message = formatTime(remainingTimeInMinutes) +" left";

    UltraHandInterface::drawAttention();
    UltraHandInterface::writeNotification(message);
}

void NotificationsController::notifyTimeExpired()
{
    notifyWithEmphasis("Time's up! The game will close.");
}

void NotificationsController::notifySessionExpired(int restMin)
{
    std::string message = restMin > 0
        ? "Session time up! Rest " + std::to_string(restMin) + " min."
        : "Session time up! Paused until daily limit resets.";
    notifyWithEmphasis(message);
}

void NotificationsController::notifyRestOver()
{
    notifyWithEmphasis("Break over - you can play now.");
}

void NotificationsController::notifyRestActive(int remainingMin)
{
    std::string message = remainingMin > 0
        ? "Forced rest: " + std::to_string(remainingMin) + " min left. Game closed."
        : "Forced rest active. Game closed until daily limit resets.";
    UltraHandInterface::writeNotification(message, kNoticeFontSize, kNoticePriority);
}

std::string NotificationsController::formatTime(int duration_in_minutes) {
    const auto& duration = minutes{duration_in_minutes};
    auto hoursPart = duration_cast<hours>(duration);
    auto minutesPart = duration_cast<minutes>(duration - hoursPart);
    std::string formatted = std::to_string(hoursPart.count()) + "h " +std::to_string(minutesPart.count()) +"mn";
    return formatted;
}
