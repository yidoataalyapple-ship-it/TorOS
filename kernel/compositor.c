/*
 * torOS Compositor
 * Alpha blending, drop shadows, window composition
 * Pixels from all windows -> single framebuffer
 */

#include "../include/toros.h"
#include "../include/window.h"

static compositor_t comp;

void compositor_init(uint32 width, uint32 height)
{
    printk_color(TERM_YELLOW, "[BOOT] Compositor...\n");

    memset(&comp, 0, sizeof(compositor_t));
    spin_init(&comp.comp_lock);

    comp.compose_width = width;
    comp.compose_height = height;
    comp.enable_shadows = 1;
    comp.enable_transparency = 1;
    comp.fps_limit = 60;
    comp.last_compose_jiffies = 0;

    /* Allocate compose buffer */
    uint32 buffer_size = width * height * sizeof(uint32);
    comp.compose_buffer = (uint32 *)kmalloc(buffer_size);
    if (comp.compose_buffer) {
        memset(comp.compose_buffer, 0, buffer_size);
    }

    comp.initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] Compositor: %dx%d, shadows=%s, alpha=%s\n",
                 width, height,
                 comp.enable_shadows ? "on" : "off",
                 comp.enable_transparency ? "on" : "off");
}

void compositor_shutdown(void)
{
    if (comp.compose_buffer) {
        /* kfree(comp.compose_buffer); */
        comp.compose_buffer = NULL;
    }
    comp.initialized = 0;
}

