#pragma once
#include <switch.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <algorithm>
#include <switch/types.h>
#include <unordered_map>
#include <cwctype>
#include "utils.h"
#include "logger.h"

#define PACKED __attribute__((packed))

using namespace alefbet::pctrl::logger;

constexpr u32 ScreenWidth = 1920;       ///< Width of the Screen
constexpr u32 ScreenHeight = 1080;      ///< Height of the Screen

extern "C" {    
    alignas(ams::os::MemoryPageSize) constinit u8 g_nv_transfer_memory[0x40000];
    extern "C" ::Result __nx_nv_create_tmem(TransferMemory *t, u32 *out_size, Permission perm) {
        *out_size = sizeof(g_nv_transfer_memory);
        return tmemCreateFromMemory(t, g_nv_transfer_memory, sizeof(g_nv_transfer_memory), perm);
    }
}

namespace alefbet {
    namespace pctrl {
        namespace gfx {
            extern "C" u64 __nx_vi_layer_id;
            //u64 _vi_layer_id = 0;

            struct Color {
                union {
                    struct {
                        u16 r: 4, g: 4, b: 4, a: 4;
                    } PACKED;
                    u16 rgba;
                };

                constexpr inline Color(u16 raw): rgba(raw) {}
                constexpr inline Color(u8 r, u8 g, u8 b, u8 a): r(r), g(g), b(b), a(a) {}
            };

            static const NvColorFormat g_nvColorFmtTable[] = {
                NvColorFormat_A8B8G8R8, // PIXEL_FORMAT_RGBA_8888
                NvColorFormat_X8B8G8R8, // PIXEL_FORMAT_RGBX_8888
                NvColorFormat_R8_G8_B8, // PIXEL_FORMAT_RGB_888   <-- doesn't work
                NvColorFormat_R5G6B5,   // PIXEL_FORMAT_RGB_565
                NvColorFormat_A8R8G8B8, // PIXEL_FORMAT_BGRA_8888
                NvColorFormat_R5G5B5A1, // PIXEL_FORMAT_RGBA_5551 <-- doesn't work
                NvColorFormat_A4B4G4R4, // PIXEL_FORMAT_RGBA_4444
            };

            /*void* __attribute__((weak)) __libnx_aligned_alloc(size_t alignment, size_t size) {
                size = (size + alignment - 1) &~ (alignment - 1);
                return aligned_alloc(alignment, size);
            }*/

            class Renderer {
                public:
                    static Renderer& get() {
                        static Renderer renderer;

                        return renderer;
                    }

                    bool isInitialized() {
                        return m_initialized;
                    }

