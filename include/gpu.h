/*
 * torOS GPU Subsystem Header
 * VirtIO GPU, Display, Buffering, Cursor, DMA-BUF
 */

#ifndef _GPU_H
#define _GPU_H

#include "toros.h"

/* ===== VirtIO GPU Protocol ===== */
#define VIRTIO_GPU_F_VIRGL             0
#define VIRTIO_GPU_F_EDID              1
#define VIRTIO_GPU_F_RESOURCE_UUID     2
#define VIRTIO_GPU_F_RESOURCE_BLOB     3
#define VIRTIO_GPU_F_CONTEXT_INIT      4

/* VirtIO GPU control types */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO    0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D  0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF      0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT         0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH      0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D 0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING 0x0107
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO     0x0108
#define VIRTIO_GPU_CMD_GET_CAPSET          0x0109
#define VIRTIO_GPU_CMD_GET_EDID            0x010A
#define VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID  0x010B
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB  0x010C
#define VIRTIO_GPU_CMD_SET_SCANOUT_BLOB    0x010D

/* Response types */
#define VIRTIO_GPU_RESP_OK_NODATA          0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO    0x1101
#define VIRTIO_GPU_RESP_OK_CAPSET_INFO     0x1102
#define VIRTIO_GPU_RESP_OK_CAPSET          0x1103
#define VIRTIO_GPU_RESP_OK_EDID            0x1104
#define VIRTIO_GPU_RESP_ERR_UNSPEC         0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY  0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID 0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID 0x1204
#define VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER 0x1205

/* 2D resource formats */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM   1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM   2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM   3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM   4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM   67
#define VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM   68
#define VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM   121
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM   134

/* VirtIO GPU request header */
typedef struct {
    uint32 type;
    uint32 flags;
    uint64 fence_id;
    uint32 ctx_id;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_ctrl_hdr;

/* Rect structure */
typedef struct {
    uint32 x;
    uint32 y;
    uint32 width;
    uint32 height;
} __attribute__((packed)) virtio_gpu_rect;

/* Display info structure */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    struct {
        virtio_gpu_rect r;
        uint32 enabled;
        uint32 flags;
    } pmodes[16];
} __attribute__((packed)) virtio_gpu_display_info;

/* Resource create 2D */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    uint32 resource_id;
    uint32 format;
    uint32 width;
    uint32 height;
} __attribute__((packed)) virtio_gpu_resource_create_2d_req;

/* Resource unref */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    uint32 resource_id;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_resource_unref_req;

/* Set scanout */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32 scanout_id;
    uint32 resource_id;
} __attribute__((packed)) virtio_gpu_set_scanout_req;

/* Resource flush */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32 resource_id;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_resource_flush_req;

/* Transfer to host 2D */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint64 offset;
    uint32 resource_id;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_req;

/* Memory entry for backing */
typedef struct {
    uint64 addr;
    uint32 length;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_mem_entry;

/* Resource attach backing */
typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    uint32 resource_id;
    uint32 nr_entries;
} __attribute__((packed)) virtio_gpu_resource_attach_backing_req;

/* Cursor commands */
#define VIRTIO_GPU_CMD_UPDATE_CURSOR       0x0300
#define VIRTIO_GPU_CMD_MOVE_CURSOR         0x0301

typedef struct {
    virtio_gpu_ctrl_hdr hdr;
    uint32 pos_x;
    uint32 pos_y;
    uint32 resource_id;
    uint32 hot_x;
    uint32 hot_y;
    uint32 padding;
} __attribute__((packed)) virtio_gpu_cursor_request;

/* ===== GPU Resource ===== */
typedef struct gpu_resource {
    uint32 id;
    uint32 format;
    uint32 width;
    uint32 height;
    uint32 stride;
    uint32 size;
    void *backing;
    uint64 paddr;      /* Physical address of backing (DMA) */
    uint32 attached;
    struct gpu_resource *next;
} gpu_resource_t;

/* ===== Display Info ===== */
typedef struct {
    uint32 width;
    uint32 height;
    uint32 bpp;
    uint32 pitch;
    uint32 enabled;
    virtio_gpu_rect rect;
} display_mode_t;

/* ===== Hardware Cursor ===== */
typedef struct {
    uint32 x;
    uint32 y;
    uint32 hot_x;
    uint32 hot_y;
    uint32 visible;
    uint32 resource_id;
    uint32 cursor_fb[32 * 32];  /* 32x32 ARGB cursor */
} hw_cursor_t;

