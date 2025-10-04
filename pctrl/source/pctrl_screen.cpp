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

namespace util = ams::util;
namespace os = ams::os;
namespace svc = ams::svc;
namespace font = alefbet::pctrl::font;
namespace dd = ams::dd;
//namespace aarch64 = ams::svc::aarch64;
namespace hos = ams::hos;
using namespace ams;
using namespace ams::literals;      
using namespace alefbet::pctrl::logger;

/* There should only be a single transfer memory (for nv). */
alignas(os::MemoryPageSize) constinit u8 g_nv_transfer_memory[0x40000];

extern "C" ::Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
    *out_size = sizeof(g_nv_transfer_memory);
    return tmemCreateFromMemory(t, g_nv_transfer_memory, sizeof(g_nv_transfer_memory), perm);
}

namespace alefbet::pctrl::srv {
    /* Include Atmosphere logo into its own anonymous namespace. */
    /*namespace {
        #include "fatal_ams_logo.inc"
    }*/          

    namespace {

        /* Screen definitions. */
        constexpr u32 PctrlScreenWidth = 1280;
        constexpr u32 PctrlScreenHeight = 720;
        constexpr u32 PctrlScreenBpp = 2;
        constexpr u32 PctrlLayerZ = 100;
        constexpr u16 BackgroundColor = RGB888_TO_RGB565(0x7, 0x7, 0x7);

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

    void GuiController::setHeapPointer(u8* heap_pointer) {
        logToFile("[Screen] Setting heap pointer to @%p\n", heap_pointer_);
        heap_pointer_ = heap_pointer;
    }

    void GuiController::InitializeFrameBufferPointer() {
        // Try to get a framebuffer from heap.
        /*{
            logToFile("[Screen] sboub 1\n");
            if (R_SUCCEEDED(os::SetMemoryHeapSize(FrameBufferRequiredSizeHeapAligned))) {                        
                g_framebuffer_pointer = reinterpret_cast<u8 *>(os::GetMemoryHeapAddress());
                logToFile("[Screen] g_framebuffer_pointer initialized on heap @%p\n", g_framebuffer_pointer);
                return;            
            }

            logToFile("[Screen] SetMemoryHeapSize failed\n");
        }*/

        if(heap_pointer_ != nullptr) {
            g_framebuffer_pointer = heap_pointer_;
            logToFile("[Screen] g_framebuffer_pointer points to heap_pointer\n");
            return;
        }

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
        ViDisplay temp_display;

        /* Try to open the display. */
        ::Result res = viOpenDisplay("Internal", std::addressof(temp_display));
        if(res != 0) {
            // Do not overreact
            logToFile("Could not open internal display\n");
            // but fail...
            return res;
        }

        /* Guarantee we close the display. */
        ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

        /* Turn on the screen. */
        if (hos::GetVersion() >= hos::Version_3_0_0) {
            viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On);
            // Best effort
            logToFile("Turn the screen on the regular way\n");
        } else {
            /* Prior to 3.0.0, the ViPowerState enum was different (0 = Off, 1 = On). */
            viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On_Deprecated);
            // Best effort
            logToFile("Turn the screen on the old way\n");
        }

        /* Set alpha to 1.0f. */
        res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
        if(res != 0) {
            logToFile("Could not set internal display alpha\n");
        }

