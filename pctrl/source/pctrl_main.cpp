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
#include "logger.h"

using namespace ams;

/* Set libnx graphics globals. */
extern "C" {

    u32 __nx_nv_transfermem_size = 0x40000;
    ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

    constexpr size_t ThreadHeapRequiredSizeBytes = util::AlignUp(256_KB, 128);
    constexpr size_t ThreadHeapRequiredSizeAligned = util::AlignUp(ThreadHeapRequiredSizeBytes, os::MemoryHeapUnitSize);
    constinit u8 *g_thread_heap_pointer = nullptr;
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

            void AllocateHeapForThread() {
                ams::Result rc = os::SetMemoryHeapSize(ThreadHeapRequiredSizeAligned);
                if (rc.IsSuccess()) {
                    g_thread_heap_pointer = reinterpret_cast<u8 *>(os::GetMemoryHeapAddress());
                    return;
                }
            }

        }
    }
}

namespace ams {
    namespace init {
        constexpr size_t MallocBufferSize = 100_KB;
        alignas(os::MemoryPageSize) constinit u8 g_malloc_buffer[MallocBufferSize];

        //void InitializeSystemModuleBeforeConstructors() {
            /* Catch has global-ctors which allocate, so we need to do this earlier than normal. */
            
        //}        

        void InitializeSystemModule() {
            /* Initialize heap. */
            alefbet::pctrl::srv::InitializeFsHeap();
            init::InitializeAllocator(g_malloc_buffer, sizeof(g_malloc_buffer));
            
            /* Initialize our connection to sm. */
            sm::Initialize();
            
            plInitialize(::PlServiceType_User);
            setInitialize();     
            setsysInitialize();
            pminfoInitialize();
            pmshellInitialize();
            i2cInitialize();
            bpcInitialize();
            hidInitialize();
            pmdmntInitialize();

            /* Initialize fs. */
            fs::InitializeForSystem();
            fs::SetAllocator(alefbet::pctrl::srv::AllocateForFs, alefbet::pctrl::srv::DeallocateForFs);
            fs::SetEnabledAutoAbort(false);

            psmInitialize(); 
            spsmInitialize();

            gpio::Initialize();

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

    void Main() {                
        alefbet::pctrl::logger::clearLog();
        alefbet::pctrl::logger::logToFile("Parental control starting\n");
                
        /* Set thread name. */
        os::SetThreadNamePointer(os::GetCurrentThread(), "alefbet.pctrl.Main");        

        alefbet::pctrl::srv::AllocateHeapForThread();

        // Loop processing the IPC server.        
        Ipc::Server ipcServer("pctrl");        
        alefbet::pctrl::srv::PctrlService service(&ipcServer, g_thread_heap_pointer);
        
        Thread threadIpc;
        ::Result rc = threadCreate(&threadIpc, startIpc, &service, g_thread_heap_pointer, ThreadHeapRequiredSizeBytes, 0x2c, -2);        
        if(R_FAILED(rc)) {
            alefbet::pctrl::logger::logToFile("Could not create the service thread, error %i.\n", rc);
            return;
        }
        rc = threadStart(&threadIpc);
        if(R_FAILED(rc)) {
            alefbet::pctrl::logger::logToFile("Could not start the service thread, error %i.\n", rc);
            return;
        }
        rc = threadWaitForExit(&threadIpc);
        if(R_FAILED(rc)) {
            alefbet::pctrl::logger::logToFile("Could not wait for the service thread to end, error %i.\n", rc);
            return;
        }

        alefbet::pctrl::logger::logToFile("Parental control ended\n");
    }

}
