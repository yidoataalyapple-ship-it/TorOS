/*
 * torOS Window Manager & Compositor Header
 * Full windowing system with alpha blending, decorations, virtual desktops
 */

#ifndef _WINDOW_H
#define _WINDOW_H

#include "toros.h"
#include "gpu.h"

/* ===== Window Styles ===== */
#define WS_BORDER           0x0001
#define WS_CAPTION          0x0002
#define WS_SYSMENU          0x0004
#define WS_THICKFRAME       0x0008
#define WS_MINIMIZEBOX      0x0010
#define WS_MAXIMIZEBOX      0x0020
#define WS_VISIBLE          0x0040
#define WS_DISABLED         0x0080
#define WS_POPUP            0x0100
#define WS_CHILD            0x0200
#define WS_OVERLAPPED       (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

/* Window states */
typedef enum {
    WSTATE_NORMAL,
    WSTATE_MINIMIZED,
    WSTATE_MAXIMIZED,
    WSTATE_HIDDEN
} window_state_t;

/* Window z-order levels */
#define ZORDER_DESKTOP      0
#define ZORDER_NORMAL       1000
#define ZORDER_TOOLTIP      5000
#define ZORDER_TOPMOST      10000
#define ZORDER_OVERLAY      20000

/* Title bar constants */
#define TITLEBAR_HEIGHT     24
#define BORDER_WIDTH        4
#define BUTTON_SIZE         18
#define BUTTON_MARGIN       2

/* Shadow constants */
#define SHADOW_SIZE         8
#define SHADOW_ALPHA        80

/* Window flags */
#define WF_NEED_REDRAW      0x01
#define WF_ACTIVE           0x02
#define WF_DIRTY            0x04
#define WF_MOVING           0x08
#define WF_RESIZING         0x10

/* Maximum windows */
#define MAX_WINDOWS         64
#define MAX_WINDOW_TITLE    128

/* Region / clipping */
typedef struct region_rect {
    int x, y, w, h;
    struct region_rect *next;
} region_rect_t;

typedef struct {
    region_rect_t *rects;
    int num_rects;
    int x, y, w, h;     /* Bounding box */
} clip_region_t;

/* Forward declarations */
struct window;
struct compositor;

/* Window structure */
typedef struct window {
    uint32 id;
    char title[MAX_WINDOW_TITLE];
    int x, y;               /* Position */
    int width, height;      /* Client area size */
    int full_width, full_height; /* Including decorations */
    uint32 style;           /* Window style flags */
    window_state_t state;
    uint32 flags;
    int z_order;            /* Higher = on top */
    uint32 bgcolor;         /* Background color (ARGB) */
    uint32 *framebuffer;    /* Window content */
    uint32 fb_size;         /* Framebuffer size in bytes */
    struct window *parent;  /* Parent window (for child windows) */
    struct window *children;
    struct window *sibling; /* Next sibling */
    struct window *next;    /* Global window list */
    void *user_data;        /* Application data */
} window_t;

/* Window manager */
typedef struct {
    window_t *window_list;
    window_t *active_window;
    window_t *desktop_window;
    window_t *focused_window;
    int window_count;
    int next_id;
    int initialized;
    spinlock_t wm_lock;
    /* Desktop dimensions */
    int desktop_width;
    int desktop_height;
    /* Moving/resizing state */
    int drag_start_x;
    int drag_start_y;
    int drag_window_start_x;
    int drag_window_start_y;
    window_t *drag_window;
    /* Dirty region tracking */
    clip_region_t dirty_region;
} window_manager_t;

/* Compositor */
typedef struct compositor {
    uint32 *compose_buffer;     /* Final composed framebuffer */
    uint32 compose_width;
    uint32 compose_height;
    uint32 enable_shadows;      /* Drop shadows enabled */
    uint32 enable_animations;   /* Window animations */
    uint32 enable_transparency; /* Per-pixel alpha */
    uint32 fps_limit;           /* Max compositor FPS */
    uint64 last_compose_jiffies;
    uint32 frame_counter;
    uint32 current_fps;
    int initialized;
    spinlock_t comp_lock;
} compositor_t;

/* Desktop Shell */
typedef struct {
    uint32 *wallpaper;          /* Wallpaper bitmap */
    uint32 wallpaper_width;
    uint32 wallpaper_height;
    uint32 taskbar_height;      /* Usually 40px */
    uint32 taskbar_color;
    uint32 taskbar_accent;      /* Taskbar highlight color */
    uint32 start_menu_open;
    uint32 start_menu_x;
    uint32 start_menu_y;
    uint32 start_menu_width;
    uint32 start_menu_height;
    uint32 clock_x, clock_y;    /* Clock position on taskbar */
    char clock_str[16];         /* Time string */
    uint64 last_clock_update;
    int initialized;
    spinlock_t shell_lock;
} desktop_shell_t;

/* Virtual Desktop */
typedef struct {
    uint32 id;
    char name[32];
    window_t *windows;          /* Windows on this desktop */
    uint32 *wallpaper;          /* Per-desktop wallpaper */
    uint32 bg_color;
    int active;
} virtual_desktop_t;

#define MAX_VIRTUAL_DESKTOPS    8

