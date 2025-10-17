#include <sys/stat.h>
#include <sys/types.h>
#include <vector>
#include <map>
#include <ctime>
#include <cstring>
#include <switch.h>

#define TESLA_INIT_IMPL
#include "tesla.hpp"
#include "gui/main_overlay.h"
#include "logger.h"
#include "AppContext.h"

//Examples : https://github.com/masagrator/Status-Monitor-Overlay/blob/master/source/main.cpp

const bool CIPHER_DATABASE = false;
constexpr SmServiceName service_name = smEncodeName("pctrl");

bool connectToService() {
    u8 exists(0);
    Result res = tipcDispatchInOut(smGetServiceSessionTipc(), 65100, service_name, exists);
    if(R_FAILED(res) || !exists) {
        logToFile("[Overlay] could not find the service");
        return false;
    } 
        
    logToFile("[Overlay] the service pctrl has been found");    

    res = smGetServiceWrapper(&getAppContext().pctrl_service, service_name);
    if(R_FAILED(res)) {
        logToFile("[Overlay] could not open service");
        logIntToFile(R_MODULE(res));
        logIntToFile(R_DESCRIPTION(res));
        return false;
    }

    logToFile("[Overlay] Connected to pctrl service");
    logIntToFile(getAppContext().pctrl_service.session);
    getAppContext().is_enabled = true;

    return true;
}

int main(int argc, char** argv) {
    smInitialize();
    fsInitialize();
    fsdevMountSdmc();

    setLogFilename("sdmc:/switch/pctrl_ovl.log");
    clearLog();
    
    connectToService();    

    tsl::loop<MainOverlay>(argc, argv);    

    logToFile("[Overlay] Close session");
    if(getAppContext().is_enabled) {
        svcCloseHandle(getAppContext().pctrl_service.session);   
    }

    logToFile("[Overlay] Exiting overlay");

    // Clean all
    if(getAppContext().pctrl_service.session > 0) {
        serviceClose(&getAppContext().pctrl_service);
    }
    fsdevUnmountAll(); 
    fsExit(); 
    smExit();    
}