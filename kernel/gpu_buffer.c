/*
 * torOS GPU Buffer Manager
 * Double/Triple Buffering + VSync
 * Tearing prevention via page flipping
 */

#include "../include/toros.h"
#include "../include/gpu.h"

/* Static buffer manager */
static gpu_buffer_manager_t buffer_mgr;
static int buffer_initialized = 0;

/* Display timing (for 60Hz VSync) */
#define VSYNC_INTERVAL_MS       16      /* ~60Hz = 16.67ms */
#define VSYNC_JIFFIES_INTERVAL  17      /* At 100Hz timer */

void gpu_buffer_init(uint32 width, uint32 height, buffer_mode_t mode)
{
    printk_color(TERM_YELLOW, "[BOOT] GPU Buffer Manager...\n");

    memset(&buffer_mgr, 0, sizeof(gpu_buffer_manager_t));
    buffer_mgr.width = width;
    buffer_mgr.height = height;
    buffer_mgr.mode = mode;
    buffer_mgr.vsync_enabled = 1;
    buffer_mgr.pending_swap = 0;
    buffer_mgr.swap_counter = 0;
    buffer_mgr.last_vsync_jiffies = 0;

    uint32 buffer_size = width * height * sizeof(uint32);

    /* Allocate front buffer */
    buffer_mgr.front = (uint32 *)page_alloc();
    if (!buffer_mgr.front) {
        printk_color(TERM_RED, "[GPU-BUF] Failed to alloc front buffer\n");
        return;
    }
    memset(buffer_mgr.front, 0, PAGE_SIZE);

    /* Allocate back buffer (always needed for double/triple) */
    if (mode >= BUFFER_DOUBLE) {
        buffer_mgr.back = (uint32 *)page_alloc();
        if (!buffer_mgr.back) {
            printk_color(TERM_RED, "[GPU-BUF] Failed to alloc back buffer\n");
            page_free(buffer_mgr.front);
            return;
        }
        memset(buffer_mgr.back, 0, PAGE_SIZE);
    } else {
        buffer_mgr.back = buffer_mgr.front;
    }

    /* Allocate third buffer (triple only) */
    if (mode >= BUFFER_TRIPLE) {
        buffer_mgr.third = (uint32 *)page_alloc();
        if (!buffer_mgr.third) {
            printk_color(TERM_RED, "[GPU-BUF] Failed to alloc third buffer\n");
            buffer_mgr.mode = BUFFER_DOUBLE;
        } else {
            memset(buffer_mgr.third, 0, PAGE_SIZE);
        }
    }

    buffer_initialized = 1;

    const char *mode_str;
    switch (mode) {
    case BUFFER_SINGLE:     mode_str = "Single"; break;
    case BUFFER_DOUBLE:     mode_str = "Double"; break;
    case BUFFER_TRIPLE:     mode_str = "Triple"; break;
    default:                mode_str = "Unknown"; break;
    }

    printk_color(TERM_GREEN, "[BOOT] GPU Buffer: %s buffering, %dx%d\n",
                 mode_str, width, height);
}

void gpu_buffer_shutdown(void)
{
    if (!buffer_initialized)
        return;

    if (buffer_mgr.third) page_free(buffer_mgr.third);
    if (buffer_mgr.back && buffer_mgr.back != buffer_mgr.front) page_free(buffer_mgr.back);
    if (buffer_mgr.front) page_free(buffer_mgr.front);

    memset(&buffer_mgr, 0, sizeof(gpu_buffer_manager_t));
    buffer_initialized = 0;
}

/* Get back buffer for drawing */
uint32 *gpu_buffer_get_back(void)
{
    if (!buffer_initialized)
        return NULL;
    return buffer_mgr.back;
}

