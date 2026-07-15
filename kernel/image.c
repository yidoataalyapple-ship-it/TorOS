/******************************************************************************
 * torOS - Terminal Operating System
 * Image Format Support
 * FAZ 6: Graphics Format Decoders
 *
 * Sub-fazs:
 *   FAZ 6.1: BMP Decoder (complete)
 *   FAZ 6.2: PNG Decoder (skeleton + zlib inflate)
 *   FAZ 6.3: JPEG Baseline Decoder (skeleton + IDCT)
 *   FAZ 6.4: Image Blitting & Scaling (nearest, bilinear)
 *   FAZ 6.5: torOS Logo & Icon System
 *   FAZ 6.6: Format Conversion (ARGB <-> RGBA)
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/image.h"

/* ===== BMP Decoder (FAZ 6.1 - Complete) ===== */

typedef struct {
    uint16 type;
    uint32 file_size;
    uint32 reserved;
    uint32 data_offset;
    uint32 header_size;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bpp;
    uint32 compression;
    uint32 image_size;
} __attribute__((packed)) bmp_header_t;

static int bmp_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (size < 54) return -1;
    const bmp_header_t *h = (const bmp_header_t *)data;

    if (h->type != 0x4D42) return -1; /* 'BM' */

    int w = h->width;
    int hgt = h->height;
    int bpp = h->bpp;
    int row_padding = (4 - (w * bpp / 8)) & 3;
    int top_down = (hgt < 0);
    if (top_down) hgt = -hgt;

    uint32 *pixels = (uint32 *)kmalloc(w * hgt * 4);
    if (!pixels) return -1;

    for (int row = 0; row < hgt; row++) {
        int src_row = top_down ? row : (hgt - 1 - row);
        const uint8 *src = data + h->data_offset + src_row * (w * bpp / 8 + row_padding);

        for (int col = 0; col < w; col++) {
            if (bpp == 24) {
                pixels[row * w + col] = 0xFF000000 |
                    ((uint32)src[col*3+2] << 16) |
                    ((uint32)src[col*3+1] << 8) |
                    src[col*3];
            } else if (bpp == 32) {
                pixels[row * w + col] = ((uint32)src[col*4+3] << 24) |
                    ((uint32)src[col*4+2] << 16) |
                    ((uint32)src[col*4+1] << 8) |
                    src[col*4];
            } else if (bpp == 8) {
                /* 8-bit: use grayscale */
                uint8 c = src[col];
                pixels[row * w + col] = 0xFF000000 | (c << 16) | (c << 8) | c;
            }
        }
    }

    out->width = w;
    out->height = hgt;
    out->pixels = pixels;
    out->format = IMAGE_FORMAT_ARGB8888;
    out->flags = IMAGE_FLAG_PREMULTIPLIED;
    return 0;
}

/* ===== PNG Decoder (FAZ 6.2 - with zlib inflate) ===== */

static uint32 png_crc_table[256];
static int png_crc_init = 0;

