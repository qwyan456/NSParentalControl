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
#include <map>
#include "pctrl_service.hpp"
#include "pctrl_font.hpp"
#include "logger.h"
#include "ipc/Command.hpp"
#include "ipc/Result.hpp"
#include "pctrl_screen.hpp"
#include "ams_bpc.h"
#include "helpers.h"
#include "database/settings.h"
#include "database/database.h"

//#define PSEC_DEBUG 1

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::database;
using namespace alefbet::pctrl::helpers;
constexpr std::string NullString = std::string("(NULL)");
constexpr u64 DefaultPin[] = { HidNpadButton_A, HidNpadButton_A, HidNpadButton_A, HidNpadButton_A };

namespace alefbet::pctrl::srv {       

    PctrlService::PctrlService(Ipc::Server* ipcServer/*, u8* heap_pointer*/)
    : ipcServer_(ipcServer)/*, heap_pointer_(heap_pointer)*/ {
        logToFile("[Service] Starting service\n");        
        ipcServer_->setRequestHandler([this](Ipc::Request * r) -> uint32_t {
            return static_cast<uint32_t>(this->commandThread(r));
        });
        logToFile("[Service] service started\n");

        /* Load shared font. */
        alefbet::pctrl::font::InitializeSharedFont();
        logToFile("[Service] Shared font loaded\n");
    }    

    PctrlService::~PctrlService() {
        /*if(ipcServer_) {
            delete ipcServer_;
        }*/
    }

    namespace actions {
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
                    actions::reboot_to_payload();
                #endif

                return; //Stop monitoring the buttons
            }

