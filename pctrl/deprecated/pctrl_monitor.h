#pragma once
#include <switch.h>
#include "database/history.h"

namespace alefbet::pctrl::srv {

    /*! \brief The monitor class is used to monitor applications usage and update the database records.
    */
    class Monitor {
        public:
            void start();

        private:
            u16 remainingTimeInMinutes(const HistoryEntry& entry);
    };

};