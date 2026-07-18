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
            u64 restEndTs_ = 0;              // 强制休息结束的绝对时间(纳秒, 基于 ARM 系统节拍计数器 cntpct_el0)
                                            // 该计数器睡眠也前进、不依赖 time 服务；
                                            // nowNs() 已做溢出防护(坑10)，可安全用于绝对结束时间比较。
    };    

};