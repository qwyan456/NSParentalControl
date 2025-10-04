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
#include "pctrl_font.hpp"
#include "pctrl_service.hpp"
#include "pctrl_screen.hpp"
#include "logger.h"

using namespace ams;

/* Set libnx graphics globals. */
extern "C" {

    u32 __nx_nv_transfermem_size = 0x40000;
    ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

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

        }

    }

    namespace init {

        void InitializeSystemModule() {
            /* Initialize heap. */
            alefbet::pctrl::srv::InitializeFsHeap();

            /* Initialize our connection to sm. */
            R_ABORT_UNLESS(sm::Initialize());

            /* Initialize fs. */
            fs::InitializeForSystem();
            fs::SetAllocator(alefbet::pctrl::srv::AllocateForFs, alefbet::pctrl::srv::DeallocateForFs);
            fs::SetEnabledAutoAbort(false);

            /* Mount the SD card. */
            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));
        }

        void FinalizeSystemModule() { /* ... */ }

        void Startup() { /* ... */ }

    }
}

namespace ams {
    
    void Main() {
        logToFile("Parental control starting");

        /* Set thread name. */
        os::SetThreadNamePointer(os::GetCurrentThread(), "pctrl_Main");
        //AMS_ASSERT(os::GetThreadPriority(os::GetCurrentThread()) == AMS_GET_SYSTEM_THREAD_PRIORITY(pctrl, Main));

        //alefbet::init::InitializeSystemModule();

        /* Load shared font. */
        R_ABORT_UNLESS(alefbet::pctrl::font::InitializeSharedFont());

        /* Loop processing the IPC server. */
        alefbet::pctrl::srv::PctrlService service;
        service.Llsten();

        logToFile("Parental control ended");
    }

}
