/*
 * torOS Window Manager
 * Window creation, manipulation, z-order, decorations, hit-testing
 */

#include "../include/toros.h"
#include "../include/window.h"

static window_manager_t wm;

/* Decoration colors */
#define DECOR_ACTIVE_TITLE      0xFF0078D7
#define DECOR_INACTIVE_TITLE    0xFF505050
#define DECOR_BORDER            0xFF404040
#define DECOR_BUTTON_CLOSE      0xFFE81123
#define DECOR_BUTTON_MIN        0xFFFFB900
#define DECOR_BUTTON_MAX        0xFF107C10
#define DECOR_CLIENT_BG         0xFFF0F0F0
#define DECOR_TEXT_COLOR        0xFFFFFFFF

/* Hit test regions for title bar buttons */
#define BUTTON_CLOSE_OFFSET     0
#define BUTTON_MAX_OFFSET       1
#define BUTTON_MIN_OFFSET       2

void wm_init(int desktop_w, int desktop_h)
{
    printk_color(TERM_YELLOW, "[BOOT] Window Manager...\n");

    memset(&wm, 0, sizeof(window_manager_t));
    spin_init(&wm.wm_lock);
    wm.desktop_width = desktop_w;
    wm.desktop_height = desktop_h;
    wm.next_id = 1;
    wm.window_count = 0;

    /* Create desktop window (full screen, no decorations) */
    wm.desktop_window = wm_create_window("Desktop", 0, 0, desktop_w, desktop_h, 0);
    if (wm.desktop_window) {
        wm.desktop_window->z_order = ZORDER_DESKTOP;
        wm.desktop_window->bgcolor = 0xFF0078D7;
    }

    wm.initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] Window Manager: %dx%d desktop, max %d windows\n",
                 desktop_w, desktop_h, MAX_WINDOWS);
}

window_t *wm_create_window(const char *title, int x, int y, int w, int h, uint32 style)
{
    if (wm.window_count >= MAX_WINDOWS)
        return NULL;

    window_t *win = (window_t *)kmalloc(sizeof(window_t));
    if (!win)
        return NULL;

    memset(win, 0, sizeof(window_t));

    win->id = wm.next_id++;
    strncpy(win->title, title ? title : "Untitled", MAX_WINDOW_TITLE - 1);
    win->title[MAX_WINDOW_TITLE - 1] = '\0';

    win->style = style;
    win->state = WSTATE_NORMAL;
    win->flags = WF_NEED_REDRAW | WF_DIRTY;
    win->z_order = ZORDER_NORMAL;
    win->bgcolor = DECOR_CLIENT_BG;

    /* Calculate dimensions with decorations */
    if (style & WS_CAPTION) {
        win->x = x;
        win->y = y;
        win->width = w;
        win->height = h;
        win->full_width = w + BORDER_WIDTH * 2;
        win->full_height = h + BORDER_WIDTH + TITLEBAR_HEIGHT;
    } else {
        win->x = x;
        win->y = y;
        win->width = w;
        win->height = h;
        win->full_width = w;
        win->full_height = h;
    }

    /* Allocate window framebuffer */
    win->fb_size = win->full_width * win->full_height * sizeof(uint32);
    win->framebuffer = (uint32 *)kmalloc(win->fb_size);
    if (win->framebuffer) {
        memset(win->framebuffer, 0, win->fb_size);
    }

    /* Add to window list sorted by z-order */
    spin_lock(&wm.wm_lock);

    window_t **pp = &wm.window_list;
    while (*pp && (*pp)->z_order <= win->z_order) {
        pp = &(*pp)->next;
    }
    win->next = *pp;
    *pp = win;

    wm.window_count++;

    spin_unlock(&wm.wm_lock);

    printk_color(TERM_CYAN, "[WM] Created window #%d '%s' (%dx%d @ %d,%d)\n",
                 win->id, win->title, w, h, x, y);

    return win;
}

