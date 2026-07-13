/*
 * torOS Advanced File System Header
 * SATA/NVMe driver, Block Device, Partition GPT/MBR, ext4/FAT32, VFS
 */

#ifndef _FS_ADVANCED_H
#define _FS_ADVANCED_H

#include "toros.h"

/* ===== Block Device ===== */
#define BLOCK_SIZE          512
#define SECTOR_SIZE         512
#define MAX_BLOCK_DEVS      4
#define BLOCK_CACHE_SIZE    64

typedef struct block_device {
    uint32 id;
    char name[16];
    uint64 total_sectors;
    uint32 sector_size;
    int (*read_sectors)(struct block_device *dev, uint64 lba, uint32 count, void *buffer);
    int (*write_sectors)(struct block_device *dev, uint64 lba, uint32 count, const void *buffer);
    int (*flush)(struct block_device *dev);
    void *private;
    struct block_device *next;
} block_device_t;

/* Block cache */
typedef struct {
    uint64 sector;
    uint8 data[BLOCK_SIZE];
    uint32 dirty;
    uint64 last_used;
} block_cache_entry_t;

/* ===== Partition Table ===== */
#define MBR_SIGNATURE       0xAA55
#define GPT_SIGNATURE       0x5452415020494645ULL

/* MBR partition entry */
typedef struct {
    uint8 status;
    uint8 chs_start[3];
    uint8 type;
    uint8 chs_end[3];
    uint32 lba_start;
    uint32 lba_count;
} __attribute__((packed)) mbr_entry_t;

/* GPT header */
typedef struct {
    uint64 signature;
    uint32 revision;
    uint32 header_size;
    uint32 crc32;
    uint32 reserved;
    uint64 current_lba;
    uint64 backup_lba;
    uint64 first_usable_lba;
    uint64 last_usable_lba;
    uint8 disk_guid[16];
    uint64 partition_table_lba;
    uint32 num_partition_entries;
    uint32 partition_entry_size;
    uint32 partition_table_crc32;
} __attribute__((packed)) gpt_header_t;

/* GPT partition entry */
typedef struct {
    uint8 type_guid[16];
    uint8 unique_guid[16];
    uint64 first_lba;
    uint64 last_lba;
    uint64 attributes;
    uint16 name[36];
} __attribute__((packed)) gpt_entry_t;

/* Partition */
typedef struct partition {
    uint32 id;
    char name[36];
    uint64 start_lba;
    uint64 end_lba;
    uint64 size_sectors;
    uint8 type_guid[16];
    block_device_t *device;
    struct partition *next;
} partition_t;

#define MAX_PARTITIONS      32

/* ===== ext4 ===== */
#define EXT4_MAGIC          0xEF53
#define EXT4_INODE_SIZE     256
#define EXT4_BLOCK_SIZE(s)  (1024 << (s))

/* ext4 superblock */
typedef struct {
    uint32 inodes_count;
    uint32 blocks_count_lo;
    uint32 r_blocks_count_lo;
    uint32 free_blocks_count_lo;
    uint32 free_inodes_count;
    uint32 first_data_block;
    uint32 block_size_log;
    uint32 blocks_per_group;
    uint32 frags_per_group;
    uint32 inodes_per_group;
    uint16 magic;
    uint16 state;
    uint16 errors;
    uint16 minor_rev_level;
    uint32 lastcheck;
    uint32 checkinterval;
    uint32 creator_os;
    uint32 rev_level;
    uint16 def_resuid;
    uint16 def_resgid;
    /* Extended fields */
    uint32 first_ino;
    uint16 inode_size;
    uint16 block_group_nr;
    uint32 feature_compat;
    uint32 feature_incompat;
    uint32 feature_ro_compat;
    uint8 uuid[16];
    char volume_name[16];
    char last_mounted[64];
} __attribute__((packed)) ext4_superblock_t;

