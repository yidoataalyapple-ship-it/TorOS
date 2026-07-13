/*
 * torfs.h — torFS: basit in-memory dosya sistemi
 * (Planda "MEVCUT": 64 dosya x 4KB, flat)
 * Faz 1.4 kapsamında /dev/input/eventN sanal aygıt dosyaları destekler.
 */
#ifndef TOROS_TORFS_H
#define TOROS_TORFS_H

#include <toros/types.h>

#define TORFS_MAX_FILES 64
#define TORFS_FILE_SIZE 4096
#define TORFS_MAX_PATH  96

enum torfs_type {
    TORFS_T_REGULAR = 0,
    TORFS_T_DEVICE,          /* read callback'li sanal aygıt */
};

/* Aygıt okuma callback'i: len bayta kadar doldur, okunan bayt veya -1 (boş) */
typedef int (*torfs_dev_read_t)(void *arg, void *buf, size_t len);

struct torfs_stat {
    u32 size;
    u32 type;
    char path[TORFS_MAX_PATH];
};

void torfs_init(void);
int  torfs_create(const char *path);                        /* 0=ok, <0 hata */
int  torfs_create_device(const char *path, torfs_dev_read_t rd, void *arg);
int  torfs_delete(const char *path);
int  torfs_write(const char *path, const void *buf, size_t len, u32 offset);
int  torfs_read(const char *path, void *buf, size_t len, u32 offset);
int  torfs_stat(const char *path, struct torfs_stat *st);
int  torfs_list(const char *prefix, struct torfs_stat *out, int max);
u32  torfs_file_count(void);

#endif