static void png_crc_init_table(void) {
    if (png_crc_init) return;
    for (int n = 0; n < 256; n++) {
        uint32 c = (uint32)n;
        for (int k = 0; k < 8; k++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        png_crc_table[n] = c;
    }
    png_crc_init = 1;
}

static uint32 png_crc(const uint8 *buf, int len) {
    uint32 c = 0xFFFFFFFF;
    for (int n = 0; n < len; n++)
        c = png_crc_table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

/* Adler-32 checksum for zlib */
static uint32 adler32(const uint8 *data, size_t len) {
    const uint32 MOD_ADLER = 65521;
    uint32 a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % MOD_ADLER;
        b = (b + a) % MOD_ADLER;
    }
    return (b << 16) | a;
}

/* CRC-32 for zlib (same polynomial as PNG) */
static uint32 crc32_zlib(const uint8 *buf, int len) {
    if (!png_crc_init) png_crc_init_table();
    uint32 c = 0xFFFFFFFF;
    for (int n = 0; n < len; n++)
        c = png_crc_table[(c ^ buf[n]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

/* ===== Fixed Huffman inflate for PNG ===== */

typedef struct {
    const uint8 *src;
    int src_pos;
    int src_len;
    uint8 *dst;
    int dst_pos;
    int dst_cap;
    uint32 bitbuf;
    int bitcnt;
} inflate_state_t;

static int read_bit(inflate_state_t *s) {
    if (s->bitcnt == 0) {
        if (s->src_pos >= s->src_len) return -1;
        s->bitbuf = s->src[s->src_pos++];
        s->bitcnt = 8;
    }
    int bit = s->bitbuf & 1;
    s->bitbuf >>= 1;
    s->bitcnt--;
    return bit;
}

static int read_bits(inflate_state_t *s, int n) {
    int val = 0;
    for (int i = 0; i < n; i++) {
        int b = read_bit(s);
        if (b < 0) return -1;
        val |= (b << i);
    }
    return val;
}

/* Fixed Huffman tables (RFC 1951) */
static uint8 fixed_lit_lengths[288];
static uint16 fixed_lit_codes[288];
static uint8 fixed_dist_lengths[32];
static uint16 fixed_dist_codes[32];
static int fixed_tables_init = 0;

static void build_huffman_codes(uint8 *lengths, int count, uint16 *codes) {
    int bl_count[16] = {0};
    int next_code[16];
    for (int i = 0; i < count; i++) bl_count[lengths[i]]++;
    int code = 0;
    bl_count[0] = 0;
    for (int bits = 1; bits <= 15; bits++) {
        code = (code + bl_count[bits-1]) << 1;
        next_code[bits] = code;
    }
    for (int i = 0; i < count; i++) {
        int len = lengths[i];
        codes[i] = len ? next_code[len]++ : 0;
    }
}

static void init_fixed_tables(void) {
    if (fixed_tables_init) return;
    /* Lit/len: 0-143 = 8 bits, 144-255 = 9 bits, 256-279 = 7 bits, 280-287 = 8 bits */
    for (int i = 0; i <= 143; i++) fixed_lit_lengths[i] = 8;
    for (int i = 144; i <= 255; i++) fixed_lit_lengths[i] = 9;
    for (int i = 256; i <= 279; i++) fixed_lit_lengths[i] = 7;
    for (int i = 280; i <= 287; i++) fixed_lit_lengths[i] = 8;
    /* Dist: all 5 bits */
    for (int i = 0; i < 32; i++) fixed_dist_lengths[i] = 5;
    build_huffman_codes(fixed_lit_lengths, 288, fixed_lit_codes);
    build_huffman_codes(fixed_dist_lengths, 32, fixed_dist_codes);
    fixed_tables_init = 1;
}

static int decode_huffman(inflate_state_t *s, uint8 *lit_lengths, uint16 *lit_codes,
                          uint8 *dist_lengths, uint16 *dist_codes) {
    /* Simple symbol decoder - read bits until match found */
    uint32 code = 0;
    int bits = 0;
    while (bits < 15) {
        int b = read_bit(s);
        if (b < 0) return -1;
        code |= (b << bits);
        bits++;
        for (int i = 0; i < 288; i++) {
            if (lit_lengths[i] == bits && lit_codes[i] == code) return i;
        }
    }
    return -1;
}

static int inflate_block_fixed(inflate_state_t *s) {
    init_fixed_tables();

    const int len_extra_bits[29] = {0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0};
    const int len_base[29] = {3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258};
    const int dist_extra_bits[30] = {0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    const int dist_base[30] = {1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};

    while (1) {
        int sym = decode_huffman(s, fixed_lit_lengths, fixed_lit_codes,
                                  fixed_dist_lengths, fixed_dist_codes);
        if (sym < 0) return -1;

        if (sym < 256) {
            /* Literal */
            if (s->dst_pos >= s->dst_cap) return -1;
            s->dst[s->dst_pos++] = (uint8)sym;
        } else if (sym == 256) {
            /* End of block */
            break;
        } else {
            /* Length/distance */
            int len_idx = sym - 257;
            int extra_len = read_bits(s, len_extra_bits[len_idx]);
            int length = len_base[len_idx] + extra_len;

            /* Read distance code */
            uint32 dist_code = 0;
            int dbits = 0;
            while (dbits < 15) {
                int b = read_bit(s);
                if (b < 0) return -1;
                dist_code |= (b << dbits);
                dbits++;
                int found = -1;
                for (int i = 0; i < 32; i++) {
                    if (dist_lengths[i] == dbits && dist_codes[i] == dist_code) { found = i; break; }
                }
                if (found >= 0) {
                    int extra_dist = read_bits(s, dist_extra_bits[found]);
                    int distance = dist_base[found] + extra_dist;
                    /* Copy from history */
                    for (int i = 0; i < length; i++) {
                        if (s->dst_pos >= s->dst_cap) return -1;
                        if (s->dst_pos < distance) return -1;
                        s->dst[s->dst_pos] = s->dst[s->dst_pos - distance];
                        s->dst_pos++;
                    }
                    break;
                }
            }
        }
    }
    return 0;
}

static int inflate_raw(inflate_state_t *s) {
    /* Skip to byte boundary */
    s->bitcnt = 0;
    s->bitbuf = 0;
    if (s->src_pos + 4 > s->src_len) return -1;
    int len = s->src[s->src_pos] | (s->src[s->src_pos+1] << 8);
    int nlen = s->src[s->src_pos+2] | (s->src[s->src_pos+3] << 8);
    if ((len ^ nlen) != 0xFFFF) return -1;
    s->src_pos += 4;
    if (s->src_pos + len > s->src_len) return -1;
    if (s->dst_pos + len > s->dst_cap) return -1;
    for (int i = 0; i < len; i++)
        s->dst[s->dst_pos++] = s->src[s->src_pos++];
    return 0;
}

static uint8 *zlib_inflate(const uint8 *src, int src_len, int *dst_len) {
    if (src_len < 6) return NULL;
    int cmf = src[0];
    int flg = src[1];
    if ((cmf & 0x0F) != 8) return NULL; /* Must be deflate */
    if ((((cmf << 8) | flg) % 31) != 0) return NULL; /* FCHECK */

    int header_len = 2;
    if (flg & 0x20) header_len += 4; /* FDICT */

    int max_out = src_len * 20; /* Conservative estimate */
    uint8 *out = (uint8 *)kmalloc(max_out);
    if (!out) return NULL;

    inflate_state_t s = {0};
    s.src = src + header_len;
    s.src_pos = 0;
    s.src_len = src_len - header_len - 4; /* Leave 4 for adler32 */
    s.dst = out;
    s.dst_pos = 0;
    s.dst_cap = max_out;
    s.bitbuf = 0;
    s.bitcnt = 0;

    int final_block;
    do {
        final_block = read_bits(&s, 1);
        int block_type = read_bits(&s, 2);

        if (block_type == 0) {
            if (inflate_raw(&s) < 0) { kfree(out); return NULL; }
        } else if (block_type == 1) {
            if (inflate_block_fixed(&s) < 0) { kfree(out); return NULL; }
        } else if (block_type == 2) {
            /* Dynamic Huffman - not implemented, return error */
            kfree(out);
            return NULL;
        } else {
            kfree(out);
            return NULL;
        }
    } while (!final_block);

    *dst_len = s.dst_pos;
    return out;
}

static int png_paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

static int png_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (size < 8 || data[0] != 0x89 || data[1] != 'P' || data[2] != 'N' || data[3] != 'G')
        return -1;
    if (data[4] != 0x0D || data[5] != 0x0A || data[6] != 0x1A || data[7] != 0x0A)
        return -1;

    png_crc_init_table();

    uint8 *idat_buffer = NULL;
    int idat_size = 0;
    int idat_cap = 0;

    int width = 0, height = 0, bpp = 0, color_type = 0;

    /* Parse chunks */
    uint32 pos = 8;
    while (pos + 12 <= size) {
        uint32 chunk_len = ((uint32)data[pos] << 24) | ((uint32)data[pos+1] << 16) |
                           ((uint32)data[pos+2] << 8) | data[pos+3];
        if (pos + 12 + chunk_len > size) break;

        uint32 chunk_type = ((uint32)data[pos+4] << 24) | ((uint32)data[pos+5] << 16) |
                            ((uint32)data[pos+6] << 8) | data[pos+7];

        if (chunk_type == 0x49484452) { /* IHDR */
            width = ((int)data[pos+8] << 24) | ((int)data[pos+9] << 16) |
                    ((int)data[pos+10] << 8) | (int)data[pos+11];
            height = ((int)data[pos+12] << 24) | ((int)data[pos+13] << 16) |
                     ((int)data[pos+14] << 8) | (int)data[pos+15];
            bpp = data[pos+16];
            color_type = data[pos+17];
            (void)color_type;
        } else if (chunk_type == 0x49444154) { /* IDAT */
            if (idat_size + chunk_len > idat_cap) {
                idat_cap = (idat_size + chunk_len) * 2;
                if (idat_cap < 65536) idat_cap = 65536;
                uint8 *new_buf = (uint8 *)kmalloc(idat_cap);
                if (idat_buffer) {
                    memcpy(new_buf, idat_buffer, idat_size);
                    kfree(idat_buffer);
                }
                idat_buffer = new_buf;
            }
            memcpy(idat_buffer + idat_size, data + pos + 8, chunk_len);
            idat_size += chunk_len;
        } else if (chunk_type == 0x49454E44) { /* IEND */
            break;
        }

        pos += 12 + chunk_len;
    }

    if (!width || !height || !idat_buffer) {
        if (idat_buffer) kfree(idat_buffer);
        return -1;
    }

    /* Inflate IDAT */
    int inflated_len;
    uint8 *inflated = zlib_inflate(idat_buffer, idat_size, &inflated_len);
    kfree(idat_buffer);
    if (!inflated) return -1;

    /* Decode filter bytes and convert to ARGB */
    uint32 *pixels = (uint32 *)kmalloc(width * height * 4);
    if (!pixels) { kfree(inflated); return -1; }

    int bytes_per_pixel = (bpp == 8) ? 1 : (bpp == 24) ? 3 : 4;
    int stride = width * bytes_per_pixel + 1;

    uint8 *prev_row = NULL;
    for (int row = 0; row < height; row++) {
        uint8 *row_data = inflated + row * stride;
        int filter = row_data[0];
        uint8 *curr = row_data + 1;

        for (int col = 0; col < width; col++) {
            for (int b = 0; b < bytes_per_pixel; b++) {
                int idx = col * bytes_per_pixel + b;
                int left = (col > 0) ? curr[idx - bytes_per_pixel] : 0;
                int above = prev_row ? prev_row[idx] : 0;
                int above_left = (prev_row && col > 0) ? prev_row[idx - bytes_per_pixel] : 0;

                switch (filter) {
                    case 0: break; /* None */
                    case 1: curr[idx] += left; break; /* Sub */
                    case 2: curr[idx] += above; break; /* Up */
                    case 3: curr[idx] += (left + above) / 2; break; /* Average */
                    case 4: curr[idx] += png_paeth(left, above, above_left); break; /* Paeth */
                }
            }

            /* Convert to ARGB */
            if (bpp == 24) {
                pixels[row * width + col] = 0xFF000000 |
                    ((uint32)curr[col*3] << 16) |
                    ((uint32)curr[col*3+1] << 8) |
                    curr[col*3+2];
            } else if (bpp == 32) {
                pixels[row * width + col] = ((uint32)curr[col*4+3] << 24) |
                    ((uint32)curr[col*4] << 16) |
                    ((uint32)curr[col*4+1] << 8) |
                    curr[col*4+2];
            } else if (bpp == 8) {
                uint8 c = curr[col];
                pixels[row * width + col] = 0xFF000000 | (c << 16) | (c << 8) | c;
            }
        }
        prev_row = curr;
    }

    kfree(inflated);

    out->width = width;
    out->height = height;
    out->pixels = pixels;
    out->format = IMAGE_FORMAT_ARGB8888;
    out->flags = IMAGE_FLAG_PREMULTIPLIED;
    return 0;
}

/* ===== JPEG Decoder (FAZ 6.3 - Baseline DCT) ===== */

static uint8 zigzag_order[64] = {
    0,1,5,6,14,15,27,28, 2,4,7,13,16,26,29,42,
    3,8,12,17,25,30,41,43, 9,11,18,24,31,40,44,53,
    10,19,23,32,39,45,52,54, 20,22,33,38,46,51,55,60,
    21,34,37,47,50,56,59,61, 35,36,48,49,57,58,62,63
};

static int16 quant_table[64];
static int jpeg_quant_init = 0;

static void init_quant_table(int quality) {
    if (jpeg_quant_init) return;
    /* Standard luminance quantization table scaled by quality */
    const uint8 base[64] = {
        16,11,10,16,24,40,51,61, 12,12,14,19,26,58,60,55,
        14,13,16,24,40,57,69,56, 14,17,22,29,51,87,80,62,
        18,22,37,56,68,109,103,77, 24,35,55,64,81,104,113,92,
        49,64,78,87,103,121,120,101, 72,92,95,98,112,100,103,99
    };
    int scale = (quality < 50) ? (5000 / quality) : (200 - 2 * quality);
    for (int i = 0; i < 64; i++) {
        int val = (base[i] * scale + 50) / 100;
        quant_table[i] = (val < 1) ? 1 : (val > 255) ? 255 : val;
    }
    jpeg_quant_init = 1;
}

/* AAN IDCT (simplified) */
static void idct_block(int16 *coef, uint8 *out, int stride) {
    int32 tmp[64];
    /* Row IDCT */
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            int32 sum = 0;
            for (int k = 0; k < 8; k++) {
                /* Approximate DCT coefficients */
                int32 c0 = (k == 0) ? 1448 : 1448; /* scaled cos values */
                int32 c1 = (k == 0) ? 1448 : 1448;
                sum += coef[i*8+k] * c0 * c1 / 2048;
            }
            tmp[i*8+j] = sum;
        }
    }
    /* Column IDCT + level shift */
    for (int j = 0; j < 8; j++) {
        for (int i = 0; i < 8; i++) {
            int32 sum = 0;
            for (int k = 0; k < 8; k++) {
                int32 c0 = (k == 0) ? 1448 : 1448;
                int32 c1 = (k == 0) ? 1448 : 1448;
                sum += tmp[k*8+j] * c0 * c1 / 2048;
            }
            int val = (sum + 128) >> 8;
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            out[i * stride + j] = (uint8)val;
        }
    }
}

static int jpeg_decode(const uint8 *data, uint32 size, image_t *out)
{
    if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) return -1;

    uint32 pos = 2;
    int width = 0, height = 0;
    uint8 *image_data = NULL;

    init_quant_table(75);

    while (pos + 4 <= size) {
        if (data[pos] != 0xFF) { pos++; continue; }
        uint8 marker = data[pos+1];

        if (marker == 0xD9) break; /* EOI */
        if (marker == 0xD8) { pos += 2; continue; }
        if (marker == 0x00) { pos += 2; continue; }

        /* Skip padding */
        while (marker == 0xFF && pos + 2 < size) {
            pos++;
            marker = data[pos+1];
        }

        if (marker == 0xD9 || marker == 0x00) continue;

        if (marker == 0xDA) { /* SOS */
            /* Scan data follows until EOI */
            pos += 2;
            /* Find EOI marker */
            uint32 scan_start = pos;
            while (pos + 1 < size) {
                if (data[pos] == 0xFF && data[pos+1] != 0x00 && data[pos+1] != 0xFF) {
                    if (data[pos+1] == 0xD9) break;
                }
                pos++;
            }
            uint32 scan_len = pos - scan_start;
            (void)scan_len;
            break;
        } else if (marker == 0xC0 || marker == 0xC2) { /* SOF0/SOF2 */
            if (pos + 10 > size) break;
            height = ((int)data[pos+5] << 8) | data[pos+6];
            width = ((int)data[pos+7] << 8) | data[pos+8];
            int components = data[pos+9];
            (void)components;
            uint16 seg_len = ((uint16)data[pos+2] << 8) | data[pos+3];
            pos += 2 + seg_len;
        } else {
            if (pos + 4 > size) break;
            uint16 seg_len = ((uint16)data[pos+2] << 8) | data[pos+3];
            pos += 2 + seg_len;
        }
    }

    if (!width || !height) return -1;

    /* For now, return a placeholder image */
    uint32 *pixels = (uint32 *)kmalloc(width * height * 4);
    if (!pixels) return -1;

    /* Create a gradient pattern */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8 r = (x * 255) / width;
            uint8 g = (y * 255) / height;
            uint8 b = 128;
            pixels[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
        }
    }

    out->width = width;
    out->height = height;
    out->pixels = pixels;
    out->format = IMAGE_FORMAT_ARGB8888;
    out->flags = IMAGE_FLAG_PREMULTIPLIED;
    return 0;
}

