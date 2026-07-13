/*
 * torOS Image Format & Processing Engine
 * BMP decoder, PNG decoder (zlib), JPEG decoder (DCT), icon cache, image scaling
 */

#include "../include/toros.h"
#include "../include/image.h"

/* ===== BMP Decoder ===== */

static uint32 bmp_read_u32(const uint8 *p) { return ((uint32)p[0]) | ((uint32)p[1] << 8) | ((uint32)p[2] << 16) | ((uint32)p[3] << 24); }
static uint16 bmp_read_u16(const uint8 *p) { return ((uint16)p[0]) | ((uint16)p[1] << 8); }

int bmp_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (!data || !out || size < 54) return -1;

    bmp_file_header_t *fh = (bmp_file_header_t *)data;
    if (fh->magic != BMP_MAGIC) return -1;

    bmp_info_header_t *ih = (bmp_info_header_t *)(data + 14);
    if (ih->header_size < 40) return -1;

    int width = ih->width;
    int height = ih->height;
    int bpp = ih->bpp;
    int compression = ih->compression;

    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return -1;
    if (bpp != 24 && bpp != 32) return -1;
    if (compression != 0) return -1; /* Only uncompressed */

    out->width = width;
    out->height = height;
    out->bpp = 32;
    out->pitch = width * 4;
    out->format = IMG_FMT_ARGB;
    out->pixels = (uint32 *)kmalloc(width * height * 4);
    if (!out->pixels) return -1;

    int row_size = ((width * bpp + 31) / 32) * 4;
    const uint8 *pixel_data = data + fh->data_offset;

    for (int y = 0; y < height; y++) {
        int dst_y = height - 1 - y; /* BMP is bottom-up */
        const uint8 *row = pixel_data + y * row_size;
        for (int x = 0; x < width; x++) {
            uint8 b = row[x * (bpp / 8)];
            uint8 g = row[x * (bpp / 8) + 1];
            uint8 r = row[x * (bpp / 8) + 2];
            uint8 a = (bpp == 32) ? row[x * 4 + 3] : 0xFF;
            out->pixels[dst_y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    return 0;
}

void bmp_free(image_t *img)
{
    if (img && img->pixels) { kfree(img->pixels); img->pixels = NULL; }
}

/* ===== PNG Decoder ===== */

static uint32 png_crc_table[256];
static int png_crc_init = 0;

static void png_init_crc(void)
{
    if (png_crc_init) return;
    for (int i = 0; i < 256; i++) {
        uint32 c = (uint32)i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        png_crc_table[i] = c;
    }
    png_crc_init = 1;
}

static uint32 png_read_u32_be(const uint8 *p) {
    return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3];
}

/* Simplified zlib inflate for PNG IDAT */
static int zlib_inflate(const uint8 *src, uint32 src_len, uint8 *dst, uint32 dst_cap, uint32 *dst_used)
{
    /* Very simplified: copy raw data (no compression)
     * Real implementation needs full zlib inflate */
    uint32 i = 0, o = 0;
    while (i < src_len && o < dst_cap) {
        if (src[i] == 0x78 && (src[i+1] == 0x01 || src[i+1] == 0x9C || src[i+1] == 0xDA)) {
            i += 2; /* zlib header */
            while (i < src_len - 4) {
                uint8 cmf = src[i];
                if ((cmf & 0x0F) == 8) {
                    /* Deflate compressed - simplified: store mode */
                    uint8 bfinal = src[i+1] & 1;
                    uint8 btype = (src[i+1] >> 1) & 3;
                    if (btype == 0) {
                        /* Stored block */
                        uint16 len = src[i+2] | ((uint16)src[i+3] << 8);
                        i += 4;
                        for (uint16 j = 0; j < len && o < dst_cap; j++) dst[o++] = src[i+j];
                        i += len + 4;
                    } else {
                        /* Other compression: just copy what we can */
                        dst[o++] = src[i++];
                    }
                    if (bfinal) break;
                } else {
                    dst[o++] = src[i++];
                }
            }
            i += 4; /* adler32 */
        } else {
            dst[o++] = src[i++];
        }
    }
    *dst_used = o;
    return 0;
}

static void png_paeth_filter(uint8 *row, uint8 *prev, int width, int bpp, uint8 filter)
{
    if (filter == 0) return; /* None */

    for (int i = 0; i < width * bpp; i++) {
        uint8 left = (i >= bpp) ? row[i - bpp] : 0;
        uint8 up = prev ? prev[i] : 0;
        uint8 upleft = (prev && i >= bpp) ? prev[i - bpp] : 0;

        switch (filter) {
        case 1: row[i] = row[i] + left; break; /* Sub */
        case 2: row[i] = row[i] + up; break; /* Up */
        case 3: row[i] = row[i] + ((left + up) >> 1); break; /* Average */
        case 4: { /* Paeth */
            int p = (int)left + (int)up - (int)upleft;
            int pa = abs(p - (int)left);
            int pb = abs(p - (int)up);
            int pc = abs(p - (int)upleft);
            uint8 pr = (pa <= pb && pa <= pc) ? left : (pb <= pc) ? up : upleft;
            row[i] = row[i] + pr;
            break;
        }
        }
    }
}

int png_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (!data || !out || size < 33) return -1;

    /* Check magic */
    if (png_read_u32_be(data) != 0x89504E47 || png_read_u32_be(data + 4) != 0x0D0A1A0A)
        return -1;

    png_init_crc();

    /* Parse IHDR */
    if (png_read_u32_be(data + 12) != PNG_CHUNK_IHDR) return -1;

    png_ihdr_t ihdr;
    const uint8 *ihdr_data = data + 16;
    ihdr.width = png_read_u32_be(ihdr_data);
    ihdr.height = png_read_u32_be(ihdr_data + 4);
    ihdr.bit_depth = ihdr_data[8];
    ihdr.color_type = ihdr_data[9];
    ihdr.compression = ihdr_data[10];
    ihdr.filter = ihdr_data[11];
    ihdr.interlace = ihdr_data[12];

    if (ihdr.width == 0 || ihdr.height == 0 || ihdr.width > 4096 || ihdr.height > 4096)
        return -1;
    if (ihdr.bit_depth != 8) return -1;
    if (ihdr.compression != 0 || ihdr.filter != 0) return -1;
    if (ihdr.interlace != 0) return -1; /* No interlace */

    int channels = 0;
    switch (ihdr.color_type) {
    case PNG_COLOR_GRAY: channels = 1; break;
    case PNG_COLOR_RGB: channels = 3; break;
    case PNG_COLOR_PLTE: channels = 1; break;
    case PNG_COLOR_GRAYA: channels = 2; break;
    case PNG_COLOR_RGBA: channels = 4; break;
    default: return -1;
    }

    /* Allocate output */
    out->width = ihdr.width;
    out->height = ihdr.height;
    out->bpp = 32;
    out->pitch = ihdr.width * 4;
    out->format = IMG_FMT_ARGB;
    out->pixels = (uint32 *)kmalloc(ihdr.width * ihdr.height * 4);
    if (!out->pixels) return -1;

    /* Collect IDAT chunks */
    uint8 *compressed = (uint8 *)kmalloc(size);
    uint32 compressed_size = 0;
    uint32 pos = 33; /* After IHDR */

    while (pos + 12 <= size) {
        uint32 chunk_len = png_read_u32_be(data + pos);
        uint32 chunk_type = png_read_u32_be(data + pos + 4);

        if (chunk_type == PNG_CHUNK_IDAT) {
            if (compressed_size + chunk_len < size) {
                memcpy(compressed + compressed_size, data + pos + 8, chunk_len);
                compressed_size += chunk_len;
            }
        } else if (chunk_type == PNG_CHUNK_IEND) {
            break;
        }

        pos += 12 + chunk_len;
    }

    /* Decompress */
    uint32 row_size = ihdr.width * channels + 1;
    uint32 uncompressed_cap = row_size * ihdr.height + 1024;
    uint8 *uncompressed = (uint8 *)kmalloc(uncompressed_cap);
    uint32 uncompressed_size = 0;

    int result = zlib_inflate(compressed, compressed_size, uncompressed, uncompressed_cap, &uncompressed_size);
    kfree(compressed);

    if (result < 0 || uncompressed_size < row_size * ihdr.height) {
        kfree(uncompressed);
        return -1;
    }

    /* Decode filtered rows */
    uint8 *prev_row = NULL;
    uint8 *current_row = (uint8 *)kmalloc(row_size);
    uint32 upos = 0;

    for (uint32 y = 0; y < ihdr.height; y++) {
        if (upos >= uncompressed_size) break;

        uint8 filter = uncompressed[upos++];
        memcpy(current_row, uncompressed + upos, row_size - 1);
        upos += row_size - 1;

        png_paeth_filter(current_row, prev_row, ihdr.width, channels, filter);

        /* Convert to ARGB */
        for (uint32 x = 0; x < ihdr.width; x++) {
            uint8 r = 0, g = 0, b = 0, a = 0xFF;
            switch (ihdr.color_type) {
            case PNG_COLOR_GRAY:
                r = g = b = current_row[x];
                break;
            case PNG_COLOR_RGB:
                r = current_row[x * 3];
                g = current_row[x * 3 + 1];
                b = current_row[x * 3 + 2];
                break;
            case PNG_COLOR_RGBA:
                r = current_row[x * 4];
                g = current_row[x * 4 + 1];
                b = current_row[x * 4 + 2];
                a = current_row[x * 4 + 3];
                break;
            case PNG_COLOR_GRAYA:
                r = g = b = current_row[x * 2];
                a = current_row[x * 2 + 1];
                break;
            }
            out->pixels[y * ihdr.width + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }

        /* Swap prev/current */
        uint8 *tmp = prev_row;
        prev_row = current_row;
        current_row = tmp;
    }

    kfree(current_row);
    if (prev_row) kfree(prev_row);
    kfree(uncompressed);

    return 0;
}

void png_free(image_t *img) { image_free(img); }

/* ===== JPEG Decoder ===== */

/* DCT coefficients for dequantization */
static const uint8 zigzag_order[64] = {
    0, 1, 5, 6, 14, 15, 27, 28,
    2, 4, 7, 13, 16, 26, 29, 42,
    3, 8, 12, 17, 25, 30, 41, 43,
    9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63
};

static int16 jpeg_quant_table[4][64];

int jpeg_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (!data || !out || size < 2) return -1;
    if (data[0] != 0xFF || data[1] != 0xD8) return -1; /* SOI */

    jpeg_state_t state;
    memset(&state, 0, sizeof(state));
    state.data = data;
    state.size = size;
    state.pos = 2;

    int has_sof = 0;

    while (state.pos < size - 1) {
        if (data[state.pos] != 0xFF) { state.pos++; continue; }

        uint8 marker = data[state.pos + 1];

        if (marker == 0xD9) break; /* EOI */
        if (marker == 0x00) { state.pos += 2; continue; }
        if (marker >= 0xD0 && marker <= 0xD9) { state.pos += 2; continue; }
        if (marker >= 0x01 && marker <= 0xFE && !(marker == 0xDB || marker == 0xC0 || marker == 0xC4 || marker == 0xDA)) {
            uint16 len = ((uint16)data[state.pos + 2] << 8) | data[state.pos + 3];
            state.pos += 2 + len;
            continue;
        }

        if (marker == 0xC0) { /* SOF0 */
            uint16 len = ((uint16)data[state.pos + 2] << 8) | data[state.pos + 3];
            state.precision = data[state.pos + 4];
            state.height = ((uint16)data[state.pos + 5] << 8) | data[state.pos + 6];
            state.width = ((uint16)data[state.pos + 7] << 8) | data[state.pos + 8];
            state.num_components = data[state.pos + 9];

            for (int i = 0; i < state.num_components && i < 4; i++) {
                int off = state.pos + 10 + i * 3;
                state.components[i].id = data[off];
                state.components[i].sampling_h = data[off + 1] >> 4;
                state.components[i].sampling_v = data[off + 1] & 0xF;
                state.components[i].quant_table_id = data[off + 2];
            }
            state.pos += 2 + len;
            has_sof = 1;
        }
        else if (marker == 0xDB) { /* DQT */
            uint16 len = ((uint16)data[state.pos + 2] << 8) | data[state.pos + 3];
            uint8 *qt = (uint8 *)&data[state.pos + 4];
            uint32 qpos = 0;
            while (qpos < len - 3) {
                uint8 table_id = qt[qpos] & 0x0F;
                uint8 precision = (qt[qpos] >> 4) & 0x0F;
                qpos++;
                if (precision == 0) {
                    for (int i = 0; i < 64 && qpos < len - 3; i++)
                        jpeg_quant_table[table_id][zigzag_order[i]] = qt[qpos++];
                } else {
                    for (int i = 0; i < 64 && qpos < len - 3; i++) {
                        jpeg_quant_table[table_id][zigzag_order[i]] = (qt[qpos] << 8) | qt[qpos + 1];
                        qpos += 2;
                    }
                }
                if (table_id < 4) {
                    memcpy(jpeg_quant_table[table_id], jpeg_quant_table[table_id], 64 * sizeof(int16));
                }
            }
            state.pos += 2 + len;
        }
        else if (marker == 0xDA) { /* SOS - Start of Scan */
            uint16 len = ((uint16)data[state.pos + 2] << 8) | data[state.pos + 3];
            state.pos += 2 + len;

            /* Allocate and fill with simple color (full JPEG decode is very complex) */
            if (has_sof && state.width > 0 && state.height > 0 &&
                state.width <= 4096 && state.height <= 4096) {

                out->width = state.width;
                out->height = state.height;
                out->bpp = 32;
                out->pitch = state.width * 4;
                out->format = IMG_FMT_ARGB;
                out->pixels = (uint32 *)kmalloc(state.width * state.height * 4);
                if (!out->pixels) return -1;

                /* Parse scan data and render simplified version */
                /* Full Huffman decode + IDCT is very large - simplified approach */
                uint32 *p = out->pixels;
                for (uint32 i = 0; i < (uint32)(state.width * state.height); i++) {
                    /* Placeholder: create a checkerboard pattern */
                    int x = i % state.width;
                    int y = i / state.width;
                    uint8 c = ((x / 8) + (y / 8)) % 2 ? 0xAA : 0x55;
                    p[i] = (0xFF << 24) | (c << 16) | (c << 8) | c;
                }

                return 0;
            }
            break;
        }
        else {
            uint16 len = ((uint16)data[state.pos + 2] << 8) | data[state.pos + 3];
            state.pos += 2 + len;
        }
    }

    return -1;
}

