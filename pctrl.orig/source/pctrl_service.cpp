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

using namespace ams;
using namespace alefbet::pctrl::logger;

namespace alefbet::pctrl::srv {  
    static os::ThreadType s_thread;
    static Handle s_service_handle;
    //static TipcService s_service;     

    PctrlService::PctrlService() 
    {}    

    static constexpr size_t StackSize = 0x8000;
    static_assert(util::IsAligned(StackSize, os::MemoryPageSize), "StackSize alignment");
    alignas(os::MemoryPageSize) u8 m_stack_mem[StackSize] = {};
    static bool canRun = true;
    
    sf::hipc::ServerManager<1, sf::hipc::DefaultServerManagerOptions, 1> g_server_manager;
    constinit sf::UnmanagedServiceObject<pctrl::srv::IService, pctrl::srv::Service> g_user_service_object;
    
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

    /*static void threadFunc(void*) {
        logToFile("[Service] Wait for requests...\n");
        while(auto* signaled_holder = g_server_manager.WaitSignaled()) {
            g_server_manager.Process(signaled_holder);
        }


        logToFile("[Service] task started\n");

        NxResult rc;
        bool has_answered = false;
        canRun = true;

        logToFile("[Service] Register service pctrl:u\n");
        rc = smRegisterService(&s_service_handle, SmServiceName{"pctrl:u"}, true, 1);
        if(rc != 0) {
            logToFile("[Service] Registration failed\n");
            return;
        }

        while(canRun) {
            logToFile("[Service] wait for a connection");

            rc = waitSingleHandle(s_service_handle, -1);
            if (R_FAILED(rc)) {
                logToFile("[Service] Could not wait for the service handle sync\n");
                return;
            }

            rc = svcReplyAndReceiveLight(s_service_handle);
            if(R_FAILED(rc)) {
                logToFile("[Service] Could not receive client request\n");
                return;
            }

            void* base = armGetTls();
            HipcParsedRequest hipc = hipcParseRequest(base);

            if (hipc.meta.type != CmifCommandType_Request) {
                logToFile("[Service] this is not a request\n");
                return;
            }

            CmifInHeader* header = (CmifInHeader*)cmifGetAlignedDataStart(hipc.data.data_words, base);
            size_t dataSize = hipc.meta.num_data_words * 4;

            if (!header) {
                logToFile("[Service] header is missing\n");
                return;
            }

            if (dataSize < sizeof(CmifInHeader)) {
                logToFile("[Service] header is malformed\n");
                return;
            }
                
            if (header->magic != CMIF_IN_HEADER_MAGIC) {
                logToFile("[Service] header magic is wrong\n");
                return;
            }                

            dataSize = dataSize - sizeof(CmifInHeader);
            //u8* data = dataSize ? ((u8*)header) + sizeof(CmifInHeader) : NULL;

            switch ((PctrlCommands)header->command_id) {
                case CmdGetRemainingTime:
                    logToFile("[Service] Command received: get remaining time\n");
                    break;
                case CmdPing:     
                    logToFile("[Service] Command received: Ping\n");   
                    handlers::AnswerToPing();
                    has_answered = true;
                    break;
                case CmdTestTimeout:
                    logToFile("[Service] Command received: test timeout\n");
                    handlers::ShowScreenTimeout();
                    break;
                default:
                    logToFile("[Service] Command received: unknown or unhandled command\n");
                    break;
            }    

            // Send the response         
            if(has_answered) {       
                svcReplyAndReceiveLight(s_service_handle);
                has_answered = false;
            }   
        }  

        logToFile("[Service] stop waiting for request\n");
    }*/

    struct Header {
        u64 magic;
        union {
            u64 cmdId;
            u64 result;
        };
    };

    void PctrlService::Listen() {            
        Result res = smRegisterService(&s_service_handle, service_name_, true, 1);
        if(R_FAILED(res)) {
            logToFile("[Service] The service registration failed: %i\n", res.GetValue());
            smUnregisterService(service_name_);
            return;            
        }        

        logToFile("[Service] Service created\n");        

        Handle client_handle;
        //Handle reply_handle;
        //s32 rIndex = 0;
        void* tls = armGetTls();
        
        while(true) {
            logToFile("[Service] Wait for next request...\n");

            res = svcWaitSynchronizationSingle(s_service_handle, UINT64_MAX);
            if(R_FAILED(res)) {
                logToFile("[Service] An error occured: %i", res);
                return;
            }

            res = svcAcceptSession(&client_handle, s_service_handle);
            if(R_FAILED(res)) {
                logToFile("[Service] Accept session failed\n");
                return;
            }

            res = svcReplyAndReceiveLight(client_handle);
            if(R_FAILED(res)) {
                logToFile("[Service] replyAndReceive failed, client disconnected?\n");
                continue;
            }

            HipcParsedRequest r = hipcParseRequest(tls);
            if(r.meta.type != CmifCommandType_Request) {
                logToFile("[Service] this is not a request. Aborting\n");
                continue;
            }

            // Verify header
            Header* header = static_cast<Header*>(cmifGetAlignedDataStart(r.data.data_words, tls));
            size_t headerSize = r.meta.num_data_words * 4;
            if(!header || headerSize < sizeof(Header) || header->magic != CMIF_IN_HEADER_MAGIC) {
                logToFile("[Service] malformed header\n");
                continue;
            }

            // Read data
            logToFile("[Service] CmdId = %i\n", header->cmdId);
            logToFile("[Service] result = %i\n", header->result);

            u8* ptrData = reinterpret_cast<u8*>(header) + sizeof(Header);
            size_t dataSize = headerSize - sizeof(Header);
            void* data = malloc(dataSize+1);
            memset(data, 0, dataSize+1);
            memcpy(data, ptrData, dataSize);

            logToFile("[Service] Recv = %s\n", data);
        }
        

        logToFile("[Service] Listen() will exit\n");
    }

    void PctrlService::CloseAndClean() {
        canRun = false;
        os::WaitThread(std::addressof(s_thread));
        os::DestroyThread(std::addressof(s_thread));     
        //threadWaitForExit(&s_thread);
        //threadClose(&s_thread);
        
        svcCloseHandle(s_service_handle);

        Result rc = smUnregisterService(service_name_);
        if (R_FAILED(rc)) {
            logToFile("[Service] Could not unregister the service\n");
        }
    }

    /** Class Service */
    void Service::Ping() {
        logToFile("[Service] Ping received\n");
        handlers::AnswerToPing();
    }
}