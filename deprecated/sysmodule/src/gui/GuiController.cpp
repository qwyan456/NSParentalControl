#include "GuiController.h"
#include <switch.h>
#include "logger.h"
#include "font.h"

#define TRY_AND_RETURN(fn, message_if_failed) \
    { \
        const auto res = (fn); \
        if(R_FAILED(res)) { \
            logToFile(message_if_failed); \
            return res; \
        }; \
    }

constexpr u32 AlignUp(u32 value, size_t alignment) {
    const u32 invmask = static_cast<u32>(alignment - 1);
    return (value + invmask) & ~invmask;
}

constexpr inline bool IsAligned(u32 value, size_t alignment) {
    const u32 invmask = static_cast<u32>(alignment - 1);
    return (value & invmask) == 0;
}

/** From literals.hpp */
constexpr inline u64 operator ""_KB(unsigned long long n) {
    return static_cast<u64>(n) * UINT64_C(1024);
}

constexpr inline u64 operator ""_MB(unsigned long long n) {
    return operator ""_KB(n) * UINT64_C(1024);
}

constexpr inline u64 operator ""_GB(unsigned long long n) {
    return operator ""_MB(n) * UINT64_C(1024);
}

namespace {
    /* Screen definitions. */
    constexpr u32 FatalScreenWidth = 1280;
    constexpr u32 FatalScreenHeight = 720;
    constexpr u32 FatalScreenBpp = 2;
    constexpr u32 FatalLayerZ = 100;

    constexpr u16 BackgroundColor = 0xcccc;

    constexpr u32 FatalScreenWidthAlignedBytes = AlignUp(FatalScreenWidth * FatalScreenBpp, 64);
    constexpr u32 FatalScreenWidthAligned = FatalScreenWidthAlignedBytes / FatalScreenBpp;

    constexpr inline size_t MemoryPageSize = 4_KB;
    constexpr inline size_t MemoryHeapUnitSize  = 2_MB;

    /* There should only be a single transfer memory (for nv). */
    alignas(MemoryPageSize) constinit u8 g_nv_transfer_memory[0x40000];

    /* There should only be a single (1280*768) framebuffer. */
    constexpr size_t FrameBufferRequiredSizeBytes       = FatalScreenWidthAlignedBytes * AlignUp(FatalScreenHeight, 128);
    constexpr size_t FrameBufferRequiredSizePageAligned = AlignUp(FrameBufferRequiredSizeBytes, MemoryPageSize);
    constexpr size_t FrameBufferRequiredSizeHeapAligned = AlignUp(FrameBufferRequiredSizeBytes, MemoryHeapUnitSize);

    constinit u8 *g_framebuffer_pointer = nullptr;

    ViDisplay m_display;
    ViLayer m_layer;
    NWindow m_win;
    NvMap m_map;
}

inline MemoryHeapManager &GetMemoryHeapManager() {
    return GetResourceManager().GetMemoryHeapManager();
}

uintptr_t GetMemoryHeapAddress() {
    return GetMemoryHeapManager().GetHeapAddress();
}

Result SetMemoryHeapSize(size_t size) {
    /* Check pre-conditions. */
    if(!IsAligned(size, MemoryHeapUnitSize)) {
        return -1;
    }

    /* Set the heap size. */
    return GetMemoryHeapManager().SetHeapSize(size);
}

void InitializeFrameBufferPointer() {
    /* Try to get a framebuffer from heap. */
    {
        if (R_SUCCEEDED(SetMemoryHeapSize(FrameBufferRequiredSizeHeapAligned))) {
            g_framebuffer_pointer = reinterpret_cast<u8 *>(GetMemoryHeapAddress());
            return;
        }
    }

    /* We couldn't use heap, so try insecure memory, from the system nonsecure pool. */
    {
        uintptr_t address = 0;
        if (R_SUCCEEDED(os::AllocateInsecureMemory(std::addressof(address), FrameBufferRequiredSizePageAligned))) {
            g_framebuffer_pointer = reinterpret_cast<u8 *>(address);
            return;
        }
    }

    /* Neither heap nor insecure is available, so we're going to have to try to raid the unsafe pool. */
    {
        /* First, increase the limit to an extremely high value. */
        size_t large_size = std::max(128_MB, FrameBufferRequiredSizeHeapAligned);
        while (svc::ResultLimitReached::Includes(svc::SetUnsafeLimit(large_size))) {
            large_size *= 2;
        }

        /* Next, map some unsafe memory. */
        uintptr_t address = 0;
        if (R_SUCCEEDED(os::AllocateUnsafeMemory(std::addressof(address), FrameBufferRequiredSizePageAligned))) {
            g_framebuffer_pointer = reinterpret_cast<u8 *>(address);
            return;
        }
    }
}