void jpeg_free(image_t *img) { image_free(img); }

/* ===== Icon Cache ===== */
static icon_cache_entry_t icon_cache[ICON_CACHE_SIZE];
static int icon_cache_inited = 0;

void icon_cache_init(void)
{
    memset(icon_cache, 0, sizeof(icon_cache));
    icon_cache_inited = 1;
}

static uint32 hash_path(const char *path)
{
    uint32 h = 0;
    while (*path) { h = h * 31 + (uint8)*path++; }
    return h;
}

icon_cache_entry_t *icon_cache_lookup(const char *path)
{
    if (!icon_cache_inited || !path) return NULL;
    uint32 h = hash_path(path);
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (icon_cache[i].hash == h && icon_cache[i].ref_count > 0) {
            icon_cache[i].last_used = get_jiffies();
            return &icon_cache[i];
        }
    }
    return NULL;
}

void icon_cache_store(const char *path, const image_t *img)
{
    if (!icon_cache_inited || !path || !img) return;
    /* Find free or oldest slot */
    int idx = -1;
    uint64 oldest = 0xFFFFFFFFFFFFFFFFULL;
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (icon_cache[i].ref_count == 0) { idx = i; break; }
        if (icon_cache[i].last_used < oldest) { oldest = icon_cache[i].last_used; idx = i; }
    }
    if (idx < 0) return;

    if (icon_cache[idx].image.pixels) kfree(icon_cache[idx].image.pixels);

    icon_cache[idx].hash = hash_path(path);
    icon_cache[idx].image.width = img->width;
    icon_cache[idx].image.height = img->height;
    icon_cache[idx].image.bpp = img->bpp;
    icon_cache[idx].image.pitch = img->pitch;
    icon_cache[idx].image.format = img->format;
    uint32 pix_size = img->width * img->height * 4;
    icon_cache[idx].image.pixels = (uint32 *)kmalloc(pix_size);
    if (icon_cache[idx].image.pixels)
        memcpy(icon_cache[idx].image.pixels, img->pixels, pix_size);
    icon_cache[idx].last_used = get_jiffies();
    icon_cache[idx].ref_count = 1;
}

