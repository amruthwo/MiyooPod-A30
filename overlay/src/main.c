#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <SDL2/SDL.h>
// Forward declarations for Go functions
extern void GoLogMsg(char* msg);
extern void DetectDevice(int width, int height);
// MiyooPod renders at 640x480 always
const int RENDER_WIDTH = 640;
const int RENDER_HEIGHT = 480;
// A30 physical framebuffer: 480x640 portrait, 32bpp, stride 1920
// Virtual size is 480x1280 (double-buffered)
#define FB_PHYS_W  480
#define FB_PHYS_H  640
#define FB_STRIDE  1920  // FB_PHYS_W * 4
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
// Direct framebuffer access
static int    fb_fd      = -1;
static void  *fb_mem     = NULL;
static size_t fb_size    = FB_PHYS_W * FB_PHYS_H * 4 * 2; // both buffers
static int    fb_buf_idx = 0; // which buffer we're writing to (0 or 1)
void c_log(const char *msg) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "[C] INFO: %s", msg);
    GoLogMsg(buffer);
}
void c_logf(const char *fmt, const char *detail) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "[C] INFO: ");
    int offset = strlen(buffer);
    snprintf(buffer + offset, sizeof(buffer) - offset, fmt, detail);
    GoLogMsg(buffer);
}
void c_logd(const char *fmt, int val) {
    char buffer[512];
    snprintf(buffer, sizeof(buffer), "[C] INFO: ");
    int offset = strlen(buffer);
    snprintf(buffer + offset, sizeof(buffer) - offset, fmt, val);
    GoLogMsg(buffer);
}
int pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            return event.key.keysym.sym;
        }
        if (event.type == SDL_KEYUP) {
            return -(event.key.keysym.sym + 1);
        }
    }
    return -1;
}
/*
 * refreshScreenPtr: rotate 640x480 ABGR8888 → 480x640 and write to /dev/fb0.
 * Uses double buffering to eliminate screen tearing:
 *   - Write to back buffer (the buffer NOT currently displayed)
 *   - Pan display to show the back buffer via FBIOPAN_DISPLAY
 *   - Swap buffer index
 *
 * 90° CCW rotation: src(x, y) → dst(y, 639-x)
 * dst_offset = dst_y * FB_PHYS_W + dst_x  =  (639-x) * FB_PHYS_W + y
 */
int refreshScreenPtr(unsigned char *pixels) {
    if (fb_mem == NULL) return -1;

    // Write to back buffer (opposite of currently displayed)
    int back = 1 - fb_buf_idx;
    unsigned int *src = (unsigned int *)pixels;
    unsigned int *dst = (unsigned int *)fb_mem + back * FB_PHYS_W * FB_PHYS_H;

    for (int y = 0; y < RENDER_HEIGHT; y++) {
        for (int x = 0; x < RENDER_WIDTH; x++) {
            unsigned int p = src[y * RENDER_WIDTH + x];
            // Swap R and B (ABGR8888 -> sunxi fb ARGB8888)
            unsigned int a = (p >> 24) & 0xFF;
            unsigned int b = (p >> 16) & 0xFF;
            unsigned int g = (p >>  8) & 0xFF;
            unsigned int r = (p      ) & 0xFF;
            unsigned int fixed = (a << 24) | (r << 16) | (g << 8) | b;
            // 90 deg CCW: dst_x=y, dst_y=(639-x)
            int dst_x = y;
            int dst_y = (RENDER_WIDTH - 1) - x;
            dst[dst_y * FB_PHYS_W + dst_x] = fixed;
        }
    }

    // Pan display to show the back buffer
    struct fb_var_screeninfo vinfo;
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
        vinfo.yoffset = back * FB_PHYS_H;
        ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);
    }

    // Swap buffer index
    fb_buf_idx = back;
    return 0;
}
int init() {
    c_log("SDL2 init (VIDEO | AUDIO)...");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        c_logf("SDL_Init failed: %s", SDL_GetError());
        return -1;
    }
    c_log("SDL_Init OK");
    // Detect display resolution from framebuffer device
    int fb_detect = open("/dev/fb0", O_RDONLY);
    int display_width = 640, display_height = 480;
    if (fb_detect >= 0) {
        struct fb_var_screeninfo vinfo;
        if (ioctl(fb_detect, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            display_width  = vinfo.xres;
            display_height = vinfo.yres;
            c_logd("Detected FB resolution width: %d",  display_width);
            c_logd("Detected FB resolution height: %d", display_height);
            DetectDevice(display_width, display_height);
        } else {
            c_log("Could not get FB info, using default 640x480");
            DetectDevice(640, 480);
        }
        close(fb_detect);
    } else {
        c_log("Could not open /dev/fb0, using default 640x480");
        DetectDevice(640, 480);
    }
    // Open framebuffer for direct rendering
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        c_log("WARNING: Could not open /dev/fb0 for writing, falling back to SDL");
    } else {
        fb_mem = mmap(NULL, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
        if (fb_mem == MAP_FAILED) {
            c_log("WARNING: mmap /dev/fb0 failed, falling back to SDL");
            fb_mem = NULL;
            close(fb_fd);
            fb_fd = -1;
        } else {
            c_log("Direct framebuffer rendering enabled (double-buffered, 480x640, 90 CCW)");
        }
    }
    // Create minimal SDL window for input handling only
    c_log("Creating window...");
    window = SDL_CreateWindow("MiyooPod",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        RENDER_WIDTH, RENDER_HEIGHT, SDL_WINDOW_FULLSCREEN);
    if (!window) {
        c_logf("SDL_CreateWindow failed: %s", SDL_GetError());
        return -1;
    }
    c_log("Window created");
    c_log("Creating renderer...");
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        c_logf("SDL_CreateRenderer failed: %s", SDL_GetError());
        return -1;
    }
    c_log("Renderer created");
    // Texture still needed as intermediate buffer if fb0 mmap failed
    c_log("Creating texture at 640x480 (ABGR8888)...");
    texture = SDL_CreateTexture(renderer,
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
        RENDER_WIDTH, RENDER_HEIGHT);
    if (!texture) {
        c_logf("SDL_CreateTexture failed: %s", SDL_GetError());
        return -1;
    }
    c_log("Texture created");
    return 0;
}
void quit() {
    if (fb_mem) {
        // Clear both framebuffer pages to black before exit
        memset(fb_mem, 0, fb_size);
        // Pan back to buffer 0
        struct fb_var_screeninfo vinfo;
        if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) == 0) {
            vinfo.yoffset = 0;
            ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo);
        }
        munmap(fb_mem, fb_size);
        fb_mem = NULL;
    }
    if (fb_fd >= 0) {
        close(fb_fd);
        fb_fd = -1;
    }
    if (texture)  { SDL_DestroyTexture(texture);   }
    if (renderer) { SDL_DestroyRenderer(renderer); }
    if (window)   { SDL_DestroyWindow(window);     }
    SDL_Quit();
}
