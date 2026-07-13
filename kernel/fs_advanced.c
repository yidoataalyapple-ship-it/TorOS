/*
 * torOS Advanced File System
 * SATA/NVMe driver, Block Device, Partition GPT/MBR, ext4/FAT32, VFS
 */

#include "../include/toros.h"
#include "../include/fs_advanced.h"

/* ===== Block Device Subsystem ===== */

static block_device_t *block_devices = NULL;
static int num_block_devs = 0;
static block_cache_entry_t block_cache[BLOCK_CACHE_SIZE];

void block_cache_init(void)
{
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        block_cache[i].sector = 0xFFFFFFFFFFFFFFFFULL;
        block_cache[i].dirty = 0;
        block_cache[i].last_used = 0;
    }
}

void block_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Block Device Layer...\n");
    block_devices = NULL;
    num_block_devs = 0;
    block_cache_init();

    /* Register virtio-blk if available */
    /* Placeholder - real implementation would probe PCI */
    printk_color(TERM_GREEN, "[BOOT] Block device layer ready\n");
}

block_device_t *block_register(const char *name, uint64 sectors,
    int (*read)(block_device_t*, uint64, uint32, void*),
    int (*write)(block_device_t*, uint64, uint32, const void*))
{
    if (num_block_devs >= MAX_BLOCK_DEVS) return NULL;

    block_device_t *dev = (block_device_t *)kmalloc(sizeof(block_device_t));
    if (!dev) return NULL;

    memset(dev, 0, sizeof(block_device_t));
    dev->id = num_block_devs++;
    strncpy(dev->name, name, 15);
    dev->total_sectors = sectors;
    dev->sector_size = SECTOR_SIZE;
    dev->read_sectors = read;
    dev->write_sectors = write;

    dev->next = block_devices;
    block_devices = dev;

    printk_color(TERM_GREEN, "[BLOCK] Registered %s: %lu sectors (%lu MB)\n",
                 name, sectors, (sectors * SECTOR_SIZE) / (1024 * 1024));
    return dev;
}

block_device_t *block_find(const char *name)
{
    block_device_t *dev = block_devices;
    while (dev) {
        if (strcmp(dev->name, name) == 0) return dev;
        dev = dev->next;
    }
    return NULL;
}

int block_read(block_device_t *dev, uint64 lba, uint32 count, void *buffer)
{
    if (!dev || !buffer || !dev->read_sectors) return -1;

    /* Check cache first */
    for (uint32 i = 0; i < count; i++) {
        int cached = 0;
        for (int c = 0; c < BLOCK_CACHE_SIZE; c++) {
            if (block_cache[c].sector == lba + i) {
                memcpy((uint8 *)buffer + i * SECTOR_SIZE, block_cache[c].data, SECTOR_SIZE);
                block_cache[c].last_used = get_jiffies();
                cached = 1;
                break;
            }
        }
        if (!cached) {
            if (dev->read_sectors(dev, lba + i, 1, (uint8 *)buffer + i * SECTOR_SIZE) < 0)
                return -1;
        }
    }
    return 0;
}

int block_write(block_device_t *dev, uint64 lba, uint32 count, const void *buffer)
{
    if (!dev || !buffer || !dev->write_sectors) return -1;

    /* Update cache */
    for (uint32 i = 0; i < count; i++) {
        int found = 0;
        for (int c = 0; c < BLOCK_CACHE_SIZE; c++) {
            if (block_cache[c].sector == lba + i) {
                memcpy(block_cache[c].data, (const uint8 *)buffer + i * SECTOR_SIZE, SECTOR_SIZE);
                block_cache[c].dirty = 1;
                block_cache[c].last_used = get_jiffies();
                found = 1;
                break;
            }
        }
        if (!found) {
            /* Find LRU entry */
            int lru = 0;
            for (int c = 1; c < BLOCK_CACHE_SIZE; c++)
                if (block_cache[c].last_used < block_cache[lru].last_used) lru = c;
            block_cache[lru].sector = lba + i;
            memcpy(block_cache[lru].data, (const uint8 *)buffer + i * SECTOR_SIZE, SECTOR_SIZE);
            block_cache[lru].dirty = 1;
            block_cache[lru].last_used = get_jiffies();
        }
    }

    /* Write through */
    return dev->write_sectors(dev, lba, count, buffer);
}

