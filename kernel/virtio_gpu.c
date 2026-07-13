/*
 * torOS VirtIO GPU Driver
 * Display output, 2D resources, scanout, hardware cursor
 */

#include "../include/toros.h"
#include "../include/gpu.h"
#include "../include/virtio.h"

/* Static GPU device */
static gpu_device_t gpu_dev;

/* Default arrow cursor (32x32, simplified) */
const uint32 default_cursor_arrow[32 * 32] = {
    [0 ... 31] = 0xFF000000,
};

/* VirtIO GPU queue indices */
#define GPU_CONTROLQ    0
#define GPU_CURSORQ     1

/* Send a control request and wait for response */
static int send_ctrl_request(void *req, uint32 req_size, void *resp, uint32 resp_size)
{
    if (!gpu_dev.virtio_initialized)
        return -1;

    /* Simple synchronous request */
    /* In production, this would use proper VirtIO queue management */
    (void)req; (void)req_size; (void)resp; (void)resp_size;
    return 0;
}

/* Send cursor request */
static int send_cursor_request(void *req, uint32 size)
{
    (void)req; (void)size;
    return 0;
}

void virtio_gpu_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] VirtIO GPU...\n");

    memset(&gpu_dev, 0, sizeof(gpu_device_t));
    spin_init(&gpu_dev.gpu_lock);

    gpu_dev.next_resource_id = 1;
    gpu_dev.scanout_id = 0;

    /* Try to find virtio-gpu device on PCI */
    /* QEMU virt: virtio-gpu-pci at specific address */
    /* For now, mark as initialized with fallback */
    gpu_dev.virtio_initialized = 0;  /* Will be 1 if device found */

    /* Setup default display mode */
    gpu_dev.display.width = FB_WIDTH;
    gpu_dev.display.height = FB_HEIGHT;
    gpu_dev.display.bpp = 32;
    gpu_dev.display.pitch = FB_WIDTH * 4;
    gpu_dev.display.enabled = 1;
    gpu_dev.display.rect.x = 0;
    gpu_dev.display.rect.y = 0;
    gpu_dev.display.rect.width = FB_WIDTH;
    gpu_dev.display.rect.height = FB_HEIGHT;

    /* Initialize hardware cursor */
    hw_cursor_init();

    /* Initialize buffer manager with double buffering */
    gpu_buffer_init(FB_WIDTH, FB_HEIGHT, BUFFER_DOUBLE);

    /* Initialize DMA-BUF subsystem */
    dmabuf_init();

    printk_color(TERM_GREEN, "[BOOT] VirtIO GPU: %dx%d@%d (fallback mode)\n",
                 gpu_dev.display.width, gpu_dev.display.height, gpu_dev.display.bpp);
}

void virtio_gpu_poll(void)
{
    /* Poll for GPU events */
}

int virtio_gpu_get_display_info(display_mode_t *mode)
{
    if (!mode)
        return -1;

    memcpy(mode, &gpu_dev.display, sizeof(display_mode_t));
    return 0;
}

/* Create a 2D resource */
gpu_resource_t *virtio_gpu_resource_create_2d(uint32 width, uint32 height, uint32 format)
{
    gpu_resource_t *res = (gpu_resource_t *)kmalloc(sizeof(gpu_resource_t));
    if (!res)
        return NULL;

    memset(res, 0, sizeof(gpu_resource_t));
    res->id = gpu_dev.next_resource_id++;
    res->width = width;
    res->height = height;
    res->format = format;
    res->stride = width * 4;
    res->size = res->stride * height;

    /* Allocate backing store */
    res->backing = kmalloc(res->size);
    if (!res->backing) {
        kfree(res);
        return NULL;
    }
    memset(res->backing, 0, res->size);
    res->attached = 1;

    /* Send create command to device */
    if (gpu_dev.virtio_initialized) {
        virtio_gpu_resource_create_2d_req req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
        req.resource_id = res->id;
        req.format = format;
        req.width = width;
        req.height = height;

        send_ctrl_request(&req, sizeof(req), NULL, 0);
    }

    /* Link into resource list */
    res->next = gpu_dev.resources;
    gpu_dev.resources = res;

    return res;
}

