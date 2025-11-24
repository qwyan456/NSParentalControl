#include "notifications_controller.h"
#include "ultrahand_interface.h"
#include <switch.h>

void NotificationsController::notifyMonitoringStarted() 
{
    UltraHandInterface::writeNotification("Parental control enabled");
}

void NotificationsController::notifyRemainingTime(int remainingTimeInMinutes)
{
    std::string message = std::to_string(remainingTimeInMinutes) + " minute" +(remainingTimeInMinutes > 1 ? "s" : "") +" left";

    UltraHandInterface::drawAttention();
    UltraHandInterface::writeNotification(message);
}