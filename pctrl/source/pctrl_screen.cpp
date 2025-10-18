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
#include "pctrl_screen.hpp"
#include "pctrl_font.hpp"
#include "logger.h"
#include "database/database.h"
#include "helpers.h"

namespace util = ams::util;
namespace os = ams::os;
namespace svc = ams::svc;
namespace font = alefbet::pctrl::font;
namespace dd = ams::dd;
namespace hos = ams::hos;
using namespace ams;
using namespace ams::literals;      
using namespace alefbet::pctrl::logger;
using namespace alefbet::pctrl::database;

/* There should only be a single transfer memory (for nv). */
alignas(os::MemoryPageSize) constinit u8 g_nv_transfer_memory[0x40000];

extern "C" ::Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
    *out_size = sizeof(g_nv_transfer_memory);
    return tmemCreateFromMemory(t, g_nv_transfer_memory, sizeof(g_nv_transfer_memory), perm);
}

namespace alefbet::pctrl::srv {
    /* Include the logo into its own anonymous namespace. */
    namespace {
        #include "pctrl_logo.inc"
    }

    namespace {

        /* Screen definitions. */
        constexpr u32 PctrlScreenWidth = 1280;
        constexpr u32 PctrlScreenHeight = 720;
        constexpr u32 PctrlScreenBpp = 2;
        constexpr u32 PctrlLayerZ = 100;
        //constexpr u16 BackgroundColor = RGB888_TO_RGB565(0x7, 0x7, 0x7);

        constexpr u32 PctrlScreenWidthAlignedBytes = util::AlignUp(PctrlScreenWidth * PctrlScreenBpp, 64);
        constexpr u32 PctrlScreenWidthAligned = PctrlScreenWidthAlignedBytes / PctrlScreenBpp;        

        /* There should only be a single (1280*768) framebuffer. */
        constexpr size_t FrameBufferRequiredSizeBytes       = PctrlScreenWidthAlignedBytes * util::AlignUp(PctrlScreenHeight, 128);
        constexpr size_t FrameBufferRequiredSizePageAligned = util::AlignUp(FrameBufferRequiredSizeBytes, os::MemoryPageSize);
        constexpr size_t FrameBufferRequiredSizeHeapAligned = util::AlignUp(FrameBufferRequiredSizeBytes, os::MemoryHeapUnitSize);

        constinit u8 *g_framebuffer_pointer = nullptr;        
    }        

    /* Pixel calculation helper. */
    constexpr u32 GetPixelOffset(u32 x, u32 y) {
        u32 tmp_pos = ((y & 127) / 16) + (x/32*8) + ((y/16/8)*(((PctrlScreenWidthAligned/2)/16*8)));
        tmp_pos *= 16*16 * 4;

        tmp_pos += ((y%16)/8)*512 + ((x%32)/16)*256 + ((y%8)/2)*64 + ((x%16)/8)*32 + (y%2)*16 + (x%8)*2; //This line is a modified version of code from the Tegra X1 datasheet.

        return tmp_pos / 2;
    }

    void GuiController::InitializeFrameBufferPointer() {
        logToFile("[Screen] InitializeFrameBufferPointer\n");

        /*if(heap_pointer_ != nullptr) {
            g_framebuffer_pointer = heap_pointer_;
            logToFile("[Screen] g_framebuffer_pointer points to heap_pointer\n");
            return;
        }*/

        // We couldn't use heap, so try insecure memory, from the system nonsecure pool.
        {
            uintptr_t address = 0;
            if (R_SUCCEEDED(os::AllocateInsecureMemory(std::addressof(address), FrameBufferRequiredSizePageAligned))) {            
                g_framebuffer_pointer = reinterpret_cast<u8 *>(address);
                logToFile("[Screen] g_framebuffer_pointer initialized in insecure memory @%p\n", g_framebuffer_pointer);
                return;
            }

            logToFile("[Screen] Allocate insecure memory failed\n");
        }

        // Neither heap nor insecure is available, so we're going to have to try to raid the unsafe pool. 
        {
            // First, increase the limit to an extremely high value.
            size_t large_size = std::max(128_MB, FrameBufferRequiredSizeHeapAligned);
            while (svc::ResultLimitReached::Includes(svc::SetUnsafeLimit(large_size))) {
                large_size *= 2;
            }

            // Next, map some unsafe memory. 
            uintptr_t address = 0;
            if (R_SUCCEEDED(os::AllocateUnsafeMemory(std::addressof(address), FrameBufferRequiredSizePageAligned))) {
                g_framebuffer_pointer = reinterpret_cast<u8 *>(address);
                logToFile("[Screen] g_framebuffer_pointer initialized in insecure pool @%p\n", g_framebuffer_pointer);
                return;
            }

            logToFile("[Screen] No memory could be allocated for framebuffer\n");
        }
    }