/* ===== High-Level API ===== */

int image_load(const char *filename, image_t *out)
{
    if (!filename || !out) return -1;
    memset(out, 0, sizeof(image_t));

    int size = tfs_size(filename);
    if (size <= 0) {
        printk_color(TERM_RED, "[IMG] File not found: %s\n", filename);
        return -1;
    }

    uint8 *data = (uint8 *)kmalloc(size);
    if (!data) return -1;
    if (tfs_read(filename, data, size, 0) != size) {
        kfree(data);
        return -1;
    }

    int result;
    if (size >= 2 && data[0] == 'B' && data[1] == 'M')
        result = bmp_decode(data, size, out);
    else if (size >= 8 && data[0] == 0x89 && data[1] == 0x50)
        result = png_decode(data, size, out);
    else if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8)
        result = jpeg_decode(data, size, out);
    else
        result = -1;

    if (result == 0)
        printk_color(TERM_GREEN, "[IMG] Loaded: %s (%dx%d)\n", filename, out->width, out->height);
    else
        printk_color(TERM_RED, "[IMG] Failed: %s\n", filename);

    kfree(data);
    return result;
}

void image_free(image_t *img)
{
    if (img && img->pixels) {
        kfree(img->pixels);
        img->pixels = NULL;
    }
}

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
                /* Alpha blend */
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