void wm_destroy_window(window_t *win)
{
    if (!win)
        return;

    spin_lock(&wm.wm_lock);

    /* Remove from window list */
    window_t **pp = &wm.window_list;
    while (*pp) {
        if (*pp == win) {
            *pp = win->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (wm.active_window == win)
        wm.active_window = NULL;
    if (wm.focused_window == win)
        wm.focused_window = NULL;
    if (wm.drag_window == win)
        wm.drag_window = NULL;

    wm.window_count--;

    spin_unlock(&wm.wm_lock);

    /* Free framebuffer */
    if (win->framebuffer) {
        /* kfree(win->framebuffer); -- simplified allocator */
    }

    printk_color(TERM_YELLOW, "[WM] Destroyed window #%d '%s'\n", win->id, win->title);
}

void wm_show_window(window_t *win)
{
    if (!win)
        return;
    win->state = WSTATE_NORMAL;
    win->flags |= WS_VISIBLE | WF_NEED_REDRAW | WF_DIRTY;
    wm_raise_window(win);
}

void wm_hide_window(window_t *win)
{
    if (!win)
        return;
    win->state = WSTATE_HIDDEN;
    win->flags &= ~WS_VISIBLE;
    win->flags |= WF_DIRTY;
    wm_invalidate_rect(win->x, win->y, win->full_width, win->full_height);
}

void wm_minimize_window(window_t *win)
{
    if (!win)
        return;
    win->state = WSTATE_MINIMIZED;
    win->flags &= ~WS_VISIBLE;
    win->flags |= WF_DIRTY;
    wm_invalidate_rect(win->x, win->y, win->full_width, win->full_height);
}

void wm_maximize_window(window_t *win)
{
    if (!win || win->state == WSTATE_MAXIMIZED)
        return;

    /* Save current position for restore */
    win->flags |= WF_DIRTY;
    win->state = WSTATE_MAXIMIZED;

    /* Fill entire desktop */
    win->x = 0;
    win->y = 0;
    win->width = wm.desktop_width;
    win->height = wm.desktop_height;
    win->full_width = wm.desktop_width;
    win->full_height = wm.desktop_height;

    win->flags |= WF_NEED_REDRAW;
    wm_invalidate_rect(0, 0, wm.desktop_width, wm.desktop_height);
}

void wm_restore_window(window_t *win)
{
    if (!win)
        return;
    win->state = WSTATE_NORMAL;
    win->flags |= WS_VISIBLE | WF_NEED_REDRAW | WF_DIRTY;
    /* Restore position would need saved state */
    wm_invalidate_rect(win->x, win->y, win->full_width, win->full_height);
}

void wm_set_active_window(window_t *win)
{
    if (!win || wm.active_window == win)
        return;

    /* Deactivate previous */
    if (wm.active_window) {
        wm.active_window->flags &= ~WF_ACTIVE;
        wm.active_window->flags |= WF_DIRTY;
    }

    wm.active_window = win;
    wm.focused_window = win;
    win->flags |= WF_ACTIVE | WF_DIRTY;

    wm_raise_window(win);
}

window_t *wm_get_active_window(void)
{
    return wm.active_window;
}

void wm_move_window(window_t *win, int x, int y)
{
    if (!win || win->state == WSTATE_MAXIMIZED)
        return;

    int old_x = win->x;
    int old_y = win->y;

    win->x = x;
    win->y = y;
    win->flags |= WF_DIRTY;

    /* Invalidate old and new position */
    wm_invalidate_rect(old_x, old_y, win->full_width, win->full_height);
    wm_invalidate_rect(x, y, win->full_width, win->full_height);
}

void wm_resize_window(window_t *win, int w, int h)
{
    if (!win || win->state == WSTATE_MAXIMIZED)
        return;

    if (w < 100) w = 100;
    if (h < 50) h = 50;
    if (w > wm.desktop_width) w = wm.desktop_width;
    if (h > wm.desktop_height) h = wm.desktop_height;

    win->width = w;
    win->height = h;

    if (win->style & WS_CAPTION) {
        win->full_width = w + BORDER_WIDTH * 2;
        win->full_height = h + BORDER_WIDTH + TITLEBAR_HEIGHT;
    } else {
        win->full_width = w;
        win->full_height = h;
    }

    win->flags |= WF_NEED_REDRAW | WF_DIRTY;
    wm_invalidate_rect(win->x, win->y, win->full_width, win->full_height);
}

void wm_raise_window(window_t *win)
{
    if (!win || !wm.initialized)
        return;

    spin_lock(&wm.wm_lock);

    /* Remove from current position */
    window_t **pp = &wm.window_list;
    while (*pp) {
        if (*pp == win) {
            *pp = win->next;
            break;
        }
        pp = &(*pp)->next;
    }

    /* Find insertion point based on z-order */
    pp = &wm.window_list;
    while (*pp && (*pp)->z_order <= win->z_order) {
        pp = &(*pp)->next;
    }

    win->next = *pp;
    *pp = win;

    spin_unlock(&wm.wm_lock);

    win->flags |= WF_DIRTY;
}

void wm_lower_window(window_t *win)
{
    if (!win)
        return;

    spin_lock(&wm.wm_lock);

    window_t **pp = &wm.window_list;
    while (*pp) {
        if (*pp == win) {
            *pp = win->next;
            break;
        }
        pp = &(*pp)->next;
    }

    win->next = wm.window_list;
    wm.window_list = win;

    spin_unlock(&wm.wm_lock);

    win->flags |= WF_DIRTY;
}

window_t *wm_find_window_at(int x, int y)
{
    if (!wm.initialized)
        return NULL;

    /* Search from top (end of list) to bottom */
    window_t *found = NULL;
    window_t *win = wm.window_list;

    while (win) {
        if ((win->flags & WS_VISIBLE) && win->state != WSTATE_MINIMIZED && win->state != WSTATE_HIDDEN) {
            if (x >= win->x && x < win->x + win->full_width &&
                y >= win->y && y < win->y + win->full_height) {
                found = win;
            }
        }
        win = win->next;
    }

    return found;
}

window_t *wm_get_desktop_window(void)
{
    return wm.desktop_window;
}

void wm_invalidate_rect(int x, int y, int w, int h)
{
    wm.dirty_region.x = x;
    wm.dirty_region.y = y;
    wm.dirty_region.w = w;
    wm.dirty_region.h = h;
}

void wm_invalidate_window(window_t *win)
{
    if (!win)
        return;
    win->flags |= WF_DIRTY | WF_NEED_REDRAW;
    wm_invalidate_rect(win->x, win->y, win->full_width, win->full_height);
}

int wm_get_window_count(void)
{
    return wm.window_count;
}

void wm_get_window_list(window_t **list, int *count)
{
    if (!list || !count)
        return;

    *count = 0;
    window_t *win = wm.window_list;
    while (win && *count < MAX_WINDOWS) {
        list[(*count)++] = win;
        win = win->next;
    }
}

void wm_dump_windows(void)
{
    printk_color(TERM_CYAN, "\n=== Window List (%d windows) ===\n", wm.window_count);

    window_t *win = wm.window_list;
    while (win) {
        const char *state_str;
        switch (win->state) {
        case WSTATE_NORMAL:     state_str = "NORMAL"; break;
        case WSTATE_MINIMIZED:  state_str = "MIN"; break;
        case WSTATE_MAXIMIZED:  state_str = "MAX"; break;
        case WSTATE_HIDDEN:     state_str = "HIDDEN"; break;
        default:                state_str = "?"; break;
        }

        const char *active_mark = (win == wm.active_window) ? " *" : "";

        printk_color(TERM_WHITE, "  #%d Z=%d %s [%s] %dx%d @ (%d,%d)%s\n",
                     win->id, win->z_order, win->title, state_str,
                     win->width, win->height, win->x, win->y, active_mark);
        win = win->next;
    }
    printk("\n");
}

/* ========== Window Decorations ========== */

void wm_draw_decorations(window_t *win, uint32 *fb, int fb_w, int fb_h)
{
    if (!win || !fb || !(win->style & WS_CAPTION))
        return;

    int is_active = (win->flags & WF_ACTIVE) ? 1 : 0;
    uint32 title_color = is_active ? DECOR_ACTIVE_TITLE : DECOR_INACTIVE_TITLE;

    /* Outer border */
    for (int by = 0; by < BORDER_WIDTH; by++) {
        for (int bx = 0; bx < win->full_width; bx++) {
            int px = win->x + bx;
            int py_top = win->y + by;
            int py_bottom = win->y + win->full_height - 1 - by;

            if (px >= 0 && px < fb_w) {
                if (py_top >= 0 && py_top < fb_h)
                    fb[py_top * fb_w + px] = DECOR_BORDER;
                if (py_bottom >= 0 && py_bottom < fb_h)
                    fb[py_bottom * fb_w + px] = DECOR_BORDER;
            }
        }
    }

    /* Side borders */
    for (int by = BORDER_WIDTH; by < win->full_height - BORDER_WIDTH; by++) {
        for (int bx = 0; bx < BORDER_WIDTH; bx++) {
            int px_left = win->x + bx;
            int px_right = win->x + win->full_width - 1 - bx;
            int py = win->y + by;

            if (py >= 0 && py < fb_h) {
                if (px_left >= 0 && px_left < fb_w)
                    fb[py * fb_w + px_left] = DECOR_BORDER;
                if (px_right >= 0 && px_right < fb_w)
                    fb[py * fb_w + px_right] = DECOR_BORDER;
            }
        }
    }

    /* Title bar background */
    for (int ty = BORDER_WIDTH; ty < BORDER_WIDTH + TITLEBAR_HEIGHT; ty++) {
        for (int tx = BORDER_WIDTH; tx < win->full_width - BORDER_WIDTH; tx++) {
            int px = win->x + tx;
            int py = win->y + ty;

            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                fb[py * fb_w + px] = title_color;
            }
        }
    }

    /* Window buttons (close, maximize, minimize) */
    if (win->style & WS_SYSMENU) {
        int btn_y = win->y + BORDER_WIDTH + (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;

        /* Close button */
        if (win->style & WS_SYSMENU) {
            int close_x = win->x + win->full_width - BORDER_WIDTH - BUTTON_SIZE - BUTTON_MARGIN;
            for (int by = 0; by < BUTTON_SIZE; by++) {
                for (int bx = 0; bx < BUTTON_SIZE; bx++) {
                    int px = close_x + bx;
                    int py = btn_y + by;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                        fb[py * fb_w + px] = DECOR_BUTTON_CLOSE;
                    }
                }
            }
        }

        /* Maximize button */
        if (win->style & WS_MAXIMIZEBOX) {
            int max_x = win->x + win->full_width - BORDER_WIDTH - BUTTON_SIZE * 2 - BUTTON_MARGIN * 2;
            for (int by = 0; by < BUTTON_SIZE; by++) {
                for (int bx = 0; bx < BUTTON_SIZE; bx++) {
                    int px = max_x + bx;
                    int py = btn_y + by;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                        fb[py * fb_w + px] = DECOR_BUTTON_MAX;
                    }
                }
            }
        }

        /* Minimize button */
        if (win->style & WS_MINIMIZEBOX) {
            int min_x = win->x + win->full_width - BORDER_WIDTH - BUTTON_SIZE * 3 - BUTTON_MARGIN * 3;
            for (int by = 0; by < BUTTON_SIZE; by++) {
                for (int bx = 0; bx < BUTTON_SIZE; bx++) {
                    int px = min_x + bx;
                    int py = btn_y + by;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                        fb[py * fb_w + px] = DECOR_BUTTON_MIN;
                    }
                }
            }
        }
    }

    /* Title text */
    if (win->title[0]) {
        int text_x = win->x + BORDER_WIDTH + 4;
        int text_y = win->y + BORDER_WIDTH + (TITLEBAR_HEIGHT - 8) / 2;
        extern void fb_draw_string(int x, int y, const char *s);
        uint32 saved_color = 0;
        extern void fb_set_color(uint32 color);
        fb_set_color(DECOR_TEXT_COLOR);
        fb_draw_string(text_x, text_y, win->title);
    }
}

