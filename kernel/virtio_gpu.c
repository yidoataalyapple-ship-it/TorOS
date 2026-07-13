/*
 * torOS VirtIO GPU Driver
 * 2D/3D acceleration via VirtIO GPU device
 * QEMU virtio-gpu-pci support
 */

#include "../include/toros.h"
#include "../include/gpu.h"
#include "../include/virtio.h"

/* VirtIO GPU PCI base */
#define VIRTIO_GPU_PCI_BASE     0x09002000

/* Control and cursor virtqueues */
#define VIRTIO_GPU_VQ_CTRL      0
#define VIRTIO_GPU_VQ_CURSOR    1

/* Static GPU state */
static volatile uint32 *virtio_gpu_regs = NULL;
static gpu_device_t gpu_dev;
static virtqueue_t ctrl_vq;
static virtqueue_t cursor_vq;

/* Request/response buffers */
static uint8 ctrl_request[4096] __attribute__((aligned(4096)));
static uint8 ctrl_response[4096] __attribute__((aligned(4096)));
static uint8 cursor_request[256] __attribute__((aligned(256)));
static uint8 cursor_response[256] __attribute__((aligned(256)));

/* Default cursor arrow - 32x32 ARGB */
const uint32 default_cursor_arrow[32 * 32] = {
    0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000,
    0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF, 0x00000000,
    0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFF000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000000, 0xFF000000, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF,
    0xFFFFFFFF, 0xFF000000, 0xFFFFFFFF,
    0xFF000000, 0xFFFFFFFF,
};

/* Inline register access */
static inline uint32 gpu_read(uint32 offset)
{
    return virtio_gpu_regs[offset >> 2];
}

static inline void gpu_write(uint32 offset, uint32 val)
{
    virtio_gpu_regs[offset >> 2] = val;
}

/* Send control request and wait for response */
static int send_ctrl_request(void *req, uint32 req_size, void *resp, uint32 resp_size)
{
    /* Copy request to buffer */
    memcpy(ctrl_request, req, req_size);

    /* Setup descriptor chain */
    vring_desc_t *desc = ctrl_vq.desc;
    desc[0].addr = (uint64)ctrl_request;
    desc[0].len = req_size;
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next = 1;

    desc[1].addr = (uint64)ctrl_response;
    desc[1].len = resp_size;
    desc[1].flags = VRING_DESC_F_WRITE;
    desc[1].next = 0;

    /* Add to available ring */
    ctrl_vq.avail->ring[ctrl_vq.avail->idx % ctrl_vq.queue_size] = 0;
    __sync_synchronize();
    ctrl_vq.avail->idx++;

    /* Notify */
    gpu_write(VIRTIO_PCI_QUEUE_NOTIFY, VIRTIO_GPU_VQ_CTRL);

    /* Poll for response (simplified - should use interrupts in production) */
    int timeout = 10000;
    while (timeout-- > 0) {
        if (ctrl_vq.last_used_idx != ctrl_vq.used->idx) {
            ctrl_vq.last_used_idx++;
            memcpy(resp, ctrl_response, resp_size);

            virtio_gpu_ctrl_hdr *hdr = (virtio_gpu_ctrl_hdr *)resp;
            if (hdr->type == VIRTIO_GPU_RESP_OK_NODATA ||
                hdr->type == VIRTIO_GPU_RESP_OK_DISPLAY_INFO ||
                hdr->type == VIRTIO_GPU_RESP_OK_EDID)
                return 0;
            return -1;
        }
        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
    }

    return -1;  /* Timeout */
}

/* Send cursor request */
static int send_cursor_request(void *req, uint32 req_size)
{
    memcpy(cursor_request, req, req_size);

    vring_desc_t *desc = cursor_vq.desc;
    desc[0].addr = (uint64)cursor_request;
    desc[0].len = req_size;
    desc[0].flags = 0;
    desc[0].next = 0;

    cursor_vq.avail->ring[cursor_vq.avail->idx % cursor_vq.queue_size] = 0;
    __sync_synchronize();
    cursor_vq.avail->idx++;

    gpu_write(VIRTIO_PCI_QUEUE_NOTIFY, VIRTIO_GPU_VQ_CURSOR);

    int timeout = 10000;
    while (timeout-- > 0) {
        if (cursor_vq.last_used_idx != cursor_vq.used->idx) {
            cursor_vq.last_used_idx++;
            return 0;
        }
        for (volatile int i = 0; i < 100; i++);
    }

    return -1;
}

/* ==================== VirtIO GPU API ==================== */

