/*
 * torOS Virtual Desktop Manager
 * Multiple desktops with independent window sets
 */

#include "../include/toros.h"
#include "../include/window.h"

static virtual_desktop_manager_t vd_mgr;

void vd_init(int num_desktops)
{
    printk_color(TERM_YELLOW, "[BOOT] Virtual Desktops...\n");

    memset(&vd_mgr, 0, sizeof(virtual_desktop_manager_t));
    spin_init(&vd_mgr.vd_lock);

    if (num_desktops < 1) num_desktops = 1;
    if (num_desktops > MAX_VIRTUAL_DESKTOPS) num_desktops = MAX_VIRTUAL_DESKTOPS;

    /* Create default desktops */
    const char *default_names[] = {
        "Desktop 1", "Desktop 2", "Desktop 3", "Desktop 4",
        "Desktop 5", "Desktop 6", "Desktop 7", "Desktop 8"
    };

    for (int i = 0; i < num_desktops; i++) {
        virtual_desktop_t *vd = &vd_mgr.desktops[i];
        vd->id = i + 1;
        strncpy(vd->name, default_names[i], 31);
        vd->name[31] = '\0';
        vd->bg_color = 0xFF0078D7 + (i * 0x00101020);
        vd->active = (i == 0) ? 1 : 0;
        vd->windows = NULL;
        vd->wallpaper = NULL;
    }

    vd_mgr.num_desktops = num_desktops;
    vd_mgr.current_desktop = 0;
    vd_mgr.initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] Virtual Desktops: %d desktops\n", num_desktops);
}

void vd_switch(int desktop_idx)
{
    if (!vd_mgr.initialized || desktop_idx < 0 || desktop_idx >= vd_mgr.num_desktops)
        return;

    if (desktop_idx == vd_mgr.current_desktop)
        return;

    printk_color(TERM_CYAN, "[VD] Switching to desktop %d: %s\n",
                 desktop_idx + 1, vd_mgr.desktops[desktop_idx].name);

    /* Deactivate current */
    vd_mgr.desktops[vd_mgr.current_desktop].active = 0;

    /* Activate new */
    vd_mgr.desktops[desktop_idx].active = 1;
    vd_mgr.current_desktop = desktop_idx;

    /* TODO: Show/hide windows based on desktop membership */
    /* For now, all windows visible on all desktops */
}

void vd_create(const char *name)
{
    if (!vd_mgr.initialized || vd_mgr.num_desktops >= MAX_VIRTUAL_DESKTOPS)
        return;

    virtual_desktop_t *vd = &vd_mgr.desktops[vd_mgr.num_desktops];
    vd->id = vd_mgr.num_desktops + 1;
    strncpy(vd->name, name ? name : "New Desktop", 31);
    vd->name[31] = '\0';
    vd->bg_color = 0xFF202040;
    vd->active = 0;
    vd->windows = NULL;
    vd->wallpaper = NULL;

    vd_mgr.num_desktops++;

    printk_color(TERM_GREEN, "[VD] Created desktop %d: %s\n", vd->id, vd->name);
}

void vd_close(int desktop_idx)
{
    if (!vd_mgr.initialized || desktop_idx <= 0 || desktop_idx >= vd_mgr.num_desktops)
        return;

    /* Cannot close desktop 0 */
    if (desktop_idx == 0)
        return;

    printk_color(TERM_YELLOW, "[VD] Closing desktop %d\n", desktop_idx + 1);

    /* Move windows to desktop 0 */
    virtual_desktop_t *vd = &vd_mgr.desktops[desktop_idx];
    if (vd->windows) {
        /* Append to desktop 0 */
        window_t *win = vd->windows;
        while (win && win->next)
            win = win->next;

        if (win) {
            win->next = vd_mgr.desktops[0].windows;
            vd_mgr.desktops[0].windows = vd->windows;
        }
        vd->windows = NULL;
    }

    /* Shift desktops down */
    for (int i = desktop_idx; i < vd_mgr.num_desktops - 1; i++) {
        vd_mgr.desktops[i] = vd_mgr.desktops[i + 1];
        vd_mgr.desktops[i].id = i + 1;
    }

    vd_mgr.num_desktops--;

    /* Switch to desktop 0 if current was closed */
    if (vd_mgr.current_desktop >= vd_mgr.num_desktops)
        vd_mgr.current_desktop = 0;

    vd_mgr.desktops[vd_mgr.current_desktop].active = 1;
}

