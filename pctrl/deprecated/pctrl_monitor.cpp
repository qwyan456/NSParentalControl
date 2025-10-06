#include "pctrl_monitor.h"
#include "logger.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"
#include <chrono>

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::helpers;
using namespace alefbet::pctrl::database;

constexpr std::chrono::minutes DELAY_IN_MINUTES(1);
constexpr std::chrono::nanoseconds DELAY_IN_NANOS = std::chrono::duration_cast<std::chrono::nanoseconds>(DELAY_IN_MINUTES); 

namespace alefbet::pctrl::srv {

    u16 Monitor::remainingTimeInMinutes(const HistoryEntry& entry) {
        auto settings = loadSettings();
        const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;

        return entry.durationInMinutes() - daily_limit;
    }

    void Monitor::start() {
        logToFile("[Monitor] Starting monitoring\n");

        while(true) {
            // Query the active application and user
            // and update the database
            const auto pid = getRunningApplicationPid();
            const auto title_id = getRunningApplicationTitleId(pid);

            if(title_id != 0) {
                const auto uid = getCurrentUser();
                if(uid != "" && uid.substr(0, 4) != "ERR#") {
                    logToFile("Title %s is currently in used by %i\n. Updating history.\n", title_id, uid);
                    const auto entry = addToHistory(accountUidFromString(uid), title_id, DELAY_IN_MINUTES.count());

                    // After database update we need to verify the limits
                    const auto remaining_time = remainingTimeInMinutes(entry);

                    if(remaining_time <= 0) {
                        logToFile("[Monitor] Timeout for the user %s\n", uid);
                    } else if(remaining_time <= 5) {
                        logToFile("[Monitor] Remaining time for user %s is exactly 5 minutes. Warn the user.\n", uid);
                    } else {
                        logToFile("[Monitor] Remaining time for user %s is %i minutes.\n", uid, remaining_time);
                    }
                }
            }

            svcSleepThread(DELAY_IN_NANOS.count()); //Wait for a minute
        }

        logToFile("[Monitor] Stopped monitoring.\n");
    }

}