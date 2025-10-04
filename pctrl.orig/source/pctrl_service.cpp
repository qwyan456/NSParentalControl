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
#include "pctrl_service.hpp"
#include "pctrl_protocol.h"
#include "pctrl_screen.hpp"
#include "logger.h" 

namespace alefbet::pctrl::srv {  
    static Thread s_thread;
    static Handle s_service_handle;
    //static TipcService s_service;     

    PctrlService::PctrlService() 
    {}    

    namespace handlers {

        void ShowScreenTimeout() {        
            s_gui.ShowScreenTimeout();
        }

        void AnswerToPing() {      
            HipcMetadata meta = { 0 };
            meta.type = CmifCommandType_Request;
            meta.num_data_words = (sizeof(CmifOutHeader) + 0x10) / 4;

            void* base = armGetTls();
            HipcRequest req = hipcMakeRequest(base, meta);
            CmifOutHeader* rawHeader = (CmifOutHeader*)cmifGetAlignedDataStart(req.data_words, base);

            rawHeader->magic = CMIF_OUT_HEADER_MAGIC;
            rawHeader->result = 0;
            rawHeader->token = 0;
        }
    }

    void threadFunc(void*) {
        Result rc;
        bool has_answered = false;

        while(true) {
            /* Wait for a client */
            rc = waitSingleHandle(s_service_handle, -1);
            if (R_FAILED(rc)) {
                logToFile("Could not wait for the service handle sync");
                return;
            }

            /* Read IPC command */
            rc = svcReplyAndReceiveLight(s_service_handle);
            if(R_FAILED(rc)) {
                logToFile("Could not receive client request");
                return;
            }

            void* base = armGetTls();
            HipcParsedRequest hipc = hipcParseRequest(base);

            if (hipc.meta.type != CmifCommandType_Request) {
                logToFile("Service: this is not a request");
                return;
            }

            CmifInHeader* header = (CmifInHeader*)cmifGetAlignedDataStart(hipc.data.data_words, base);
            size_t dataSize = hipc.meta.num_data_words * 4;

            if (!header) {
                logToFile("Service: header is missing");
                return;
            }

            if (dataSize < sizeof(CmifInHeader)) {
                logToFile("Service: header is malformed");
                return;
            }
                
            if (header->magic != CMIF_IN_HEADER_MAGIC) {
                logToFile("Service: header magic is wrong");
                return;
            }                

            dataSize = dataSize - sizeof(CmifInHeader);
            //u8* data = dataSize ? ((u8*)header) + sizeof(CmifInHeader) : NULL;

            switch ((PctrlCommands)header->command_id) {
                case CmdGetRemainingTime:
                    logToFile("Command received: get remaining time");
                    break;
                case CmdPing:     
                    logToFile("Command received: Ping");   
                    handlers::AnswerToPing();
                    has_answered = true;
                case CmdTestTimeout:
                    logToFile("Command received: test timeout");
                    handlers::ShowScreenTimeout();
                    break;
                default:
                    logToFile("Command received: unknown or unhandled command");
                    break;
            }    

            // Send the response         
            if(has_answered) {       
                svcReplyAndReceiveLight(s_service_handle);
                has_answered = false;
            }     
        }   
    }

    bool PctrlService::Llsten() {     
        /* Start a thread */
        Result rc = threadCreate(&s_thread, &threadFunc, NULL, NULL, 0x20, 0, -2);
        if (R_FAILED(rc)) {
            logToFile("Could not create the service thread");
            return false;
        }

        Result res = smRegisterService(&s_service_handle, service_name_, true, 1);
        if(R_FAILED(res)) {
            logToFile("The service registration failed.");
            logIntToFile(res.GetValue());
            smUnregisterService(service_name_);
            return false;            
        }        
        
        //tipcCreate(&s_service, s_service_handle);
        rc = threadStart(&s_thread);

        if (R_FAILED(rc)) {
            logToFile("Could not start the service thread");
            return false;
        }          

        is_ready_ = true;
        return true;
    }

    void PctrlService::closeAndClean() {
        threadWaitForExit(&s_thread);
        threadClose(&s_thread);
        
        svcCloseHandle(s_service_handle);

        Result rc = smUnregisterService(service_name_);
        if (R_FAILED(rc)) {
            logToFile("Could not unregister the service");
        }
    }
}