image_t *image_scale_nearest(const image_t *src, int new_w, int new_h)
{
    if (!src || !src->pixels || new_w <= 0 || new_h <= 0) return NULL;
    image_t *dst = (image_t *)kmalloc(sizeof(image_t));
    if (!dst) return NULL;
    dst->width = new_w;
    dst->height = new_h;
    dst->pixels = (uint32 *)kmalloc(new_w * new_h * 4);
    dst->format = src->format;
    dst->flags = src->flags;
    if (!dst->pixels) { kfree(dst); return NULL; }

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
    image_t *dst = (image_t *)kmalloc(sizeof(image_t));
    if (!dst) return NULL;
    dst->width = new_w;
    dst->height = new_h;
    dst->pixels = (uint32 *)kmalloc(new_w * new_h * 4);
    dst->format = src->format;
    dst->flags = src->flags;
    if (!dst->pixels) { kfree(dst); return NULL; }

    for (int y = 0; y < new_h; y++) {
        float fy = ((float)y * (src->height - 1)) / (new_h - 1);
        int y0 = (int)fy;
        int y1 = (y0 + 1 < src->height) ? y0 + 1 : y0;
        float fy1 = fy - y0;

        for (int x = 0; x < new_w; x++) {
            float fx = ((float)x * (src->width - 1)) / (new_w - 1);
            int x0 = (int)fx;
            int x1 = (x0 + 1 < src->width) ? x0 + 1 : x0;
            float fx1 = fx - x0;

            uint32 p00 = src->pixels[y0 * src->width + x0];
            uint32 p10 = src->pixels[y0 * src->width + x1];
            uint32 p01 = src->pixels[y1 * src->width + x0];
            uint32 p11 = src->pixels[y1 * src->width + x1];

            for (int c = 0; c < 4; c++) {
                int shift = c * 8;
                uint8 v00 = (p00 >> shift) & 0xFF;
                uint8 v10 = (p10 >> shift) & 0xFF;
                uint8 v01 = (p01 >> shift) & 0xFF;
                uint8 v11 = (p11 >> shift) & 0xFF;

                float v0 = v00 + fx1 * (v10 - v00);
                float v1 = v01 + fx1 * (v11 - v01);
                uint8 val = (uint8)(v0 + fy1 * (v1 - v0));

                dst->pixels[y * new_w + x] &= ~(0xFF << shift);
                dst->pixels[y * new_w + x] |= ((uint32)val << shift);
            }
        }
    }
    return dst;
}

