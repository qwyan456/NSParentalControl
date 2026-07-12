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

    // 动态堆（参考可运营 sysmodule 标准做法）：boot2 阶段用 svcSetHeapSize
    // 在运行时扩展堆，不占用静态 BSS（4MB 静态 BSS 会挤掉 qlaunch 导致 2001-0132 开机崩溃）。
    // 2.5MB 足以容纳超时全屏提示所需的 ~1.9MB 帧缓冲（RGBA_4444）。
    constexpr size_t TotalHeapSize = ams::util::AlignUp(2500_KB, ams::os::MemoryHeapUnitSize);

    constexpr size_t ThreadServiceStackRequiredSizeBytes = ams::util::AlignUp(256_KB, 128);
    constexpr size_t ThreadServiceStackRequiredSizeAligned = ams::util::AlignUp(ThreadServiceStackRequiredSizeBytes, ams::os::MemoryPageSize);

    constexpr size_t ThreadMonitorStackRequiredSizeBytes = ams::util::AlignUp(128_KB, 128);
    constexpr size_t ThreadMonitorStackRequiredSizeAligned = ams::util::AlignUp(ThreadMonitorStackRequiredSizeBytes, ams::os::MemoryPageSize);
    
    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_service_memory[ThreadServiceStackRequiredSizeAligned];
    alignas(ams::os::MemoryPageSize) constinit u8 g_thread_monitor_memory[ThreadMonitorStackRequiredSizeAligned];

    // Minimize fs resource usage (sys-clk/nx-ovlloader 标准设置)
    u32 __nx_fs_num_sessions = 1;
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

    // FIX: 动态堆（参考可运营 sysmodule）：boot2 安全，且为全屏帧缓冲留出空间
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

    // FIX: __appInit 严格遵循 sys-clk 模式 — 只做 sm + setsys
    // fs/fsdevMountSdmc 推迟到 main() 中执行
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
    }

    void __appExit(void)
    {
        smExit();
    }

    void __wrap_exit(void)
    {
        svcExitProcess();        
        __builtin_unreachable();
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

    // FIX: 等待 qlaunch 就绪（sys-clk 的 WaitForQLaunch 模式）
    void waitForQLaunch() {
        u64 pid = 0;
        Result rc;
        int attempts = 0;
        do {
            rc = pmdmntGetProcessId(&pid, 0x0100000000000023ULL);
            if(R_SUCCEEDED(rc) && pid != 0) {
                logInfo("[Main] qlaunch is ready (pid=%llu)\n", pid);
                return;
            }
            svcSleepThread(500'000'000); // 500ms
            attempts++;
        } while(attempts < 60); // 最多 30 秒
        
        logError("[Main] qlaunch not detected after 30s, starting anyway\n");
    }

}

int main(int argc, char **argv)
{    
    if (hosversionBefore(9,0,0))
        exit(1);

    // FIX: fsInitialize + fsdevMountSdmc 在这里执行（sys-clk 模式）
    // 不在 __appInit 中执行，避免 boot2 阶段的 FS 冲突
    Result rc = fsInitialize();
    if (R_FAILED(rc))
        fatalThrow(MAKERESULT(Module_HomebrewLoader, 2));
    fsdevMountSdmc();

    clearLog();
    auto settings = loadSettings();
    if(settings.contains(SETTING_LOGLEVEL)) {
        const auto& logLevel = settings[SETTING_LOGLEVEL].int_value;
        setLogLevel(static_cast<LogLevel>(logLevel));
    } else {
        setLogLevel(INFO);
    }

    logInfo("[Main] Parental control starting\n");

    // FIX: 立即注册 IPC 服务（参考可运营 sysmodule）——服务在开机瞬间即可用，
    // 避免此前等待 qlaunch/ns 的 32 秒空窗期内 Overlay 误报 “not installed”。
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

    // 服务已注册，下面再准备 Monitor 所需的 pmdmnt/ns/qlaunch（仅影响监控逻辑，不影响服务可用性）
    pmdmntInitialize();
    
    logInfo("[Main] Waiting for qlaunch to be ready\n");
    alefbet::pctrl::waitForQLaunch();

    // 额外等待 2 秒让 HOME Menu 完全初始化
    svcSleepThread(2'000'000'000LL);

    // 初始化 ns 服务
    nsInitialize();

    Thread threadMonitor;
    alefbet::pctrl::srv::Monitor* monitor = new alefbet::pctrl::srv::Monitor();
    void* monitorArgs[2] { monitor, service };
    rc = threadCreate(&threadMonitor, alefbet::pctrl::startMonitor, &monitorArgs, g_thread_monitor_memory, ThreadMonitorStackRequiredSizeAligned, 0x2c, -2);
    if(R_FAILED(rc)) {
        logError("Could not create the monitor thread, error %i:%i.\n", R_MODULE(rc), R_DESCRIPTION(rc));
        return 4;
    }
    rc = threadStart(&threadMonitor);
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
