/*
 * mm.c — Kernel heap: serbest listeli allocator
 * Identity map altında VA == PA; alloc_page DMA için uygundur.
 */
#include <toros/mm.h>
#include <toros/printf.h>
#include <toros/string.h>
#include <toros/spinlock.h>

/* Linker sembolleri */
extern u8 __kernel_end[];

/* Heap: kernel sonundan itibaren (16 MB) */
#define HEAP_SIZE (16 * MB)

static u8 *heap_start;
static u8 *heap_end;
static spinlock_t heap_lock = SPINLOCK_INIT;

struct blk_hdr {
    u32 size;              /* payload bayt */
    u32 free;
    struct blk_hdr *next;  /* tüm bloklar listesi */
    u32 magic;
};

#define BLK_MAGIC 0x544F5246UL  /* "TORF" */
#define HDR_SIZE ALIGN_UP(sizeof(struct blk_hdr), 16)

static struct blk_hdr *blk_list;
static size_t used_bytes;

void mm_init(void)
{
    heap_start = (u8 *)ALIGN_UP((uintptr_t)__kernel_end, PAGE_SIZE);
    heap_end = heap_start + HEAP_SIZE;
    blk_list = NULL;
    used_bytes = 0;
    kinfo("Heap: %p - %p (%lu MB)\n", heap_start, heap_end, HEAP_SIZE / MB);
}

static struct blk_hdr *find_free(size_t size)
{
    for (struct blk_hdr *b = blk_list; b; b = b->next)
        if (b->free && b->size >= size)
            return b;
    return NULL;
}

void *kmalloc(size_t size)
{
    if (size == 0)
        size = 16;
    size = ALIGN_UP(size, 16);

    u64 flags = spin_lock_irqsave(&heap_lock);

    struct blk_hdr *b = find_free(size);
    if (!b) {
        /* Heap sonundan yeni blok */
        static u8 *bump;
        if (!bump)
            bump = heap_start;
        if (bump + HDR_SIZE + size >= heap_end) {
            spin_unlock_irqrestore(&heap_lock, flags);
            kerr("kmalloc: heap doldu (%lu bayt istendi)\n", size);
            return NULL;
        }
        b = (struct blk_hdr *)bump;
        bump += HDR_SIZE + size;
        b->size = (u32)size;
        b->magic = BLK_MAGIC;
        b->next = blk_list;
        blk_list = b;
    }

    b->free = 0;
    used_bytes += b->size;
    spin_unlock_irqrestore(&heap_lock, flags);
    return (u8 *)b + HDR_SIZE;
}

void *kzalloc(size_t size)
{
    void *p = kmalloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}

void kfree(void *ptr)
{
    if (!ptr)
        return;
    struct blk_hdr *b = (struct blk_hdr *)((u8 *)ptr - HDR_SIZE);
    if (b->magic != BLK_MAGIC) {
        kerr("kfree: bozuk blok %p\n", ptr);
        return;
    }
    u64 flags = spin_lock_irqsave(&heap_lock);
    b->free = 1;
    if (used_bytes >= b->size)
        used_bytes -= b->size;
    spin_unlock_irqrestore(&heap_lock, flags);
}

/* -------- DMA sayfaları (4KB hizalı, identity map) -------- */

static u8 *page_bump;

void *alloc_pages(size_t bytes)
{
    size_t need = ALIGN_UP(bytes, PAGE_SIZE);
    u64 flags = spin_lock_irqsave(&heap_lock);
    if (!page_bump)
        page_bump = (u8 *)ALIGN_UP((uintptr_t)heap_start + HEAP_SIZE / 2, PAGE_SIZE);
    if (page_bump + need >= heap_end) {
        spin_unlock_irqrestore(&heap_lock, flags);
        kerr("alloc_pages: alan yok (%lu bayt)\n", need);
        return NULL;
    }
    void *p = page_bump;
    page_bump += need;
    spin_unlock_irqrestore(&heap_lock, flags);
    memset(p, 0, need);
    return p;
}

void *alloc_page(void)
{
    return alloc_pages(PAGE_SIZE);
}

void free_pages(void *ptr)
{
    /* Bump allocator: geri verme no-op (kernel aygıt bellekleri ömürlük) */
    (void)ptr;
}

size_t mm_heap_total(void) { return HEAP_SIZE; }
size_t mm_heap_used(void)  { return used_bytes + (size_t)(page_bump ? page_bump - heap_start : 0); }
