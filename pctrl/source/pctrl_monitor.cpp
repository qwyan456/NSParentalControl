#include "pctrl_monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include <chrono>

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;

constexpr std::chrono::seconds DelayInSeconds(15);
constexpr std::chrono::nanoseconds DelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(DelayInSeconds); 

namespace alefbet::pctrl::srv {

    u16 Monitor::remainingTimeInMinutes(const HistoryEntry& entry) {
        auto settings = loadSettings();
        const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;

        return entry.durationInMinutes() - daily_limit;
    }

    void Monitor::start() {
        logToFile("[Monitor] Starting monitoring\n");

        while(true) {
            logToFile("Update usages\n");
            // Query the active application and user
            // and update the database
            const auto pid = getRunningApplicationPid();
            const auto title_id = getRunningApplicationTitleId(pid);

            if(title_id != 0) {
                const auto user = getCurrentUser();

                if(user.isValid()) {
                    logToFile("Title %i is currently in used by %s. Updating history.\n", title_id, user.nickname.c_str());
                    const auto entry = addToHistory(user.uid, title_id, DelayInSeconds.count());

                    // After database update we need to verify the limits
                    const auto remaining_time = remainingTimeInMinutes(entry);
                    const auto uid_str = accountUidToString(user.uid);                    

                    if(remaining_time <= 0) {
                        logToFile("[Monitor] Timeout for the user %s\n", uid_str);
                    } else if(remaining_time <= 5) {
                        logToFile("[Monitor] Remaining time for user %i is exactly 5 minutes. Warn the user.\n", uid_str);
                    } else {
                        logToFile("[Monitor] Remaining time for user %s is %i minutes.\n", user.nickname.c_str(), remaining_time);
                    }
                } else {
                    logToFile("No user found\n");
                }
            } else {
                logToFile("No title running\n");
            }

            svcSleepThread(DelayInNanos.count()); //Wait for a minute
        }

        logToFile("[Monitor] Stopped monitoring.\n");
    }

}