extern "C" ::Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
    *out_size = sizeof(g_nv_transfer_memory);
    return tmemCreateFromMemory(t, g_nv_transfer_memory, sizeof(g_nv_transfer_memory), perm);
}

Result SetupDisplayInternal() {
    ViDisplay temp_display;

    /* Try to open the display. */
    Result res = viOpenDisplay("Internal", std::addressof(temp_display));
    if(R_FAILED(res)) {
        // Do not overreact
        logToFile("Could not open internal display");
        // but fail...
        return res;
    }

    /* Guarantee we close the display. */
    ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

    /* Turn on the screen. */
    if (hos::GetVersion() >= hos::Version_3_0_0) {
        viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On);
        // Best effort
        logToFile("Could not turn the internal screen on");
    } else {
        /* Prior to 3.0.0, the ViPowerState enum was different (0 = Off, 1 = On). */
        viSetDisplayPowerState(std::addressof(temp_display), ViPowerState_On_Deprecated);
        // Best effort
        logToFile("Could not turn the internal screen on");
    }

    /* Set alpha to 1.0f. */
    res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
    if(R_FAILED(res)) {
        logToFile("Could not set internal display alpha");
    }

    return 0;
}

Result SetupDisplayExternal() {
    ViDisplay temp_display;
    /* Try to open the display. */
    Result res = viOpenDisplay("External", std::addressof(temp_display));
    if(R_FAILED(res)) {
        // Do not overreact
        logToFile("Could not open external display");
        // but fail...
        return res;
    }

    /* Guarantee we close the display. */
    ON_SCOPE_EXIT { viCloseDisplay(std::addressof(temp_display)); };

    /* Set alpha to 1.0f. */
    res = viSetDisplayAlpha(std::addressof(temp_display), 1.0f);
    if(R_FAILED(res)) {
        logToFile("Could not set external display alpha");
    }

    return 0;
}

Result InitializeNativeWindow() {
    /* Setup nv driver. */
    TRY_AND_RETURN(
        nvInitialize(),
        "Could not initialize NV service"
    )

    TRY_AND_RETURN(
        nvMapInit(),
        "Could not init Map"
    )

    TRY_AND_RETURN(
        nvFenceInit(),
        "Could not init fence"
    )

    /* Create nvmap. */
    TRY_AND_RETURN(
        nvMapCreate(std::addressof(m_map), g_framebuffer_pointer, FrameBufferRequiredSizeBytes, 0x20000, NvKind_Pitch, true),
        "Could not create Map"
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
        grbuf.planes[0].width               = FatalScreenWidth;
        grbuf.planes[0].height              = FatalScreenHeight;
        grbuf.planes[0].color_format        = NvColorFormat_R5G6B5;
        grbuf.planes[0].layout              = NvLayout_BlockLinear;
        grbuf.planes[0].kind                = NvKind_Generic_16BX2;
        grbuf.planes[0].block_height_log2   = 4;
        grbuf.nvmap_id                      = nvMapGetId(std::addressof(m_map));
        grbuf.stride                        = FatalScreenWidthAligned;
        grbuf.total_size                    = FrameBufferRequiredSizeBytes;
        grbuf.planes[0].pitch               = FatalScreenWidthAlignedBytes;
        grbuf.planes[0].size                = FrameBufferRequiredSizeBytes;
        grbuf.planes[0].offset              = 0;

        TRY_AND_RETURN(
            nwindowConfigureBuffer(std::addressof(m_win), 0, std::addressof(grbuf)),
            "Could not configure nwindow buffer"
        )
    }

    return 0;
}