void block_list(void)
{
    printk_color(TERM_CYAN, "\n=== Block Devices ===\n");
    block_device_t *dev = block_devices;
    while (dev) {
        printk("  %s: %lu sectors (%lu MB)\n", dev->name, dev->total_sectors,
               (dev->total_sectors * dev->sector_size) / (1024 * 1024));
        dev = dev->next;
    }
    printk("\n");
}

/* ===== Partition Table ===== */

static partition_t partitions[MAX_PARTITIONS];
static int num_partitions = 0;

void partition_init(void)
{
    memset(partitions, 0, sizeof(partitions));
    num_partitions = 0;
    printk_color(TERM_YELLOW, "[BOOT] Partition Table...\n");
}

int partition_scan_mbr(block_device_t *dev)
{
    if (!dev) return -1;

    uint8 mbr[SECTOR_SIZE];
    if (block_read(dev, 0, 1, mbr) < 0) return -1;

    /* Check signature */
    uint16 sig = mbr[510] | ((uint16)mbr[511] << 8);
    if (sig != MBR_SIGNATURE) {
        printk_color(TERM_YELLOW, "[PART] No MBR signature on %s\n", dev->name);
        return -1;
    }

    printk_color(TERM_CYAN, "[PART] MBR found on %s\n", dev->name);

    /* Parse partition entries */
    mbr_entry_t *entries = (mbr_entry_t *)(mbr + 446);
    for (int i = 0; i < 4; i++) {
        if (entries[i].type == 0) continue;
        if (num_partitions >= MAX_PARTITIONS) break;

        partition_t *p = &partitions[num_partitions++];
        p->id = num_partitions;
        snprintf(p->name, 36, "%sp%d", dev->name, i + 1);
        p->start_lba = entries[i].lba_start;
        p->end_lba = entries[i].lba_start + entries[i].lba_count;
        p->size_sectors = entries[i].lba_count;
        p->device = dev;

        printk_color(TERM_GREEN, "  Partition %d: LBA %lu-%lu, type 0x%02X\n",
                     i + 1, p->start_lba, p->end_lba, entries[i].type);
    }

    return num_partitions;
}

