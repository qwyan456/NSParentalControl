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
#include <stratosphere.hpp>
#include <switch.h>
#include <thread>
#include "pctrl_service.hpp"
#include "pctrl_screen.hpp"
//#include "pctrl_monitor.h"
#include "logger.h"

using namespace ams;
using namespace alefbet::pctrl::logger;

/* Set libnx graphics globals. */
extern "C" {

    //u32 __nx_nv_transfermem_size = 0x40000;
    //ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

    constexpr size_t ServiceThreadStackRequiredSizeBytes = util::AlignUp(256_KB, 128);
    constexpr size_t ServiceThreadStackRequiredSizeAligned = util::AlignUp(ServiceThreadStackRequiredSizeBytes, os::MemoryHeapUnitSize);
    
    constexpr size_t MonitorThreadStackRequiredSizeBytes = util::AlignUp(128_KB, 128);
    constexpr size_t MonitorThreadStackRequiredSizeAligned = util::AlignUp(MonitorThreadStackRequiredSizeBytes, os::MemoryHeapUnitSize);
    
    constinit u8 *g_heap_pointer = nullptr;
}

namespace alefbet {

    namespace pctrl::srv {     
        
        namespace {

            constinit u8 g_fs_heap_memory[2_KB];
            lmem::HeapHandle g_fs_heap_handle;

            void *AllocateForFs(size_t size) {
                return lmem::AllocateFromExpHeap(g_fs_heap_handle, size);
            }

            void DeallocateForFs(void *p, size_t size) {
                AMS_UNUSED(size);
                return lmem::FreeToExpHeap(g_fs_heap_handle, p);
            }

            void InitializeFsHeap() {
                g_fs_heap_handle = lmem::CreateExpHeap(g_fs_heap_memory, sizeof(g_fs_heap_memory), lmem::CreateOption_ThreadSafe);
            }

            void AllocateStackForThreads() {
                logToFile("Allocate stack for threads\n");
                ams::Result rc = os::SetMemoryHeapSize(ServiceThreadStackRequiredSizeAligned /*+ MonitorThreadStackRequiredSizeAligned*/);
                if (rc.IsSuccess()) {
                    g_heap_pointer = reinterpret_cast<u8 *>(os::GetMemoryHeapAddress());
                    return;
                }
                logToFile("Stack allocated for threads\n");
            }

        }
    }
}

namespace ams {
    namespace init {
        constexpr size_t MallocBufferSize = 128_KB;
        alignas(os::MemoryPageSize) constinit u8 g_malloc_buffer[MallocBufferSize];

        //void InitializeSystemModuleBeforeConstructors() {
            /* Catch has global-ctors which allocate, so we need to do this earlier than normal. */
            
        //}        

        void InitializeSystemModule() {
            /* Initialize heap. */
            alefbet::pctrl::srv::InitializeFsHeap();
            //TODO: useful?
            init::InitializeAllocator(g_malloc_buffer, sizeof(g_malloc_buffer));
            
            /* Initialize our connection to sm. */
            sm::Initialize();
            
            plInitialize(::PlServiceType_User);
            setInitialize();     
            setsysInitialize();
            //pminfoInitialize();
            //pmshellInitialize();
            i2cInitialize();
            bpcInitialize();
            hidInitialize();
            pmdmntInitialize();
            nsInitialize();

            /* Initialize fs. */
            fs::InitializeForSystem();
            fs::SetAllocator(alefbet::pctrl::srv::AllocateForFs, alefbet::pctrl::srv::DeallocateForFs);
            fs::SetEnabledAutoAbort(false);

            //psmInitialize(); 
            //spsmInitialize();

            //gpio::Initialize();

            /* Mount the SD card. */
            fs::MountSdCard("sdmc");            
        }

        void FinalizeSystemModule() { /* ... */ }

        void Startup() { /* ... */ }

    }    

    void startIpc(void* service) {
        alefbet::pctrl::srv::PctrlService* srv = static_cast<alefbet::pctrl::srv::PctrlService*>(service);        
        srv->listen();            
    }

    /*void startMonitor(void* monitor) {
        alefbet::pctrl::srv::Monitor* mon = static_cast<alefbet::pctrl::srv::Monitor*>(monitor);
        mon->start();
    }*/

    void Main() {                
        alefbet::pctrl::logger::clearLog();
        alefbet::pctrl::logger::logToFile("Parental control starting\n");
                
        /* Set thread name. */
        os::SetThreadNamePointer(os::GetCurrentThread(), "alefbet.pctrl.Main");        

        alefbet::pctrl::srv::AllocateStackForThreads();

        ::Result rc = 0;

        /*NsApplicationRecord records[20];
        s32 rCount = 0;
        rc = nsListApplicationRecord(std::addressof(records[0]), 20, 0, &rCount);
 
        logToFile("rc=%i\n", rc);
        logToFile("Title count=%i\n", rCount);
        NsApplicationControlData control_data;
        u64 actual_size = 0;
        for(int i = 0 ; i < rCount ; i++) {
            NsApplicationRecord rec = records[i];                        
            nsGetApplicationControlData(NsApplicationControlSource_Storage, rec.application_id, &control_data, sizeof(control_data), &actual_size);
            std::string app_name = std::string(control_data.nacp.lang[0].name);
            logToFile("app: %i, %i, %s\n", rec.application_id, rec.type, app_name);
        }*/

        // Loop processing the IPC server.        
        Ipc::Server ipcServer("pctrl");        
        alefbet::pctrl::srv::PctrlService service(&ipcServer/*, g_heap_pointer*/);
        
        Thread threadIpc;
        //TODO: replace heap with malloc buffer?
        rc = threadCreate(&threadIpc, startIpc, &service, g_heap_pointer, ServiceThreadStackRequiredSizeBytes, 0x2c, -2);        
        if(R_FAILED(rc)) {
            logToFile("Could not create the service thread, error %i.\n", rc);
            return;
        }
        rc = threadStart(&threadIpc);
        if(R_FAILED(rc)) {
            logToFile("Could not start the service thread, error %i.\n", rc);
            return;
        }

        /*Thread threadMonitor;
        alefbet::pctrl::srv::Monitor monitor;
        rc = threadCreate(&threadMonitor, startMonitor, &monitor, g_heap_pointer, MonitorThreadStackRequiredSizeBytes, 0x2c, -2);
        if(R_FAILED(rc)) {
            logToFile("Could not create the monitor thread, error %i.\n", rc);
            return;
        }
        rc = threadStart(&threadMonitor);
        if(R_FAILED(rc)) {
            logToFile("Could not start the monitor thread, error %i.\n", rc);
            return;
        }*/

        rc = threadWaitForExit(&threadIpc);
        if(R_FAILED(rc)) {
            logToFile("Could not wait for the service thread to end, error %i.\n", rc);
            return;
        }

        /*rc = threadWaitForExit(&threadMonitor);
        if(R_FAILED(rc)) {
            logToFile("Could not wait for the monitor thread to end, error %i.\n", rc);
            return;
        }*/

        logToFile("Parental control ended\n");
    }

}
