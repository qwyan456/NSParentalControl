#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include "logger.h"
#include "utils.h"
#include "ipc/Server.hpp"
#include "service.hpp"
#include "monitor.h"

using namespace alefbet::pctrl::logger;

#ifdef __cplusplus
extern "C" {
#endif

    //static char g_argv[2048];
    //static char g_nextArgv[2048];
    //static char g_nextNroPath[512];
    //u64  g_nroAddr = 0;
    //static u64  g_nroSize = 0;
    //static NroHeader g_nroHeader;

    /*static u64 g_appletHeapSize = 0;
    static u64 g_appletHeapReservationSize = 0;*/

    constexpr size_t TotalHeapSize = ams::util::AlignUp(6_MB, ams::os::MemoryHeapUnitSize);

    constexpr size_t ThreadServiceStackRequiredSizeBytes = ams::util::AlignUp(512_KB, 128);
    constexpr size_t ThreadServiceStackRequiredSizeAligned = ams::util::AlignUp(ThreadServiceStackRequiredSizeBytes, ams::os::MemoryPageSize);

    constexpr size_t ThreadMonitorStackRequiredSizeBytes = ams::util::AlignUp(256_KB, 128);
    constexpr size_t ThreadMonitorStackRequiredSizeAligned = ams::util::AlignUp(ThreadMonitorStackRequiredSizeBytes, ams::os::MemoryPageSize);
    
    constinit u8 *g_heap_pointer = nullptr;

    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_service_memory[ThreadServiceStackRequiredSizeAligned];
    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_monitor_memory[ThreadMonitorStackRequiredSizeAligned];

    //static u128 g_userIdStorage;

    //static u8 g_savedTls[0x100];

    // Minimize fs resource usage
    u32 __nx_fsdev_direntry_cache_size = 1;
    bool __nx_fsdev_support_cwd = false;

    // Used by trampoline.s
    Result g_lastRet = 0;

    extern void* __stack_top; // Defined in libnx.
    #define STACK_SIZE 0x10000 // Change this if main-thread stack size ever changes.    

    u32 __nx_applet_type = AppletType_None;
    //u32 __nx_fs_num_sessions = 1;
    //u32  __nx_nv_transfermem_size = 0x40000;
    ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

    /** End of Tesla global variables */

    void __libnx_initheap(void)
    {
        //static char g_innerheap[0x4000];

        extern char* fake_heap_start;
        extern char* fake_heap_end;        
        void* addr = nullptr;
        Result rc = svcSetHeapSize(&addr, TotalHeapSize);
        if(R_SUCCEEDED(rc)) {
            fake_heap_start = (char*)addr;
            fake_heap_end   = fake_heap_start + TotalHeapSize;
        }
    }

    void __appInit(void)
    {
        Result rc;

        rc = smInitialize();
        if (R_FAILED(rc))
            fatalThrow(MAKERESULT(Module_HomebrewLoader, 1));

        rc = setsysInitialize();
        if (R_SUCCEEDED(rc)) {
            SetSysFirmwareVersion fw;
            rc = setsysGetFirmwareVersion(&fw);
            if (R_SUCCEEDED(rc))
                hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
            //g_appletHeapSize = TotalHeapSize; //0x600000;
            //g_appletHeapReservationSize = 0x00;
            setsysExit();
        }

        rc = fsInitialize();
        if (R_FAILED(rc))
            fatalThrow(MAKERESULT(Module_HomebrewLoader, 2));

        
        if (hosversionAtLeast(16,0,0)) {
            plInitialize(PlServiceType_User);
        } else {
            plInitialize(PlServiceType_System);
        }

        i2cInitialize();
        bpcInitialize();
        hidInitialize();
        hidsysInitialize();
        pmdmntInitialize();
        nsInitialize();
        smExit();
    }

    void __wrap_exit(void)
    {
        smExit(); 
        svcExitProcess();        
        __builtin_unreachable();
    }

    /*static void*  g_heapAddr;
    static size_t g_heapSize;

    static bool setupHbHeap(void)
    {
        void* addr = NULL;
        u64 size = g_appletHeapSize;

        Result rc = svcSetHeapSize(&addr, size);

        if (R_FAILED(rc) || addr==NULL)
            fatalThrow(MAKERESULT(Module_HomebrewLoader, 9));

        g_heapAddr = addr;
        g_heapSize = size;        

        return g_heapSize > 0;

    }*/

    void testMemory() {
        //logToFile("[Main] g_heapAddr=%p\n", g_heapAddr);
        //logToFile("[Main] g_heapSize=%p\n", g_heapSize);

        int on_stack = 0;
        int* on_heap = new int(0);
        logToFile("@on_stack=%p\n", &on_stack);
        logToFile("@on_heap=%p\n", (void*)on_heap);

        extern char* fake_heap_start;
        extern char* fake_heap_end;
        logToFile("@fake_heap_start=%p, @fake_heap_end=%p, size==%i\n", (void*)fake_heap_start, (void*)fake_heap_end, (uintptr_t)fake_heap_end-(uintptr_t)fake_heap_start);

        //Memory allocation test
        /*for(int s = 0x1000 ; s <= 0x150000 ; s += 5000) {
            void* ptr = aligned_alloc(0x1000, s);
            if(ptr != nullptr) {
                logToFile("Allocation of %i bytes succeeded. @ptr=%p\n", s, (void*)ptr);
                free(ptr);
            } else {
                logToFile("Allocation of %i bytes failed\n", s);    
                break;                                            
            }            
        } */       
    }


}