void virtio_gpu_resource_unref(gpu_resource_t *res)
{
    if (!res)
        return;

    /* Send unref command */
    if (gpu_dev.virtio_initialized) {
        virtio_gpu_resource_unref_req req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
        req.resource_id = res->id;

        send_ctrl_request(&req, sizeof(req), NULL, 0);
    }

    /* Remove from list */
    gpu_resource_t **pp = &gpu_dev.resources;
    while (*pp) {
        if (*pp == res) {
            *pp = res->next;
            break;
        }
        pp = &(*pp)->next;
    }

    if (res->backing)
        kfree(res->backing);
    kfree(res);
}

int virtio_gpu_resource_attach_backing(gpu_resource_t *res, void *data, uint32 size)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    res->paddr = (uint64)(uintptr)data;

    /* Build attach backing request */
    uint32 req_size = sizeof(virtio_gpu_resource_attach_backing_req) + sizeof(virtio_gpu_mem_entry);
    uint8 *req_buf = (uint8 *)kmalloc(req_size);
    if (!req_buf)
        return -1;

    virtio_gpu_resource_attach_backing_req *req = (virtio_gpu_resource_attach_backing_req *)req_buf;
    memset(req, 0, req_size);
    req->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req->resource_id = res->id;
    req->nr_entries = 1;

    virtio_gpu_mem_entry *entry = (virtio_gpu_mem_entry *)(req_buf + sizeof(virtio_gpu_resource_attach_backing_req));
    entry->addr = (uint64)(uintptr)data;
    entry->length = size;

    send_ctrl_request(req_buf, req_size, NULL, 0);
    kfree(req_buf);

    res->backing = data;
    res->attached = 1;
    return 0;
}

int virtio_gpu_resource_detach_backing(gpu_resource_t *res)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_ctrl_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.type = VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING;

    send_ctrl_request(&hdr, sizeof(hdr), NULL, 0);
    res->attached = 0;
    return 0;
}

int virtio_gpu_transfer_to_host_2d(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_transfer_to_host_2d_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.offset = 0;
    req.resource_id = res->id;

    return send_ctrl_request(&req, sizeof(req), NULL, 0);
}

int virtio_gpu_set_scanout(uint32 scanout_id, gpu_resource_t *res)
{
    if (!gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_set_scanout_req req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.r.x = 0;
    req.r.y = 0;
    req.r.width = res ? res->width : 0;
    req.r.height = res ? res->height : 0;
    req.scanout_id = scanout_id;
    req.resource_id = res ? res->id : 0;

    return send_ctrl_request(&req, sizeof(req), NULL, 0);
}

int virtio_gpu_resource_flush(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h)
{
    if (!res)
        return -1;

    /* Transfer then flush */
    virtio_gpu_transfer_to_host_2d(res, x, y, w, h);

    if (gpu_dev.virtio_initialized) {
        virtio_gpu_resource_flush_req req;
        memset(&req, 0, sizeof(req));
        req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
        req.r.x = x;
        req.r.y = y;
        req.r.width = w;
        req.r.height = h;
        req.resource_id = res->id;

        return send_ctrl_request(&req, sizeof(req), NULL, 0);
    }

    /* Fallback: copy to framebuffer */
    if (res->backing) {
        extern void fb_present(const uint32 *src);
        fb_present((const uint32 *)res->backing);
    }
    return 0;
}

/* ======== GPU Core API ======== */

void gpu_subsystem_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] GPU Subsystem...\n");
    virtio_gpu_init();
    printk_color(TERM_GREEN, "[BOOT] GPU Subsystem ready\n");
}

void gpu_get_display_info(display_mode_t *info)
{
    virtio_gpu_get_display_info(info);
}

