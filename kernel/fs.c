/*
 * torFS - torOS File System
 * Simple in-memory filesystem
 */

#include "../include/toros.h"

#define TFS_MAX_FILES       64
#define TFS_MAX_FILENAME    16
#define TFS_BLOCK_SIZE      1024
#define TFS_BLOCKS_PER_FILE 4
#define TFS_MAX_FILE_SIZE   (TFS_BLOCK_SIZE * TFS_BLOCKS_PER_FILE)

#define TFS_TYPE_FILE       1
#define TFS_TYPE_DIR        2

typedef struct {
    char name[TFS_MAX_FILENAME];
    uint8 type;
    uint32 size;
    uint32 blocks[TFS_BLOCKS_PER_FILE];
    uint64 ctime;
    uint64 mtime;
    uint8 used;
} tfs_file_t;

typedef struct {
    char magic[8];
    uint32 version;
    uint32 num_files;
    uint32 num_blocks;
    uint32 free_blocks;
    uint64 created;
} tfs_sb_t;

static tfs_sb_t *sb = NULL;
static tfs_file_t *files = NULL;
static uint8 *block_data = NULL;
static uint8 *block_bitmap = NULL;
static uint32 total_blocks;
static spinlock_t fs_lock;

#define TFS_MAGIC   "torFS\0\0\0"

static int alloc_block(void)
{
    for (uint32 i = 0; i < total_blocks; i++)
        if (!(block_bitmap[i / 8] & (1 << (i % 8)))) {
            block_bitmap[i / 8] |= (1 << (i % 8));
            sb->free_blocks--;
            return i;
        }
    return -1;
}

static void free_block(uint32 blk)
{
    if (blk < total_blocks) { block_bitmap[blk / 8] &= ~(1 << (blk % 8)); sb->free_blocks++; }
}

static tfs_file_t *find_file(const char *name)
{
    for (int i = 0; i < TFS_MAX_FILES; i++)
        if (files[i].used && strcmp(files[i].name, name) == 0) return &files[i];
    return NULL;
}

static tfs_file_t *find_free_file(void)
{
    for (int i = 0; i < TFS_MAX_FILES; i++) if (!files[i].used) return &files[i];
    return NULL;
}

static uint8 *get_block_data(uint32 block)
{
    return (block < total_blocks) ? block_data + (block * TFS_BLOCK_SIZE) : NULL;
}

void tfs_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] torFS...\n");
    spin_init(&fs_lock);
    
    sb = (tfs_sb_t *)kmalloc(sizeof(tfs_sb_t));
    files = (tfs_file_t *)kmalloc(sizeof(tfs_file_t) * TFS_MAX_FILES);
    total_blocks = 256;
    block_data = (uint8 *)kmalloc(TFS_BLOCK_SIZE * total_blocks);
    block_bitmap = (uint8 *)kmalloc((total_blocks + 7) / 8);
    
    if (!sb || !files || !block_data || !block_bitmap) panic("tfs_init: no memory");
    
    memset(files, 0, sizeof(tfs_file_t) * TFS_MAX_FILES);
    memset(block_data, 0, TFS_BLOCK_SIZE * total_blocks);
    memset(block_bitmap, 0, (total_blocks + 7) / 8);
    
    memcpy(sb->magic, TFS_MAGIC, 8);
    sb->version = 1;
    sb->num_files = 0;
    sb->num_blocks = total_blocks;
    sb->free_blocks = total_blocks;
    sb->created = get_jiffies();
    
    alloc_block();
    
    printk_color(TERM_GREEN, "[BOOT] torFS ready: %d files, %d KB\n",
                 TFS_MAX_FILES, (total_blocks * TFS_BLOCK_SIZE) / 1024);
}

int tfs_create(const char *name)
{
    spin_lock(&fs_lock);
    if (!name || strlen(name) >= TFS_MAX_FILENAME) { spin_unlock(&fs_lock); return -1; }
    if (find_file(name)) { spin_unlock(&fs_lock); printk_color(TERM_RED, "[FS] '%s' exists\n", name); return -1; }
    
    tfs_file_t *f = find_free_file();
    if (!f) { spin_unlock(&fs_lock); printk_color(TERM_RED, "[FS] No free entries\n"); return -1; }
    
    memset(f, 0, sizeof(tfs_file_t));
    strcpy(f->name, name);
    f->type = TFS_TYPE_FILE;
    f->used = 1;
    f->ctime = f->mtime = get_jiffies();
    sb->num_files++;
    
    spin_unlock(&fs_lock);
    printk_color(TERM_GREEN, "[FS] Created '%s'\n", name);
    return 0;
}

