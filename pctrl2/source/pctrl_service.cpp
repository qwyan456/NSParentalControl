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
#include <switch.h>
#include <thread>
#include <cstring>
#include "pctrl_service.hpp"
#include "pctrl_font.hpp"
#include "logger.h"
#include "ipc/Command.hpp"
#include "ipc/Result.hpp"
#include "pctrl_screen.hpp"
#include "ams_bpc.h"

//#define PSEC_DEBUG 1

using namespace alefbet::pctrl::logger;

namespace alefbet::pctrl::srv {       

    PctrlService::PctrlService(Ipc::Server* ipcServer, u8* heap_pointer)
    : ipcServer_(ipcServer), heap_pointer_(heap_pointer) {
        logToFile("[Service] Starting service\n");        
        ipcServer_->setRequestHandler([this](Ipc::Request * r) -> uint32_t {
            return static_cast<uint32_t>(this->commandThread(r));
        });
        logToFile("[Service] service started\n");

        /* Load shared font. */
        alefbet::pctrl::font::InitializeSharedFont();
    }    

    PctrlService::~PctrlService() {
        /*if(ipcServer_) {
            delete ipcServer_;
        }*/
    }

    namespace helpers {
        #define IRAM_PAYLOAD_MAX_SIZE 0x24000
        static u8 g_reboot_payload[IRAM_PAYLOAD_MAX_SIZE];

        void reboot_to_payload(void) {
            smExit(); //Required to connect to ams:bpc
            
            ::Result rc = amsBpcInitialize();
            if (rc != 0) {
                logToFile("[Service] Failed to initialize ams:bpc: %i\n", rc);
                logToFile("[Service] Shutdown.\n");
                bpcShutdownSystem();
                return;
            }

            FILE *f = fopen("sdmc:/atmosphere/reboot_payload.bin", "rb");
            if (f == NULL) {
                logToFile("[Service] Failed to open atmosphere/reboot_payload.bin!\n");
                logToFile("[Service] Shutdown.\n");
                bpcShutdownSystem();
                return;
            } else {
                fread(g_reboot_payload, 1, sizeof(g_reboot_payload), f);
                fclose(f);
            }

            rc = amsBpcSetRebootPayload(g_reboot_payload, IRAM_PAYLOAD_MAX_SIZE);
            if (rc != 0) {
                logToFile("[Service] Failed to set reboot payload: %i\n", rc);
                logToFile("[Service] Shutdown.\n");
                bpcShutdownSystem();
            } else {
                spsmShutdown(true);
            }
        }

        u64 getRunningApplicationPid() {
            u64 process_id = 0;

            ::Result rc = pmdmntGetApplicationProcessId(&process_id);
            if(R_FAILED(rc)) {
                logToFile("[Service] Could not get application process ID: %i\n", rc);
                return 0;
            }

            return process_id;
        }

        u64 getRunningApplicationTitleId() {
            u64 title_id = 0;

            auto process_id = getRunningApplicationPid();
            if(process_id == 0) {
                return 0;
            }
            

            ::Result rc = pmdmntGetProgramId(&title_id, process_id);
            if(R_FAILED(rc)) {
                logToFile("[Service] Could not get the title ID for the process %i\n", process_id);
                return 0;
            }

            return title_id;
        }
    }

    void PctrlService::showScreenTimeout() {        
        logToFile("[Service] Test requested\n");        

        // Block unless a button has been pressed
        PadState pad;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeAny(&pad);

        gui_.ShowScreenTimeout();

        while(true) {
            padUpdate(&pad);
            u64 kDown = padGetButtonsDown(&pad);

            if(kDown & HidNpadButton_Minus) {
                logToFile("[Service] User wants to shutdown\n");
                #ifdef PSEC_DEBUG
                    // In debug mode we only hide the screen
                    gui_.HideScreen();
                #else 
                    // Otherwise we shut the system down
                    helpers::reboot_to_payload();
                #endif

                return; //Stop monitoring the buttons
            }

            svcSleepThread(100'000);
        }
    }

    Ipc::Result PctrlService::getRunningApplication(Ipc::Request* request) {
        //auto process_id = helpers::getRunningApplicationPid();
        auto title_id = helpers::getRunningApplicationTitleId();

        request->appendReplyValue(title_id);
        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getCurrentUser(Ipc::Request* request) {            
        ::Result rc = accountInitialize(AccountServiceType_System);
        if(R_FAILED(rc)) {
            logToFile("[Service] Could not initialize account service: %i\n", rc);
            return Ipc::Result::Error;
        }

        AccountUid uid;
        rc = accountGetPreselectedUser(&uid);
        if(R_FAILED(rc)) {
            logToFile("[Service] Could not get preselected user: %i\n", rc);
            accountExit();
            return Ipc::Result::Error;
        }

        AccountProfile profile;
        AccountUserData user_data;
        AccountProfileBase base;
        rc = accountProfileGet(&profile, &user_data, &base);
        if(R_FAILED(rc)) {
            logToFile("[Service] Could not get account profile: %i\n", rc);
            accountProfileClose(&profile);
            accountExit();
            return Ipc::Result::Error;
        }

        request->appendReplyValue(std::string(base.nickname));
        accountProfileClose(&profile);
        accountExit();

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getUsersList(Ipc::Request*) {
        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getUserData(const std::string&, Ipc::Request*) {
        return Ipc::Result::Ok;
    }

    void PctrlService::setUserLimits(const std::string&, const u32&) {
        
    }

    void PctrlService::setAdminPin(const std::string&) {

    }

    Ipc::Result PctrlService::verifyAdminPin(const std::string&, Ipc::Request*) {
        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::commandThread(Ipc::Request * request) {
        logToFile("[Service] request received: CmdId=%i\n", request->cmd());
        Ipc::Result rc;

        switch (static_cast<Ipc::Command>(request->cmd())) {            
            case Ipc::Command::Test: {
                showScreenTimeout();
                break;
            }
            case Ipc::Command::GetCurrentUser: {
                return getCurrentUser(request);
            }
            case Ipc::Command::GetUsersList: {
                return getUsersList(request);
            }        
            case Ipc::Command::GetUserData: {
                std::string username;
                rc = request->readRequestData(username);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not red request data (username)\n");                    
                }
                return getUserData(username, request);                
            }
            case Ipc::Command::GetRunningApplication: {
                return getRunningApplication(request);
            }
            case Ipc::Command::SetUserLimits: {
                std::string username;
                rc = request->readRequestData(username);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (username)\n");
                    break;
                }
                u32 dailyLimitInMinutes = 0;
                rc = request->readRequestValue(dailyLimitInMinutes);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (daily limit)\n");
                }
                setUserLimits(username, dailyLimitInMinutes);
                break;
            }
            case Ipc::Command::SetAdminPin: {
                std::string pin;
                rc = request->readRequestData(pin);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (PIN)\n");
                }
                setAdminPin(pin);
                break;
            }
            case Ipc::Command::VerifyAdminPin: {
                std::string pin;
                rc = request->readRequestData(pin);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (PIN)\n");
                }
                return verifyAdminPin(pin, request);
            }
            default: {
                logToFile("[Service] command %i not handled.\n", request->cmd());                
                return Ipc::Result::UnknownCommand;
            }

        }        

        return Ipc::Result::Ok;
    }
    
    void PctrlService::listen() {                
        logToFile("[Service] Start listening\n");

        while(true) {            
            if (!ipcServer_->process()) {
                // When an error occurs we exit
                return;
            }

            svcSleepThread(5'000);
        }

        logToFile("[Service] Listen() will exit\n");
    }

}