int gpu_resource_create(gpu_resource_t **res, uint32 w, uint32 h, uint32 fmt)
{
    if (!res)
        return -1;
    *res = virtio_gpu_resource_create_2d(w, h, fmt);
    return *res ? 0 : -1;
}

void gpu_resource_destroy(gpu_resource_t *res)
{
    virtio_gpu_resource_unref(res);
}

int gpu_flush_scanout(void)
{
    if (gpu_dev.resources) {
        return virtio_gpu_resource_flush(gpu_dev.resources,
                                          0, 0,
                                          gpu_dev.display.width,
                                          gpu_dev.display.height);
    }
    return -1;
}

int gpu_flush_rect(uint32 x, uint32 y, uint32 w, uint32 h)
{
    if (gpu_dev.resources) {
        return virtio_gpu_resource_flush(gpu_dev.resources, x, y, w, h);
    }
    return -1;
}

void gpu_poll_events(void)
{
    virtio_gpu_poll();
}

int gpu_is_initialized(void)
{
    return gpu_dev.virtio_initialized;
}

/* ======== Hardware Cursor via VirtIO GPU ======== */

void hw_cursor_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Hardware Cursor...\n");

    memset(&gpu_dev.cursor, 0, sizeof(hw_cursor_t));
    gpu_dev.cursor.visible = 1;
    gpu_dev.cursor.hot_x = 0;
    gpu_dev.cursor.hot_y = 0;

    hw_cursor_load_default();

    printk_color(TERM_GREEN, "[BOOT] Hardware Cursor ready\n");
}

void hw_cursor_show(void)
{
    gpu_dev.cursor.visible = 1;

    /* Send update to GPU */
    virtio_gpu_cursor_request req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    req.pos_x = gpu_dev.cursor.x;
    req.pos_y = gpu_dev.cursor.y;
    req.resource_id = gpu_dev.cursor.resource_id;
    req.hot_x = gpu_dev.cursor.hot_x;
    req.hot_y = gpu_dev.cursor.hot_y;

    send_cursor_request(&req, sizeof(req));
}

void hw_cursor_hide(void)
{
    gpu_dev.cursor.visible = 0;

    virtio_gpu_cursor_request req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    req.pos_x = 0;
    req.pos_y = 0;
    req.resource_id = 0;

    send_cursor_request(&req, sizeof(req));
}

void hw_cursor_move(uint32 x, uint32 y)
{
    gpu_dev.cursor.x = x;
    gpu_dev.cursor.y = y;

    virtio_gpu_cursor_request req;
    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_MOVE_CURSOR;
    req.pos_x = x;
    req.pos_y = y;

    send_cursor_request(&req, sizeof(req));
}

void hw_cursor_set_pos(uint32 x, uint32 y)
{
    hw_cursor_move(x, y);
}

void hw_cursor_set_hotspot(uint32 hot_x, uint32 hot_y)
{
    gpu_dev.cursor.hot_x = hot_x;
    gpu_dev.cursor.hot_y = hot_y;
}

void hw_cursor_set_image(const uint32 *rgba_data, uint32 width, uint32 height)
{
    if (!rgba_data || width > 32 || height > 32)
        return;

    /* Copy to cursor buffer */
    for (uint32 i = 0; i < height; i++) {
        for (uint32 j = 0; j < width; j++) {
            gpu_dev.cursor.cursor_fb[i * 32 + j] = rgba_data[i * width + j];
        }
    }
}

void hw_cursor_load_default(void)
{
    /* Load default arrow cursor */
    for (int i = 0; i < 32 * 32; i++) {
        gpu_dev.cursor.cursor_fb[i] = default_cursor_arrow[i % 32];
    }
}

int hw_cursor_is_visible(void)
{
    return gpu_dev.cursor.visible;
}

void hw_cursor_get_pos(uint32 *x, uint32 *y)
{
    if (x) *x = gpu_dev.cursor.x;
    if (y) *y = gpu_dev.cursor.y;
}
