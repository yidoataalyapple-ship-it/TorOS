/*
 * torOS Image Format & Processing Header
 * BMP, PNG, JPEG decoder, icon cache, image scaling
 */

#ifndef _IMAGE_H
#define _IMAGE_H

#include "toros.h"

/* ===== Image Structure ===== */
typedef struct {
    uint32 *pixels;     /* ARGB pixel data */
    int width;
    int height;
    int pitch;          /* Bytes per row */
    int bpp;            /* Bits per pixel */
    int format;         /* Image format */
} image_t;

/* ===== Image Formats ===== */
#define IMG_FMT_ARGB        0
#define IMG_FMT_RGB         1
#define IMG_FMT_RGBA        2
#define IMG_FMT_BGRA        3
#define IMG_FMT_GRAY        4

/* ===== BMP ===== */
#define BMP_MAGIC           0x4D42

/* BMP info header */
typedef struct {
    uint32 header_size;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bpp;
    uint32 compression;
    uint32 image_size;
    int32 x_ppm;
    int32 y_ppm;
    uint32 colors_used;
    uint32 colors_important;
} __attribute__((packed)) bmp_info_header_t;

/* BMP file header */
typedef struct {
    uint16 magic;
    uint32 file_size;
    uint16 reserved1;
    uint16 reserved2;
    uint32 data_offset;
} __attribute__((packed)) bmp_file_header_t;

int bmp_decode(const uint8 *data, uint32 size, image_t *out);
int bmp_encode(const image_t *img, uint8 **out, uint32 *out_size);
void bmp_free(image_t *img);

/* ===== PNG ===== */
#define PNG_MAGIC_1         0x89504E47
#define PNG_MAGIC_2         0x0D0A1A0A

#define PNG_CHUNK_IHDR      0x49484452
#define PNG_CHUNK_IDAT      0x49444154
#define PNG_CHUNK_IEND      0x49454E44
#define PNG_CHUNK_PLTE      0x504C5445
#define PNG_CHUNK_tRNS      0x74524E53
#define PNG_CHUNK_gAMA      0x67414D41

#define PNG_COLOR_GRAY      0
#define PNG_COLOR_RGB       2
#define PNG_COLOR_PLTE      3
#define PNG_COLOR_GRAYA     4
#define PNG_COLOR_RGBA      6

typedef struct {
    uint32 width;
    uint32 height;
    uint8 bit_depth;
    uint8 color_type;
    uint8 compression;
    uint8 filter;
    uint8 interlace;
} png_ihdr_t;

int png_decode(const uint8 *data, uint32 size, image_t *out);
void png_free(image_t *img);

/* ===== JPEG ===== */
#define JPEG_SOI            0xFFD8
#define JPEG_EOI            0xFFD9
#define JPEG_APP0           0xFFE0
#define JPEG_DQT            0xFFDB
#define JPEG_SOF0           0xFFC0
#define JPEG_DHT            0xFFC4
#define JPEG_SOS            0xFFDA
#define JPEG_DRI            0xFFDD
#define JPEG_COM            0xFFFE

/* JPEG component */
typedef struct {
    uint8 id;
    uint8 sampling_h;
    uint8 sampling_v;
    uint8 quant_table_id;
    uint8 dc_huffman_id;
    uint8 ac_huffman_id;
} jpeg_component_t;

/* JPEG decoder state */
typedef struct {
    uint16 width, height;
    uint8 num_components;
    uint8 precision;
    jpeg_component_t components[4];
    uint8 quant_tables[4][64];
    /* Huffman tables simplified */
    uint16 restart_interval;
    const uint8 *data;
    uint32 size;
    uint32 pos;
    uint8 bit_pos;
    uint8 prev_byte;
} jpeg_state_t;

int jpeg_decode(const uint8 *data, uint32 size, image_t *out);
void jpeg_free(image_t *img);

/* ===== Icon Cache ===== */
#define ICON_CACHE_SIZE     64
#define ICON_MAX_SIZE       256

typedef struct {
    uint32 hash;        /* Path hash */
    image_t image;
    uint64 last_used;
    uint32 ref_count;
} icon_cache_entry_t;

void icon_cache_init(void);
icon_cache_entry_t *icon_cache_lookup(const char *path);
void icon_cache_store(const char *path, const image_t *img);
void icon_cache_invalidate(const char *path);
void icon_cache_flush(void);

/* ===== Image Scaling ===== */
image_t *image_scale_nearest(const image_t *src, int new_w, int new_h);
image_t *image_scale_bilinear(const image_t *src, int new_w, int new_h);
image_t *image_scale_bicubic(const image_t *src, int new_w, int new_h);
void image_scale_inplace(const image_t *src, image_t *dst);

/* ===== Image Processing ===== */
void image_draw(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y);
void image_draw_scaled(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y, int w, int h);
void image_draw_clipped(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y, int sx, int sy, int sw, int sh);
void image_free(image_t *img);
image_t *image_create(int width, int height);
image_t *image_clone(const image_t *src);
void image_fill(image_t *img, uint32 color);
void image_clear(image_t *img);

/* ===== Image Loader ===== */
int image_load(const char *filename, image_t *out);
int image_load_from_memory(const uint8 *data, uint32 size, image_t *out);
void image_get_info(const char *filename, int *width, int *height, int *format);

/* ===== Color conversion ===== */
uint32 rgb_to_argb(uint8 r, uint8 g, uint8 b);
uint32 rgba_to_argb(uint32 rgba);
void argb_to_rgba(uint32 argb, uint8 *r, uint8 *g, uint8 *b, uint8 *a);

#endif
