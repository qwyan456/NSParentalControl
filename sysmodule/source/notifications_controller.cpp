#include "notifications_controller.h"
#include "ultrahand_interface.h"
#include <switch.h>
#include <chrono>

using namespace std::chrono;

// 醒目通知参数：比默认(28/1)更大的字号与更高的优先级，让 UltraHand toast 更显眼
// 注意：UltraHand 通知弹窗固定 448px 宽、字号被钳到 34、文本居中绘制且超出部分被
// scissor 裁掉（两端都会丢字）。已在本机用 DejaVuSans(比 Switch 共享字体更宽)做保守
// 上界测量：所有提示文案在 font=34 下实测 ≤ 348px（余量 ~100px），可确保真机不裁切。
constexpr int kNoticeFontSize = 44;   // 会被 UltraHand 钳到 34
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
    // 原 "Time's up! The game will close." 在 448px/34 字号下被裁切(实测 461px)，缩短为 348px
    notifyWithEmphasis("Time up! Closing game.");
}

void NotificationsController::notifySessionExpired(int restMin)
{
    // 原文案过长会被裁切(短版 439px 临界/长版 695px 严重裁切)，统一缩短为 ≤ 344px
    std::string message = restMin > 0
        ? "Time up! Rest " + std::to_string(restMin) + " min."
        : "Time up! Daily limit.";
    notifyWithEmphasis(message);
}

void NotificationsController::notifyRestOver()
{
    // 原 "Break over - you can play now." 在 448px/34 字号下被裁切(实测 454px)，缩短为 320px
    notifyWithEmphasis("Break over! Play now.");
}

void NotificationsController::notifyRestActive(int remainingMin)
{
    // 提示刻意缩短：UltraHand 通知弹窗固定 448px 宽、字号被钳到 34，
    // 过长文本会被 scissor 裁掉末尾（看不到“Game closed.”等）。保持单行 ≤ ~22 字符以完整显示。
    std::string message = remainingMin > 0
        ? "Rest: " + std::to_string(remainingMin) + " min left"
        : "Rest until daily reset";
    UltraHandInterface::writeNotification(message, kNoticeFontSize, kNoticePriority);
}

std::string NotificationsController::formatTime(int duration_in_minutes) {
    const auto& duration = minutes{duration_in_minutes};
    auto hoursPart = duration_cast<hours>(duration);
    auto minutesPart = duration_cast<minutes>(duration - hoursPart);
    std::string formatted = std::to_string(hoursPart.count()) + "h " +std::to_string(minutesPart.count()) +"mn";
    return formatted;
}