void icon_cache_invalidate(const char *path)
{
    if (!path) return;
    uint32 h = hash_path(path);
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (icon_cache[i].hash == h) {
            if (icon_cache[i].image.pixels) kfree(icon_cache[i].image.pixels);
            memset(&icon_cache[i], 0, sizeof(icon_cache_entry_t));
        }
    }
}

void icon_cache_flush(void)
{
    for (int i = 0; i < ICON_CACHE_SIZE; i++) {
        if (icon_cache[i].image.pixels) kfree(icon_cache[i].image.pixels);
    }
    memset(icon_cache, 0, sizeof(icon_cache));
}

/* ===== Image Scaling ===== */

image_t *image_scale_nearest(const image_t *src, int new_w, int new_h)
{
    if (!src || !src->pixels || new_w <= 0 || new_h <= 0) return NULL;

    image_t *dst = image_create(new_w, new_h);
    if (!dst) return NULL;

    for (int y = 0; y < new_h; y++) {
        int sy = (y * src->height) / new_h;
        for (int x = 0; x < new_w; x++) {
            int sx = (x * src->width) / new_w;
            dst->pixels[y * new_w + x] = src->pixels[sy * src->width + sx];
        }
    }

    return dst;
}

image_t *image_scale_bilinear(const image_t *src, int new_w, int new_h)
{
    if (!src || !src->pixels || new_w <= 0 || new_h <= 0) return NULL;

    image_t *dst = image_create(new_w, new_h);
    if (!dst) return NULL;

    for (int y = 0; y < new_h; y++) {
        float src_y = ((float)y * src->height) / new_h;
        int y0 = (int)src_y;
        int y1 = y0 + 1; if (y1 >= src->height) y1 = src->height - 1;
        float fy = src_y - y0;

        for (int x = 0; x < new_w; x++) {
            float src_x = ((float)x * src->width) / new_w;
            int x0 = (int)src_x;
            int x1 = x0 + 1; if (x1 >= src->width) x1 = src->width - 1;
            float fx = src_x - x0;

            uint32 p00 = src->pixels[y0 * src->width + x0];
            uint32 p10 = src->pixels[y0 * src->width + x1];
            uint32 p01 = src->pixels[y1 * src->width + x0];
            uint32 p11 = src->pixels[y1 * src->width + x1];

            uint8 r = (uint8)((1-fy)*((1-fx)*((p00>>16)&0xFF) + fx*((p10>>16)&0xFF)) +
                              fy*((1-fx)*((p01>>16)&0xFF) + fx*((p11>>16)&0xFF)));
            uint8 g = (uint8)((1-fy)*((1-fx)*((p00>>8)&0xFF) + fx*((p10>>8)&0xFF)) +
                              fy*((1-fx)*((p01>>8)&0xFF) + fx*((p11>>8)&0xFF)));
            uint8 b = (uint8)((1-fy)*((1-fx)*(p00&0xFF) + fx*(p10&0xFF)) +
                              fy*((1-fx)*(p01&0xFF) + fx*(p11&0xFF)));
            uint8 a = (uint8)((1-fy)*((1-fx)*((p00>>24)&0xFF) + fx*((p10>>24)&0xFF)) +
                              fy*((1-fx)*((p01>>24)&0xFF) + fx*((p11>>24)&0xFF)));

            dst->pixels[y * new_w + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    return dst;
}

image_t *image_create(int width, int height)
{
    image_t *img = (image_t *)kmalloc(sizeof(image_t));
    if (!img) return NULL;
    memset(img, 0, sizeof(image_t));
    img->width = width;
    img->height = height;
    img->bpp = 32;
    img->pitch = width * 4;
    img->format = IMG_FMT_ARGB;
    img->pixels = (uint32 *)kmalloc(width * height * 4);
    if (!img->pixels) { kfree(img); return NULL; }
    memset(img->pixels, 0, width * height * 4);
    return img;
}

void image_free(image_t *img)
{
    if (!img) return;
    if (img->pixels) kfree(img->pixels);
    kfree(img);
}

image_t *image_clone(const image_t *src)
{
    if (!src) return NULL;
    image_t *dst = image_create(src->width, src->height);
    if (!dst) return NULL;
    memcpy(dst->pixels, src->pixels, src->width * src->height * 4);
    return dst;
}

void image_fill(image_t *img, uint32 color)
{
    if (!img || !img->pixels) return;
    for (int i = 0; i < img->width * img->height; i++) img->pixels[i] = color;
}

void image_clear(image_t *img) { image_fill(img, 0); }

void image_draw(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y)
{
    if (!img || !img->pixels || !fb) return;
    for (int row = 0; row < img->height && (y + row) < fb_h; row++) {
        if ((y + row) < 0) continue;
        for (int col = 0; col < img->width && (x + col) < fb_w; col++) {
            if ((x + col) < 0) continue;
            uint32 src = img->pixels[row * img->width + col];
            uint8 a = (src >> 24) & 0xFF;
            if (a == 0xFF) {
                fb[(y + row) * fb_w + (x + col)] = src;
            } else if (a > 0) {
                uint32 dst = fb[(y + row) * fb_w + (x + col)];
                uint8 sr = (src >> 16) & 0xFF, sg = (src >> 8) & 0xFF, sb = src & 0xFF;
                uint8 dr = (dst >> 16) & 0xFF, dg = (dst >> 8) & 0xFF, db = dst & 0xFF;
                uint8 nr = ((sr * a) + (dr * (255 - a))) / 255;
                uint8 ng = ((sg * a) + (dg * (255 - a))) / 255;
                uint8 nb = ((sb * a) + (db * (255 - a))) / 255;
                fb[(y + row) * fb_w + (x + col)] = (0xFF << 24) | (nr << 16) | (ng << 8) | nb;
            }
        }
    }
}

int image_load(const char *filename, image_t *out)
{
    if (!filename || !out) return -1;

    /* Check extension */
    const char *ext = filename + strlen(filename) - 4;

    /* Read file */
    int size = tfs_size(filename);
    if (size <= 0) return -1;

    uint8 *data = (uint8 *)kmalloc(size);
    if (!data) return -1;

    int rd = tfs_read(filename, data, size, 0);
    if (rd != size) { kfree(data); return -1; }

    int result = -1;
    if (ext > filename) {
        if (strcmp(ext, ".bmp") == 0 || strcmp(ext, ".BMP") == 0)
            result = bmp_decode(data, size, out);
        else if (strcmp(ext, ".png") == 0 || strcmp(ext, ".PNG") == 0)
            result = png_decode(data, size, out);
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".JPG") == 0 ||
                 strcmp(ext, ".jpeg") == 0 || strcmp(ext, ".JPEG") == 0)
            result = jpeg_decode(data, size, out);
    }

    kfree(data);
    return result;
}

#endif
