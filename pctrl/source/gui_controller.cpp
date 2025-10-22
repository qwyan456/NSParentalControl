#include "gui_controller.h"
#include "logger.h"
#include "gui/renderer.hpp"

using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::gfx;

namespace images {
    #include "inc/chrono_0_pc.inc"    
    #include "inc/chrono_9_pc.inc"
    #include "inc/chrono_17_pc.inc"
    #include "inc/chrono_25_pc.inc"
    #include "inc/chrono_34_pc.inc"
    #include "inc/chrono_42_pc.inc"
    #include "inc/chrono_50_pc.inc"
    #include "inc/chrono_59_pc.inc"
    #include "inc/chrono_67_pc.inc"
    #include "inc/chrono_75_pc.inc"
    #include "inc/chrono_84_pc.inc"
    #include "inc/chrono_92_pc.inc"
    #include "inc/chrono_100_pc.inc"
    #include "inc/chrono_1_mn.inc"
    #include "inc/chrono_2_mn.inc"
    #include "inc/chrono_3_mn.inc"
    #include "inc/chrono_4_mn.inc"
    #include "inc/chrono_5_mn.inc"
}

void GuiController::showScreenTimeout() {
    logToFile("[Gui] ShowScreenTimeout\n");

    showRemainingTimePanel();

    logToFile("[Gui] ShowScreenTimeout loop ended\n");
}

void GuiController::hideScreenTimeout() {
    logToFile("[Gui] Hide timeout screen\n");
    auto& renderer = Renderer::get();
    renderer.exit();    
}

void GuiController::showRemainingTimePanel() {
    if(remaining_time_visible_) return;
    remaining_time_visible_ = true;

    logToFile("[Gui] Show remaining time panel\n");
    width_ = 128; //Width must be a multiple of 64
    height_ = 144;

    u16 posX = ScreenWidth - width_ - 20;
    u16 posY = 20;    

    showOverlay(width_, height_, posX, posY);

    auto& renderer = Renderer::get();
    renderer.startFrame();
        
    // Draw the timer icon
    //renderer.drawBitmap(0, 0, width_, height_, images::chrono_100_pc);
    
    renderer.endFrame();
}   

void GuiController::updateRemainingTimePanel(const u16& remaining_time_in_minutes, const u16& limit_in_minutes) {
    if(!remaining_time_visible_) return;

    logToFile("[Gui] Update remaining time panel\n");
    auto& renderer = Renderer::get();
    renderer.startFrame();        
    renderer.clearScreen();

    // Draw the timer icon
    const u8* image_data = nullptr;
    
    float remaining_time_in_percent = (float)remaining_time_in_minutes * 100.0f / (float)limit_in_minutes;
    logToFile("[Gui] remaining_time_in_percent=%f, remaining_time_in_minutes=%i, limit_in_minutes=%i\n", remaining_time_in_percent, remaining_time_in_minutes, limit_in_minutes);

    if(remaining_time_in_minutes >= 0 && remaining_time_in_minutes <= 5) {
        // Manage the 5 last minutes
        switch(remaining_time_in_minutes) {
            case 5: image_data = images::chrono_5_mn; break;
            case 4: image_data = images::chrono_4_mn; break;
            case 3: image_data = images::chrono_3_mn; break;
            case 2: image_data = images::chrono_2_mn; break;
            case 1: image_data = images::chrono_1_mn; break;
        }        
    } else if(remaining_time_in_minutes > 5 && remaining_time_in_percent <= 8) {
        image_data = images::chrono_9_pc;
    } else if(remaining_time_in_percent > 8 && remaining_time_in_percent <= 16) {
        image_data = images::chrono_17_pc;
    } else if(remaining_time_in_percent > 16 && remaining_time_in_percent <= 24) {
        image_data = images::chrono_25_pc;
    } else if(remaining_time_in_percent > 24 && remaining_time_in_percent <= 32) {
        image_data = images::chrono_34_pc;
    } else if(remaining_time_in_percent > 32 && remaining_time_in_percent <= 40) {
        image_data = images::chrono_42_pc;
    } else if(remaining_time_in_percent > 40 && remaining_time_in_percent <= 48) {
        image_data = images::chrono_50_pc;
    } else if(remaining_time_in_percent > 48 && remaining_time_in_percent <= 56) {
        image_data = images::chrono_59_pc;
    } else if(remaining_time_in_percent > 56 && remaining_time_in_percent <= 64) {
        image_data = images::chrono_67_pc;
    } else if(remaining_time_in_percent > 64 && remaining_time_in_percent <= 72) {
        image_data = images::chrono_75_pc;
    } else if(remaining_time_in_percent > 72 && remaining_time_in_percent <= 80) {
        image_data = images::chrono_84_pc;
    } else if(remaining_time_in_percent > 80 && remaining_time_in_percent <= 88) {
        image_data = images::chrono_92_pc;
    } else if(remaining_time_in_percent > 88) {
        image_data = images::chrono_100_pc;
    }

    if(image_data != nullptr) {
        renderer.drawBitmap(0, 0, width_, height_, image_data);
    } else {
        logToFile("[Gui] No rule found to show an image.\n");
    }
    
    renderer.endFrame();
}

void GuiController::hideRemainingTimePanel() {
    logToFile("[Gui] Hide remaining time panel\n");
    auto& renderer = Renderer::get();
    renderer.exit();
    remaining_time_visible_ = false;
}

void GuiController::showOverlay(u16 width, u16 height, u16 posX, u16 posY) {
    logToFile("[Gui] show Overlay of size %ix%i\n", width, height);

    auto& renderer = Renderer::get();    
    renderer.init(width, height, posX, posY);
    clearScreen();

    //requestForeground(true);
}

void GuiController::clearScreen() {
    auto& renderer = Renderer::get();
    renderer.startFrame();
    renderer.clearScreen();
    renderer.endFrame();
}

/**
 * @brief libnx hid:sys shim that gives or takes away frocus to or from the process with the given aruid
 *
 * @param enable Give focus or take focus
 * @param aruid Aruid of the process to focus/unfocus
 * @return Result Result
 */
/*Result GuiController::hidsysEnableAppletToGetInput(bool enable, u64 aruid) {
    const struct {
        u8 permitInput;
        u64 appletResourceUserId;
    } in = { enable != 0, aruid };

    return serviceDispatchIn(hidsysGetServiceSession(), 503, in);
}*/

/*void GuiController::requestForeground(bool enabled) {
    u64 applicationAruid = 0, appletAruid = 0;

    for (u64 programId = 0x0100000000001000UL; programId < 0x0100000000001020UL; programId++) {
        pmdmntGetProcessId(&appletAruid, programId);

        if (appletAruid != 0)
            hidsysEnableAppletToGetInput(!enabled, appletAruid);
    }

    pmdmntGetApplicationProcessId(&applicationAruid);
    hidsysEnableAppletToGetInput(!enabled, applicationAruid);

    hidsysEnableAppletToGetInput(true, 0);
}*/