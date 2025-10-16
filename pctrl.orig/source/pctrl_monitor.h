#pragma once
#include <switch.h>
#include "database/history.h"
#include "pctrl_service.hpp"

namespace alefbet::pctrl::srv {

    /*! \brief The monitor class is used to monitor applications usage and update the database records.
    */
    class Monitor {
        public:
            void start();

            void setService(alefbet::pctrl::srv::PctrlService* service) {
                service_ = service;
            }

        private:
            s16 remainingTimeInMinutes(const HistoryEntry& entry);

        private:
            alefbet::pctrl::srv::PctrlService* service_ = nullptr;
    };    

};