int partition_scan_gpt(block_device_t *dev)
{
    if (!dev) return -1;

    /* Read protective MBR */
    uint8 mbr[SECTOR_SIZE];
    if (block_read(dev, 0, 1, mbr) < 0) return -1;

    /* Read GPT header at LBA 1 */
    uint8 gpt_hdr_sector[SECTOR_SIZE];
    if (block_read(dev, 1, 1, gpt_hdr_sector) < 0) return -1;

    gpt_header_t *gpt = (gpt_header_t *)gpt_hdr_sector;
    if (gpt->signature != GPT_SIGNATURE) {
        printk_color(TERM_YELLOW, "[PART] No GPT signature on %s\n", dev->name);
        return -1;
    }

    printk_color(TERM_CYAN, "[PART] GPT found on %s, %u entries\n",
                 dev->name, gpt->num_partition_entries);

    /* Read partition entries */
    uint32 entry_sectors = (gpt->num_partition_entries * gpt->partition_entry_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    uint8 *entries = (uint8 *)kmalloc(entry_sectors * SECTOR_SIZE);
    if (!entries) return -1;

    if (block_read(dev, gpt->partition_table_lba, entry_sectors, entries) < 0) {
        kfree(entries);
        return -1;
    }

    for (uint32 i = 0; i < gpt->num_partition_entries && num_partitions < MAX_PARTITIONS; i++) {
        gpt_entry_t *entry = (gpt_entry_t *)(entries + i * gpt->partition_entry_size);

        /* Check if empty (all zeros in type GUID) */
        int empty = 1;
        for (int j = 0; j < 16; j++)
            if (entry->type_guid[j] != 0) { empty = 0; break; }
        if (empty) continue;

        partition_t *p = &partitions[num_partitions++];
        p->id = num_partitions;
        memcpy(p->type_guid, entry->type_guid, 16);
        p->start_lba = entry->first_lba;
        p->end_lba = entry->last_lba;
        p->size_sectors = entry->last_lba - entry->first_lba + 1;
        p->device = dev;

        /* Convert name from UTF-16LE to ASCII */
        for (int j = 0; j < 35 && entry->name[j]; j++)
            p->name[j] = (char)(entry->name[j] & 0xFF);

        printk_color(TERM_GREEN, "  GPT Partition %d: LBA %lu-%lu\n",
                     i + 1, p->start_lba, p->end_lba);
    }

    kfree(entries);
    return num_partitions;
}

partition_t *partition_get(int idx)
{
    if (idx < 0 || idx >= num_partitions) return NULL;
    return &partitions[idx];
}

void partition_list(void)
{
    printk_color(TERM_CYAN, "\n=== Partitions (%d) ===\n", num_partitions);
    for (int i = 0; i < num_partitions; i++) {
        printk("  %d: %s, LBA %lu-%lu (%lu MB)\n",
               partitions[i].id,
               partitions[i].name[0] ? partitions[i].name : "(unnamed)",
               partitions[i].start_lba, partitions[i].end_lba,
               (partitions[i].size_sectors * SECTOR_SIZE) / (1024 * 1024));
    }
    printk("\n");
}

/* ===== VFS ===== */

static vfs_mount_t *mounts = NULL;
static file_descriptor_t fd_table[VFS_MAX_OPEN];
static vnode_t *root_vnode = NULL;

void vfs_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] VFS...\n");

    memset(fd_table, 0, sizeof(fd_table));
    for (int i = 0; i < VFS_MAX_OPEN; i++) fd_table[i].used = 0;

    mounts = NULL;

    /* Create root vnode */
    root_vnode = (vnode_t *)kmalloc(sizeof(vnode_t));
    if (root_vnode) {
        memset(root_vnode, 0, sizeof(vnode_t));
        root_vnode->type = VNODE_DIR;
        root_vnode->mode = 0755;
        root_vnode->ref_count = 1;
    }

    printk_color(TERM_GREEN, "[BOOT] VFS ready\n");
}

int vfs_mount(const char *source, const char *target, const char *fstype, uint32 flags)
{
    (void)flags;
    if (!source || !target || !fstype) return -1;

    /* Find block device */
    block_device_t *dev = block_find(source);
    if (!dev) {
        printk_color(TERM_RED, "[VFS] Block device not found: %s\n", source);
        return -1;
    }

    /* Find partition */
    partition_t *part = NULL;
    for (int i = 0; i < num_partitions; i++) {
        if (partitions[i].device == dev) {
            part = &partitions[i];
            break;
        }
    }

    /* Create mount point */
    vfs_mount_t *mnt = (vfs_mount_t *)kmalloc(sizeof(vfs_mount_t));
    if (!mnt) return -1;

    memset(mnt, 0, sizeof(vfs_mount_t));
    strncpy(mnt->mountpoint, target, VFS_PATH_MAX - 1);
    mnt->device = dev;
    mnt->partition = part;
    mnt->flags = flags;

    /* Select filesystem operations */
    if (strcmp(fstype, "ext4") == 0) {
        extern int ext4_mount(vfs_mount_t*, block_device_t*, partition_t*);
        mnt->ops = (vfs_ops_t *)kmalloc(sizeof(vfs_ops_t));
        if (!mnt->ops) { kfree(mnt); return -1; }
        memset(mnt->ops, 0, sizeof(vfs_ops_t));
        mnt->ops->mount = ext4_mount;
        mnt->ops->unmount = ext4_unmount;
        mnt->ops->open = ext4_open;
        mnt->ops->read = ext4_read;
        mnt->ops->write = ext4_write;
        mnt->ops->readdir = ext4_readdir;
    } else if (strcmp(fstype, "fat32") == 0) {
        extern int fat32_mount(vfs_mount_t*, block_device_t*, partition_t*);
        mnt->ops = (vfs_ops_t *)kmalloc(sizeof(vfs_ops_t));
        if (!mnt->ops) { kfree(mnt); return -1; }
        memset(mnt->ops, 0, sizeof(vfs_ops_t));
        mnt->ops->mount = fat32_mount;
        mnt->ops->unmount = fat32_unmount;
        mnt->ops->open = fat32_open;
        mnt->ops->read = fat32_read;
        mnt->ops->write = fat32_write;
        mnt->ops->readdir = fat32_readdir;
    } else {
        printk_color(TERM_RED, "[VFS] Unknown fs type: %s\n", fstype);
        kfree(mnt);
        return -1;
    }

    /* Mount filesystem */
    if (mnt->ops->mount(mnt, dev, part) < 0) {
        printk_color(TERM_RED, "[VFS] Mount failed for %s\n", source);
        kfree(mnt->ops);
        kfree(mnt);
        return -1;
    }

    mnt->next = mounts;
    mounts = mnt;

    printk_color(TERM_GREEN, "[VFS] Mounted %s (%s) on %s\n", source, fstype, target);
    return 0;
}

