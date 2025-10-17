#pragma once

#include <switch.h>
#include <sys/stat.h>
#include "logger.h"
#include "debug.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" 

//#include "stb_truetype.h"

u16 img_alert_time_out_width = 799;
u16 img_alert_time_out_height = 396;

const u16 ScreenWidth = 1280;
const u16 ScreenHeight = 720;
u16 FramebufferWidth = ScreenWidth;
u16 FramebufferHeight = ScreenHeight;
u8 FramebufferPixelSize = 2; //RGBA4444

typedef enum {
    RAW = 0,
    PNG = 1
} ImageFormat;

typedef struct  {
    const char* name;
    u8* data;
    u32 size_in_bytes;
    u32 nb_pixels;
} ImageDesc;

ImageDesc imagesBuffer[2];
ImageDesc noImage = { NULL, NULL, 0, 0 };
bool isNoImage(ImageDesc desc) {
    return desc.data == noImage.data && desc.name == noImage.name && desc.size_in_bytes == noImage.size_in_bytes;
}

typedef u16* Buffer;
typedef u16 Color;
typedef struct {
    u8 r;
    u8 g;
    u8 b;
    u8 a;
} Pixel8888;

typedef struct {
    u16 w;
    u16 h;
} Dimensions;

u64 framebufferSize() {
    return FramebufferWidth * FramebufferHeight * FramebufferPixelSize; //RGBA_4444
}

int imagesBufferCapacity() {
    return sizeof(imagesBuffer) / sizeof(ImageDesc);
}

int nbImagesInBuffer() {
    int cnt = 0;

    for(int i = 0 ; i < imagesBufferCapacity() ; i++) {        
        ImageDesc* desc = &imagesBuffer[i];
        if(desc->data) {
            cnt++;
        }
    }

    return cnt;
}

void initImagesBuffers() {
    logToFile("initImagesBuffer");
    memset(imagesBuffer, 0, sizeof(imagesBuffer));
}

void clearImagesBuffers() {
    // Deallocate data pointers for images
    for(int i = 0 ; i < nbImagesInBuffer() ; i++) {        
        ImageDesc desc = imagesBuffer[i];
        if(desc.data) {
            logToFile("Clear image data");
            free(desc.data);
            desc.data = NULL;
        }
    }
}

Color makeColor(u16 r, u16 g, u16 b, u16 a) {
    return RGBA4(r, g, b, a);
}

void setPixel(Framebuffer* fb, int x, int y, Color color) {
    Buffer buf = fb->buf_linear;
    buf[y*FramebufferWidth + x] = color;
}

inline void drawRect(Framebuffer* fb, s16 x, s16 y, s16 w, s16 h, Color color) {
    for (s16 x1 = x; x1 < (x + w); x1++)
        for (s16 y1 = y; y1 < (y + h); y1++)
            setPixel(fb, x1, y1, color);
}

void drawLine(Framebuffer* fb, int x0, int y0, int x1, int y1, Color color) {    
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while(1) {
        setPixel(fb, x0, y0, color);
        if(x0 == x1 && y0 == y1) break;
        e2 = 2*err;
        if(e2 >= dy) { err += dy; x0 += sx; }
        if(e2 <= dx) { err += dx; y0 += sy; }
    }
}

Pixel8888 getPixelFromImage8888(u8* pixmap, u16 x, u16 y, u16 w) {
    u8* p = pixmap + (y * w +x) * 4;
    Pixel8888 pixel = { 
        p[0], 
        p[1], 
        p[2], 
        p[3] 
    };

    return pixel;    
}

ImageDesc loadPngImageIntoBuffer(const char* filename) {    
    s32 _w = 0, _h = 0, _n = 0;
    u8* data = stbi_load(filename, &_w, &_h, &_n, 4);
    logToFile("PNG load finished");
    logIntToFile(_n);
    logIntToFile(_w);
    logIntToFile(_h);

    if(!data) {
        logToFile("PNG image load failed");
        return noImage;
    }

    // Store image into buffer
    for( int i = 0 ; i < sizeof(imagesBuffer) ; i++) {
        ImageDesc* desc = &imagesBuffer[i];
        if(desc && !desc->data) {
            // The slot is free
            logToFile("Adding image at slot");
            logIntToFile(i);
            desc->data = data;
            desc->name = filename;
            desc->size_in_bytes = _w * _h * 4; // Size of the loaded data
            desc->nb_pixels = _w * _h; // 4 bytes per pixel in RGBA8888
            return *desc;
        }
    }

    logToFile("No room left in the images buffer");

    return noImage;
}

