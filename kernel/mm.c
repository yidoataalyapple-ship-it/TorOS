/*
 * torOS Memory Manager
 * Simple page allocator (4KB pages)
 * + slab allocator for small objects
 */

#include "../include/toros.h"

/* Page frame database */
static uint8 *page_bitmap = NULL;
static usize total_pages = 0;
static usize free_page_count = 0;
static usize bitmap_pages = 0;

/* Kernel heap for kmalloc */
static uint8 *heap_start = NULL;
static uint8 *heap_end = NULL;
static uint8 *heap_ptr = NULL;

#define HEAP_SIZE   (4 * 1024 * 1024)  /* 4MB heap */

void mm_init(void)
{
    /* Kernel ends here, memory starts after */
    uintptr mem_start = (uintptr)_kernel_end;
    mem_start = (mem_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    /* Available RAM from kernel end to end of RAM */
    uintptr mem_end = KERNEL_BASE + RAM_SIZE;
    total_pages = (mem_end - mem_start) / PAGE_SIZE;

    /* Bitmap at start of available memory */
    bitmap_pages = (total_pages + PAGE_SIZE * 8 - 1) / (PAGE_SIZE * 8);
    page_bitmap = (uint8 *)mem_start;

    /* Mark all pages as free */
    for (usize i = 0; i < total_pages / 8; i++)
        page_bitmap[i] = 0;

    /* Mark bitmap pages as used */
    for (usize i = 0; i < bitmap_pages; i++) {
        page_bitmap[i / 8] |= (1 << (i % 8));
    }

    /* Mark heap pages as used */
    usize heap_pages = HEAP_SIZE / PAGE_SIZE;
    usize heap_start_page = bitmap_pages;
    for (usize i = heap_start_page; i < heap_start_page + heap_pages; i++)
        page_bitmap[i / 8] |= (1 << (i % 8));

    free_page_count = total_pages - bitmap_pages - heap_pages;

    /* Setup heap */
    heap_start = (uint8 *)(mem_start + bitmap_pages * PAGE_SIZE + HEAP_SIZE);
    heap_end = heap_start + HEAP_SIZE;
    heap_ptr = heap_start;

    printk_color(TERM_GREEN, "[MM] Total: %d pages, Free: %d pages, Bitmap: %d pages\n",
                 total_pages, free_page_count, bitmap_pages);
}

void *page_alloc(void)
{
    for (usize i = 0; i < total_pages; i++) {
        if (!(page_bitmap[i / 8] & (1 << (i % 8)))) {
            page_bitmap[i / 8] |= (1 << (i % 8));
            free_page_count--;
            return (void *)((uintptr)_kernel_end +
                           ((i + bitmap_pages) * PAGE_SIZE));
        }
    }
    return NULL;  /* Out of memory */
}

void page_free(void *page)
{
    uintptr addr = (uintptr)page;
    uintptr mem_start = (uintptr)_kernel_end;
    mem_start = (mem_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    usize idx = (addr - (mem_start + bitmap_pages * PAGE_SIZE)) / PAGE_SIZE;
    if (idx < total_pages) {
        page_bitmap[idx / 8] &= ~(1 << (idx % 8));
        free_page_count++;
    }
}

usize get_free_pages(void)
{
    return free_page_count;
}

void *kmalloc(usize size)
{
    size = (size + 7) & ~7;  /* 8-byte align */
    if (heap_ptr + size > heap_end)
        return NULL;
    void *p = heap_ptr;
    heap_ptr += size;
    return p;
}

void kfree(void *ptr)
{
    /* Simple bump allocator - no free for now */
    (void)ptr;
}