/* Hit test for window decorations */
int wm_hit_test(window_t *win, int x, int y)
{
    if (!win)
        return HT_NOWHERE;

    /* Translate to window coordinates */
    int wx = x - win->x;
    int wy = y - win->y;

    /* Check if inside window at all */
    if (wx < 0 || wy < 0 || wx >= win->full_width || wy >= win->full_height)
        return HT_NOWHERE;

    /* Title bar area */
    if (win->style & WS_CAPTION) {
        if (wy >= BORDER_WIDTH && wy < BORDER_WIDTH + TITLEBAR_HEIGHT) {
            /* Check buttons */
            int btn_y_start = BORDER_WIDTH + (TITLEBAR_HEIGHT - BUTTON_SIZE) / 2;
            int btn_y_end = btn_y_start + BUTTON_SIZE;

            if (wy >= btn_y_start && wy < btn_y_end) {
                /* Close button */
                int close_x = win->full_width - BORDER_WIDTH - BUTTON_SIZE - BUTTON_MARGIN;
                if (wx >= close_x && wx < close_x + BUTTON_SIZE)
                    return HT_CLOSE;

                /* Maximize button */
                int max_x = win->full_width - BORDER_WIDTH - BUTTON_SIZE * 2 - BUTTON_MARGIN * 2;
                if ((win->style & WS_MAXIMIZEBOX) && wx >= max_x && wx < max_x + BUTTON_SIZE)
                    return HT_MAXIMIZE;

                /* Minimize button */
                int min_x = win->full_width - BORDER_WIDTH - BUTTON_SIZE * 3 - BUTTON_MARGIN * 3;
                if ((win->style & WS_MINIMIZEBOX) && wx >= min_x && wx < min_x + BUTTON_SIZE)
                    return HT_MINIMIZE;
            }

            return HT_CAPTION;
        }
    }

    /* Border resize areas */
    if (win->style & WS_THICKFRAME) {
        int right = win->full_width - BORDER_WIDTH;
        int bottom = win->full_height - BORDER_WIDTH;

        if (wx < BORDER_WIDTH) {
            if (wy < BORDER_WIDTH) return HT_TOPLEFT;
            if (wy >= bottom) return HT_BOTTOMLEFT;
            return HT_LEFT;
        }
        if (wx >= right) {
            if (wy < BORDER_WIDTH) return HT_TOPRIGHT;
            if (wy >= bottom) return HT_BOTTOMRIGHT;
            return HT_RIGHT;
        }
        if (wy < BORDER_WIDTH) return HT_TOP;
        if (wy >= bottom) return HT_BOTTOM;
    }

    return HT_CLIENT;
}