ImageDesc loadRawImageIntoBuffer(const char* filename) {
    logToFile("Loading RAW image");
    logToFile(filename);

    struct stat st;
    if(stat(filename, &st) != 0) {
        logToFile("Could not get image information");
        return noImage;
    }

    FILE *f = fopen(filename, "rb");
    if(!f) {
        logToFile("Image file not loaded");
        return noImage;
    }

    u8* data = (u8*)malloc(st.st_size);
    if(!data) {
        logToFile("malloc failed");
        return noImage;
    }

    u32 len = fread(data, 1, st.st_size, f);
    fclose(f);

    // Store image into buffer
    for( int i = 0 ; i < sizeof(imagesBuffer) ; i++) {
        ImageDesc* desc = &imagesBuffer[i];
        if(desc && !desc->data) {
            // The slot is free
            logToFile("Adding image at slot");
            logIntToFile(i);
            desc->data = data;
            desc->name = filename;
            desc->size_in_bytes = len; // Size of the loaded data
            desc->nb_pixels = len / 4; // 4 bytes per pixel in RGBA8888
            return *desc;
        }
    }

    logToFile("No room left in the images buffer");

    return noImage;
}

ImageDesc loadImageIntoBuffer(const char* filename, ImageFormat format) {
    size_t lenstr = strlen(filename);
    if(lenstr < 4) {
        return noImage;
    }

    switch(format) {
        case RAW: return loadRawImageIntoBuffer(filename);
        case PNG: return loadPngImageIntoBuffer(filename);        
    } 

    logToFile("Image format undefined");
    return noImage;
    
}

ImageDesc getImageFromBuffer(const char* filename) {
    for( int i = 0 ; i < nbImagesInBuffer() ; i++) {        
        ImageDesc desc = imagesBuffer[i];
        
        if(isNoImage(desc)) return noImage; // No image after
        
        if(desc.name && strcmp(desc.name, filename) == 0) {
            return desc;
        }
    }

    logToFile("no image in buffer");
    return noImage;
}

void drawImage(Framebuffer* fb, const char* filename, ImageFormat format, u16 x, u16 y, u16 w, u16 h) {    
    ImageDesc img = getImageFromBuffer(filename);
    if(isNoImage(img)) {
        img = loadImageIntoBuffer(filename, format);
    }    

    u32 offset = 0;
    u8* fb_start = (u8*)fb->buf_linear;
    u8* dst = NULL;
    u32 cnt = 0;
    u16 _x = 0;
    u16 _y = 0;
    Pixel8888 pixel8888;
    u16 pixel4444 = 0;

    for(int idx = 0 ; idx < img.nb_pixels ; idx++) {
        // We work on a pixel basis        
        if(_x == w) {
            _x = 0;
            _y++;
        } else {
            _x++;
        }

        // Get next pixel from the pixmap
        pixel8888 = getPixelFromImage8888(img.data, _x, _y, w);
        // Convert to the framebuffer encoding
        pixel4444 = RGBA4_FROM_RGBA8(pixel8888.r, pixel8888.g, pixel8888.b, pixel8888.a);

        // Calculate the address of the pixel in the framebuffer
        offset = (_y + y) * fb->stride; // y offset
        offset += (_x + x) * FramebufferPixelSize; // x offset
        dst = fb_start + offset;

        if(dst >= fb_start + framebufferSize()) {
            logToFile("Framebuffer overflow detected");
            logPointerToFile(fb_start);
            logIntToFile(offset);
            logPointerToFile(dst);
            logIntToFile(framebufferSize());
            logIntToFile(_x);
            logIntToFile(_y);
            logIntToFile(fb->stride);
            return;
        }

        // Write the pixel in the framebuffer
        *(u16*)dst = pixel4444;
        cnt++;
    }
}