                    void init(u16 width, u16 height, u16 posX, u16 posY) {
                        LayerPosX = posX;
                        LayerPosY = posY;
                        FramebufferWidth  = width;
                        FramebufferHeight = height;
                        LayerWidth  = FramebufferWidth;
                        LayerHeight = FramebufferHeight;
                        
                        logToFile("[Renderer] LayerWidth=%i, LayerHeight=%i, LayerPosX=%i, LayerPosY=%i, FramebufferWidth=%i, FramebufferHeight=%i\n", LayerWidth, LayerHeight, LayerPosX, LayerPosY, FramebufferWidth, FramebufferHeight);

                        if (this->m_initialized)
                            return;

                        //generateAruid();

                        Result rc = smInitialize();
                        if(R_FAILED(rc)) {                         
                            return;
                        }                        

                        rc = viInitialize(ViServiceType_Manager);
                        logToFile("viInitialize %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        rc = viOpenDefaultDisplay(&this->m_display);
                        logToFile("viOpenDefaultDisplay %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        rc = viGetDisplayVsyncEvent(&this->m_display, &this->m_vsyncEvent);
                        logToFile("viGetDisplayVsyncEvent %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        //u64 aruid = appletGetAppletResourceUserId();
                        //logToFile("[Renderer] aruid=%i\n", aruid);
                        //rc = viCreateManagedLayer(&this->m_display, static_cast<ViLayerFlags>(0), 0, &__nx_vi_layer_id);
                        rc = viCreateLayer(&this->m_display, &this->m_layer);
                        logToFile("__nx_vi_layer_id=%i\n", __nx_vi_layer_id);
                        logToFile("viCreateLayer %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        //rc = viSetLayerScalingMode(&this->m_layer, ViScalingMode_FitToLayer);
                        rc = viSetLayerZ(&this->m_layer, 99);
                        logToFile("viSetLayerZ %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }                        
                        
                        //rc = viAddToLayerStack(&this->m_layer, static_cast<ViLayerStack>(8));
                        //logToFile("viAddToLayerStack %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        
                        rc = viSetLayerSize(&this->m_layer, LayerWidth, LayerHeight);
                        logToFile("viSetLayerSize %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }                        

                        rc = viSetLayerPosition(&this->m_layer, LayerPosX, LayerPosY);
                        logToFile("viSetLayerPosition %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        rc = nwindowCreateFromLayer(&this->m_window, &this->m_layer);
                        logToFile("nwindowCreateFromLayer %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }
                        
                        //rc = framebufferCreate(&this->m_framebuffer, &this->m_window, FramebufferWidth, FramebufferHeight, PIXEL_FORMAT_RGBA_4444, 2);                        
                        if(!initializeWindow(LayerWidth, LayerHeight, 2)) {
                            smExit();
                            return;
                        }
                        //logToFile("framebufferCreate %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        logToFile("@framebuffer.buf=%p\n", this->m_framebuffer.buf);
                        logToFile("@framebuffer.fb_size=%p\n", this->m_framebuffer.fb_size);
                        
                        rc = setInitialize();
                        if(R_FAILED(rc)) {
                            smExit();
                            return;
                        }

                        rc = initFonts();
                        if(R_FAILED(rc)) {
                            setExit();
                            smExit();
                            return;
                        }

                        setExit();
                        smExit();

                        this->m_initialized = true;
                    }        

                    /**
                     * @brief Handles opacity of drawn colors for fadeout. Pass all colors through this function in order to apply opacity properly
                     *
                     * @param c Original color
                     * @return Color with applied opacity
                     */
                    static Color a(const Color &c) {
                        return (c.rgba & 0x0FFF) | (static_cast<u8>(c.a * Renderer::s_opacity) << 12);
                    }
                    
                    /**
                     * @brief Draw a single pixel onto the screen
                     *
                     * @param x X pos
                     * @param y Y pos
                     * @param color Color
                     */
                    inline void setPixel(s32 x, s32 y, Color color) {
                        if (x < 0 || y < 0 || x >= FramebufferWidth || y >= FramebufferHeight)
                            return;

                        u32 offset = this->getPixelOffset(x, y);

                        if (offset != UINT32_MAX)
                            static_cast<Color*>(m_framebuffer.buf)[offset] = color;
                            //static_cast<Color*>(this->getCurrentFramebuffer())[offset] = color;
                    }

                    /**
                     * @brief Blends two colors
                     *
                     * @param src Source color
                     * @param dst Destination color
                     * @param alpha Opacity
                     * @return Blended color
                     */
                    inline u8 blendColor(u8 src, u8 dst, u8 alpha) {
                        u8 oneMinusAlpha = 0x0F - alpha;

                        return (dst * alpha + src * oneMinusAlpha) / float(0xF);
                    }

                    /**
                     * @brief Draws a single source blended pixel onto the screen
                     *
                     * @param x X pos
                     * @param y Y pos
                     * @param color Color
                     */
                    inline void setPixelBlendSrc(s32 x, s32 y, Color color) {
                        if (x < 0 || y < 0 || x >= FramebufferWidth || y >= FramebufferHeight)
                            return;

                        u32 offset = this->getPixelOffset(x, y);

                        if (offset == UINT32_MAX)
                            return;

                        //Color src((static_cast<u16*>(this->getCurrentFramebuffer()))[offset]);
                        Color src((static_cast<u16*>(m_framebuffer.buf))[offset]);
                        Color dst(color);
                        Color end(0);

                        end.r = this->blendColor(src.r, dst.r, dst.a);
                        end.g = this->blendColor(src.g, dst.g, dst.a);
                        end.b = this->blendColor(src.b, dst.b, dst.a);
                        end.a = src.a;

                        this->setPixel(x, y, end);
                    }

                    /**
                     * @brief Draws a single destination blended pixel onto the screen
                     *
                     * @param x X pos
                     * @param y Y pos
                     * @param color Color
                     */
                    inline void setPixelBlendDst(s32 x, s32 y, Color color) {
                        if (x < 0 || y < 0 || x >= FramebufferWidth || y >= FramebufferHeight)
                            return;

                        u32 offset = this->getPixelOffset(x, y);

                        if (offset == UINT32_MAX)
                            return;

                        //Color src((static_cast<u16*>(this->getCurrentFramebuffer()))[offset]);
                        Color src((static_cast<u16*>(m_framebuffer.buf))[offset]);
                        Color dst(color);
                        Color end(0);

                        end.r = this->blendColor(src.r, dst.r, dst.a);
                        end.g = this->blendColor(src.g, dst.g, dst.a);
                        end.b = this->blendColor(src.b, dst.b, dst.a);
                        end.a = std::min(dst.a + src.a, 0xF);

                        this->setPixel(x, y, end);
                    }

                    /**
                     * @brief Draws a rectangle of given sizes
                     *
                     * @param x X pos
                     * @param y Y pos
                     * @param w Width
                     * @param h Height
                     * @param color Color
                     */
                    inline void drawRect(s32 x, s32 y, s32 w, s32 h, Color color) {
                        for (s32 x1 = x; x1 < (x + w); x1++)
                            for (s32 y1 = y; y1 < (y + h); y1++)
                                this->setPixelBlendDst(x1, y1, color);
                    }

                    void drawCircle(s32 centerX, s32 centerY, u16 radius, bool filled, Color color) {
                        s32 x = radius;
                        s32 y = 0;
                        s32 radiusError = 0;
                        s32 xChange = 1 - (radius << 1);
                        s32 yChange = 0;

                        while (x >= y) {
                            if(filled) {
                                for (s32 i = centerX - x; i <= centerX + x; i++) {
                                    s32 y0 = centerY + y;
                                    s32 y1 = centerY - y;
                                    s32 x0 = i;

                                    this->setPixelBlendDst(x0, y0, color);
                                    this->setPixelBlendDst(x0, y1, color);
                                }

                                for (s32 i = centerX - y; i <= centerX + y; i++) {
                                    s32 y0 = centerY + x;
                                    s32 y1 = centerY - x;
                                    s32 x0 = i;

                                    this->setPixelBlendDst(x0, y0, color);
                                    this->setPixelBlendDst(x0, y1, color);
                                }

                                y++;
                                radiusError += yChange;
                                yChange += 2;
                                if (((radiusError << 1) + xChange) > 0) {
                                    x--;
                                    radiusError += xChange;
                                    xChange += 2;
                                }
                            } else {
                                this->setPixelBlendDst(centerX + x, centerY + y, color);
                                this->setPixelBlendDst(centerX + y, centerY + x, color);
                                this->setPixelBlendDst(centerX - y, centerY + x, color);
                                this->setPixelBlendDst(centerX - x, centerY + y, color);
                                this->setPixelBlendDst(centerX - x, centerY - y, color);
                                this->setPixelBlendDst(centerX - y, centerY - x, color);
                                this->setPixelBlendDst(centerX + y, centerY - x, color);
                                this->setPixelBlendDst(centerX + x, centerY - y, color);

                                if(radiusError <= 0) {
                                    y++;
                                    radiusError += 2 * y + 1;
                                } else {
                                    x--;
                                    radiusError -= 2 * x + 1;
                                }
                            }
                        }
                    }

                    /**
                     * @brief Draws a RGBA8888 bitmap from memory
                     *
                     * @param x X start position
                     * @param y Y start position
                     * @param w Bitmap width
                     * @param h Bitmap height
                     * @param bmp Pointer to bitmap data
                     */
                    void drawBitmap(s32 x, s32 y, s32 w, s32 h, const u8 *bmp) {
                        for (s32 y1 = 0; y1 < h; y1++) {
                            for (s32 x1 = 0; x1 < w; x1++) {
                                const Color color = { static_cast<u8>(bmp[0] >> 4), static_cast<u8>(bmp[1] >> 4), static_cast<u8>(bmp[2] >> 4), static_cast<u8>(bmp[3] >> 4) };
                                //setPixelBlendSrc(x + x1, y + y1, a(color));
                                setPixel(x + x1, y + y1, a(color));
                                bmp += 4;
                            }
                        }
                    }

                    /**
                     * @brief Draws a string
                     *
                     * @param string String to draw
                     * @param monospace Draw string in monospace font
                     * @param x X pos
                     * @param y Y pos
                     * @param fontSize Height of the text drawn in pixels
                     * @param color Text color. Use transparent color to skip drawing and only get the string's dimensions
                     * @return Dimensions of drawn string
                     */
                    std::pair<u32, u32> drawString(const char* string, bool monospace, s32 x, s32 y, float fontSize, Color color, ssize_t maxWidth = 0) {
                        s32 maxX = x;
                        s32 currX = x;
                        s32 currY = y;

                        struct Glyph {
                            stbtt_fontinfo *currFont;
                            float currFontSize;
                            int bounds[4];
                            int xAdvance;
                            u8 *glyphBmp;
                            int width, height;
                        };

                        static std::unordered_map<u64, Glyph> s_glyphCache;

                        do {
                            if (maxWidth > 0 && maxWidth < (currX - x))
                                break;

                            u32 currCharacter;
                            ssize_t codepointWidth = decode_utf8(&currCharacter, reinterpret_cast<const u8*>(string));

                            if (codepointWidth <= 0)
                                break;

                            string += codepointWidth;

                            if (currCharacter == '\n') {
                                maxX = std::max(currX, maxX);

                                currX = x;
                                currY += fontSize;

                                continue;
                            }

                            u64 key = (static_cast<u64>(currCharacter) << 32) | static_cast<u64>(monospace) << 31 | static_cast<u64>(std::bit_cast<u32>(fontSize));

                            Glyph *glyph = nullptr;

                            auto it = s_glyphCache.find(key);
                            if (it == s_glyphCache.end()) {
                                /* Cache glyph */
                                glyph = &s_glyphCache.emplace(key, Glyph()).first->second;

                                if (stbtt_FindGlyphIndex(&this->m_extFont, currCharacter))
                                    glyph->currFont = &this->m_extFont;
                                else if(this->m_hasLocalFont && stbtt_FindGlyphIndex(&this->m_stdFont, currCharacter)==0)
                                    glyph->currFont = &this->m_localFont;
                                else
                                    glyph->currFont = &this->m_stdFont;

                                glyph->currFontSize = stbtt_ScaleForPixelHeight(glyph->currFont, fontSize);

                                stbtt_GetCodepointBitmapBoxSubpixel(glyph->currFont, currCharacter, glyph->currFontSize, glyph->currFontSize,
                                                                    0, 0, &glyph->bounds[0], &glyph->bounds[1], &glyph->bounds[2], &glyph->bounds[3]);

                                int yAdvance = 0;
                                stbtt_GetCodepointHMetrics(glyph->currFont, monospace ? 'W' : currCharacter, &glyph->xAdvance, &yAdvance);

                                glyph->glyphBmp = stbtt_GetCodepointBitmap(glyph->currFont, glyph->currFontSize, glyph->currFontSize, currCharacter, &glyph->width, &glyph->height, nullptr, nullptr);
                            } else {
                                /* Use cached glyph */
                                glyph = &it->second;
                            }

                            if (glyph->glyphBmp != nullptr && !std::iswspace(currCharacter) && fontSize > 0 && color.a != 0x0) {

                                auto x = currX + glyph->bounds[0];
                                auto y = currY + glyph->bounds[1];
                                for (s32 bmpY = 0; bmpY < glyph->height; bmpY++) {
                                    for (s32 bmpX = 0; bmpX < glyph->width; bmpX++) {
                                        auto bmpColor = glyph->glyphBmp[glyph->width * bmpY + bmpX] >> 4;
                                        if (bmpColor == 0xF) {
                                            this->setPixel(x + bmpX, y + bmpY, color);
                                        } else if (bmpColor != 0x0) {
                                            Color tmpColor = color;
                                            tmpColor.a = bmpColor * (float(tmpColor.a) / 0xF);
                                            this->setPixelBlendDst(x + bmpX, y + bmpY, tmpColor);
                                        }
                                    }
                                }

                            }

                            currX += static_cast<s32>(glyph->xAdvance * glyph->currFontSize);

                        } while (*string != '\0');

                        maxX = std::max(currX, maxX);

                        return { maxX - x, currY - y };
                    }

                    Result initFonts() {
                        static PlFontData stdFontData, localFontData, extFontData;

                        // Nintendo's default font
                        plGetSharedFontByType(&stdFontData, PlSharedFontType_Standard);

                        u8 *fontBuffer = reinterpret_cast<u8*>(stdFontData.address);
                        stbtt_InitFont(&this->m_stdFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

                        u64 languageCode;
                        if (R_SUCCEEDED(setGetSystemLanguage(&languageCode))) {
                            // Check if need localization font
                            SetLanguage setLanguage;
                            setMakeLanguage(languageCode, &setLanguage);
                            m_hasLocalFont = true;
                            switch (setLanguage) {
                            case SetLanguage_ZHCN:
                            case SetLanguage_ZHHANS:
                                plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseSimplified);
                                break;
                            case SetLanguage_KO:
                                plGetSharedFontByType(&localFontData, PlSharedFontType_KO);
                                break;
                            case SetLanguage_ZHTW:
                            case SetLanguage_ZHHANT:
                                plGetSharedFontByType(&localFontData, PlSharedFontType_ChineseTraditional);
                                break;
                            default:
                                this->m_hasLocalFont = false;
                                break;
                            }

                            if (this->m_hasLocalFont) {
                                fontBuffer = reinterpret_cast<u8*>(localFontData.address);
                                stbtt_InitFont(&this->m_localFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));
                            }
                        }

                        // Nintendo's extended font containing a bunch of icons
                        plGetSharedFontByType(&extFontData, PlSharedFontType_NintendoExt);

                        fontBuffer = reinterpret_cast<u8*>(extFontData.address);
                        stbtt_InitFont(&this->m_extFont, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0));

                        return 0;
                    }

                    inline void waitForVSync() {                        
                        eventWait(&this->m_vsyncEvent, UINT64_MAX);
                    }

                    inline void startFrame() {
                        //this->m_currentFramebuffer = framebufferBegin(&this->m_framebuffer, nullptr);                        
                    }

                    inline void fillScreen(Color color) {
                        logToFile("[Renderer] fillScreen\n");
                        //std::fill_n(static_cast<Color*>(this->getCurrentFramebuffer()), this->getFramebufferSize() / sizeof(Color), color);
                        std::fill_n(static_cast<Color*>(m_framebuffer.buf), m_framebuffer.fb_size / sizeof(Color), color);
                        logToFile("[Renderer] ended\n");
                    }

                    inline void clearScreen() {
                        logToFile("[Renderer] clearScreen\n");
                        this->fillScreen({ 0x00, 0x00, 0x00, 0x00 });
                        logToFile("[Renderer] ended\n");
                    }

                    inline void endFrame() {
                        logToFile("[Renderer] endFrame\n");

                        this->waitForVSync();
                        //framebufferEnd(&this->m_framebuffer);
                        
                        s32 slot = 0;
                        logToFile("[Renderer] dequeue buffer\n");
                        Result rc = nwindowDequeueBuffer(&m_window, &slot, nullptr);
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] nwindowDequeueBuffer() failed. %i:%i", R_DESCRIPTION(rc), R_MODULE(rc));
                            return;
                        }

                        // Copy data into framebuffer
                        logToFile("[Renderer] flush data cache\n");
                        armDCacheFlush(m_framebuffer.buf, m_framebuffer.fb_size);
                        /*if(R_FAILED(rc)) {
                            logToFile("[Renderer] svcFlushDataCache() failed. %i:%i", R_DESCRIPTION(rc), R_MODULE(rc));
                            return;
                        }*/

                        logToFile("[Renderer] queue buffer\n");
                        rc = nwindowQueueBuffer(&m_window, m_window.cur_slot, NULL);
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] nwindowDequeueBuffer() failed. %i:%i", R_DESCRIPTION(rc), R_MODULE(rc));
                            return;
                        }

                        logToFile("[Renderer] ended\n");
                        //this->m_currentFramebuffer = nullptr;
                    }

                    static void setOpacity(float opacity) {
                        opacity = std::clamp(opacity, 0.0F, 1.0F);

                        Renderer::s_opacity = opacity;
                    }

                    /**
                     * @brief Decodes a x and y coordinate into a offset into the swizzled framebuffer
                     *
                     * @param x X pos
                     * @param y Y Pos
                     * @return Offset
                     */
                    u32 getPixelOffset(s32 x, s32 y) {
                        /*if (!this->m_scissoringStack.empty()) {
                            auto currScissorConfig = this->m_scissoringStack.top();
                            if (x < currScissorConfig.x ||
                                y < currScissorConfig.y ||
                                x > currScissorConfig.x + currScissorConfig.w ||
                                y > currScissorConfig.y + currScissorConfig.h)
                                    return UINT32_MAX;
                        }*/

                        u32 tmpPos = ((y & 127) / 16) + (x / 32 * 8) + ((y / 16 / 8) * (((FramebufferWidth / 2) / 16 * 8)));
                        tmpPos *= 16 * 16 * 4;

                        tmpPos += ((y % 16) / 8) * 512 + ((x % 32) / 16) * 256 + ((y % 8) / 2) * 64 + ((x % 16) / 8) * 32 + (y % 2) * 16 + (x % 8) * 2;

                        return tmpPos / 2;
                    }

                    void exit() {
                        if (!this->m_initialized)
                            return;

                        logToFile("[Renderer] exit\n");
                        //framebufferClose(&this->m_framebuffer);
                        logToFile("[Renderer] fb closed\n");
                        nwindowClose(&this->m_window);
                        logToFile("[Renderer] window closed\n");
                        //viDestroyManagedLayer(&this->m_layer);
                        Result rc = viCloseLayer(&this->m_layer);
                        logToFile("[Renderer] layer closed %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        rc = viCloseDisplay(&this->m_display);
                        logToFile("[Renderer] display closed %i:%i\n", R_MODULE(rc), R_DESCRIPTION(rc));
                        eventClose(&this->m_vsyncEvent);
                        logToFile("[Renderer] event closed\n");
                        viExit();
                        logToFile("[Renderer] vi closed\n");
                        this->m_initialized = false;
                    }

                private:
                    static Result viAddToLayerStack(ViLayer *layer, ViLayerStack stack) {
                        const struct {
                            u32 stack;
                            u64 layerId;
                        } in = { stack, layer->layer_id };

                        return serviceDispatchIn(viGetSession_IManagerDisplayService(), 6000, in);
                    }

                    /*inline void* getCurrentFramebuffer() {
                        return this->m_currentFramebuffer;
                    }*/

                    /*inline void* getNextFramebuffer() {
                        return static_cast<u8*>(this->m_framebuffer.buf) + this->getNextFramebufferSlot() * this->getFramebufferSize();
                    }*/

                    /*inline size_t getFramebufferSize() {
                        return this->m_framebuffer.fb_size;
                    }*/

                    /*inline size_t getFramebufferCount() {
                        return this->m_framebuffer.num_fbs;
                    }*/

                    /*inline u8 getCurrentFramebufferSlot() {
                        return this->m_window.cur_slot;
                    }*/

                    /*inline u8 getNextFramebufferSlot() {
                        return (this->getCurrentFramebufferSlot() + 1) % this->getFramebufferCount();
                    }*/

                    bool initializeWindow(u16 width, u16 height, int num_fbs) {
                        Result rc = nvInitialize();
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] Could not initialize nv service : %i:%i.\n", R_DESCRIPTION(rc), R_MODULE(rc));
                            return false;
                        }

                        rc = nvMapInit();
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] nvMapInit() failed : %i:%i.\n", R_DESCRIPTION(rc), R_MODULE(rc));                            
                            nvExit();
                            return false;
                        }

