/*
 * mm.h — Kernel bellek yöneticisi
 *  - kmalloc/kfree: serbest listeli allocator
 *  - alloc_page: 4KB hizalı DMA sayfaları (identity map: VA == PA)
 */
#ifndef TOROS_MM_H
#define TOROS_MM_H

#include <toros/types.h>

#define PAGE_SIZE 4096UL

void  mm_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void  kfree(void *ptr);
void *alloc_page(void);              /* tek 4KB sayfa, 4KB hizalı */
void *alloc_pages(size_t bytes);     /* ard arda sayfalar, 4KB hizalı */
void  free_pages(void *ptr);

size_t mm_heap_total(void);
size_t mm_heap_used(void);

#endif
