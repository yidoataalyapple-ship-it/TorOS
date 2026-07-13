/*
 * torOS DMA-BUF Subsystem
 * Zero-copy GPU <-> CPU framebuffer sharing
 */

#include "../include/toros.h"
#include "../include/gpu.h"

static dmabuf_handle_t *dmabuf_list = NULL;
static uint32 next_dmabuf_id = 1;
static spinlock_t dmabuf_lock;
static int dmabuf_initialized = 0;

void dmabuf_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] DMA-BUF...\n");

    dmabuf_list = NULL;
    next_dmabuf_id = 1;
    spin_init(&dmabuf_lock);
    dmabuf_initialized = 1;

    printk_color(TERM_GREEN, "[BOOT] DMA-BUF ready\n");
}

dmabuf_handle_t *dmabuf_create(uint32 size)
{
    if (!dmabuf_initialized || size == 0)
        return NULL;

    spin_lock(&dmabuf_lock);

    /* Allocate DMA-BUF handle */
    dmabuf_handle_t *dmabuf = (dmabuf_handle_t *)kmalloc(sizeof(dmabuf_handle_t));
    if (!dmabuf) {
        spin_unlock(&dmabuf_lock);
        return NULL;
    }

    /* Allocate physical pages */
    uint32 num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *pages = page_alloc();
    if (!pages) {
        kfree(dmabuf);
        spin_unlock(&dmabuf_lock);
        return NULL;
    }

    memset(pages, 0, num_pages * PAGE_SIZE);

    dmabuf->id = next_dmabuf_id++;
    dmabuf->resource_id = 0;
    dmabuf->size = size;
    dmabuf->vaddr = pages;
    dmabuf->paddr = (uint64)pages;  /* Identity mapped */
    dmabuf->ref_count = 1;
    dmabuf->next = dmabuf_list;
    dmabuf_list = dmabuf;

    spin_unlock(&dmabuf_lock);

    printk_color(TERM_CYAN, "[DMA-BUF] Created #%d: %d bytes @ %p\n",
                 dmabuf->id, size, pages);

    return dmabuf;
}

void dmabuf_free(dmabuf_handle_t *dmabuf)
{
    if (!dmabuf || !dmabuf_initialized)
        return;

    spin_lock(&dmabuf_lock);

    /* Remove from list */
    dmabuf_handle_t **pp = &dmabuf_list;
    while (*pp) {
        if (*pp == dmabuf) {
            *pp = dmabuf->next;
            break;
        }
        pp = &(*pp)->next;
    }

    /* Free pages */
    if (dmabuf->vaddr) {
        page_free(dmabuf->vaddr);
    }

    /* Free handle */
    kfree(dmabuf);

    spin_unlock(&dmabuf_lock);
}

dmabuf_handle_t *dmabuf_get(uint32 id)
{
    if (!dmabuf_initialized)
        return NULL;

    spin_lock(&dmabuf_lock);

    dmabuf_handle_t *d = dmabuf_list;
    while (d) {
        if (d->id == id) {
            spin_unlock(&dmabuf_lock);
            return d;
        }
        d = d->next;
    }

    spin_unlock(&dmabuf_lock);
    return NULL;
}

void dmabuf_ref(dmabuf_handle_t *dmabuf)
{
    if (!dmabuf || !dmabuf_initialized)
        return;

    spin_lock(&dmabuf_lock);
    dmabuf->ref_count++;
    spin_unlock(&dmabuf_lock);
}

void dmabuf_unref(dmabuf_handle_t *dmabuf)
{
    if (!dmabuf || !dmabuf_initialized)
        return;

    spin_lock(&dmabuf_lock);
    dmabuf->ref_count--;
    int should_free = (dmabuf->ref_count <= 0);
    spin_unlock(&dmabuf_lock);

    if (should_free) {
        dmabuf_free(dmabuf);
    }
}

/* Export a GPU resource as DMA-BUF */
int dmabuf_export(gpu_resource_t *res, dmabuf_handle_t **out)
{
    if (!res || !out || !dmabuf_initialized)
        return -1;

    dmabuf_handle_t *dmabuf = dmabuf_create(res->size);
    if (!dmabuf)
        return -1;

    dmabuf->resource_id = res->id;

    /* Map resource memory to DMA-BUF */
    if (res->backing) {
        memcpy(dmabuf->vaddr, res->backing, res->size);
    }

    *out = dmabuf;

    printk_color(TERM_CYAN, "[DMA-BUF] Exported resource #%d as DMA-BUF #%d\n",
                 res->id, dmabuf->id);

    return 0;
}

/* Import a DMA-BUF as GPU resource */
int dmabuf_import(uint32 id, gpu_resource_t **out)
{
    if (!out || !dmabuf_initialized)
        return -1;

    dmabuf_handle_t *dmabuf = dmabuf_get(id);
    if (!dmabuf)
        return -1;

    /* Create GPU resource pointing to DMA-BUF memory */
    gpu_resource_t *res = (gpu_resource_t *)kmalloc(sizeof(gpu_resource_t));
    if (!res)
        return -1;

    memset(res, 0, sizeof(gpu_resource_t));
    res->id = dmabuf->resource_id;
    res->width = FB_WIDTH;
    res->height = FB_HEIGHT;
    res->format = VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM;
    res->stride = FB_WIDTH * 4;
    res->size = dmabuf->size;
    res->backing = dmabuf->vaddr;
    res->attached = 1;

    *out = res;

    printk_color(TERM_CYAN, "[DMA-BUF] Imported DMA-BUF #%d as resource\n", id);

    return 0;
}

/* Get DMA-BUF list for debugging */
void dmabuf_list_all(void)
{
    printk_color(TERM_CYAN, "\n=== DMA-BUF Handles ===\n");

    spin_lock(&dmabuf_lock);

    dmabuf_handle_t *d = dmabuf_list;
    int count = 0;

    while (d) {
        printk("  #%d: res=%d, size=%d, refs=%d, vaddr=%p\n",
               d->id, d->resource_id, d->size, d->ref_count, d->vaddr);
        count++;
        d = d->next;
    }

    printk_color(TERM_CYAN, "  Total: %d handles\n\n", count);

    spin_unlock(&dmabuf_lock);
}

/* Attach DMA-BUF to GPU resource for zero-copy */
int dmabuf_attach_resource(dmabuf_handle_t *dmabuf, gpu_resource_t *res)
{
    if (!dmabuf || !res)
        return -1;

    /* Point GPU resource to DMA-BUF memory */
    res->backing = dmabuf->vaddr;
    res->paddr = dmabuf->paddr;
    dmabuf->resource_id = res->id;

    printk_color(TERM_CYAN, "[DMA-BUF] Attached DMA-BUF #%d to resource #%d\n",
                 dmabuf->id, res->id);

    return 0;
}

/* Detach DMA-BUF from GPU resource */
void dmabuf_detach_resource(dmabuf_handle_t *dmabuf, gpu_resource_t *res)
{
    if (!dmabuf || !res)
        return;

    res->backing = NULL;
    res->paddr = 0;
    dmabuf->resource_id = 0;

    printk_color(TERM_CYAN, "[DMA-BUF] Detached DMA-BUF #%d from resource #%d\n",
                 dmabuf->id, res->id);
}
