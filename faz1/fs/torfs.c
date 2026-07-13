/*
 * torfs.c — torFS: in-memory dosya sistemi
 * 64 dosya x 4KB + /dev sanal aygıt dosyaları (Faz 1.4)
 */
#include <toros/torfs.h>
#include <toros/printf.h>
#include <toros/string.h>
#include <toros/spinlock.h>

struct torfs_entry {
    int  used;
    u32  type;
    u32  size;
    char path[TORFS_MAX_PATH];
    u8   data[TORFS_FILE_SIZE];
    torfs_dev_read_t dev_read;
    void *dev_arg;
};

static struct torfs_entry entries[TORFS_MAX_FILES];
static spinlock_t fs_lock = SPINLOCK_INIT;

void torfs_init(void)
{
    memset(entries, 0, sizeof(entries));
}

static struct torfs_entry *find(const char *path)
{
    for (int i = 0; i < TORFS_MAX_FILES; i++)
        if (entries[i].used && strcmp(entries[i].path, path) == 0)
            return &entries[i];
    return NULL;
}

static struct torfs_entry *alloc_entry(void)
{
    for (int i = 0; i < TORFS_MAX_FILES; i++)
        if (!entries[i].used)
            return &entries[i];
    return NULL;
}

static int create_typed(const char *path, u32 type, torfs_dev_read_t rd, void *arg)
{
    if (!path || strlen(path) >= TORFS_MAX_PATH)
        return -1;

    u64 flags = spin_lock_irqsave(&fs_lock);
    if (find(path)) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -2;  /* zaten var */
    }
    struct torfs_entry *e = alloc_entry();
    if (!e) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -3;  /* dolu */
    }
    e->used = 1;
    e->type = type;
    e->size = 0;
    e->dev_read = rd;
    e->dev_arg = arg;
    strncpy(e->path, path, TORFS_MAX_PATH - 1);
    spin_unlock_irqrestore(&fs_lock, flags);
    return 0;
}

int torfs_create(const char *path)
{
    return create_typed(path, TORFS_T_REGULAR, NULL, NULL);
}

int torfs_create_device(const char *path, torfs_dev_read_t rd, void *arg)
{
    return create_typed(path, TORFS_T_DEVICE, rd, arg);
}

int torfs_delete(const char *path)
{
    u64 flags = spin_lock_irqsave(&fs_lock);
    struct torfs_entry *e = find(path);
    if (!e) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -1;
    }
    e->used = 0;
    spin_unlock_irqrestore(&fs_lock, flags);
    return 0;
}

int torfs_write(const char *path, const void *buf, size_t len, u32 offset)
{
    u64 flags = spin_lock_irqsave(&fs_lock);
    struct torfs_entry *e = find(path);
    if (!e || e->type != TORFS_T_REGULAR) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -1;
    }
    if (offset >= TORFS_FILE_SIZE) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -2;
    }
    size_t n = len;
    if (offset + n > TORFS_FILE_SIZE)
        n = TORFS_FILE_SIZE - offset;
    memcpy(e->data + offset, buf, n);
    if (offset + n > e->size)
        e->size = offset + (u32)n;
    spin_unlock_irqrestore(&fs_lock, flags);
    return (int)n;
}

int torfs_read(const char *path, void *buf, size_t len, u32 offset)
{
    /* Aygıt dosyaları: callback üzerinden (lock dışı) */
    struct torfs_entry *e;
    u64 flags = spin_lock_irqsave(&fs_lock);
    e = find(path);
    if (!e) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -1;
    }
    if (e->type == TORFS_T_DEVICE) {
        torfs_dev_read_t rd = e->dev_read;
        void *arg = e->dev_arg;
        spin_unlock_irqrestore(&fs_lock, flags);
        if (!rd)
            return -1;
        return rd(arg, buf, len);
    }
    if (offset >= e->size) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return 0;
    }
    size_t n = len;
    if (offset + n > e->size)
        n = e->size - offset;
    memcpy(buf, e->data + offset, n);
    spin_unlock_irqrestore(&fs_lock, flags);
    return (int)n;
}

int torfs_stat(const char *path, struct torfs_stat *st)
{
    u64 flags = spin_lock_irqsave(&fs_lock);
    struct torfs_entry *e = find(path);
    if (!e) {
        spin_unlock_irqrestore(&fs_lock, flags);
        return -1;
    }
    st->size = e->size;
    st->type = e->type;
    strncpy(st->path, e->path, TORFS_MAX_PATH - 1);
    spin_unlock_irqrestore(&fs_lock, flags);
    return 0;
}

int torfs_list(const char *prefix, struct torfs_stat *out, int max)
{
    int n = 0;
    size_t plen = prefix ? strlen(prefix) : 0;
    u64 flags = spin_lock_irqsave(&fs_lock);
    for (int i = 0; i < TORFS_MAX_FILES && n < max; i++) {
        if (!entries[i].used)
            continue;
        if (plen && strncmp(entries[i].path, prefix, plen) != 0)
            continue;
        out[n].size = entries[i].size;
        out[n].type = entries[i].type;
        strncpy(out[n].path, entries[i].path, TORFS_MAX_PATH - 1);
        n++;
    }
    spin_unlock_irqrestore(&fs_lock, flags);
    return n;
}

u32 torfs_file_count(void)
{
    u32 n = 0;
    u64 flags = spin_lock_irqsave(&fs_lock);
    for (int i = 0; i < TORFS_MAX_FILES; i++)
        if (entries[i].used)
            n++;
    spin_unlock_irqrestore(&fs_lock, flags);
    return n;
}
