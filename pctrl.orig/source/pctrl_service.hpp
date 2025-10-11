/*
 * Copyright (c) Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once
#include <switch.h>
#include <vector>
#include "ipc/Server.hpp"
#include "pctrl_screen.hpp"
//#include "pctrl_screen.hpp"
//#include "pctrl_i_service.hpp"

namespace alefbet::pctrl::srv {

    constexpr SmServiceName service_name_ = smEncodeName("pctrl");    

    class PctrlService {
        public:
            PctrlService(Ipc::Server* ipcServer/*, u8* heap_pointer*/);
            ~PctrlService();
            void listen();            
            //void closeAndClean();

        private:
            //void process();
            Ipc::Result commandThread(Ipc::Request*);

            // Handlers
            void showScreenTimeout();
            Ipc::Result getRunningApplication(Ipc::Request*);
            Ipc::Result getCurrentUserUid(Ipc::Request*);
            Ipc::Result getCurrentUserNickname(Ipc::Request*);
            Ipc::Result getUsersList(Ipc::Request*);
            Ipc::Result getUserUsageTime(Ipc::Request*);
            Ipc::Result getUserRemainingTime(Ipc::Request*);
            Ipc::Result setUserLimits(const u32&);
            Ipc::Result setAdminPin(const std::string&);
            Ipc::Result verifyAdminPin(Ipc::Request*);

        private:
            bool is_ready_ = false;
            bool session_opened_ = false;
            //Handle handle_server = 0;
            //Handle handle_client = 0;     
            Ipc::Server *ipcServer_ = nullptr;
            //u8* heap_pointer_ = nullptr;
            GuiController gui_;
    };

}

