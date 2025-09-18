// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logger.h"
#include "debug.h"
#include "GuiController.h"

// Include the main libnx system header, for Switch development
#include <switch.h>

// Size of the inner heap (adjust as necessary).
#define INNER_HEAP_SIZE 0x4000
bool app_ready = false;

// Newlib heap configuration function (makes malloc/free work).
void __libnx_initheap(void)
{
    return;
    static u8 inner_heap[INNER_HEAP_SIZE];
    extern void* fake_heap_start;
    extern void* fake_heap_end;

    // Configure the newlib heap.
    fake_heap_start = inner_heap;
    fake_heap_end   = inner_heap + sizeof(inner_heap);
}

// Service initialization.
void __appInit(void)
{
    Result rc;

    // Open a service manager session.
    rc = smInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    // Retrieve the current version of Horizon OS.
    rc = setsysInitialize();
    if (R_SUCCEEDED(rc)) {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    // Enable this if you want to use HID.
    /*rc = hidInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));*/

    // Enable this if you want to use time.
    /*rc = timeInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_Time));

    __libnx_init_time();*/

    // Disable this if you don't want to use the filesystem.
    rc = fsInitialize();
    if (R_FAILED(rc))
        diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_InitFail_FS));

    // Disable this if you don't want to use the SD card filesystem.
    fsdevMountSdmc();

    clearLog();    
    app_ready = true;
        
}

// Service deinitialization.
void __appExit(void)
{
    logToFile("Closing");

    // Close extra services you added to __appInit here.
    //hidExit();
    fsdevUnmountAll(); // Disable this if you don't want to use the SD card filesystem.
    fsExit(); // Disable this if you don't want to use the filesystem.
    smExit();
    //timeExit(); // Enable this if you want to use time.
    //hidExit(); // Enable this if you want to use HID.
}

// Main program entrypoint
int main(int argc, char* argv[])
{    
    if(!app_ready) {
        return 1;
    }
    
    logToFile("Starting Parental Control sysmodule");

    sleep(3);
    showTimeoutAlert();
    sleep(10);

    // Initialize data structures

    // Initialize database if needed

    // Load database

    // Create service

    // Start monitoring thread    
    
    // If you need threads, you can use threadCreate etc.    
    
    logToFile("Parental control ended");

    return 0;
}