typedef struct {
    virtual_desktop_t desktops[MAX_VIRTUAL_DESKTOPS];
    int current_desktop;
    int num_desktops;
    int initialized;
    spinlock_t vd_lock;
} virtual_desktop_manager_t;

/* ========== Window Manager API ========== */
void wm_init(int desktop_w, int desktop_h);
window_t *wm_create_window(const char *title, int x, int y, int w, int h, uint32 style);
void wm_destroy_window(window_t *win);
void wm_show_window(window_t *win);
void wm_hide_window(window_t *win);
void wm_minimize_window(window_t *win);
void wm_maximize_window(window_t *win);
void wm_restore_window(window_t *win);
void wm_set_active_window(window_t *win);
window_t *wm_get_active_window(void);
void wm_move_window(window_t *win, int x, int y);
void wm_resize_window(window_t *win, int w, int h);
void wm_raise_window(window_t *win);
void wm_lower_window(window_t *win);
window_t *wm_find_window_at(int x, int y);
window_t *wm_get_desktop_window(void);
void wm_invalidate_rect(int x, int y, int w, int h);
void wm_invalidate_window(window_t *win);
int wm_get_window_count(void);
void wm_get_window_list(window_t **list, int *count);
void wm_dump_windows(void);

/* Window decorations */
void wm_draw_decorations(window_t *win, uint32 *fb, int fb_w, int fb_h);
int wm_hit_test(window_t *win, int x, int y);
#define HT_CLIENT       0
#define HT_CAPTION      1
#define HT_CLOSE        2
#define HT_MINIMIZE     3
#define HT_MAXIMIZE     4
#define HT_LEFT         5
#define HT_RIGHT        6
#define HT_TOP          7
#define HT_BOTTOM       8
#define HT_TOPLEFT      9
#define HT_TOPRIGHT     10
#define HT_BOTTOMLEFT   11
#define HT_BOTTOMRIGHT  12
#define HT_NOWHERE      13

/* ========== Compositor API ========== */
void compositor_init(uint32 width, uint32 height);
void compositor_shutdown(void);
void compositor_compose(void);
void compositor_compose_rect(int x, int y, int w, int h);
void compositor_compose_window(window_t *win);
void compositor_set_shadows(int enable);
void compositor_set_transparency(int enable);
void compositor_set_fps_limit(uint32 fps);
uint32 *compositor_get_buffer(void);
int compositor_is_initialized(void);
void compositor_window_opacity(window_t *win, uint8 alpha);

/* Alpha blending */
uint32 compositor_alpha_blend(uint32 dst, uint32 src);
void compositor_blit_alpha(uint32 *dst, int dst_w, int dst_h,
                           uint32 *src, int src_w, int src_h,
                           int dx, int dy, uint8 global_alpha);
void compositor_draw_shadow(uint32 *fb, int fb_w, int fb_h,
                            int x, int y, int w, int h);

/* ========== Clipping / Region API ========== */
void clip_region_init(clip_region_t *region);
void clip_region_free(clip_region_t *region);
void clip_region_add_rect(clip_region_t *region, int x, int y, int w, int h);
void clip_region_subtract_rect(clip_region_t *region, int x, int y, int w, int h);
int clip_region_intersects(const clip_region_t *region, int x, int y, int w, int h);
void clip_region_set_rect(clip_region_t *region, int x, int y, int w, int h);
void clip_region_clear(clip_region_t *region);
void clip_region_dump(const clip_region_t *region);

/* Clip drawing operations */
void clip_draw_rect(clip_region_t *clip, uint32 *fb, int fb_w, int fb_h,
                    int x, int y, int w, int h, uint32 color);
void clip_blit(clip_region_t *clip, uint32 *dst, int dst_w, int dst_h,
               uint32 *src, int src_w, int src_h,
               int dx, int dy, int sx, int sy, int w, int h);

/* ========== Desktop Shell API ========== */
void desktop_shell_init(uint32 width, uint32 height);
void desktop_shell_draw(uint32 *fb, int fb_w, int fb_h);
void desktop_shell_draw_taskbar(uint32 *fb, int fb_w, int fb_h);
void desktop_shell_draw_start_menu(uint32 *fb, int fb_w, int fb_h);
void desktop_shell_draw_clock(uint32 *fb, int fb_w, int fb_h);
void desktop_shell_update_clock(void);
void desktop_shell_toggle_start_menu(void);
void desktop_shell_set_wallpaper(const uint32 *bitmap, uint32 w, uint32 h);
void desktop_shell_click(int x, int y);
int desktop_shell_hit_test(int x, int y);
#define DSHT_NONE       0
#define DSHT_TASKBAR    1
#define DSHT_START      2
#define DSHT_CLOCK      3
#define DSHT_WINDOW_BTN 4

/* ========== Virtual Desktop API ========== */
void vd_init(int num_desktops);
void vd_switch(int desktop_idx);
void vd_create(const char *name);
void vd_close(int desktop_idx);
void vd_move_window_to(window_t *win, int desktop_idx);
void vd_move_active_window_to(int desktop_idx);
int vd_get_current(void);
int vd_get_count(void);
const char *vd_get_name(int desktop_idx);
void vd_next(void);
void vd_prev(void);
void vd_draw_switcher(uint32 *fb, int fb_w, int fb_h);

#endif