                        rc = nvFenceInit();
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] nvFenceInit() failed : %i:%i.\n", R_DESCRIPTION(rc), R_MODULE(rc));
                            nvMapClose(&m_framebuffer.map);
                            nvExit();
                            return false;
                        }

                        //const NvColorFormat colorfmt = g_nvColorFmtTable[format-PIXEL_FORMAT_RGBA_8888];
                        //const u32 bytes_per_pixel = ((u64)colorfmt >> 3) & 0x1F;
                        const u32 bytes_per_pixel = 4;
                        const u32 width_aligned_bytes = (width*bytes_per_pixel + 63) &~ 63; // GOBs are 64 bytes wide
                        const u32 width_aligned = width_aligned_bytes / bytes_per_pixel;
                        const u32 block_height_log2 = 4; // According to TRM this is the optimal value (SIXTEEN_GOBS)
                        const u32 block_height = 8 * (1U << block_height_log2);
                        const u32 height_aligned = (height + block_height - 1) &~ (block_height - 1);
                        const u32 fb_size = width_aligned_bytes*height_aligned;
                        const u32 buf_size = (num_fbs*fb_size + 0xFFF) &~ 0xFFF; // needs to be page aligned

                        // Allocate framebuffer
                        m_framebuffer.buf = aligned_alloc(0x1000, buf_size);
                        if (!m_framebuffer.buf)
                            rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