        return 0;
    }

    ::Result GuiController::SetupDisplayExternal() {
        ViDisplay temp_display;
        /* Try to open the display. */
        ::Result res = viOpenDisplay("External", std::addressof(temp_display));
        if(res != 0) {
            // Do not overreact
            logToFile("Could not open external display\n");
            // but fail...
            return res;
        }

        /* Guarantee we close the display. */
        ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

        /* Set alpha to 1.0f. */
        res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
        if(res != 0) {
            logToFile("Could not set external display alpha\n");
        }

        return 0;
    }

    ::Result GuiController::PrepareScreenForDrawing() {    
        /* Connect to vi. */
        ::Result res = viInitialize(ViServiceType_Manager);
        if(res != 0) {
            logToFile("Could not initialise VI service\n");
            return res; //Fail
        }

        /* Close other content. */
        viSetContentVisibility(false);

        /* Setup the two displays. */
        // Ignore errors (they are logged)
        SetupDisplayInternal();
        SetupDisplayExternal();

        /* Open the default display. */
        TRY_AND_RETURN(
            viOpenDefaultDisplay(std::addressof(m_display)), 
            "Could not open default display. Aborting.\n"
        )

        /* Reset the display magnification to its default value. */
        s32 display_width, display_height;
        TRY_AND_RETURN(
            viGetDisplayLogicalResolution(std::addressof(m_display), std::addressof(display_width), std::addressof(display_height)),
            "Could not get display logical resolution.\n"
        )

        /* viSetDisplayMagnification was added in 3.0.0. */
        if (hos::GetVersion() >= hos::Version_3_0_0) {
            TRY_AND_RETURN(
                viSetDisplayMagnification(std::addressof(m_display), 0, 0, display_width, display_height),
                "Could not reset display magnification.\n"
            )
        }

        /* Create layer to draw to. */
        TRY_AND_RETURN(
            viCreateLayer(std::addressof(m_display), std::addressof(m_layer)),
            "Could not create layer.\n"
        )

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
                "Could not set layer size.\n"
            )

            /* Set the layer's Z at display maximum, to be above everything else .*/
            TRY_AND_RETURN(
                viSetLayerZ(std::addressof(m_layer), PctrlLayerZ),
                "Could not set layer z position.\n"
            )

            /* Center the layer in the screen. */
            TRY_AND_RETURN(
                viSetLayerPosition(std::addressof(m_layer), layer_x, layer_y),
                "Could not set layer position.\n"
            )

            /* Create framebuffer. */
            TRY_AND_RETURN(
                nwindowCreateFromLayer(std::addressof(m_win), std::addressof(m_layer)),
                "Could not create nwindow from layer.\n"
            )

            TRY_AND_RETURN(
                InitializeNativeWindow(),
                "Could not initialize native window.\n"
            )
        }

        return 0;
    }

    void GuiController::PreRenderFrameBuffer() {            
        /* Allocate a frame buffer. */
        InitializeFrameBufferPointer();
        if(g_framebuffer_pointer == nullptr) {
            logToFile("[Screen] The framebuffer pointer is null. Aborting.\n");
            return;
        }

        /* Pre-render the image into the static framebuffer. */
        u16 *tiled_buf = reinterpret_cast<u16 *>(g_framebuffer_pointer);

        /* Temporarily use the NV transfer memory as font backing heap. */
        font::SetHeapMemory(g_nv_transfer_memory, sizeof(g_nv_transfer_memory));
        ON_SCOPE_EXIT { std::memset(g_nv_transfer_memory, 0, sizeof(g_nv_transfer_memory)); };

        /* Let the font manager know about our framebuffer. */
        font::ConfigureFontFramebuffer(tiled_buf, GetPixelOffset);
        font::SetFontColor(0xFFFF);

        /* Draw a background. */
        for (size_t i = 0; i < FrameBufferRequiredSizeBytes / sizeof(*tiled_buf); i++) {
            tiled_buf[i] = BackgroundColor;
        }
        const u32 start_x = 32, start_y = 64;
        /* Draw the atmosphere logo in the upper right corner. */            
        /*for (size_t y = 0; y < AtmosphereLogoHeight; y++) {
            for (size_t x = 0; x < AtmosphereLogoWidth; x++) {
                tiled_buf[GetPixelOffset(PctrlScreenWidth - AtmosphereLogoWidth - start_x + x, start_x + y)] = AtmosphereLogoData[y * AtmosphereLogoWidth + x];
            }
        }*/

        /* Draw error message and firmware. */
        font::SetPosition(start_x, start_y);
        font::SetFontSize(16.0f);
        font::PrintFormat("PrintFormat");
        font::AddSpacingLines(1.0f);
        font::PrintFormatLine("PrintFormatLine");
        font::AddSpacingLines(0.5f);            
    }

    ::Result GuiController::InitializeNativeWindow() {
        //* Setup nv driver. */
        TRY_AND_RETURN(
            nvInitialize(),
            "Could not initialize NV service\n"
        )

        TRY_AND_RETURN(
            nvMapInit(),
            "Could not init Map\n"
        )

        TRY_AND_RETURN(
            nvFenceInit(),
            "Could not init fence\n"
        )

        /* Create nvmap. */
        TRY_AND_RETURN(
            nvMapCreate(std::addressof(m_map), g_framebuffer_pointer, FrameBufferRequiredSizeBytes, 0x20000, NvKind_Pitch, true),
            "Could not create Map\n"
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
                "Could not configure nwindow buffer\n"
            )
        }

        return 0;        
    }

    void GuiController::DisplayPreRenderedFrame() {
        s32 slot;

        logToFile("[Screen] Before nwindowDequeueBuffer\n");
        ::Result res = nwindowDequeueBuffer(std::addressof(m_win), std::addressof(slot), nullptr);
        if(res != 0) {
            logToFile("Could not dequeue buffer for nwindow\n");
            return;
        }

        logToFile("[Screen] Before dd:FlushDataCache\n");
        dd::FlushDataCache(g_framebuffer_pointer, FrameBufferRequiredSizeBytes);
        logToFile("[Screen] After dd::FlushDataCache\n");
        R_ABORT_UNLESS(nwindowQueueBuffer(std::addressof(m_win), m_win.cur_slot, NULL));
    }

    ::Result GuiController::ShowScreenTimeout() {
        logToFile("[Screen] Show timeout screen\n");

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
        
        return 0;
    }

    void GuiController::HideScreen() {
        logToFile("[Screen] Hide any screen\n");
        nwindowClose(std::addressof(m_win));
        //viCloseLayer(&m_layer);
        //viSetLayerZ(&m_layer, 0);
    }


}