/* ext4 inode */
typedef struct {
    uint16 mode;
    uint16 uid;
    uint32 size_lo;
    uint32 atime;
    uint32 ctime;
    uint32 mtime;
    uint32 dtime;
    uint16 gid;
    uint16 links_count;
    uint32 blocks_lo;
    uint32 flags;
    uint32 osd1;
    uint32 block[15];   /* Direct, indirect, double, triple */
    uint32 generation;
    uint32 file_acl;
    uint32 size_hi;
    uint32 faddr;
    uint32 osd2[3];
} __attribute__((packed)) ext4_inode_t;

/* ext4 directory entry */
typedef struct {
    uint32 inode;
    uint16 rec_len;
    uint8 name_len;
    uint8 file_type;
    char name[];
} __attribute__((packed)) ext4_dir_entry_t;

/* File types */
#define EXT4_FT_UNKNOWN     0
#define EXT4_FT_REG_FILE    1
#define EXT4_FT_DIR         2
#define EXT4_FT_CHRDEV      3
#define EXT4_FT_BLKDEV      4
#define EXT4_FT_FIFO        5
#define EXT4_FT_SOCK        6
#define EXT4_FT_SYMLINK     7

/* ===== FAT32 ===== */
#define FAT32_BOOT_SIG      0x29
#define FAT32_EOF           0x0FFFFFFF
#define FAT32_BAD           0x0FFFFFF7
#define FAT32_FREE          0x00000000

/* FAT32 BIOS Parameter Block */
typedef struct {
    uint8 jmp[3];
    uint8 oem[8];
    uint16 bytes_per_sector;
    uint8 sectors_per_cluster;
    uint16 reserved_sectors;
    uint8 num_fats;
    uint16 root_entries;
    uint16 total_sectors_16;
    uint8 media_type;
    uint16 sectors_per_fat_16;
    uint16 sectors_per_track;
    uint16 num_heads;
    uint32 hidden_sectors;
    uint32 total_sectors_32;
    uint32 sectors_per_fat_32;
    uint16 flags;
    uint16 version;
    uint32 root_cluster;
    uint16 fsinfo_sector;
    uint16 backup_boot_sector;
    uint8 reserved[12];
    uint8 drive_num;
    uint8 reserved1;
    uint8 boot_sig;
    uint32 volume_id;
    uint8 volume_label[11];
    uint8 fs_type[8];
} __attribute__((packed)) fat32_boot_sector_t;

/* FAT32 directory entry */
typedef struct {
    uint8 name[11];
    uint8 attr;
    uint8 ntres;
    uint8 crt_time_tenth;
    uint16 crt_time;
    uint16 crt_date;
    uint16 lst_acc_date;
    uint16 fst_clus_hi;
    uint16 wrt_time;
    uint16 wrt_date;
    uint16 fst_clus_lo;
    uint32 file_size;
} __attribute__((packed)) fat32_dir_entry_t;

#define FAT_ATTR_READ_ONLY  0x01
#define FAT_ATTR_HIDDEN     0x02
#define FAT_ATTR_SYSTEM     0x04
#define FAT_ATTR_VOLUME_ID  0x08
#define FAT_ATTR_DIRECTORY  0x10
#define FAT_ATTR_ARCHIVE    0x20
#define FAT_ATTR_LFN        0x0F

/* ===== VFS ===== */
#define VFS_MAX_OPEN        64
#define VFS_PATH_MAX        256
#define VFS_NAME_MAX        128

/* VFS node types */
typedef enum {
    VNODE_NONE,
    VNODE_FILE,
    VNODE_DIR,
    VNODE_CHARDEV,
    VNODE_BLOCKDEV,
    VNODE_FIFO,
    VNODE_SYMLINK,
    VNODE_SOCKET
} vnode_type_t;

/* Forward declarations */
struct vfs_mount;
struct vnode;

/* VFS operations */
typedef struct vfs_ops {
    int (*mount)(struct vfs_mount *mnt, block_device_t *dev, partition_t *part);
    int (*unmount)(struct vfs_mount *mnt);
    int (*open)(struct vnode *node, int flags);
    int (*close)(struct vnode *node);
    int (*read)(struct vnode *node, void *buffer, uint64 offset, uint32 size);
    int (*write)(struct vnode *node, const void *buffer, uint64 offset, uint32 size);
    int (*create)(struct vnode *parent, const char *name, uint32 mode);
    int (*mkdir)(struct vnode *parent, const char *name, uint32 mode);
    int (*unlink)(struct vnode *parent, const char *name);
    int (*readdir)(struct vnode *node, void *buffer, uint32 size);
    int (*stat)(struct vnode *node, void *statbuf);
} vfs_ops_t;

