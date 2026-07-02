#include <switch.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include "logger.h"
#include "database/database.h"
#include "utils.h"
#include "ipc/Server.hpp"
#include "service.h"
#include "monitor.h"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::database;

#ifdef __cplusplus
extern "C" {
#endif

    constexpr size_t TotalHeapSize = ams::util::AlignUp(2500_KB, ams::os::MemoryHeapUnitSize);

    constexpr size_t ThreadServiceStackRequiredSizeBytes = ams::util::AlignUp(256_KB, 128);
    constexpr size_t ThreadServiceStackRequiredSizeAligned = ams::util::AlignUp(ThreadServiceStackRequiredSizeBytes, ams::os::MemoryPageSize);

    constexpr size_t ThreadMonitorStackRequiredSizeBytes = ams::util::AlignUp(128_KB, 128);
    constexpr size_t ThreadMonitorStackRequiredSizeAligned = ams::util::AlignUp(ThreadMonitorStackRequiredSizeBytes, ams::os::MemoryPageSize);
    
    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_service_memory[ThreadServiceStackRequiredSizeAligned];
    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_monitor_memory[ThreadMonitorStackRequiredSizeAligned];

    // Minimize fs resource usage
    u32 __nx_fsdev_direntry_cache_size = 1;
    bool __nx_fsdev_support_cwd = false;

    u32 __nx_applet_type = AppletType_None;
    ViLayerFlags __nx_vi_stray_layer_flags = (ViLayerFlags)0;

    alignas(ams::os::MemoryPageSize) constinit u8 g_nv_transfer_memory[0x40000];
    ::Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
        *out_size = sizeof(g_nv_transfer_memory);
        
        ::Result rc = tmemCreateFromMemory(t, g_nv_transfer_memory, sizeof(g_nv_transfer_memory), perm);
        if(R_FAILED(rc)) {
            logError("[Main] Could not create TransferMemory\n");            
        } else {
            logDebug("[Main] Successfully created TransferMemory\n");
        }

        return rc;
    }

    void __libnx_initheap(void)
    {
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
            setsysExit();
        }

        rc = fsInitialize();
        if (R_FAILED(rc))
            fatalThrow(MAKERESULT(Module_HomebrewLoader, 2));

        // FIX: 移除所有非必需的服务初始化
        // i2cInitialize() — 完全未使用
        // bpcInitialize() — 仅 rebootToPayload() 需要，改为按需初始化
        // hidInitialize() / hidsysInitialize() — sysmodule 未使用
        // plInitialize() — 仅字体渲染需要，移到 screen_timeout/renderer 按需初始化
        // pmdmntInitialize() / nsInitialize() — 移到 main() 中初始化
        // 这些服务在 boot2 阶段打开会占用系统资源，导致 HOME Menu 初始化冲突
    }

    void __wrap_exit(void)
    {
        // FIX: 移除 smExit()，sm 会在进程退出时自动清理
        svcExitProcess();        
        __builtin_unreachable();
    }

    void testMemory() {
        int on_stack = 0;
        int* on_heap = new int(0);

        extern char* fake_heap_start;
        extern char* fake_heap_end;

        //Memory allocation test
        /*for(int s = 0x1000 ; s <= 0x150000 ; s += 5000) {
            void* ptr = aligned_alloc(0x1000, s);
            if(ptr != nullptr) {
                logDebug("Allocation of %i bytes succeeded. @ptr=%p\n", s, (void*)ptr);
                free(ptr);
            } else {
                logDebug("Allocation of %i bytes failed\n", s);    
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

        monitor->setService(service);
        monitor->loop();
    }

}

int main(int argc, char **argv)
{    
    if (hosversionBefore(9,0,0))
        exit(1);

    clearLog();
    auto settings = loadSettings();
    if(settings.contains(SETTING_LOGLEVEL)) {
        const auto& logLevel = settings[SETTING_LOGLEVEL].int_value;
        setLogLevel(static_cast<LogLevel>(logLevel));
    } else {
        setLogLevel(INFO);
    }

    logInfo("[Main] Parental control starting\n");

    //testMemory();

    // FIX: pmdmnt/ns 延迟到系统就绪后再初始化
    // 在 boot2 阶段初始化这些服务并立即启动 Monitor 会与 HOME Menu 的
    // account 服务初始化产生冲突，导致 HOME Menu (0100000000000023) 崩溃

    ::Result rc = 0;

    // 立即启动 IPC Server，让 overlay 能连接到 pctrl 服务
    Ipc::Server* ipcServer = new Ipc::Server("pctrl");
    alefbet::pctrl::srv::Service* service = new alefbet::pctrl::srv::Service(ipcServer);    

    Thread threadIpc;
    rc = threadCreate(&threadIpc, alefbet::pctrl::startIpc, service, g_thread_service_memory, ThreadServiceStackRequiredSizeAligned, 0x2c, -2);               
    if(R_FAILED(rc)) {
        logError("Could not create the service thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 2;
    }
    rc = threadStart(&threadIpc);
    if(R_FAILED(rc)) {
        logError("Could not start the service thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 3;
    }

    // FIX: 等待系统完全启动后再初始化 pmdmnt/ns 并启动 Monitor
    // HOME Menu (qlaunch) 需要几秒钟才能完全启动
    // 过早调用 pmdmnt/account 等服务会干扰 HOME Menu 的初始化
    logInfo("[Main] Waiting for system to be ready before starting monitor\n");
    svcSleepThread(10'000'000'000LL); // 10 seconds

    // 系统就绪后初始化 pmdmnt 和 ns
    pmdmntInitialize();
    nsInitialize();
        
    Thread threadMonitor;
    alefbet::pctrl::srv::Monitor* monitor = new alefbet::pctrl::srv::Monitor();
    void* monitorArgs[2] { monitor, service };
    rc = threadCreate(&threadMonitor, alefbet::pctrl::startMonitor, &monitorArgs, g_thread_monitor_memory, ThreadMonitorStackRequiredSizeAligned, 0x2c, -2);
    if(R_FAILED(rc)) {
        logError("Could not create the monitor thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 4;
    }
    rc = threadStart(&threadMonitor); // Run the monitor's loop
    if(R_FAILED(rc)) {
        logError("Could not start the monitor thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 5;
    }

    // Start monitoring
    monitor->start(); 

    rc = threadWaitForExit(&threadIpc);
    if(R_FAILED(rc)) {
        logError("Could not wait for the service thread to end, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 6;
    }

    rc = threadWaitForExit(&threadMonitor);
    if(R_FAILED(rc)) {
        logError("Could not wait for the monitor thread to end, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 7;
    }

    delete monitor;
    delete service;

    logInfo("Parental control ended\n");

    return 0;
}
