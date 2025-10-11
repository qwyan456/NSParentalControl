#include "pctrl_monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include <chrono>

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;

constexpr std::chrono::minutes DelayInMinutes(1);
constexpr std::chrono::nanoseconds DelayInNanos = std::chrono::duration_cast<std::chrono::nanoseconds>(DelayInMinutes); 

namespace alefbet::pctrl::srv {

    /*! \brief Returns the remaining time for the current user in minutes

    The remaining time can be negative when the user outpass his limit.
    */
    s16 Monitor::remainingTimeInMinutes(const HistoryEntry& entry) {
        auto settings = loadSettings();
        const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;

        logToFile("[Monitor] daily limit=%i\n", daily_limit);
        logToFile("[Monitor] duration=%i\n", entry.durationInMinutes());

        return daily_limit - entry.durationInMinutes();            
    }

    void Monitor::start() {
        logToFile("[Monitor] Starting monitoring\n");

        while(true) {
            logToFile("[Monitor] Update usages\n");
            // Query the active application and user
            // and update the database
            const auto pid = getRunningApplicationPid();
            const auto title_id = getRunningApplicationTitleId(pid);

            if(title_id != 0) {
                const auto user = getCurrentUser();

                if(user.isValid()) {
                    logToFile("[Monitor] Title %i is currently in used by %s. Updating history.\n", title_id, user.nickname.c_str());
                    const auto entry = addToHistory(user.uid, title_id, DelayInMinutes.count());

                    if(!entry.isValid()) {
                        continue;
                    }

                    // After database update we need to verify the limits
                    const auto remaining_time = remainingTimeInMinutes(entry);
                    const auto uid_str = accountUidToString(user.uid);                    

                    if(remaining_time <= 0) {
                        logToFile("[Monitor] Timeout for the user %s\n", user.nickname.c_str());
                    } else if(remaining_time <= 5) {
                        logToFile("[Monitor] Remaining time for user %i is exactly 5 minutes. Warn the user.\n", uid_str);
                    } else {
                        logToFile("[Monitor] Remaining time for user %s is %i minutes.\n", user.nickname.c_str(), remaining_time);
                    }
                } else {
                    logToFile("[Monitor] No user found\n");
                }
            } else {
                logToFile("[Monitor] No title running\n");
            }

            svcSleepThread(DelayInNanos.count()); //Wait for a minute
        }

        logToFile("[Monitor] Stopped monitoring.\n");
    }

}