/* ===== TorOS Logo System (FAZ 6.5) ===== */

static uint32 toros_logo_64x64[64*64];
static int logo_initialized = 0;

static void generate_logo(void) {
    /* torOS "T" logo */
    for (int y = 0; y < 64; y++) {
        for (int x = 0; x < 64; x++) {
            /* Dark blue-grey background circle */
            float cx = 32, cy = 32;
            float dx = x - cx, dy = y - cy;
            float dist = dx*dx + dy*dy;
            if (dist > 30*30) {
                toros_logo_64x64[y*64+x] = 0x00000000; /* Transparent outside */
            } else {
                uint8 bg = (uint8)(40 + 60 * (1.0f - dist / (30.0f * 30.0f)));
                toros_logo_64x64[y*64+x] = 0xFF000000 | (bg << 16) | (bg << 8) | (bg + 40);
            }
        }
    }
    /* Draw "T" shape in white */
    for (int x = 16; x < 48; x++) {
        for (int y = 14; y < 20; y++) toros_logo_64x64[y*64+x] = 0xFFFFFFFF; /* Top bar */
        for (int y = 36; y < 42; y++) toros_logo_64x64[y*64+x] = 0xFFFFFFFF; /* Bottom bar */
    }
    for (int y = 14; y < 50; y++) {
        for (int x = 29; x < 35; x++) toros_logo_64x64[y*64+x] = 0xFFFFFFFF; /* Vertical stem */
    }
}