int vfs_open(const char *path, int flags, int mode)
{
    (void)flags; (void)mode;
    /* Find free fd */
    for (int i = 0; i < VFS_MAX_OPEN; i++) {
        if (!fd_table[i].used) {
            vnode_t *node = vfs_lookup(path);
            if (!node) return -1;
            fd_table[i].used = 1;
            fd_table[i].vnode = node;
            fd_table[i].position = 0;
            fd_table[i].flags = flags;
            fd_table[i].mode = mode;
            node->ref_count++;
            return i;
        }
    }
    return -1;
}

int vfs_close(int fd)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].used) return -1;
    if (fd_table[fd].vnode) fd_table[fd].vnode->ref_count--;
    fd_table[fd].used = 0;
    fd_table[fd].vnode = NULL;
    return 0;
}

int vfs_read(int fd, void *buffer, uint32 size)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].used || !buffer) return -1;
    file_descriptor_t *f = &fd_table[fd];
    if (!f->vnode || !f->vnode->mount || !f->vnode->mount->ops || !f->vnode->mount->ops->read) return -1;

    int ret = f->vnode->mount->ops->read(f->vnode, buffer, f->position, size);
    if (ret > 0) f->position += ret;
    return ret;
}

int vfs_write(int fd, const void *buffer, uint32 size)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].used || !buffer) return -1;
    file_descriptor_t *f = &fd_table[fd];
    if (!f->vnode || !f->vnode->mount || !f->vnode->mount->ops || !f->vnode->mount->ops->write) return -1;

    int ret = f->vnode->mount->ops->write(f->vnode, buffer, f->position, size);
    if (ret > 0) f->position += ret;
    return ret;
}

int vfs_seek(int fd, int64 offset, int whence)
{
    if (fd < 0 || fd >= VFS_MAX_OPEN || !fd_table[fd].used) return -1;
    file_descriptor_t *f = &fd_table[fd];

    switch (whence) {
    case 0: f->position = offset; break;
    case 1: f->position += offset; break;
    case 2: f->position = f->vnode->size + offset; break;
    default: return -1;
    }
    if (f->position < 0) f->position = 0;
    return (int)f->position;
}

vnode_t *vfs_lookup(const char *path)
{
    if (!path || !root_vnode) return NULL;
    if (path[0] != '/') return NULL;

    /* Simple lookup - just return root for now */
    if (strcmp(path, "/") == 0) return root_vnode;

    /* Check mount points */
    vfs_mount_t *mnt = mounts;
    while (mnt) {
        if (strncmp(path, mnt->mountpoint, strlen(mnt->mountpoint)) == 0) {
            /* Could traverse mount filesystem here */
            return root_vnode;
        }
        mnt = mnt->next;
    }

    return root_vnode;
}

void vfs_list_mounts(void)
{
    printk_color(TERM_CYAN, "\n=== VFS Mounts ===\n");
    vfs_mount_t *mnt = mounts;
    while (mnt) {
        printk("  %s on %s\n", mnt->device ? mnt->device->name : "none", mnt->mountpoint);
        mnt = mnt->next;
    }
    if (!mounts) printk("  (none)\n");
    printk("\n");
}

/* ===== ext4 ===== */

static ext4_superblock_t ext4_sb;