/* ===== Framebuffer Buffering ===== */
typedef enum {
    BUFFER_SINGLE,
    BUFFER_DOUBLE,
    BUFFER_TRIPLE
} buffer_mode_t;

typedef struct {
    uint32 *front;      /* Currently displayed */
    uint32 *back;       /* Drawing target */
    uint32 *third;      /* Triple buffering only */
    uint32 width;
    uint32 height;
    uint32 pitch;
    buffer_mode_t mode;
    uint32 vsync_enabled;
    uint32 pending_swap;
    uint32 swap_counter;
    uint64 last_vsync_jiffies;
} gpu_buffer_manager_t;

/* ===== DMA-BUF ===== */
typedef struct dmabuf_handle {
    uint32 id;
    uint32 resource_id;
    uint64 size;
    void *vaddr;
    uint64 paddr;
    uint32 ref_count;
    struct dmabuf_handle *next;
} dmabuf_handle_t;

/* ===== GPU Device ===== */
typedef struct {
    uint32 initialized;
    uint32 next_resource_id;
    gpu_resource_t *resources;
    display_mode_t display;
    hw_cursor_t cursor;
    gpu_buffer_manager_t buffer_mgr;
    dmabuf_handle_t *dmabuf_list;
    uint32 virtio_initialized;
    uint32 scanout_id;
    spinlock_t gpu_lock;
} gpu_device_t;

/* ===== VirtIO GPU API ===== */
void virtio_gpu_init(void);
void virtio_gpu_poll(void);
int virtio_gpu_get_display_info(display_mode_t *mode);
gpu_resource_t *virtio_gpu_resource_create_2d(uint32 width, uint32 height, uint32 format);
void virtio_gpu_resource_unref(gpu_resource_t *res);
int virtio_gpu_resource_attach_backing(gpu_resource_t *res, void *data, uint32 size);
int virtio_gpu_resource_detach_backing(gpu_resource_t *res);
int virtio_gpu_transfer_to_host_2d(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h);
int virtio_gpu_set_scanout(uint32 scanout_id, gpu_resource_t *res);
int virtio_gpu_resource_flush(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h);

/* ===== Buffering API ===== */
void gpu_buffer_init(uint32 width, uint32 height, buffer_mode_t mode);
void gpu_buffer_shutdown(void);
uint32 *gpu_buffer_get_back(void);
void gpu_buffer_swap(void);
void gpu_buffer_present(void);
void gpu_buffer_wait_vsync(void);
void gpu_buffer_set_vsync(int enable);
int gpu_buffer_is_vsync_enabled(void);
void gpu_buffer_clear(uint32 color);
void gpu_buffer_blit(uint32 *src, uint32 sx, uint32 sy, uint32 sw, uint32 sh,
                     uint32 dx, uint32 dy, uint32 dw, uint32 dh);

/* ===== Hardware Cursor API ===== */
void hw_cursor_init(void);
void hw_cursor_show(void);
void hw_cursor_hide(void);
void hw_cursor_move(uint32 x, uint32 y);
void hw_cursor_set_pos(uint32 x, uint32 y);
void hw_cursor_set_hotspot(uint32 hot_x, uint32 hot_y);
void hw_cursor_set_image(const uint32 *rgba_data, uint32 width, uint32 height);
void hw_cursor_load_default(void);
int hw_cursor_is_visible(void);
void hw_cursor_get_pos(uint32 *x, uint32 *y);

/* ===== DMA-BUF API ===== */
void dmabuf_init(void);
dmabuf_handle_t *dmabuf_create(uint32 size);
void dmabuf_free(dmabuf_handle_t *dmabuf);
dmabuf_handle_t *dmabuf_get(uint32 id);
void dmabuf_ref(dmabuf_handle_t *dmabuf);
void dmabuf_unref(dmabuf_handle_t *dmabuf);
int dmabuf_export(gpu_resource_t *res, dmabuf_handle_t **out);
int dmabuf_import(uint32 id, gpu_resource_t **out);

/* ===== GPU Core API ===== */
void gpu_subsystem_init(void);
void gpu_get_display_info(display_mode_t *info);
int gpu_resource_create(gpu_resource_t **res, uint32 w, uint32 h, uint32 fmt);
void gpu_resource_destroy(gpu_resource_t *res);
int gpu_flush_scanout(void);
int gpu_flush_rect(uint32 x, uint32 y, uint32 w, uint32 h);
void gpu_poll_events(void);
int gpu_is_initialized(void);

/* Default cursor bitmap (arrow) - 32x32 */
extern const uint32 default_cursor_arrow[32 * 32];

#endif