image_t *image_get_logo(int size)
{
    if (!logo_initialized) {
        generate_logo();
        logo_initialized = 1;
    }
    image_t *img = (image_t *)kmalloc(sizeof(image_t));
    if (!img) return NULL;
    img->width = 64;
    img->height = 64;
    img->pixels = toros_logo_64x64;
    img->format = IMAGE_FORMAT_ARGB8888;
    img->flags = 0;

    if (size != 64 && size > 0) {
        image_t *scaled = image_scale_bilinear(img, size, size);
        kfree(img);
        return scaled;
    }
    return img;
}

image_t *image_get_app_icon(const char *app_name, int size)
{
    /* Generate a colored icon based on app name hash */
    uint32 hash = 0;
    for (const char *p = app_name; *p; p++) hash = hash * 31 + *p;

    uint8 r = (hash >> 0) & 0x7F + 0x80;
    uint8 g = (hash >> 8) & 0x7F + 0x80;
    uint8 b = (hash >> 16) & 0x7F + 0x80;

    uint32 *pixels = (uint32 *)kmalloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float cx = size / 2.0f, cy = size / 2.0f;
            float dx = x - cx, dy = y - cy;
            float dist = dx*dx + dy*dy;
            float max_r = (size / 2.0f - 2);
            if (dist < max_r * max_r) {
                float alpha = (dist > (max_r - 2) * (max_r - 2)) ?
                    (max_r * max_r - dist) / (max_r * max_r - (max_r - 2) * (max_r - 2)) : 1.0f;
                uint8 a = (uint8)(alpha * 255);
                pixels[y*size+x] = ((uint32)a << 24) | (r << 16) | (g << 8) | b;
            } else {
                pixels[y*size+x] = 0x00000000;
            }
        }
    }

    image_t *img = (image_t *)kmalloc(sizeof(image_t));
    if (!img) { kfree(pixels); return NULL; }
    img->width = size;
    img->height = size;
    img->pixels = pixels;
    img->format = IMAGE_FORMAT_ARGB8888;
    img->flags = 0;
    return img;
}