                        rc = nvMapCreate(&m_framebuffer.map, m_framebuffer.buf, buf_size, 0x20000, NvKind_Pitch, true);
                        if(R_FAILED(rc)) {
                            logToFile("[Renderer] nvMapCreate() failed : %i:%i.\n", R_DESCRIPTION(rc), R_MODULE(rc));                            
                            nvMapClose(&m_framebuffer.map);
                            nvExit();
                            return false;
                        }

                        {
                            NvGraphicBuffer grbuf               = {};
                            grbuf.header.num_ints               = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
                            grbuf.unk0                          = -1;
                            grbuf.magic                         = 0xDAFFCAFF;
                            grbuf.pid                           = 42;
                            grbuf.usage                         = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
                            grbuf.format                        = PIXEL_FORMAT_RGBA_8888;
                            grbuf.ext_format                    = PIXEL_FORMAT_RGBA_8888;
                            grbuf.num_planes                    = 1;
                            grbuf.planes[0].width               = width;
                            grbuf.planes[0].height              = height;
                            grbuf.planes[0].color_format        = NvColorFormat_A8B8G8R8;
                            grbuf.planes[0].layout              = NvLayout_BlockLinear;
                            grbuf.planes[0].kind                = NvKind_Generic_16BX2;
                            grbuf.planes[0].block_height_log2   = block_height_log2;
                            grbuf.nvmap_id                      = nvMapGetId(std::addressof(m_framebuffer.map));
                            grbuf.stride                        = width_aligned;
                            grbuf.total_size                    = fb_size;
                            grbuf.planes[0].pitch               = width_aligned_bytes;
                            grbuf.planes[0].size                = fb_size;
                            grbuf.planes[0].offset              = 0;            

                            rc = nwindowConfigureBuffer(std::addressof(m_window), 0, std::addressof(grbuf));
                            if(R_FAILED(rc)) {
                                logToFile("[Renderer] nwindowConfigureBuffer() failed : %i:%i.\n", R_DESCRIPTION(rc), R_MODULE(rc));
                                nvMapClose(&m_framebuffer.map);
                                nvExit();
                                return false;
                            }
                        }