namespace alefbet::pctrl {

    void startIpc(void* service) {
        alefbet::pctrl::srv::Service* srv = static_cast<alefbet::pctrl::srv::Service*>(service);        
        srv->listen();            
    }

    void startMonitor(void* args) {
        void** _args = (void**)args;
        alefbet::pctrl::srv::Monitor* monitor = static_cast<alefbet::pctrl::srv::Monitor*>(_args[0]);
        alefbet::pctrl::srv::Service* service = static_cast<alefbet::pctrl::srv::Service*>(_args[1]);

        logToFile("@monitor=%p\n", monitor);
        logToFile("@service=%p\n", service);

        alefbet::pctrl::srv::Monitor* mon = static_cast<alefbet::pctrl::srv::Monitor*>(monitor);
        mon->setService(service);
        mon->loop();
    }

}

int main(int argc, char **argv)
{    
    if (hosversionBefore(9,0,0))
        exit(1);

    clearLog();
    logToFile("[Main] Parental control starting\n");

    /*if(!setupHbHeap()) {
        logToFile("[Main] Could not continue without a valid heap.\n");
        return 1;
    }*/

    testMemory();

    ::Result rc = 0;

    // Loop processing the IPC server.        
    Ipc::Server* ipcServer = new Ipc::Server("pctrl");
    logToFile("@ipcServer=%p\n", (void*)ipcServer);
    alefbet::pctrl::srv::Service* service = new alefbet::pctrl::srv::Service(ipcServer);
    logToFile("@service=%p\n", (void*)service);

    Thread threadIpc;
    //TODO: replace heap with malloc buffer?
    rc = threadCreate(&threadIpc, alefbet::pctrl::startIpc, service, g_thread_service_memory, ThreadServiceStackRequiredSizeAligned, 0x2c, -2);               
    if(R_FAILED(rc)) {
        logToFile("Could not create the service thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 2;
    }
    rc = threadStart(&threadIpc);
    if(R_FAILED(rc)) {
        logToFile("Could not start the service thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 3;
    }
        
    Thread threadMonitor;
    alefbet::pctrl::srv::Monitor* monitor = new alefbet::pctrl::srv::Monitor();
    void* monitorArgs[2] { monitor, service };
    rc = threadCreate(&threadMonitor, alefbet::pctrl::startMonitor, &monitorArgs, g_thread_monitor_memory, ThreadMonitorStackRequiredSizeAligned, 0x2c, -2);
    if(R_FAILED(rc)) {
        logToFile("Could not create the monitor thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 4;
    }
    rc = threadStart(&threadMonitor); // Run the monitor's loop
    if(R_FAILED(rc)) {
        logToFile("Could not start the monitor thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 5;
    }

    monitor->start(); // Start monitoring

    rc = threadWaitForExit(&threadIpc);
    if(R_FAILED(rc)) {
        logToFile("Could not wait for the service thread to end, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 6;
    }

    rc = threadWaitForExit(&threadMonitor);
    if(R_FAILED(rc)) {
        logToFile("Could not wait for the monitor thread to end, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 7;
    }

    delete monitor;
    delete service;

    logToFile("Parental control ended\n");

    return 0;
}