/* Alpha blend two ARGB pixels */
uint32 compositor_alpha_blend(uint32 dst, uint32 src)
{
    uint8 sa = (src >> 24) & 0xFF;

    if (sa == 0) return dst;
    if (sa == 0xFF) return src;

    uint8 sr = (src >> 16) & 0xFF;
    uint8 sg = (src >> 8) & 0xFF;
    uint8 sb = src & 0xFF;

    uint8 dr = (dst >> 16) & 0xFF;
    uint8 dg = (dst >> 8) & 0xFF;
    uint8 db = dst & 0xFF;

    uint8 inv_sa = 255 - sa;

    uint8 r = ((sr * sa) + (dr * inv_sa)) / 255;
    uint8 g = ((sg * sa) + (dg * inv_sa)) / 255;
    uint8 b = ((sb * sa) + (db * inv_sa)) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/* Blit with per-pixel alpha and global alpha */
void compositor_blit_alpha(uint32 *dst, int dst_w, int dst_h,
                           uint32 *src, int src_w, int src_h,
                           int dx, int dy, uint8 global_alpha)
{
    if (!dst || !src)
        return;

    for (int y = 0; y < src_h; y++) {
        int dst_y = dy + y;
        if (dst_y < 0 || dst_y >= dst_h)
            continue;

        for (int x = 0; x < src_w; x++) {
            int dst_x = dx + x;
            if (dst_x < 0 || dst_x >= dst_w)
                continue;

            uint32 src_pixel = src[y * src_w + x];

            /* Apply global alpha */
            if (global_alpha < 255) {
                uint8 sa = (src_pixel >> 24) & 0xFF;
                sa = (sa * global_alpha) / 255;
                src_pixel = (sa << 24) | (src_pixel & 0x00FFFFFF);
            }

            dst[dst_y * dst_w + dst_x] = compositor_alpha_blend(
                dst[dst_y * dst_w + dst_x], src_pixel);
        }
    }
}

/* Draw drop shadow around a rectangle */
void compositor_draw_shadow(uint32 *fb, int fb_w, int fb_h,
                            int x, int y, int w, int h)
{
    if (!comp.enable_shadows || !fb)
        return;

    /* Shadow below and to the right */
    for (int sy = 0; sy < SHADOW_SIZE; sy++) {
        int shadow_y = y + h + sy;
        if (shadow_y < 0 || shadow_y >= fb_h)
            continue;

        for (int sx = 0; sx < w + SHADOW_SIZE; sx++) {
            int shadow_x = x + sx;
            if (shadow_x < 0 || shadow_x >= fb_w)
                continue;

            /* Fade shadow */
            uint8 alpha = SHADOW_ALPHA * (SHADOW_SIZE - sy) / SHADOW_SIZE;
            uint32 shadow_pixel = (alpha << 24);

            fb[shadow_y * fb_w + shadow_x] = compositor_alpha_blend(
                fb[shadow_y * fb_w + shadow_x], shadow_pixel);
        }
    }

    /* Shadow to the right */
    for (int sy = 0; sy < h; sy++) {
        int shadow_y = y + sy;
        if (shadow_y < 0 || shadow_y >= fb_h)
            continue;

        for (int sx = 0; sx < SHADOW_SIZE; sx++) {
            int shadow_x = x + w + sx;
            if (shadow_x < 0 || shadow_x >= fb_w)
                continue;

            uint8 alpha = SHADOW_ALPHA * (SHADOW_SIZE - sx) / SHADOW_SIZE;
            uint32 shadow_pixel = (alpha << 24);

            fb[shadow_y * fb_w + shadow_x] = compositor_alpha_blend(
                fb[shadow_y * fb_w + shadow_x], shadow_pixel);
        }
    }

    /* Corner shadow */
    for (int sy = 0; sy < SHADOW_SIZE; sy++) {
        int shadow_y = y + h + sy;
        if (shadow_y < 0 || shadow_y >= fb_h)
            continue;

        for (int sx = 0; sx < SHADOW_SIZE; sx++) {
            int shadow_x = x + w + sx;
            if (shadow_x < 0 || shadow_x >= fb_w)
                continue;

            uint8 alpha = SHADOW_ALPHA * (SHADOW_SIZE - sy) * (SHADOW_SIZE - sx) /
                          (SHADOW_SIZE * SHADOW_SIZE);
            uint32 shadow_pixel = (alpha << 24);

            fb[shadow_y * fb_w + shadow_x] = compositor_alpha_blend(
                fb[shadow_y * fb_w + shadow_x], shadow_pixel);
        }
    }
}

/* Compose a single window */
void compositor_compose_window(window_t *win)
{
    if (!win || !comp.compose_buffer || !win->framebuffer)
        return;

    /* Skip minimized/hidden windows */
    if (win->state == WSTATE_MINIMIZED || win->state == WSTATE_HIDDEN)
        return;

    /* Draw shadow for normal windows with decorations */
    if (comp.enable_shadows && (win->style & WS_CAPTION)) {
        compositor_draw_shadow(comp.compose_buffer, comp.compose_width, comp.compose_height,
                               win->x, win->y, win->full_width, win->full_height);
    }

    /* Render widgets into the window's own framebuffer first (FAZ 4) */
    if (win->style & WS_CAPTION || win->id != 1) {
        extern void widget_draw_all(window_t *win);
        widget_draw_all(win);
    }

    /* Blit window content */
    if (comp.enable_transparency) {
        compositor_blit_alpha(comp.compose_buffer, comp.compose_width, comp.compose_height,
                              win->framebuffer, win->full_width, win->full_height,
                              win->x, win->y, 255);
    } else {
        /* Fast opaque blit */
        for (int y = 0; y < win->full_height; y++) {
            int dst_y = win->y + y;
            if (dst_y < 0 || dst_y >= (int)comp.compose_height)
                continue;

            for (int x = 0; x < win->full_width; x++) {
                int dst_x = win->x + x;
                if (dst_x < 0 || dst_x >= (int)comp.compose_width)
                    continue;

                uint32 pixel = win->framebuffer[y * win->full_width + x];
                uint8 alpha = (pixel >> 24) & 0xFF;
                if (alpha == 0xFF) {
                    comp.compose_buffer[dst_y * comp.compose_width + dst_x] = pixel;
                } else if (alpha > 0) {
                    comp.compose_buffer[dst_y * comp.compose_width + dst_x] =
                        compositor_alpha_blend(
                            comp.compose_buffer[dst_y * comp.compose_width + dst_x],
                            pixel);
                }
            }
        }
    }
}

/* Full compose - all windows */
void compositor_compose(void)
{
    if (!comp.initialized || !comp.compose_buffer)
        return;

    /* FPS limiting */
    if (comp.fps_limit > 0) {
        uint64 elapsed = get_jiffies() - comp.last_compose_jiffies;
        uint64 min_interval = 100 / comp.fps_limit;  /* At 100Hz timer */
        if (elapsed < min_interval)
            return;
    }

    comp.last_compose_jiffies = get_jiffies();
    comp.frame_counter++;

    /* Clear compose buffer */
    memset(comp.compose_buffer, 0,
           comp.compose_width * comp.compose_height * sizeof(uint32));

    /* Compose desktop background first */
    extern void desktop_shell_draw(uint32 *fb, int fb_w, int fb_h);
    desktop_shell_draw(comp.compose_buffer, comp.compose_width, comp.compose_height);

    /* Compose all windows in z-order (back-to-front) */
    extern window_manager_t wm;

    /* Occlusion culling: precompute which windows are fully covered by
     * a single higher window (common case: maximized window on top) */
    window_t *win = wm.window_list;
    while (win) {
        win->occluded = 0;
        if (win->state != WSTATE_MINIMIZED && win->state != WSTATE_HIDDEN &&
            (win->flags & WS_VISIBLE)) {
            for (window_t *top = win->next; top; top = top->next) {
                if (top->state == WSTATE_MINIMIZED || top->state == WSTATE_HIDDEN ||
                    !(top->flags & WS_VISIBLE))
                    continue;
                if (top->x <= win->x && top->y <= win->y &&
                    top->x + top->full_width >= win->x + win->full_width &&
                    top->y + top->full_height >= win->y + win->full_height) {
                    win->occluded = 1;
                    break;
                }
            }
        }
        win = win->next;
    }

    win = wm.window_list;
    while (win) {
        if (win->state != WSTATE_MINIMIZED && win->state != WSTATE_HIDDEN && !win->occluded) {
            compositor_compose_window(win);

            /* Draw decorations on top */
            if (win->style & WS_CAPTION) {
                extern void wm_draw_decorations(window_t *win, uint32 *fb, int fb_w, int fb_h);
                wm_draw_decorations(win, comp.compose_buffer, comp.compose_width, comp.compose_height);
            }
        }
        win = win->next;
    }

    /* Draw taskbar on top of everything */
    extern void desktop_shell_draw_taskbar(uint32 *fb, int fb_w, int fb_h);
    desktop_shell_draw_taskbar(comp.compose_buffer, comp.compose_width, comp.compose_height);
}

/* Compose only a specific rectangle (damage region tracking, FAZ 3.2/3.3) */
void compositor_compose_rect(int x, int y, int w, int h)
{
    if (!comp.initialized || !comp.compose_buffer)
        return;

    /* Clamp to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)comp.compose_width)  w = comp.compose_width - x;
    if (y + h > (int)comp.compose_height) h = comp.compose_height - y;
    if (w <= 0 || h <= 0)
        return;

    /* Damage clip region */
    clip_region_t dmg;
    clip_region_init(&dmg);
    clip_region_add_rect(&dmg, x, y, w, h);

    /* 1. Desktop background, clipped to damage region */
    extern void desktop_shell_draw_clipped(uint32 *fb, int fb_w, int fb_h, clip_region_t *clip);
    desktop_shell_draw_clipped(comp.compose_buffer, comp.compose_width, comp.compose_height, &dmg);

    /* 2. Windows back-to-front, only those intersecting the damage rect */
    extern window_manager_t wm;
    for (window_t *win = wm.window_list; win; win = win->next) {
        if (win->state == WSTATE_MINIMIZED || win->state == WSTATE_HIDDEN)
            continue;
        if (!(win->flags & WS_VISIBLE))
            continue;
        if (!clip_region_intersects(&dmg, win->x, win->y, win->full_width, win->full_height))
            continue;

        /* Render widgets, then blit window content clipped to the damage region */
        if (win->style & WS_CAPTION || win->id != 1) {
            extern void widget_draw_all(window_t *win);
            widget_draw_all(win);
        }
        if (win->framebuffer) {
            if (comp.enable_shadows && (win->style & WS_CAPTION)) {
                compositor_draw_shadow(comp.compose_buffer, comp.compose_width,
                                       comp.compose_height, win->x, win->y,
                                       win->full_width, win->full_height);
            }
            clip_blit(&dmg, comp.compose_buffer, comp.compose_width, comp.compose_height,
                      win->framebuffer, win->full_width, win->full_height,
                      win->x, win->y, 0, 0, win->full_width, win->full_height);
        }

        /* Decorations (drawn only for intersecting windows) */
        if (win->style & WS_CAPTION) {
            extern void wm_draw_decorations(window_t *win, uint32 *fb, int fb_w, int fb_h);
            wm_draw_decorations(win, comp.compose_buffer, comp.compose_width, comp.compose_height);
        }
    }

    /* 3. Taskbar if the damage rect touches it */
    extern uint32 desktop_shell_taskbar_height(void);
    int tb_h = (int)desktop_shell_taskbar_height();
    if (tb_h > 0 && clip_region_intersects(&dmg, 0, comp.compose_height - tb_h,
                                           comp.compose_width, tb_h)) {
        extern void desktop_shell_draw_taskbar(uint32 *fb, int fb_w, int fb_h);
        desktop_shell_draw_taskbar(comp.compose_buffer, comp.compose_width, comp.compose_height);
    }

    clip_region_free(&dmg);
    comp.frame_counter++;
}

void compositor_set_shadows(int enable)
{
    comp.enable_shadows = enable ? 1 : 0;
}

void compositor_set_transparency(int enable)
{
    comp.enable_transparency = enable ? 1 : 0;
}

void compositor_set_fps_limit(uint32 fps)
{
    comp.fps_limit = fps;
}

uint32 *compositor_get_buffer(void)
{
    return comp.compose_buffer;
}

int compositor_is_initialized(void)
{
    return comp.initialized;
}

void compositor_window_opacity(window_t *win, uint8 alpha)
{
    if (!win || !win->framebuffer)
        return;

    /* Modify framebuffer alpha */
    uint32 count = win->full_width * win->full_height;
    for (uint32 i = 0; i < count; i++) {
        uint32 pixel = win->framebuffer[i];
        uint8 old_alpha = (pixel >> 24) & 0xFF;
        uint8 new_alpha = (old_alpha * alpha) / 255;
        win->framebuffer[i] = (new_alpha << 24) | (pixel & 0x00FFFFFF);
    }

    win->flags |= WF_DIRTY;
}

/* Get current FPS */
uint32 compositor_get_fps(void)
{
    return comp.current_fps;
}

/* Update FPS counter */
void compositor_update_fps(void)
{
    static uint64 last_fps_update = 0;
    static uint32 last_frame_count = 0;

    uint64 now = get_jiffies();
    if (now - last_fps_update >= 100) {  /* 1 second */
        comp.current_fps = comp.frame_counter - last_frame_count;
        last_frame_count = comp.frame_counter;
        last_fps_update = now;
    }
}