Result PrepareScreenForDrawing() {
    /* Connect to vi. */
    Result res = viInitialize(ViServiceType_Manager);
    if(R_FAILED(res)) {
        logToFile("Could not initialise VI service");
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
        "Could not open default display. Aborting"
    )

    /* Reset the display magnification to its default value. */
    s32 display_width, display_height;    
    TRY_AND_RETURN(
        viGetDisplayLogicalResolution(std::addressof(m_display), std::addressof(display_width), std::addressof(display_height)),
        "Could not get display logical resolution"
    )

    /* viSetDisplayMagnification was added in 3.0.0. */
    if (hos::GetVersion() >= hos::Version_3_0_0) {
        TRY_AND_RETURN(
            viSetDisplayMagnification(std::addressof(m_display), 0, 0, display_width, display_height),
            "Could not reset display magnification"
        )
    }

    /* Create layer to draw to. */
    TRY_AND_RETURN(
        viCreateLayer(std::addressof(m_display), std::addressof(m_layer)),
        "Could not create layer"
    )

    /* Setup the layer. */
    {
        /* Display a layer of 1280 x 720 at 1.5x magnification */
        /* NOTE: N uses 2 (770x400) RGBA4444 buffers (tiled buffer + linear). */
        /* We use a single 1280x720 tiled RGB565 buffer. */
        constexpr s32 RawWidth = FatalScreenWidth;
        constexpr s32 RawHeight = FatalScreenHeight;
        constexpr s32 LayerWidth = ((RawWidth) * 3) / 2;
        constexpr s32 LayerHeight = ((RawHeight) * 3) / 2;

        const float layer_x = static_cast<float>((display_width - LayerWidth) / 2);
        const float layer_y = static_cast<float>((display_height - LayerHeight) / 2);

        TRY_AND_RETURN(
            viSetLayerSize(std::addressof(m_layer), LayerWidth, LayerHeight),
            "Could not set layer size"
        )

        /* Set the layer's Z at display maximum, to be above everything else .*/
        TRY_AND_RETURN(
            viSetLayerZ(std::addressof(m_layer), FatalLayerZ),
            "Could not set layer z position"
        )

        /* Center the layer in the screen. */
        TRY_AND_RETURN(
            viSetLayerPosition(std::addressof(m_layer), layer_x, layer_y),
            "Could not set layer position"
        )

        /* Create framebuffer. */
        TRY_AND_RETURN(
            nwindowCreateFromLayer(std::addressof(m_win), std::addressof(m_layer)),
            "Could not create nwindow from layer"
        )

        TRY_AND_RETURN(
            InitializeNativeWindow(),
            "Could not initialize native window"
        )
    }

    return 0;
}

/* Pixel calculation helper. */
constexpr u32 GetPixelOffset(u32 x, u32 y) {
    u32 tmp_pos = ((y & 127) / 16) + (x/32*8) + ((y/16/8)*(((FatalScreenWidthAligned/2)/16*8)));
    tmp_pos *= 16*16 * 4;

    tmp_pos += ((y%16)/8)*512 + ((x%32)/16)*256 + ((y%8)/2)*64 + ((x%16)/8)*32 + (y%2)*16 + (x%8)*2;//This line is a modified version of code from the Tegra X1 datasheet.

    return tmp_pos / 2;
}

bool PreRenderFrameBuffer() {
    //const FatalConfig &config = GetFatalConfig();

    /* Allocate a frame buffer. */
    InitializeFrameBufferPointer();
    if(g_framebuffer_pointer == nullptr) {
        logToFile("The framebuffer memory could not be set");
        return false;
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
            tiled_buf[GetPixelOffset(FatalScreenWidth - AtmosphereLogoWidth - start_x + x, start_x + y)] = AtmosphereLogoData[y * AtmosphereLogoWidth + x];
        }
    }*/

    /* Draw error message and firmware. */
    font::SetPosition(start_x, start_y);
    font::SetFontSize(16.0f);
    font::Print("font::Print");    
    font::AddSpacingLines(0.5f);
    font::PrintLine("font::PrintLine");
    font::AddSpacingLines(0.5f);    
}

void DisplayPreRenderedFrame() {
    s32 slot;

    Result res = nwindowDequeueBuffer(std::addressof(m_win), std::addressof(slot), nullptr);
    if(R_FAILED(res)) {
        logToFile("Could not dequeue buffer for nwindow");
        return;
    }

    dd::FlushDataCache(g_framebuffer_pointer, FrameBufferRequiredSizeBytes);
    R_ABORT_UNLESS(nwindowQueueBuffer(std::addressof(m_win), m_win.cur_slot, NULL));
}

void showTimeoutAlert() {
    /* Pre-render the framebuffer. */
    PreRenderFrameBuffer();

    /* Prepare screen for drawing. */
    if(R_FAILED(PrepareScreenForDrawing())) {
        logToFile("Pre-render failed. Aborting.");
        return;
    }

    /* Display the pre-rendered frame. */
    DisplayPreRenderedFrame();
}
