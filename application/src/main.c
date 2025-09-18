// Include the most common headers from the C standard library
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include the main libnx system header, for Switch development
#include <switch.h>
#include "debug.h"
#include "logger.h"
#include "gui_helper.hpp"

/*#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"*/

// Main program entrypoint
int main(int argc, char* argv[])
{
    clearLog();    

    // Retrieve the default window
    NWindow* win = nwindowGetDefault();
    /*Result rc = viInitialize(ViServiceType_Application);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    ViDisplay m_display;
    rc = viOpenDefaultDisplay(&m_display);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    u64 layer_id;
    rc = viCreateManagedLayer(&m_display, (ViLayerFlags)0, 0, &layer_id);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    ViLayer layer;
    rc = viCreateLayer(&m_display, &layer);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    rc = viSetLayerZ(&layer, 100);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    rc = viSetLayerSize(&layer, FB_WIDTH, FB_HEIGHT);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    NWindow win;
    rc = nwindowCreateFromLayer(&win, &layer);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }*/

    // Create a linear double-buffered framebuffer
    Framebuffer fb;
    logToFile("framebufferCreate");
    Result rc = framebufferCreate(&fb, win, FramebufferWidth, FramebufferHeight, PIXEL_FORMAT_RGBA_4444, 2);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    logToFile("framebufferMakeLinear");
    rc = framebufferMakeLinear(&fb);
    logIntToFile(debugResult(rc));
    if(R_FAILED(rc)) {
        return 1;
    }

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);
        
    u16 img_width = 799;
    u16 img_height = 396;
    u16 posX = (FramebufferWidth - img_width) / 2;
    u16 posY = (FramebufferHeight - img_height) / 2;

    initImagesBuffers();

    // Main loop
    while (appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been
        // newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus)
            break; // break in order to return to hbmenu

        // Retrieve the framebuffer
        u32 stride = 0;
        Buffer framebuf = (Buffer) framebufferBegin(&fb, &stride);

        if(!framebuf) {
            logToFile("framebufferBegin failed");
            break;
        }

        drawRect(&fb, 0, 0, FramebufferWidth, FramebufferHeight, makeColor(200, 200, 200, 40));
        drawLine(&fb, 0, 0, FramebufferWidth, FramebufferHeight, makeColor(255, 0, 0, 255));     
        //drawText(&fb, "du texte pour voir...", 10, 100, FramebufferWidth-10, 40, 20);
        drawImage(&fb, "sdmc:/switch/alert_time_out.png", PNG, posX, posY, img_width, img_height);

        // We're done rendering, so we end the frame here.
        framebufferEnd(&fb);
    }

    clearImagesBuffers();

    framebufferClose(&fb);
    return 0;
}
