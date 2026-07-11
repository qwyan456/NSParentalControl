#include <chrono>

class NotificationsController {
public:
    NotificationsController() = delete;

    static void notifyMonitoringStarted();
    static void notifyRemainingTime(int remainingTimeInMinutes);
    static void notifyTimeExpired();
    static void notifySessionExpired(int restMin);
    static void notifyRestOver();
    static void notifyRestActive(int remainingMin);

    // 以「重复发送 + 间隔」的方式延长 UltraHand toast 在屏幕上的可见时间，
    // 并使用更大的字号与更高的优先级让提示更醒目。
    // （UltraHand 的通知 .notify 不支持直接设置显示时长，重复写入可使其持续重新显示）
    static void notifyWithEmphasis(const std::string& message, int repeats = 3, int intervalMs = 1500);

private:
    static std::string formatTime(int duration_in_minutes);
};