/*bool getStandardFont(PlFontData* fontData) {
    logToFile("plInitialize");
    Result rc = plInitialize(PlServiceType_User);
    logIntToFile(debugResult(rc));
    
    if(R_FAILED(rc)) {
        return false;
    }

    rc = plGetSharedFontByType(fontData, PlSharedFontType_Standard);
    if(R_FAILED(rc)) {
        logToFile("plGetSharedFontByType failed");
        logIntToFile(debugResult(rc));
        plExit();
        return false;
    }

    logToFile("Font loaded");
    logPointerToFile(fontData->address);
    logIntToFile(fontData->offset);
    logIntToFile(fontData->size);

    plExit();
    return true;
}

void blit_grayscale_to_fb(u8 *bitmap, int bw, int bh,
                          int pos_x, int pos_y,
                          u8 *fb, int fb_width, int fb_height, int fb_stride) {
    for (int y = 0; y < bh; y++) {
        for (int x = 0; x < bw; x++) {
            int dst_x = pos_x + x;
            int dst_y = pos_y + y;

            if (dst_x < 0 || dst_x >= fb_width || dst_y < 0 || dst_y >= fb_height)
                continue;

            u8 val = bitmap[y * bw + x];  // 0–255 (niveaux de gris)

            // Conversion 8 bits → 4 bits
            u8 r = val >> 4;  // 0–15
            u8 g = val >> 4;
            u8 b = val >> 4;
            u8 a = 0xF;       // opaque

            // Construire un pixel RGBA4444
            uint16_t pixel = (r << 12) | (g << 8) | (b << 4) | a;

            // Destination : framebuffer (16 bits/pixel)
            uint16_t *dst = (uint16_t*)(fb + dst_y * fb_stride + dst_x * 2);
            *dst = pixel;
        }
    }
}

void drawText(Framebuffer* buf, const char* txt, u16 x, u16 y, u16 w, u16 h, u8 lh) {
    PlFontData fontData; 
    
    if(!getStandardFont(&fontData)) {
        logToFile("Font not loaded. aborting.");
        return;
    }     

    stbtt_fontinfo info;    
    u8* fontBuffer = (u8*)fontData.address;
    if(!stbtt_InitFont(&info, fontBuffer, stbtt_GetFontOffsetForIndex(fontBuffer, 0))) {
        logToFile("Font init failed");
        return;
    }

    logIntToFile(1);
    u8* bitmap = calloc(w * h, sizeof(u8));

    logIntToFile(2);
    float scale = stbtt_ScaleForPixelHeight(&info, lh);

    int _x = 0;
    int ascent, descent, lineGap;
    logIntToFile(3);
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    
    ascent = roundf(ascent * scale);
    descent = roundf(descent * scale);

    for(int i = 0 ; i < strlen(txt) ; ++i) {
        int ax, lsb;

        logIntToFile(4);
        stbtt_GetCodepointHMetrics(&info, txt[i], &ax, &lsb);

        int c_x1, c_y1, c_x2, c_y2;
        logIntToFile(5);
        stbtt_GetCodepointBitmapBox(&info, txt[i], scale, scale, &c_x1, &c_y1, &c_x2, &c_y2);

        int _y = ascent + c_y1;

        int byteOffset = _x + roundf(lsb * scale) + (_y * w);
        logIntToFile(6);
        stbtt_MakeCodepointBitmap(&info, bitmap + byteOffset, c_x2 - c_x1, c_y2 - c_y1, w, scale, scale, txt[i]);

        _x += roundf(ax * scale);

        int kern;
        logIntToFile(7);
        kern = stbtt_GetCodepointKernAdvance(&info, txt[i], txt[i + 1]);
        _x += roundf(kern * scale);
    }
    
    //Copy the bitmap into the framebuffer
    logIntToFile(8);
    blit_grayscale_to_fb(bitmap, w, h, x, y, buf->buf_linear, buf->width_aligned, buf->height_aligned, buf->stride);

    logIntToFile(9);
    free(bitmap);
}
*/