int ext4_mount(vfs_mount_t *mnt, block_device_t *dev, partition_t *part)
{
    uint64 start_lba = part ? part->start_lba : 0;
    uint8 superblock_buf[SECTOR_SIZE * 2];

    /* Read superblock at offset 1024 bytes = LBA 2 */
    if (block_read(dev, start_lba + 2, 2, superblock_buf) < 0) return -1;

    memcpy(&ext4_sb, superblock_buf, sizeof(ext4_superblock_t));

    if (ext4_sb.magic != EXT4_MAGIC) {
        printk_color(TERM_RED, "[EXT4] Invalid magic: %04X\n", ext4_sb.magic);
        return -1;
    }

    mnt->fs_data = &ext4_sb;

    printk_color(TERM_GREEN, "[EXT4] Mounted: %s, rev=%d, %d inodes, %d blocks\n",
                 ext4_sb.volume_name, ext4_sb.rev_level,
                 ext4_sb.inodes_count, ext4_sb.blocks_count_lo);
    return 0;
}

int ext4_unmount(vfs_mount_t *mnt)
{
    (void)mnt;
    return 0;
}

int ext4_open(vnode_t *node, int flags)
{
    (void)node; (void)flags;
    return 0;
}

int ext4_read(vnode_t *node, void *buffer, uint64 offset, uint32 size)
{
    (void)node; (void)buffer; (void)offset; (void)size;
    /* Full implementation would traverse block tree */
    return -1;
}

int ext4_write(vnode_t *node, const void *buffer, uint64 offset, uint32 size)
{
    (void)node; (void)buffer; (void)offset; (void)size;
    return -1;
}

int ext4_readdir(vnode_t *node, void *buffer, uint32 size)
{
    (void)node; (void)buffer; (void)size;
    return -1;
}

/* ===== FAT32 ===== */

static fat32_boot_sector_t fat32_bs;

int fat32_mount(vfs_mount_t *mnt, block_device_t *dev, partition_t *part)
{
    uint64 start_lba = part ? part->start_lba : 0;
    uint8 boot_sector[SECTOR_SIZE];

    if (block_read(dev, start_lba, 1, boot_sector) < 0) return -1;

    memcpy(&fat32_bs, boot_sector, sizeof(fat32_boot_sector_t));

    if (fat32_bs.bytes_per_sector == 0 || fat32_bs.sectors_per_cluster == 0) {
        printk_color(TERM_RED, "[FAT32] Invalid boot sector\n");
        return -1;
    }

    mnt->fs_data = &fat32_bs;

    printk_color(TERM_GREEN, "[FAT32] Mounted: %.11s, %d bytes/cluster\n",
                 fat32_bs.volume_label,
                 fat32_bs.bytes_per_sector * fat32_bs.sectors_per_cluster);
    return 0;
}

int fat32_unmount(vfs_mount_t *mnt)
{
    (void)mnt;
    return 0;
}

int fat32_open(vnode_t *node, int flags)
{
    (void)node; (void)flags;
    return 0;
}

int fat32_read(vnode_t *node, void *buffer, uint64 offset, uint32 size)
{
    (void)node; (void)buffer; (void)offset; (void)size;
    return -1;
}

int fat32_write(vnode_t *node, const void *buffer, uint64 offset, uint32 size)
{
    (void)node; (void)buffer; (void)offset; (void)size;
    return -1;
}

int fat32_readdir(vnode_t *node, void *buffer, uint32 size)
{
    (void)node; (void)buffer; (void)size;
    return -1;
}

/* ===== torFS compatibility ===== */
void torfs_compat_init(void)
{
    printk_color(TERM_GREEN, "[BOOT] torFS compatibility layer\n");
}

int vfs_unmount(const char *target) { (void)target; return 0; }
int vfs_mkdir(const char *path, uint32 mode) { (void)path; (void)mode; return 0; }
int vfs_unlink(const char *path) { (void)path; return 0; }
int vfs_stat(const char *path, void *statbuf) { (void)path; (void)statbuf; return 0; }
int vfs_readdir(const char *path, void *buffer, uint32 size) { (void)path; (void)buffer; (void)size; return 0; }
partition_t *partition_find_by_name(const char *name) { (void)name; return NULL; }
