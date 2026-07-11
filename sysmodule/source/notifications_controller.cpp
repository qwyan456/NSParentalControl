#include "notifications_controller.h"
#include "ultrahand_interface.h"
#include <switch.h>
#include <chrono>

using namespace std::chrono;

void NotificationsController::notifyMonitoringStarted() 
{
    UltraHandInterface::writeNotification("Parental control enabled");
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
    UltraHandInterface::drawAttention();
    UltraHandInterface::writeNotification("Time's up! The game will close.");
}

void NotificationsController::notifySessionExpired(int restMin)
{
    UltraHandInterface::drawAttention();
    std::string message = restMin > 0
        ? "Session time up! Rest " + std::to_string(restMin) + " min, then you can play again."
        : "Session time up! Play is paused until your daily limit resets.";
    UltraHandInterface::writeNotification(message);
}

void NotificationsController::notifyRestOver()
{
    UltraHandInterface::drawAttention();
    UltraHandInterface::writeNotification("Break over - you can play now.");
}

std::string NotificationsController::formatTime(int duration_in_minutes) {
    const auto& duration = minutes{duration_in_minutes};
    auto hoursPart = duration_cast<hours>(duration);
    auto minutesPart = duration_cast<minutes>(duration - hoursPart);
    std::string formatted = std::to_string(hoursPart.count()) + "h " +std::to_string(minutesPart.count()) +"mn";
    return formatted;
}