/* VFS mount point */
typedef struct vfs_mount {
    char mountpoint[VFS_PATH_MAX];
    vfs_ops_t *ops;
    block_device_t *device;
    partition_t *partition;
    void *fs_data;
    uint32 flags;
    struct vfs_mount *next;
} vfs_mount_t;

/* V-node */
typedef struct vnode {
    uint32 inode_num;
    vnode_type_t type;
    uint32 mode;
    uint64 size;
    uint32 uid;
    uint32 gid;
    uint64 atime;
    uint64 mtime;
    uint64 ctime;
    uint32 ref_count;
    vfs_mount_t *mount;
    void *fs_private;
    struct vnode *parent;
    struct vnode *children;
    struct vnode *next_sibling;
} vnode_t;

/* ===== File descriptor ===== */
typedef struct {
    int used;
    vnode_t *vnode;
    uint64 position;
    uint32 flags;
    int mode;
} file_descriptor_t;

/* ===== File System API ===== */

/* Block device */
void block_init(void);
block_device_t *block_register(const char *name, uint64 sectors,
    int (*read)(block_device_t*, uint64, uint32, void*),
    int (*write)(block_device_t*, uint64, uint32, const void*));
block_device_t *block_find(const char *name);
int block_read(block_device_t *dev, uint64 lba, uint32 count, void *buffer);
int block_write(block_device_t *dev, uint64 lba, uint32 count, const void *buffer);
void block_list(void);

/* Partition */
void partition_init(void);
int partition_scan_mbr(block_device_t *dev);
int partition_scan_gpt(block_device_t *dev);
partition_t *partition_get(int idx);
partition_t *partition_find_by_name(const char *name);
void partition_list(void);

/* VFS */
void vfs_init(void);
int vfs_mount(const char *source, const char *target, const char *fstype, uint32 flags);
int vfs_unmount(const char *target);
int vfs_open(const char *path, int flags, int mode);
int vfs_close(int fd);
int vfs_read(int fd, void *buffer, uint32 size);
int vfs_write(int fd, const void *buffer, uint32 size);
int vfs_seek(int fd, int64 offset, int whence);
int vfs_mkdir(const char *path, uint32 mode);
int vfs_unlink(const char *path);
int vfs_stat(const char *path, void *statbuf);
int vfs_readdir(const char *path, void *buffer, uint32 size);
vnode_t *vfs_lookup(const char *path);
void vfs_list_mounts(void);

/* ext4 */
int ext4_mount(vfs_mount_t *mnt, block_device_t *dev, partition_t *part);
int ext4_unmount(vfs_mount_t *mnt);
int ext4_open(vnode_t *node, int flags);
int ext4_read(vnode_t *node, void *buffer, uint64 offset, uint32 size);
int ext4_write(vnode_t *node, const void *buffer, uint64 offset, uint32 size);
int ext4_readdir(vnode_t *node, void *buffer, uint32 size);

/* FAT32 */
int fat32_mount(vfs_mount_t *mnt, block_device_t *dev, partition_t *part);
int fat32_unmount(vfs_mount_t *mnt);
int fat32_open(vnode_t *node, int flags);
int fat32_read(vnode_t *node, void *buffer, uint64 offset, uint32 size);
int fat32_write(vnode_t *node, const void *buffer, uint64 offset, uint32 size);
int fat32_readdir(vnode_t *node, void *buffer, uint32 size);

/* Block cache */
void block_cache_init(void);
int block_cache_read(block_device_t *dev, uint64 sector, void *buffer);
int block_cache_write(block_device_t *dev, uint64 sector, const void *buffer);
void block_cache_flush(void);
void block_cache_flush_dev(block_device_t *dev);

/* torFS compatibility layer */
void torfs_compat_init(void);

#endif