/* Swap back and front buffers (page flip) */
void gpu_buffer_swap(void)
{
    if (!buffer_initialized || buffer_mgr.mode == BUFFER_SINGLE)
        return;

    if (buffer_mgr.mode == BUFFER_TRIPLE && buffer_mgr.third) {
        /* Triple buffer: rotate front -> third, back -> front, third -> back */
        uint32 *old_front = buffer_mgr.front;
        buffer_mgr.front = buffer_mgr.back;
        buffer_mgr.back = buffer_mgr.third;
        buffer_mgr.third = old_front;
    } else {
        /* Double buffer: swap front and back */
        uint32 *temp = buffer_mgr.front;
        buffer_mgr.front = buffer_mgr.back;
        buffer_mgr.back = temp;
    }

    buffer_mgr.swap_counter++;
    buffer_mgr.pending_swap = 1;
}

/* Present front buffer to display */
void gpu_buffer_present(void)
{
    if (!buffer_initialized)
        return;

    if (buffer_mgr.vsync_enabled) {
        gpu_buffer_wait_vsync();
    }

    /* In a real GPU, this would trigger a page flip */
    /* For now, we just mark the swap as done */
    buffer_mgr.pending_swap = 0;
}

/* Wait for vertical blank (VSync) */
void gpu_buffer_wait_vsync(void)
{
    if (!buffer_initialized)
        return;

    /* Wait until next VSync interval */
    uint64 current_jiffies = get_jiffies();
    uint64 next_vsync = buffer_mgr.last_vsync_jiffies + VSYNC_JIFFIES_INTERVAL;

    if (current_jiffies < next_vsync) {
        uint64 wait_ms = (next_vsync - current_jiffies) * 10;
        if (wait_ms > 0 && wait_ms < 100)
            rtc_mdelay((uint32)wait_ms);
    }

    buffer_mgr.last_vsync_jiffies = get_jiffies();
}

/* Enable/disable VSync */
void gpu_buffer_set_vsync(int enable)
{
    if (!buffer_initialized)
        return;
    buffer_mgr.vsync_enabled = enable ? 1 : 0;
    printk_color(TERM_CYAN, "[GPU-BUF] VSync %s\n", enable ? "enabled" : "disabled");
}

int gpu_buffer_is_vsync_enabled(void)
{
    if (!buffer_initialized)
        return 0;
    return buffer_mgr.vsync_enabled;
}

/* Clear back buffer */
void gpu_buffer_clear(uint32 color)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return;

    uint32 *buf = buffer_mgr.back;
    uint32 count = buffer_mgr.width * buffer_mgr.height;

    for (uint32 i = 0; i < count; i++)
        buf[i] = color;
}