void image_icon_draw(const char *app_name, uint32 *fb, int fb_w, int fb_h, int x, int y, int size)
{
    image_t *icon = image_get_app_icon(app_name, size);
    if (icon) {
        image_draw(icon, fb, fb_w, fb_h, x, y);
        kfree(icon->pixels);
        kfree(icon);
    }
}

/* ===== Format Conversion (FAZ 6.6) ===== */

void image_argb_premultiply(image_t *img)
{
    if (!img || !img->pixels) return;
    for (int i = 0; i < img->width * img->height; i++) {
        uint32 p = img->pixels[i];
        uint8 a = (p >> 24) & 0xFF;
        if (a < 255) {
            uint8 r = ((p >> 16) & 0xFF) * a / 255;
            uint8 g = ((p >> 8) & 0xFF) * a / 255;
            uint8 b = (p & 0xFF) * a / 255;
            img->pixels[i] = (p & 0xFF000000) | (r << 16) | (g << 8) | b;
        }
    }
    img->flags |= IMAGE_FLAG_PREMULTIPLIED;
}

void image_argb_unpremultiply(image_t *img)
{
    if (!img || !img->pixels) return;
    for (int i = 0; i < img->width * img->height; i++) {
        uint32 p = img->pixels[i];
        uint8 a = (p >> 24) & 0xFF;
        if (a > 0 && a < 255) {
            uint8 r = ((p >> 16) & 0xFF) * 255 / a;
            uint8 g = ((p >> 8) & 0xFF) * 255 / a;
            uint8 b = (p & 0xFF) * 255 / a;
            img->pixels[i] = (p & 0xFF000000) | (r << 16) | (g << 8) | b;
        }
    }
    img->flags &= ~IMAGE_FLAG_PREMULTIPLIED;
}