void virtio_gpu_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] VirtIO GPU...\n");

    virtio_gpu_regs = (volatile uint32 *)VIRTIO_GPU_PCI_BASE;

    /* Reset */
    gpu_write(VIRTIO_PCI_STATUS, 0);

    /* Acknowledge */
    gpu_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE);

    /* Driver present */
    gpu_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    /* Negotiate features */
    uint32 host_features = gpu_read(VIRTIO_PCI_HOST_FEATURES);
    uint32 guest_features = host_features & ~(1 << VIRTIO_F_VERSION_1);
    gpu_write(VIRTIO_PCI_GUEST_FEATURES, guest_features);

    /* Features OK */
    gpu_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE |
               VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);

    /* Setup control queue */
    gpu_write(VIRTIO_PCI_QUEUE_SEL, VIRTIO_GPU_VQ_CTRL);
    uint32 ctrl_qsize = gpu_read(VIRTIO_PCI_QUEUE_NUM);
    if (ctrl_qsize > 16) ctrl_qsize = 16;

    ctrl_vq.queue_size = ctrl_qsize;
    ctrl_vq.queue_index = VIRTIO_GPU_VQ_CTRL;
    ctrl_vq.last_used_idx = 0;

    void *ctrl_pages = page_alloc();
    if (!ctrl_pages) {
        printk_color(TERM_RED, "[VIRTIO-GPU] Failed to alloc control queue\n");
        return;
    }
    memset(ctrl_pages, 0, PAGE_SIZE);

    ctrl_vq.desc = (vring_desc_t *)ctrl_pages;
    ctrl_vq.avail = (vring_avail_t *)((uint8 *)ctrl_pages + sizeof(vring_desc_t) * ctrl_qsize);
    ctrl_vq.used = (vring_used_t *)((uint8 *)ctrl_pages + PAGE_SIZE / 2);
    ctrl_vq.queue_pages = ctrl_pages;

    gpu_write(VIRTIO_PCI_QUEUE_PFN, (uint32)((uint64)ctrl_pages >> 12));

    /* Setup cursor queue */
    gpu_write(VIRTIO_PCI_QUEUE_SEL, VIRTIO_GPU_VQ_CURSOR);
    uint32 cur_qsize = gpu_read(VIRTIO_PCI_QUEUE_NUM);
    if (cur_qsize > 4) cur_qsize = 4;

    cursor_vq.queue_size = cur_qsize;
    cursor_vq.queue_index = VIRTIO_GPU_VQ_CURSOR;
    cursor_vq.last_used_idx = 0;

    void *cur_pages = page_alloc();
    if (!cur_pages) {
        printk_color(TERM_RED, "[VIRTIO-GPU] Failed to alloc cursor queue\n");
        return;
    }
    memset(cur_pages, 0, PAGE_SIZE);

    cursor_vq.desc = (vring_desc_t *)cur_pages;
    cursor_vq.avail = (vring_avail_t *)((uint8 *)cur_pages + sizeof(vring_desc_t) * cur_qsize);
    cursor_vq.used = (vring_used_t *)((uint8 *)cur_pages + PAGE_SIZE / 2);
    cursor_vq.queue_pages = cur_pages;

    gpu_write(VIRTIO_PCI_QUEUE_PFN, (uint32)((uint64)cur_pages >> 12));

    /* DRIVER_OK */
    gpu_write(VIRTIO_PCI_STATUS, VIRTIO_STATUS_ACKNOWLEDGE |
               VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK |
               VIRTIO_STATUS_DRIVER_OK);

    /* Initialize GPU device state */
    memset(&gpu_dev, 0, sizeof(gpu_device_t));
    spin_init(&gpu_dev.gpu_lock);
    gpu_dev.next_resource_id = 1;
    gpu_dev.scanout_id = 0;
    gpu_dev.virtio_initialized = 1;

    /* Get display info */
    display_mode_t mode;
    if (virtio_gpu_get_display_info(&mode) == 0) {
        gpu_dev.display = mode;
        printk_color(TERM_GREEN, "[VIRTIO-GPU] Display: %dx%d @ %d bpp\n",
                     mode.width, mode.height, mode.bpp);
    } else {
        /* Fallback */
        gpu_dev.display.width = FB_WIDTH;
        gpu_dev.display.height = FB_HEIGHT;
        gpu_dev.display.bpp = 32;
        gpu_dev.display.pitch = FB_WIDTH * 4;
        printk_color(TERM_YELLOW, "[VIRTIO-GPU] Using fallback display\n");
    }

    printk_color(TERM_GREEN, "[BOOT] VirtIO GPU ready\n");
}

int virtio_gpu_get_display_info(display_mode_t *mode)
{
    if (!mode || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_ctrl_hdr req;
    virtio_gpu_display_info resp;

    memset(&req, 0, sizeof(req));
    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    if (send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp)) < 0)
        return -1;

    /* Find first enabled display */
    for (int i = 0; i < 16; i++) {
        if (resp.pmodes[i].enabled) {
            mode->width = resp.pmodes[i].r.width;
            mode->height = resp.pmodes[i].r.height;
            mode->bpp = 32;
            mode->pitch = mode->width * 4;
            mode->enabled = 1;
            mode->rect = resp.pmodes[i].r;
            return 0;
        }
    }

    return -1;
}

