#include <chrono>

class NotificationsController {
public:
    NotificationsController() = delete;

    static void notifyMonitoringStarted();
    static void notifyRemainingTime(int remainingTimeInMinutes);
    static void notifyTimeExpired();

private:
    static std::string formatTime(int duration_in_minutes);
};