                        m_framebuffer.fb_size = fb_size;
                        m_framebuffer.win = &m_window;
                        m_framebuffer.has_init = true;
                        m_framebuffer.height_aligned = height_aligned;
                        m_framebuffer.num_fbs = num_fbs;
                        m_framebuffer.stride = width_aligned; 
                        
                        return true;
                    }

                    /*Result _framebufferCreate(Framebuffer* fb, NWindow *win, u32 width, u32 height, u32 format, u32 num_fbs)
                    {
                        Result rc = 0;
                        if (!fb || !nwindowIsValid(win) || !width || !height || format < PIXEL_FORMAT_RGBA_8888 || format > PIXEL_FORMAT_RGBA_4444 || num_fbs < 1 || num_fbs > 3)
                            return MAKERESULT(Module_Libnx, LibnxError_BadInput);

                        rc = nwindowSetDimensions(win, width, height);
                        if (R_FAILED(rc))
                            return rc;

                        rc = nvInitialize();
                        if (R_SUCCEEDED(rc)) {
                            rc = nvMapInit();
                            if (R_SUCCEEDED(rc)) {
                                rc = nvFenceInit();
                                if (R_FAILED(rc)) {
                                    nvMapExit();
                                }
                            }
                            if (R_FAILED(rc)) {
                                nvExit();
                            }
                        }

                        if (R_FAILED(rc))
                            return rc;

                        memset(fb, 0, sizeof(*fb));
                        fb->has_init = true;
                        fb->win = win;
                        fb->num_fbs = num_fbs;

                        const NvColorFormat colorfmt = g_nvColorFmtTable[format-PIXEL_FORMAT_RGBA_8888];
                        const u32 bytes_per_pixel = ((u64)colorfmt >> 3) & 0x1F;
                        const u32 block_height_log2 = 4; // According to TRM this is the optimal value (SIXTEEN_GOBS)
                        const u32 block_height = 8 * (1U << block_height_log2);

                        NvGraphicBuffer grbuf = {0};
                        grbuf.header.num_ints = (sizeof(NvGraphicBuffer) - sizeof(NativeHandle)) / 4;
                        grbuf.unk0 = -1;
                        grbuf.magic = 0xDAFFCAFF;
                        grbuf.pid = 42;
                        grbuf.usage = GRALLOC_USAGE_HW_COMPOSER | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_HW_TEXTURE;
                        grbuf.format = format;
                        grbuf.ext_format = format;
                        grbuf.num_planes = 1;
                        grbuf.planes[0].width = width;
                        grbuf.planes[0].height = height;
                        grbuf.planes[0].color_format = colorfmt;
                        grbuf.planes[0].layout = NvLayout_BlockLinear;
                        grbuf.planes[0].kind = NvKind_Generic_16BX2;
                        grbuf.planes[0].block_height_log2 = block_height_log2;

                        // Calculate buffer dimensions and sizes
                        const u32 width_aligned_bytes = (width*bytes_per_pixel + 63) &~ 63; // GOBs are 64 bytes wide
                        const u32 width_aligned = width_aligned_bytes / bytes_per_pixel;
                        const u32 height_aligned = (height + block_height - 1) &~ (block_height - 1);
                        const u32 fb_size = width_aligned_bytes*height_aligned;
                        const u32 buf_size = (num_fbs*fb_size + 0xFFF) &~ 0xFFF; // needs to be page aligned

                        fb->buf = __libnx_aligned_alloc(0x1000, buf_size);
                        if (!fb->buf)
                            rc = MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);

                        if (R_SUCCEEDED(rc))
                            rc = nvMapCreate(&fb->map, fb->buf, buf_size, 0x20000, NvKind_Pitch, true);

                        if (R_SUCCEEDED(rc)) {
                            grbuf.nvmap_id = nvMapGetId(&fb->map);
                            grbuf.stride = width_aligned;
                            grbuf.total_size = fb_size;
                            grbuf.planes[0].pitch = width_aligned_bytes;
                            grbuf.planes[0].size = fb_size;

                            for (u32 i = 0; i < num_fbs; i ++) {
                                grbuf.planes[0].offset = i*fb_size;
                                rc = nwindowConfigureBuffer(win, i, &grbuf);
                                if (R_FAILED(rc))
                                    break;
                            }
                        }

                        if (R_SUCCEEDED(rc)) {
                            fb->stride = width_aligned_bytes;
                            fb->width_aligned = width_aligned;
                            fb->height_aligned = height_aligned;
                            fb->fb_size = fb_size;
                        }

                        if (R_FAILED(rc)) {
                            framebufferClose(fb);
                        }

                        return rc;
                    }*/

