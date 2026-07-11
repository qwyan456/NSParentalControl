#include <chrono>

class NotificationsController {
public:
    NotificationsController() = delete;

    static void notifyMonitoringStarted();
    static void notifyRemainingTime(int remainingTimeInMinutes);
    static void notifyTimeExpired();
    static void notifySessionExpired(int restMin);
    static void notifyRestOver();

private:
    static std::string formatTime(int duration_in_minutes);
};