void vd_move_window_to(window_t *win, int desktop_idx)
{
    if (!vd_mgr.initialized || !win || desktop_idx < 0 || desktop_idx >= vd_mgr.num_desktops)
        return;

    /* Remove from current desktop */
    for (int i = 0; i < vd_mgr.num_desktops; i++) {
        window_t **pp = &vd_mgr.desktops[i].windows;
        while (*pp) {
            if (*pp == win) {
                *pp = win->next;
                break;
            }
            pp = &(*pp)->next;
        }
    }

    /* Add to new desktop */
    win->next = vd_mgr.desktops[desktop_idx].windows;
    vd_mgr.desktops[desktop_idx].windows = win;

    /* Hide if not on current desktop */
    if (desktop_idx != vd_mgr.current_desktop) {
        wm_hide_window(win);
    } else {
        wm_show_window(win);
    }
}

void vd_move_active_window_to(int desktop_idx)
{
    extern window_manager_t wm;
    if (wm.active_window)
        vd_move_window_to(wm.active_window, desktop_idx);
}

int vd_get_current(void)
{
    return vd_mgr.current_desktop;
}

int vd_get_count(void)
{
    return vd_mgr.num_desktops;
}

const char *vd_get_name(int desktop_idx)
{
    if (desktop_idx < 0 || desktop_idx >= vd_mgr.num_desktops)
        return "Invalid";
    return vd_mgr.desktops[desktop_idx].name;
}

void vd_next(void)
{
    int next = vd_mgr.current_desktop + 1;
    if (next >= vd_mgr.num_desktops)
        next = 0;
    vd_switch(next);
}

void vd_prev(void)
{
    int prev = vd_mgr.current_desktop - 1;
    if (prev < 0)
        prev = vd_mgr.num_desktops - 1;
    vd_switch(prev);
}

void vd_draw_switcher(uint32 *fb, int fb_w, int fb_h)
{
    if (!vd_mgr.initialized || !fb)
        return;

    int sw_w = 200;
    int sw_h = vd_mgr.num_desktops * 50 + 20;
    int sw_x = (fb_w - sw_w) / 2;
    int sw_y = (fb_h - sw_h) / 2;

    /* Background */
    for (int y = 0; y < sw_h; y++) {
        for (int x = 0; x < sw_w; x++) {
            int px = sw_x + x;
            int py = sw_y + y;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                uint32 bg = fb[py * fb_w + px];
                uint8 r = ((bg >> 16) & 0xFF) * 50 / 100;
                uint8 g = ((bg >> 8) & 0xFF) * 50 / 100;
                uint8 b = (bg & 0xFF) * 50 / 100;
                fb[py * fb_w + px] = 0xFF000000 | (r << 16) | (g << 8) | b;
            }
        }
    }

    /* Border */
    for (int x = 0; x < sw_w; x++) {
        int top = sw_y;
        int bot = sw_y + sw_h - 1;
        if (top >= 0 && top < fb_h) fb[top * fb_w + sw_x + x] = 0xFF666688;
        if (bot >= 0 && bot < fb_h) fb[bot * fb_w + sw_x + x] = 0xFF666688;
    }

    /* Desktop items */
    extern void fb_set_color(uint32 color);
    extern void fb_draw_string(int x, int y, const char *s);

    for (int i = 0; i < vd_mgr.num_desktops; i++) {
        int item_y = sw_y + 10 + i * 50;
        int item_h = 40;

        /* Highlight current */
        if (i == vd_mgr.current_desktop) {
            for (int y = 0; y < item_h; y++) {
                for (int x = 10; x < sw_w - 10; x++) {
                    int px = sw_x + x;
                    int py = item_y + y;
                    if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                        fb[py * fb_w + px] = 0xFF0078D7;
                    }
                }
            }
            fb_set_color(0xFFFFFFFF);
        } else {
            fb_set_color(0xFFCCCCDD);
        }

        char label[48];
        strcpy(label, " ");
        strcat(label, vd_mgr.desktops[i].name);
        fb_draw_string(sw_x + 20, item_y + 12, label);
    }
}

/* Get desktop for a window */
int vd_get_window_desktop(window_t *win)
{
    if (!vd_mgr.initialized || !win)
        return -1;

    for (int i = 0; i < vd_mgr.num_desktops; i++) {
        window_t *w = vd_mgr.desktops[i].windows;
        while (w) {
            if (w == win)
                return i;
            w = w->next;
        }
    }

    return -1;
}

/* Check if window is on current desktop */
int vd_is_window_on_current(window_t *win)
{
    return vd_get_window_desktop(win) == vd_mgr.current_desktop;
}