    /* Task implementations. */
    ::Result GuiController::SetupDisplayInternal() {
        logToFile("[Screen] SetupDisplayInternal\n");

        ViDisplay temp_display;

        /* Try to open the display. */
        ::Result res = viOpenDisplay("Internal", std::addressof(temp_display));
        if(res != 0) {
            // Do not overreact
            logToFile("[Screen] Could not open internal display\n");
            // but fail...
            return res;
        }

        /* Guarantee we close the display. */
        ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

        /* Turn on the screen. */
        if (hos::GetVersion() >= hos::Version_3_0_0) {
            viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On);
            // Best effort
            logToFile("[Screen] Turn the screen on the regular way\n");
        } else {
            /* Prior to 3.0.0, the ViPowerState enum was different (0 = Off, 1 = On). */
            viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On_Deprecated);
            // Best effort
            logToFile("[Screen] Turn the screen on the old way\n");
        }

        /* Set alpha to 1.0f. */
        res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
        if(res != 0) {
            logToFile("[Screen] Could not set internal display alpha\n");
        }

        return 0;
    }

    ::Result GuiController::SetupDisplayExternal() {
        logToFile("[Screen] SetupDisplayExternal\n");

        ViDisplay temp_display;
        /* Try to open the display. */
        ::Result res = viOpenDisplay("External", std::addressof(temp_display));
        if(res != 0) {
            // Do not overreact
            logToFile("[Screen] Could not open external display\n");
            // but fail...
            return res;
        }

        /* Guarantee we close the display. */
        ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

        /* Set alpha to 1.0f. */
        res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
        if(res != 0) {
            logToFile("[Screen] Could not set external display alpha\n");
        }

        return 0;
    }

    ::Result GuiController::PrepareScreenForDrawing() {    
        logToFile("[Screen] PrepareScreenForDrawing\n");

        /* Connect to vi. */
        ::Result res = viInitialize(ViServiceType_Manager);
        if(res != 0) {
            logToFile("[Screen] Could not initialise VI service\n");
            return res; //Fail
        }

        /* Close other content. */
        //viSetContentVisibility(false);

        /* Setup the two displays. */
        // Ignore errors (they are logged)
        SetupDisplayInternal();
        SetupDisplayExternal();

        /* Open the default display. */
        TRY_AND_RETURN(
            viOpenDefaultDisplay(std::addressof(m_display)), 
            "[Screen] Could not open default display. Aborting.\n"
        )

        /* Reset the display magnification to its default value. */
        s32 display_width, display_height;
        TRY_AND_RETURN(
            viGetDisplayLogicalResolution(std::addressof(m_display), std::addressof(display_width), std::addressof(display_height)),
            "[Screen] Could not get display logical resolution.\n"
        )

        /* viSetDisplayMagnification was added in 3.0.0. */
        if (hos::GetVersion() >= hos::Version_3_0_0) {
            TRY_AND_RETURN(
                viSetDisplayMagnification(std::addressof(m_display), 0, 0, display_width, display_height),
                "[Screen] Could not reset display magnification.\n"
            )
        }

        /* Create layer to draw to. */
        TRY_AND_RETURN(
            viCreateLayer(std::addressof(m_display), std::addressof(m_layer)),
            "[Screen] Could not create layer.\n"
        )

        logToFile("[Screen] Layer address=%p\n", (void*)std::addressof(m_layer));

        /* Setup the layer. */
        {
            /* Display a layer of 1280 x 720 at 1.5x magnification */
            /* NOTE: N uses 2 (770x400) RGBA4444 buffers (tiled buffer + linear). */
            /* We use a single 1280x720 tiled RGB565 buffer. */
            constexpr s32 RawWidth = PctrlScreenWidth;
            constexpr s32 RawHeight = PctrlScreenHeight;
            constexpr s32 LayerWidth = ((RawWidth) * 3) / 2;
            constexpr s32 LayerHeight = ((RawHeight) * 3) / 2;

            const float layer_x = static_cast<float>((display_width - LayerWidth) / 2);
            const float layer_y = static_cast<float>((display_height - LayerHeight) / 2);

            TRY_AND_RETURN(
                viSetLayerSize(std::addressof(m_layer), LayerWidth, LayerHeight),
                "[Screen] Could not set layer size.\n"
            )

            /* Set the layer's Z at display maximum, to be above everything else .*/
            TRY_AND_RETURN(
                viSetLayerZ(std::addressof(m_layer), PctrlLayerZ),
                "[Screen] Could not set layer z position.\n"
            )

            /* Center the layer in the screen. */
            TRY_AND_RETURN(
                viSetLayerPosition(std::addressof(m_layer), layer_x, layer_y),
                "[Screen] Could not set layer position.\n"
            )

            /* Create framebuffer. */
            TRY_AND_RETURN(
                nwindowCreateFromLayer(std::addressof(m_win), std::addressof(m_layer)),
                "[Screen] Could not create nwindow from layer.\n"
            )

            TRY_AND_RETURN(
                InitializeNativeWindow(),
                "[Screen] Could not initialize native window.\n"
            )
        }

        return 0;
    }

    void GuiController::PreRenderFrameBuffer() {   
        logToFile("[Screen] PreRenderFrameBuffer\n");

        /* Allocate a frame buffer. */
        InitializeFrameBufferPointer();
        if(g_framebuffer_pointer == nullptr) {
            logToFile("[Screen] The framebuffer pointer is null. Aborting.\n");
            return;
        }

        /* Temporarily use the NV transfer memory as font backing heap. */
        font::SetHeapMemory(g_nv_transfer_memory, sizeof(g_nv_transfer_memory));
        ON_SCOPE_EXIT { std::memset(g_nv_transfer_memory, 0, sizeof(g_nv_transfer_memory)); };

        PreRenderContents();
    }

    ::Result GuiController::PreRenderContents() {
        logToFile("[Screen] PreRenderContents\n");

        /* Pre-render the image into the static framebuffer. */
        u16 *tiled_buf = reinterpret_cast<u16 *>(g_framebuffer_pointer);

        /* Let the font manager know about our framebuffer. */
        font::ConfigureFontFramebuffer(tiled_buf, GetPixelOffset);
        font::SetFontColor(0x000F);

        /* Draw a background. */
        for (size_t i = 0; i < FrameBufferRequiredSizeBytes / sizeof(*tiled_buf); i++) {
            tiled_buf[i] = pctrl_logo[0];
        }
        
        u32 logo_x = (PctrlScreenWidth - LogoWidth) / 2;
        u32 logo_y = (PctrlScreenHeight - LogoHeight) / 2;

        /* Draw the logo in the center of the screen */
        for (size_t y = 0; y < LogoHeight; y++) {
            for (size_t x = 0; x < LogoWidth; x++) {
                tiled_buf[GetPixelOffset(logo_x + x, logo_y + y)] = pctrl_logo[y * LogoWidth + x];
            }
        }

        /* Show message */
        font::SetPosition(336, 318);
        font::SetFontSize(62.0f);
        font::PrintFormat("Your time is up!");
        font::SetPosition(473, 402);
        font::SetFontSize(25.0f);
        font::PrintFormat("Press Vol+ to reboot");
        
        return 0;
    }

    ::Result GuiController::InitializeNativeWindow() {
        logToFile("[Screen] InitializeNativeWindow\n");

        //* Setup nv driver. */
        TRY_AND_RETURN(
            nvInitialize(),
            "[Screen] Could not initialize NV service\n"
        )

        TRY_AND_RETURN(
            nvMapInit(),
            "[Screen] Could not init Map\n"
        )

        TRY_AND_RETURN(
            nvFenceInit(),
            "[Screen] Could not init fence\n"
        )

        /* Create nvmap. */
        TRY_AND_RETURN(
            nvMapCreate(std::addressof(m_map), g_framebuffer_pointer, FrameBufferRequiredSizeBytes, 0x20000, NvKind_Pitch, true),
            "[Screen] Could not create Map\n"
        )

        /* Setup graphics buffer. */
        {
            NvGraphicBuffer grbuf               = {};
            grbuf.header.num_ints               = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
            grbuf.unk0                          = -1;
            grbuf.magic                         = 0xDAFFCAFF;
            grbuf.pid                           = 42;
            grbuf.usage                         = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
            grbuf.format                        = PIXEL_FORMAT_RGB_565;
            grbuf.ext_format                    = PIXEL_FORMAT_RGB_565;
            grbuf.num_planes                    = 1;
            grbuf.planes[0].width               = PctrlScreenWidth;
            grbuf.planes[0].height              = PctrlScreenHeight;
            grbuf.planes[0].color_format        = NvColorFormat_R5G6B5;
            grbuf.planes[0].layout              = NvLayout_BlockLinear;
            grbuf.planes[0].kind                = NvKind_Generic_16BX2;
            grbuf.planes[0].block_height_log2   = 4;
            grbuf.nvmap_id                      = nvMapGetId(std::addressof(m_map));
            grbuf.stride                        = PctrlScreenWidthAligned;
            grbuf.total_size                    = FrameBufferRequiredSizeBytes;
            grbuf.planes[0].pitch               = PctrlScreenWidthAlignedBytes;
            grbuf.planes[0].size                = FrameBufferRequiredSizeBytes;
            grbuf.planes[0].offset              = 0;            

            TRY_AND_RETURN(
                nwindowConfigureBuffer(std::addressof(m_win), 0, std::addressof(grbuf)),
                "[Screen] Could not configure nwindow buffer\n"
            )
        }

        return 0;        
    }

    void GuiController::DisplayPreRenderedFrame() {
        logToFile("[Screen] DisplayPreRenderedFrame\n");

        s32 slot;

        logToFile("[Screen] Before nwindowDequeueBuffer\n");
        ::Result res = nwindowDequeueBuffer(std::addressof(m_win), std::addressof(slot), nullptr);
        if(res != 0) {
            logToFile("[Screen] Could not dequeue buffer for nwindow\n");
            return;
        }

        logToFile("[Screen] Before dd:FlushDataCache\n");
        dd::FlushDataCache(g_framebuffer_pointer, FrameBufferRequiredSizeBytes);
        logToFile("[Screen] After dd::FlushDataCache\n");
        R_ABORT_UNLESS(nwindowQueueBuffer(std::addressof(m_win), m_win.cur_slot, NULL));
    }

    ::Result GuiController::ShowScreenTimeout() {
        logToFile("[Screen] Show timeout screen\n");
        auto settings = loadSettings();

        /*if(settings.contains(SETTING_WORKING_MODE)) {
            if(settings[SETTING_WORKING_MODE].int_value == WorkingModeInfo) {
                logToFile("[Screen] display mode is information.\n");
                return 0;
            }
        }*/

        /* Pre-render the framebuffer. */
        this->PreRenderFrameBuffer();

        /* Prepare screen for drawing. */
        ::Result res = this->PrepareScreenForDrawing();
        if(res != 0) {
            logToFile("[Screen] PrepareScreenForDrawing failed\n");
            return res;
        }

        /* Display the pre-rendered frame. */
        this->DisplayPreRenderedFrame();
        
        /* Wait for the user to press the Volume button */
        if(WaitForVolumeButton()) {
            logToFile("[Screen] Volume button pressed, shutdown the console\n");
            helpers::rebootToPayload();
            AMS_INFINITE_LOOP(); // Wait for shutdown to be effective
        } else {
            logToFile("[Screen] Error: returned from WaitForPowerButton() without pressing button\n");
        }

        return 0;
    }

    /*::Result GuiController::ShowRemainingTime() {
        logToFile("[Screen] Show the remaining time\n");   
        auto settings = loadSettings();

        if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
            if(settings[SETTING_SHOW_REMAINING_TIME].int_value == 0) {
                logToFile("[Screen] display mode is blocking.\n");
                return 0;
            }
        }     

        // Pre-render the framebuffer. 
        this->PreRenderFrameBuffer();

        // Prepare screen for drawing. 
        ::Result res = this->PreRenderContents();
        if(res != 0) {
            logToFile("[Screen] PreRenderContents failed\n");
            return res;
        }

        // Display the pre-rendered frame. 
        this->DisplayPreRenderedFrame();

        return 0;
    }

    ::Result GuiController::UpdateRemainingTime(u8 remaining_time_in_minutes) {
        logToFile("[Screen] Update remaining time\n");
        auto settings = loadSettings();

        if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
            if(settings[SETTING_SHOW_REMAINING_TIME].int_value == 0) {
                logToFile("[Screen] display mode is blocking.\n");
                return 0;
            }
        }

        this->m_remaining_time_in_minutes = remaining_time_in_minutes;

        this->DisplayPreRenderedFrame();

        return 0;
    }

    ::Result GuiController::ShowScreenWarning() {
        logToFile("[Screen] Show warning screen\n");

        auto settings = loadSettings();

        if(settings.contains(SETTING_SHOW_REMAINING_TIME)) {
            if(settings[SETTING_SHOW_REMAINING_TIME].int_value > 0) {
                logToFile("[Screen] display mode is blocking.\n");
                return 0;
            }
        }

        // Todo

        return 0;
    }*/

    void GuiController::HideScreen() {
        logToFile("[Screen] Hide any screen\n");
        nwindowClose(std::addressof(m_win));
        viSetContentVisibility(true);
        //viCloseLayer(&m_layer);
        //viSetLayerZ(&m_layer, 0);
    }

    bool GuiController::WaitForVolumeButton() {
        gpio::Initialize();
        gpio::GpioPadSession vol_btn;
        if(gpio::OpenSession(std::addressof(vol_btn), gpio::DeviceCode_ButtonVolUp).IsFailure()) {
            logToFile("[Screen] Failed to open session with GPIO\n");
        }

        ON_SCOPE_EXIT { gpio::CloseSession(std::addressof(vol_btn)); };

        gpio::SetDirection(std::addressof(vol_btn), gpio::Direction_Input);

        while(true) {
            if(gpio::GetValue(std::addressof(vol_btn)) == gpio::GpioValue_Low) {
                logToFile("[Screen] Volume button has been pressed\n");
                return true;
            }

            os::SleepThread(TimeSpan::FromMilliSeconds(100));
        }

        return false;
    }

}