                    /*void framebufferClose(Framebuffer* fb)
                    {
                        if (!fb || !fb->has_init)
                            return;

                        logToFile("1\n");
                        if (fb->buf_linear)
                            free(fb->buf_linear);

                        if (fb->buf) {                            
                            nwindowReleaseBuffers(fb->win);
                            logToFile("2\n");
                            nvMapClose(&fb->map);
                            logToFile("3\n");
                            free(fb->buf);
                            logToFile("4\n");
                        }

                        memset(fb, 0, sizeof(*fb));
                        logToFile("5\n");
                        nvFenceExit();
                        logToFile("6\n");
                        nvMapExit();
                        logToFile("7\n");
                        nvExit();
                        logToFile("8\n");
                    }*/

                private:
                    u16 LayerWidth  = 0;
                    u16 LayerHeight = 0;
                    u16 LayerPosX   = 0;
                    u16 LayerPosY   = 0;
                    u16 FramebufferWidth  = 0;
                    u16 FramebufferHeight = 0;
                    u64 aruid_ = 0;
                    bool m_initialized = false;
                    ViDisplay m_display;
                    ViLayer m_layer;
                    Event m_vsyncEvent;
                    bool m_hasLocalFont = false;
                    stbtt_fontinfo m_stdFont, m_localFont, m_extFont;
                    static inline float s_opacity = 1.0F;

                    NWindow m_window;
                    //NvMap m_map;
                    Framebuffer m_framebuffer;
                    //void* m_currentFramebuffer = nullptr;
            };

        }
    }
}