int tfs_write(const char *name, const void *data, uint32 size, uint32 offset)
{
    spin_lock(&fs_lock);
    tfs_file_t *f = find_file(name);
    if (!f) { spin_unlock(&fs_lock); return -1; }
    if (offset + size > TFS_MAX_FILE_SIZE) size = TFS_MAX_FILE_SIZE - offset;
    
    const uint8 *src = (const uint8 *)data;
    uint32 written = 0, pos = offset;
    
    while (written < size) {
        uint32 block_idx = pos / TFS_BLOCK_SIZE;
        uint32 block_off = pos % TFS_BLOCK_SIZE;
        if (block_idx >= TFS_BLOCKS_PER_FILE) break;
        if (f->blocks[block_idx] == 0) { int blk = alloc_block(); if (blk < 0) break; f->blocks[block_idx] = blk; }
        uint8 *blk_data = get_block_data(f->blocks[block_idx]);
        if (!blk_data) break;
        uint32 to_write = TFS_BLOCK_SIZE - block_off;
        if (to_write > size - written) to_write = size - written;
        memcpy(blk_data + block_off, src + written, to_write);
        written += to_write; pos += to_write;
    }
    if (pos > f->size) f->size = pos;
    f->mtime = get_jiffies();
    spin_unlock(&fs_lock);
    return written;
}

int tfs_read(const char *name, void *buf, uint32 size, uint32 offset)
{
    spin_lock(&fs_lock);
    tfs_file_t *f = find_file(name);
    if (!f) { spin_unlock(&fs_lock); return -1; }
    if (offset >= f->size) { spin_unlock(&fs_lock); return 0; }
    if (offset + size > f->size) size = f->size - offset;
    
    uint8 *dst = (uint8 *)buf;
    uint32 read = 0, pos = offset;
    while (read < size) {
        uint32 block_idx = pos / TFS_BLOCK_SIZE;
        uint32 block_off = pos % TFS_BLOCK_SIZE;
        if (block_idx >= TFS_BLOCKS_PER_FILE || f->blocks[block_idx] == 0) break;
        uint8 *blk_data = get_block_data(f->blocks[block_idx]);
        if (!blk_data) break;
        uint32 to_read = TFS_BLOCK_SIZE - block_off;
        if (to_read > size - read) to_read = size - read;
        memcpy(dst + read, blk_data + block_off, to_read);
        read += to_read; pos += to_read;
    }
    spin_unlock(&fs_lock);
    return read;
}

int tfs_delete(const char *name)
{
    spin_lock(&fs_lock);
    tfs_file_t *f = find_file(name);
    if (!f) { spin_unlock(&fs_lock); return -1; }
    for (int i = 0; i < TFS_BLOCKS_PER_FILE; i++) if (f->blocks[i]) { free_block(f->blocks[i]); f->blocks[i] = 0; }
    f->used = 0;
    f->name[0] = '\0';
    sb->num_files--;
    spin_unlock(&fs_lock);
    printk_color(TERM_GREEN, "[FS] Deleted '%s'\n", name);
    return 0;
}

int tfs_size(const char *name)
{
    spin_lock(&fs_lock);
    tfs_file_t *f = find_file(name);
    int s = f ? (int)f->size : -1;
    spin_unlock(&fs_lock);
    return s;
}

void tfs_ls(void)
{
    spin_lock(&fs_lock);
    printk_color(TERM_CYAN, "\n=== torFS Files ===\n\n");
    int count = 0;
    for (int i = 0; i < TFS_MAX_FILES; i++)
        if (files[i].used) {
            printk_color(TERM_GREEN, "  %-16s ", files[i].name);
            printk("%5d bytes\n", files[i].size);
            count++;
        }
    if (count == 0) printk_color(TERM_YELLOW, "  (empty)\n");
    printk_color(TERM_CYAN, "\n  %d files, %d blocks free\n\n", count, sb->free_blocks);
    spin_unlock(&fs_lock);
}

void tfs_stat(void)
{
    spin_lock(&fs_lock);
    printk_color(TERM_CYAN, "\n=== torFS Stats ===\n\n");
    printk("  Magic:    %.7s\n  Version:  %d\n  Files:    %d/%d\n  Blocks:   %d/%d free\n  Size:     %d KB\n  Max file: %d KB\n\n",
           sb->magic, sb->version, sb->num_files, TFS_MAX_FILES,
           sb->free_blocks, sb->num_blocks,
           (total_blocks * TFS_BLOCK_SIZE) / 1024, TFS_MAX_FILE_SIZE / 1024);
    spin_unlock(&fs_lock);
}

void tfs_create_sample(void)
{
    tfs_create("welcome.txt");
    const char *w = "Welcome to torOS!\n==================\nBare-metal ARM64 OS.\nBuilt from scratch.\n";
    tfs_write("welcome.txt", w, strlen(w), 0);
    tfs_create("todo.txt");
    const char *t = "Roadmap:\n[x] Bootloader\n[x] UART\n[x] MMU\n[x] GICv3\n[x] SMP\n[x] torFS\n[ ] Keyboard\n[ ] Network\n";
    tfs_write("todo.txt", t, strlen(t), 0);
    printk_color(TERM_GREEN, "[FS] Sample files created\n");
}