/* Blit from source buffer to back buffer */
void gpu_buffer_blit(uint32 *src, uint32 sx, uint32 sy, uint32 sw, uint32 sh,
                     uint32 dx, uint32 dy, uint32 dw, uint32 dh)
{
    if (!buffer_initialized || !buffer_mgr.back || !src)
        return;

    /* Simple nearest-neighbor scaling blit */
    for (uint32 y = 0; y < dh; y++) {
        uint32 src_y = sy + (y * sh) / dh;
        uint32 dst_y = dy + y;

        if (dst_y >= buffer_mgr.height)
            break;

        for (uint32 x = 0; x < dw; x++) {
            uint32 src_x = sx + (x * sw) / dw;
            uint32 dst_x = dx + x;

            if (dst_x >= buffer_mgr.width)
                break;

            /* Simple alpha blend if needed */
            uint32 src_pixel = src[src_y * sw + src_x];
            uint32 dst_idx = dst_y * buffer_mgr.width + dst_x;

            uint8 alpha = (src_pixel >> 24) & 0xFF;
            if (alpha == 0xFF) {
                buffer_mgr.back[dst_idx] = src_pixel;
            } else if (alpha > 0) {
                /* Alpha blending */
                uint32 dst_pixel = buffer_mgr.back[dst_idx];
                uint8 sr = (src_pixel >> 16) & 0xFF;
                uint8 sg = (src_pixel >> 8) & 0xFF;
                uint8 sb = src_pixel & 0xFF;
                uint8 dr = (dst_pixel >> 16) & 0xFF;
                uint8 dg = (dst_pixel >> 8) & 0xFF;
                uint8 db = dst_pixel & 0xFF;

                uint8 r = ((sr * alpha) + (dr * (255 - alpha))) / 255;
                uint8 g = ((sg * alpha) + (dg * (255 - alpha))) / 255;
                uint8 b = ((sb * alpha) + (db * (255 - alpha))) / 255;

                buffer_mgr.back[dst_idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }
}

/* Get current frame rate estimate (swaps per second) */
uint32 gpu_buffer_get_fps(void)
{
    static uint64 last_jiffies = 0;
    static uint32 last_swaps = 0;
    static uint32 fps = 0;

    uint64 current = get_jiffies();
    if (current - last_jiffies >= 100) {  /* 1 second */
        fps = (buffer_mgr.swap_counter - last_swaps);
        last_swaps = buffer_mgr.swap_counter;
        last_jiffies = current;
    }

    return fps;
}

/* Get buffer info */
void gpu_buffer_get_info(uint32 *width, uint32 *height, buffer_mode_t *mode)
{
    if (width) *width = buffer_mgr.width;
    if (height) *height = buffer_mgr.height;
    if (mode) *mode = buffer_mgr.mode;
}

/* Direct pixel access to back buffer */
void gpu_buffer_putpixel(uint32 x, uint32 y, uint32 color)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return;
    if (x >= buffer_mgr.width || y >= buffer_mgr.height)
        return;

    buffer_mgr.back[y * buffer_mgr.width + x] = color;
}

uint32 gpu_buffer_getpixel(uint32 x, uint32 y)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return 0;
    if (x >= buffer_mgr.width || y >= buffer_mgr.height)
        return 0;

    return buffer_mgr.back[y * buffer_mgr.width + x];
}

/* Draw filled rectangle on back buffer */
void gpu_buffer_fill_rect(uint32 x, uint32 y, uint32 w, uint32 h, uint32 color)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return;

    for (uint32 row = 0; row < h && (y + row) < buffer_mgr.height; row++) {
        for (uint32 col = 0; col < w && (x + col) < buffer_mgr.width; col++) {
            buffer_mgr.back[(y + row) * buffer_mgr.width + (x + col)] = color;
        }
    }
}

/* Draw horizontal line */
void gpu_buffer_hline(uint32 x, uint32 y, uint32 len, uint32 color)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return;
    if (y >= buffer_mgr.height)
        return;

    for (uint32 i = 0; i < len && (x + i) < buffer_mgr.width; i++) {
        buffer_mgr.back[y * buffer_mgr.width + x + i] = color;
    }
}

/* Draw vertical line */
void gpu_buffer_vline(uint32 x, uint32 y, uint32 len, uint32 color)
{
    if (!buffer_initialized || !buffer_mgr.back)
        return;
    if (x >= buffer_mgr.width)
        return;

    for (uint32 i = 0; i < len && (y + i) < buffer_mgr.height; i++) {
        buffer_mgr.back[(y + i) * buffer_mgr.width + x] = color;
    }
}

/* Copy front buffer to legacy framebuffer (for compatibility) */
void gpu_buffer_copy_to_fb(void)
{
    if (!buffer_initialized || !buffer_mgr.front)
        return;

    extern uint32 *fb_base;
    extern int fb_initialized;

    if (fb_initialized && fb_base) {
        uint32 copy_size = buffer_mgr.width * buffer_mgr.height;
        if (copy_size > (FB_WIDTH * FB_HEIGHT))
            copy_size = FB_WIDTH * FB_HEIGHT;

        memcpy(fb_base, buffer_mgr.front, copy_size * sizeof(uint32));
    }
}