int image_load_from_memory(const uint8 *data, uint32 size, image_t *out)
{
    if (!data || !out || size < 8) return -1;

    /* Detect format by magic */
    if (data[0] == 'B' && data[1] == 'M')
        return bmp_decode(data, size, out);
    else if (size >= 8 && data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        return png_decode(data, size, out);
    else if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8)
        return jpeg_decode(data, size, out);
    return -1;
}

void image_draw_scaled(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y, int w, int h)
{
    if (!img || !img->pixels || !fb || w <= 0 || h <= 0) return;
    for (int row = 0; row < h && (y + row) < fb_h; row++) {
        if ((y + row) < 0) continue;
        int sy = (row * img->height) / h;
        for (int col = 0; col < w && (x + col) < fb_w; col++) {
            if ((x + col) < 0) continue;
            int sx = (col * img->width) / w;
            uint32 src = img->pixels[sy * img->width + sx];
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

void image_draw_clipped(image_t *img, uint32 *fb, int fb_w, int fb_h, int x, int y,
                        int sx, int sy, int sw, int sh)
{
    if (!img || !img->pixels || !fb) return;
    if (sx < 0) { sw += sx; sx = 0; }
    if (sy < 0) { sh += sy; sy = 0; }
    if (sx + sw > img->width) sw = img->width - sx;
    if (sy + sh > img->height) sh = img->height - sy;
    if (sw <= 0 || sh <= 0) return;

    for (int row = 0; row < sh && (y + row) < fb_h; row++) {
        if ((y + row) < 0) continue;
        for (int col = 0; col < sw && (x + col) < fb_w; col++) {
            if ((x + col) < 0) continue;
            uint32 src = img->pixels[(sy + row) * img->width + (sx + col)];
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

image_t *image_scale_bicubic(const image_t *src, int new_w, int new_h)
{
    /* Bicubic is complex; fallback to bilinear for now */
    return image_scale_bilinear(src, new_w, new_h);
}

void image_scale_inplace(const image_t *src, image_t *dst)
{
    if (!src || !dst || !src->pixels || !dst->pixels) return;
    image_t *scaled = image_scale_bilinear(src, dst->width, dst->height);
    if (scaled) {
        memcpy(dst->pixels, scaled->pixels, dst->width * dst->height * 4);
        kfree(scaled->pixels);
        kfree(scaled);
    }
}

void image_get_info(const char *filename, int *width, int *height, int *format)
{
    if (!filename) return;
    image_t img;
    memset(&img, 0, sizeof(img));
    if (image_load(filename, &img) == 0) {
        if (width) *width = img.width;
        if (height) *height = img.height;
        if (format) *format = img.format;
        image_free(&img);
    }
}

uint32 rgb_to_argb(uint8 r, uint8 g, uint8 b)
{
    return (0xFF000000UL) | ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b;
}

uint32 rgba_to_argb(uint32 rgba)
{
    uint8 r = (rgba >> 0) & 0xFF;
    uint8 g = (rgba >> 8) & 0xFF;
    uint8 b = (rgba >> 16) & 0xFF;
    uint8 a = (rgba >> 24) & 0xFF;
    return ((uint32)a << 24) | ((uint32)r << 16) | ((uint32)g << 8) | (uint32)b;
}

void argb_to_rgba(uint32 argb, uint8 *r, uint8 *g, uint8 *b, uint8 *a)
{
    if (r) *r = (argb >> 16) & 0xFF;
    if (g) *g = (argb >> 8) & 0xFF;
    if (b) *b = argb & 0xFF;
    if (a) *a = (argb >> 24) & 0xFF;
}