gpu_resource_t *virtio_gpu_resource_create_2d(uint32 width, uint32 height, uint32 format)
{
    if (!gpu_dev.virtio_initialized)
        return NULL;

    spin_lock(&gpu_dev.gpu_lock);

    uint32 res_id = gpu_dev.next_resource_id++;

    virtio_gpu_resource_create_2d req;
    virtio_gpu_ctrl_hdr resp;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.resource_id = res_id;
    req.format = format;
    req.width = width;
    req.height = height;

    if (send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp)) < 0) {
        spin_unlock(&gpu_dev.gpu_lock);
        return NULL;
    }

    /* Create resource struct */
    gpu_resource_t *res = (gpu_resource_t *)kmalloc(sizeof(gpu_resource_t));
    if (!res) {
        spin_unlock(&gpu_dev.gpu_lock);
        return NULL;
    }

    memset(res, 0, sizeof(gpu_resource_t));
    res->id = res_id;
    res->format = format;
    res->width = width;
    res->height = height;
    res->stride = width * 4;
    res->size = width * height * 4;

    /* Add to resource list */
    res->next = gpu_dev.resources;
    gpu_dev.resources = res;

    spin_unlock(&gpu_dev.gpu_lock);

    printk_color(TERM_CYAN, "[VIRTIO-GPU] Resource #%d: %dx%d fmt=%d\n",
                 res_id, width, height, format);

    return res;
}

void virtio_gpu_resource_unref(gpu_resource_t *res)
{
    if (!res || !gpu_dev.virtio_initialized)
        return;

    spin_lock(&gpu_dev.gpu_lock);

    virtio_gpu_resource_unref req;
    virtio_gpu_ctrl_hdr resp;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    req.resource_id = res->id;

    send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp));

    /* Remove from list */
    gpu_resource_t **pp = &gpu_dev.resources;
    while (*pp) {
        if (*pp == res) {
            *pp = res->next;
            break;
        }
        pp = &(*pp)->next;
    }

    spin_unlock(&gpu_dev.gpu_lock);

    printk_color(TERM_CYAN, "[VIRTIO-GPU] Resource #%d unref\n", res->id);
}

int virtio_gpu_resource_attach_backing(gpu_resource_t *res, void *data, uint32 size)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    uint32 num_entries = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32 req_size = sizeof(virtio_gpu_resource_attach_backing) +
                      sizeof(virtio_gpu_mem_entry) * num_entries;

    uint8 *req_buf = (uint8 *)kmalloc(req_size);
    if (!req_buf)
        return -1;

    virtio_gpu_resource_attach_backing *req = (virtio_gpu_resource_attach_backing *)req_buf;
    virtio_gpu_mem_entry *entries = (virtio_gpu_mem_entry *)(req_buf + sizeof(virtio_gpu_resource_attach_backing));

    memset(req_buf, 0, req_size);
    req->hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req->resource_id = res->id;
    req->nr_entries = num_entries;

    uintptr addr = (uintptr)data;
    for (uint32 i = 0; i < num_entries; i++) {
        entries[i].addr = addr + i * PAGE_SIZE;
        entries[i].length = (size < PAGE_SIZE) ? size : PAGE_SIZE;
        size -= entries[i].length;
    }

    virtio_gpu_ctrl_hdr resp;
    int ret = send_ctrl_request(req_buf, req_size, &resp, sizeof(resp));

    if (ret == 0)
        res->attached = 1;

    return ret;
}

int virtio_gpu_transfer_to_host_2d(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_transfer_to_host_2d req;
    virtio_gpu_ctrl_hdr resp;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.offset = 0;
    req.resource_id = res->id;

    return send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp));
}

int virtio_gpu_set_scanout(uint32 scanout_id, gpu_resource_t *res)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_set_scanout req;
    virtio_gpu_ctrl_hdr resp;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.r.x = 0;
    req.r.y = 0;
    req.r.width = res->width;
    req.r.height = res->height;
    req.scanout_id = scanout_id;
    req.resource_id = res->id;

    return send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp));
}

int virtio_gpu_resource_flush(gpu_resource_t *res, uint32 x, uint32 y, uint32 w, uint32 h)
{
    if (!res || !gpu_dev.virtio_initialized)
        return -1;

    virtio_gpu_resource_flush req;
    virtio_gpu_ctrl_hdr resp;

    memset(&req, 0, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.r.x = x;
    req.r.y = y;
    req.r.width = w;
    req.r.height = h;
    req.resource_id = res->id;

    return send_ctrl_request(&req, sizeof(req), &resp, sizeof(resp));
}

void virtio_gpu_poll(void)
{
    /* Check for async responses or interrupts */
    if (!gpu_dev.virtio_initialized)
        return;

    /* Process used buffers */
    while (ctrl_vq.last_used_idx != ctrl_vq.used->idx) {
        ctrl_vq.last_used_idx++;
    }
}

/* ==================== GPU Subsystem API ==================== */

void gpu_subsystem_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] GPU Subsystem...\n");

    virtio_gpu_init();

    printk_color(TERM_GREEN, "[BOOT] GPU subsystem ready\n");
}

void gpu_get_display_info(display_mode_t *info)
{
    if (!info)
        return;
    *info = gpu_dev.display;
}

int gpu_resource_create(gpu_resource_t **res, uint32 w, uint32 h, uint32 fmt)
{
    if (!res)
        return -1;
    *res = virtio_gpu_resource_create_2d(w, h, fmt);
    return (*res) ? 0 : -1;
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
