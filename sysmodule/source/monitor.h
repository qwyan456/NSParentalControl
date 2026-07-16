#pragma once
#include <switch.h>
#include "database/history.h"
#include "service.h"

namespace alefbet::pctrl::srv {

    /*! \brief The monitor class is used to monitor applications usage and update the database records.
    */
    class Monitor {
        public:            
            void start();
            void stop();
            void loop();
            bool isRunning() const {
                return running_;
            }            

            void setService(alefbet::pctrl::srv::Service* service) {
                service_ = service;
                service->setMonitor(this);
            }

        private:            
            //s16 remainingTimeInMinutes(const HistoryEntry& entry);
            bool shouldSendNotification(int remainingTimeInMinutes);

        private:
            alefbet::pctrl::srv::Service* service_ = nullptr;
            bool running_ = false;
            bool notified_ = false;
            u64 currentTitle_ = 0;
            bool inRest_ = false;            // 是否处于强制休息冷却中
            int sessionElapsedMin_ = 0;      // 当前连续单次会话已累计分钟数
            u64 restEndTs_ = 0;              // 强制休息结束的绝对墙钟时间(纳秒, TimeType_LocalSystemClock)
                                            // 用 RTC 真实时间，系统灭屏睡眠也前进；
                                            // 旧的“每循环 -1 分钟”计数器会因睡眠冻结而算错。
    };    

};