            svcSleepThread(100'000);
        }
    }

    Ipc::Result PctrlService::getRunningApplication(Ipc::Request* request) {
        auto process_id = helpers::getRunningApplicationPid();
        if(process_id > 0) {
            auto title_id = helpers::getRunningApplicationTitleId(process_id);
            auto app_name = helpers::getApplicationName(title_id);
            request->appendReplyValue(app_name);
        } else {
            request->appendReplyValue(NullString);
        }
        
        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getCurrentUserUid(Ipc::Request* request) {   
        const auto current_title = helpers::getRunningApplicationPid();        
        if(current_title == 0) {
            // If there is no title we don"t query on user
            request->appendReplyValue(NullString);
            return Ipc::Result::Ok;
        } 

        const auto current_user = helpers::getCurrentUser();
        if(current_user.isValid()) {
            request->appendReplyValue(accountUidToString(current_user.uid));
        } else {
            request->appendReplyValue(NullString);
        }

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getCurrentUserNickname(Ipc::Request* request) {    
        const auto current_title = helpers::getRunningApplicationPid();        
        if(current_title == 0) {
            // If there is no title we don"t query on user
            request->appendReplyValue(NullString);
            return Ipc::Result::Ok;
        } 

        const auto current_user = helpers::getCurrentUser();
        if(current_user.isValid()) {
            logToFile("[Service] replying UID=%s\n", current_user.nickname.c_str());
            request->appendReplyValue(current_user.nickname);
        } else {
            logToFile("[Service] replying UID=(NULL)\n");
            request->appendReplyValue(NullString);
        }

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getUsersList(Ipc::Request*) {
        // Useful?
        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getUserRemainingTime(Ipc::Request* request) {
        //const auto uidS = accountUidFromString(uid);
        const auto user = helpers::getCurrentUser();
        if(!user.isValid()) {
            logToFile("[Service] There is no user\n");
            request->appendReplyValue(0);
            return Ipc::Result::Ok;
        }

        logToFile("[Service] Get usage time for user %s\n", user.nickname.c_str());

        const auto& date = today();
        if(date.empty()) {
            return Ipc::Result::Error;
        }

        const auto history = getHistory(user.uid, date);
        auto usage_time_in_minutes = (u16)0;        

        // Compute the total usage for today 
        for(const auto& entry: history) {
            usage_time_in_minutes += entry.durationInMinutes();
        }

        // Get the daily limit
        auto settings = loadSettings();
        const auto daily_limit = settings[SETTING_DAILY_LIMIT_GLOBAL].int_value;
        s16 remaining_time_in_minutes = 0;
        if(daily_limit > usage_time_in_minutes) {
            remaining_time_in_minutes = daily_limit - usage_time_in_minutes;
        }

        logToFile("[Service] User=%s, usage=%i minutes, remaining=%i minutes\n", user.nickname.c_str(), usage_time_in_minutes, remaining_time_in_minutes);

        request->appendReplyValue(remaining_time_in_minutes);

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::getUserUsageTime(Ipc::Request* request) {
        const auto user = helpers::getCurrentUser();
        if(!user.isValid()) {
            logToFile("[Service] There is no user\n");
            return Ipc::Result::Ok;
        }

        logToFile("[Service] Get remaining time for user %s\n", user.nickname.c_str());

        const auto history = getHistory(user.uid, today());
        auto usage_time_in_minutes = (u16)0;        

        // Compute the total usage for today 
        for(const auto& entry: history) {
            usage_time_in_minutes += entry.durationInMinutes();
        }

        logToFile("[Service] User=%s, usage=%i minutes\n", user.nickname.c_str(), usage_time_in_minutes);

        request->appendReplyValue(usage_time_in_minutes);

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::setUserLimits(const u32& limit_in_minutes) {
        // Now the limit is global not per user so we don't use the first argument

        auto settings = loadSettings();

        Setting setting;        
        setting.key = SETTING_DAILY_LIMIT_GLOBAL;
        setting.type = INTEGER;
        setting.int_value = limit_in_minutes;

        saveSetting(settings, setting);

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::setAdminPin(const std::string& pin) {
        logToFile("[Service] Setting admin PIN to %s", pin);
        auto settings = loadSettings();
        
        Setting settingPin;
        settingPin.type = STRING;
        settingPin.string_value = pin;
        settingPin.key = SETTING_ADMIN_PIN;

        saveSetting(settings, settingPin);

        return Ipc::Result::Ok;        
    }

    Ipc::Result PctrlService::verifyAdminPin(Ipc::Request* request) {
        std::string pin;
        Ipc::Result rc = request->readRequestValue(pin);
        if(rc != Ipc::Result::Ok) {
            logToFile("[Service] Could not read request data (PIN)\n");
            return Ipc::Result::BadInput;
        }

        auto settings = loadSettings();

        auto adminPin = settings[SETTING_ADMIN_PIN].string_value;
        if(adminPin.empty()) {
            adminPin = std::to_string(DefaultPin[0]) +"," +std::to_string(DefaultPin[1]) +"," +std::to_string(DefaultPin[2]) +"," +std::to_string(DefaultPin[3]);
            logToFile("[Service] No PIN defined. Using default.\n");
        }
        logToFile("[Service] verify admin PIN. Recv=%s. ref=%s\n", pin.c_str(), adminPin.c_str());
        request->appendReplyValue(adminPin == pin ? true : false);

        return Ipc::Result::Ok;
    }

    Ipc::Result PctrlService::commandThread(Ipc::Request* request) {
        logToFile("[Service] request received: CmdId=%i\n", request->cmd());
        Ipc::Result rc;

        switch (static_cast<Ipc::Command>(request->cmd())) {            
            case Ipc::Command::Test: {
                showScreenTimeout();
                break;
            }
            case Ipc::Command::GetCurrentUserUid: {
                return getCurrentUserUid(request);
            }
            case Ipc::Command::GetCurrentUserNickname: {
                return getCurrentUserNickname(request);
            }
            case Ipc::Command::GetUsersList: {
                return getUsersList(request);
            }        
            case Ipc::Command::GetUserUsageTime: {                                
                return getUserUsageTime(request);                
            }
            case Ipc::Command::GetUserRemainingTime: {                
                return getUserRemainingTime(request);   
            }
            case Ipc::Command::GetRunningApplication: {
                return getRunningApplication(request);
            }
            case Ipc::Command::SetUserLimits: {
                /*std::string username;
                rc = request->readRequestData(username);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (username)\n");
                    break;
                }*/
                u32 dailyLimitInMinutes = 0;
                /*rc = request->readRequestValue(dailyLimitInMinutes);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (daily limit)\n");
                }*/
                return setUserLimits(dailyLimitInMinutes);
            }
            case Ipc::Command::SetAdminPin: {
                std::string pin;
                rc = request->readRequestData(pin);
                if(rc != Ipc::Result::Ok) {
                    logToFile("[Service] Could not read request data (PIN)\n");
                }
                return setAdminPin(pin);
            }
            case Ipc::Command::VerifyAdminPin: {                
                return verifyAdminPin(request);
            }
            default: {
                logToFile("[Service] command %i not handled.\n", request->cmd());                
                return Ipc::Result::UnknownCommand;
            }

        }        

        return Ipc::Result::Ok;
    }
    
    void PctrlService::listen() {                
        logToFile("[Service] Starting listening\n");

        while(true) {            
            if (!ipcServer_->process()) {
                // When an error occurs we exit
                return;
            }

            svcSleepThread(5'000);
        }

        logToFile("[Service] Stopped listening\n");
    }

}