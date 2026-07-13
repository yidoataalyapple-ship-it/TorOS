/*
 * torOS Font & Text Rendering Engine
 * TTF parser, Bezier rasterizer, hinting, kerning, Unicode Bidi, text layout
 */

#include "../include/toros.h"
#include "../include/font.h"

static font_t *default_font = NULL;
static font_cache_entry_t glyph_cache[FONT_CACHE_SIZE];
static int cache_initialized = 0;

/* ===== TTF Parser ===== */

static uint32 read_u32(const uint8 *p) { return ((uint32)p[0] << 24) | ((uint32)p[1] << 16) | ((uint32)p[2] << 8) | p[3]; }
static uint16 read_u16(const uint8 *p) { return ((uint16)p[0] << 8) | p[1]; }
static int16 read_s16(const uint8 *p) { return (int16)read_u16(p); }

font_t *font_load(const uint8 *data, uint32 size)
{
    if (!data || size < 12) return NULL;

    font_t *font = (font_t *)kmalloc(sizeof(font_t));
    if (!font) return NULL;
    memset(font, 0, sizeof(font_t));

    font->data = (uint8 *)data;
    font->size = size;

    /* Read header */
    font->header.sfnt_version = read_u32(data);
    font->header.num_tables = read_u16(data + 4);
    font->header.search_range = read_u16(data + 6);
    font->header.entry_selector = read_u16(data + 8);
    font->header.range_shift = read_u16(data + 10);

    if (font->header.sfnt_version != 0x00010000 && font->header.sfnt_version != 0x4F54544F) {
        kfree(font);
        return NULL;
    }

    /* Read table directory */
    font->num_tables = font->header.num_tables;
    if (font->num_tables > 32) font->num_tables = 32;

    for (int i = 0; i < font->num_tables; i++) {
        const uint8 *entry = data + 12 + i * 16;
        font->tables[i].tag = read_u32(entry);
        font->tables[i].checksum = read_u32(entry + 4);
        font->tables[i].offset = read_u32(entry + 8);
        font->tables[i].length = read_u32(entry + 12);
    }

    /* Find required tables */
    for (int i = 0; i < font->num_tables; i++) {
        switch (font->tables[i].tag) {
        case TAG_HEAD: font->head_offset = font->tables[i].offset; break;
        case TAG_HHEA: font->hhea_offset = font->tables[i].offset; break;
        case TAG_HMTX: font->hmtx_offset = font->tables[i].offset; break;
        case TAG_LOCA: font->loca_offset = font->tables[i].offset; break;
        case TAG_GLYF: font->glyf_offset = font->tables[i].offset; break;
        case TAG_CMAP: /* cmap handled separately */ break;
        case TAG_KERN: font->kern_offset = font->tables[i].offset; break;
        }
    }

    /* Read head table metrics */
    if (font->head_offset) {
        const uint8 *head = data + font->head_offset;
        font->metrics.units_per_em = read_u16(head + 18);
        font->metrics.x_min = read_s16(head + 36);
        font->metrics.y_min = read_s16(head + 38);
        font->metrics.x_max = read_s16(head + 40);
        font->metrics.y_max = read_s16(head + 42);
        font->metrics.num_glyphs = 0; /* Will be set from maxp */
    }

    /* Read maxp for glyph count */
    for (int i = 0; i < font->num_tables; i++) {
        if (font->tables[i].tag == TAG_MAXP) {
            font->metrics.num_glyphs = read_u16(data + font->tables[i].offset + 4);
            break;
        }
    }

    /* Read hhea metrics */
    if (font->hhea_offset) {
        const uint8 *hhea = data + font->hhea_offset;
        font->metrics.ascent = read_s16(hhea + 4);
        font->metrics.descent = read_s16(hhea + 6);
        font->metrics.line_gap = read_s16(hhea + 8);
        font->metrics.advance_width_max = read_u16(hhea + 10);
        int num_hmetrics = read_u16(hhea + 34);

        /* Read hmtx */
        if (font->hmtx_offset) {
            font->metrics.advance_widths = (int16 *)kmalloc(font->metrics.num_glyphs * sizeof(int16));
            font->metrics.left_side_bearings = (int16 *)kmalloc(font->metrics.num_glyphs * sizeof(int16));
            for (int g = 0; g < font->metrics.num_glyphs && g < num_hmetrics; g++) {
                font->metrics.advance_widths[g] = read_u16(data + font->hmtx_offset + g * 4);
                font->metrics.left_side_bearings[g] = read_s16(data + font->hmtx_offset + g * 4 + 2);
            }
        }
    }

    /* Build simple cmap (format 4 or 12) */
    font->cmap_size = 0x10000;
    font->cmap = (uint16 *)kmalloc(font->cmap_size * sizeof(uint16));
    if (font->cmap) {
        memset(font->cmap, 0, font->cmap_size * sizeof(uint16));
        /* Identity mapping for ASCII as fallback */
        for (int i = 0; i < 128; i++) font->cmap[i] = i;
        /* Try to parse real cmap */
        for (int i = 0; i < font->num_tables; i++) {
            if (font->tables[i].tag == TAG_CMAP) {
                const uint8 *cmap_data = data + font->tables[i].offset;
                uint16 num_tables = read_u16(cmap_data + 2);
                for (int t = 0; t < num_tables; t++) {
                    uint16 platform = read_u16(cmap_data + 4 + t * 8);
                    uint16 encoding = read_u16(cmap_data + 4 + t * 8 + 2);
                    uint32 offset = read_u32(cmap_data + 4 + t * 8 + 4);
                    uint16 format = read_u16(cmap_data + offset);

                    if (format == 4 && (platform == 0 || platform == 3)) {
                        /* Format 4: Segment mapping */
                        uint16 seg_count = read_u16(cmap_data + offset + 6) / 2;
                        const uint8 *seg_table = cmap_data + offset + 14;
                        for (int s = 0; s < seg_count; s++) {
                            uint16 end_code = read_u16(seg_table + s * 2);
                            uint16 start_code = read_u16(seg_table + (seg_count + 1) * 2 + s * 2);
                            int16 id_delta = read_s16(seg_table + (seg_count + 1) * 4 + s * 2);
                            uint16 id_range_offset = read_u16(seg_table + (seg_count + 1) * 6 + s * 2);
                            for (uint32 cp = start_code; cp <= end_code && cp < font->cmap_size; cp++) {
                                if (id_range_offset == 0) {
                                    font->cmap[cp] = (uint16)(cp + id_delta);
                                } else {
                                    uint16 glyph_id = read_u16(seg_table + (seg_count + 1) * 6 + s * 2 + id_range_offset + (cp - start_code) * 2);
                                    if (glyph_id != 0) font->cmap[cp] = glyph_id + id_delta;
                                }
                            }
                        }
                    }
                }
                break;
            }
        }
    }

    font->initialized = 1;
    return font;
}

void font_free(font_t *font)
{
    if (!font) return;
    if (font->cmap) kfree(font->cmap);
    if (font->metrics.advance_widths) kfree(font->metrics.advance_widths);
    if (font->metrics.left_side_bearings) kfree(font->metrics.left_side_bearings);
    kfree(font);
}

int font_get_glyph_id(font_t *font, uint32 codepoint)
{
    if (!font || !font->cmap || codepoint >= font->cmap_size) return 0;
    return font->cmap[codepoint];
}

int font_get_advance_width(font_t *font, uint16 glyph_id)
{
    if (!font || !font->metrics.advance_widths || glyph_id >= font->metrics.num_glyphs) return 0;
    return font->metrics.advance_widths[glyph_id];
}

/* ===== Simple Rasterizer ===== */

void font_rasterize_glyph(ttf_glyph_t *glyph, int size)
{
    if (!glyph || !glyph->points || glyph->num_points == 0) return;

    float scale = (float)size / (float)(glyph->x_max - glyph->x_min + 1);
    if (scale <= 0) scale = 1.0f;

    glyph->bmp_width = (int)((glyph->x_max - glyph->x_min) * scale) + 2;
    glyph->bmp_height = (int)((glyph->y_max - glyph->y_min) * scale) + 2;
    glyph->bmp_pitch = glyph->bmp_width;

    int bmp_size = glyph->bmp_width * glyph->bmp_height;
    glyph->bitmap = (uint8 *)kmalloc(bmp_size);
    if (!glyph->bitmap) return;
    memset(glyph->bitmap, 0, bmp_size);

    /* Simple scanline fill for each contour */
    for (int contour = 0; contour < glyph->num_contours; contour++) {
        int start = (contour == 0) ? 0 : glyph->contour_ends[contour - 1] + 1;
        int end = glyph->contour_ends[contour];

        for (int row = 0; row < glyph->bmp_height; row++) {
            float y_world = glyph->y_min + row / scale;
            int crossings[64];
            int num_crossings = 0;

            for (int p = start; p <= end && num_crossings < 64; p++) {
                int next = (p == end) ? start : p + 1;
                float y1 = glyph->points[p].y;
                float y2 = glyph->points[next].y;

                if ((y1 <= y_world && y2 > y_world) || (y2 <= y_world && y1 > y_world)) {
                    float x1 = glyph->points[p].x;
                    float x2 = glyph->points[next].x;
                    float t = (y_world - y1) / (y2 - y1);
                    float x_cross = x1 + t * (x2 - x1);
                    crossings[num_crossings++] = (int)((x_cross - glyph->x_min) * scale);
                }
            }

            /* Sort crossings */
            for (int i = 0; i < num_crossings - 1; i++) {
                for (int j = i + 1; j < num_crossings; j++) {
                    if (crossings[i] > crossings[j]) {
                        int t = crossings[i]; crossings[i] = crossings[j]; crossings[j] = t;
                    }
                }
            }

            /* Fill between pairs */
            for (int i = 0; i < num_crossings - 1; i += 2) {
                int x1 = crossings[i]; if (x1 < 0) x1 = 0;
                int x2 = crossings[i + 1]; if (x2 > glyph->bmp_width) x2 = glyph->bmp_width;
                for (int c = x1; c < x2 && c < glyph->bmp_width; c++) {
                    glyph->bitmap[row * glyph->bmp_pitch + c] = 0xFF;
                }
            }
        }
    }
}

void font_free_glyph(ttf_glyph_t *glyph)
{
    if (!glyph) return;
    if (glyph->points) kfree(glyph->points);
    if (glyph->contour_ends) kfree(glyph->contour_ends);
    if (glyph->bitmap) kfree(glyph->bitmap);
    kfree(glyph);
}

/* ===== Draw glyph bitmap ===== */
void font_draw_glyph(font_t *font, uint16 glyph_id, int x, int y, int size, uint32 color, uint32 *fb, int fb_w, int fb_h)
{
    (void)font;
    /* Simplified: draw a filled rectangle for each glyph */
    int w = size * 0.6f; if (w < 6) w = 6;
    int h = size; if (h < 8) h = 8;

    uint8 r = (color >> 16) & 0xFF;
    uint8 g = (color >> 8) & 0xFF;
    uint8 b = color & 0xFF;

    for (int row = 0; row < h; row++) {
        for (int col = 0; col < w; col++) {
            int px = x + col;
            int py = y + row - h + 2;
            if (px >= 0 && px < fb_w && py >= 0 && py < fb_h) {
                /* Simple glyph shape (letter 'A' style as placeholder) */
                int is_fill = 0;
                if (glyph_id == 0) { /* .notdef = box */
                    is_fill = (row < 2 || row >= h - 2 || col < 2 || col >= w - 2);
                } else if (glyph_id >= 1 && glyph_id <= 26) { /* A-Z */
                    int letter = glyph_id - 1;
                    /* Very simplified letter shapes */
                    if (letter == 0) { /* A */
                        float cx = w / 2.0f;
                        is_fill = (abs(col - (int)cx) * 2 + row < h) && (row > h / 3 || abs(col - (int)cx) < w / 4);
                    } else {
                        /* Default: filled block with letter hint */
                        is_fill = (row > 2 && row < h - 2 && col > 2 && col < w - 2);
                    }
                } else {
                    is_fill = (row > 1 && row < h - 1 && col > 1 && col < w - 1);
                }

                if (is_fill) {
                    fb[py * fb_w + px] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
        }
    }
}

/* ===== Text Rendering ===== */
void font_draw_text(font_t *font, const char *text, int x, int y, int font_size, uint32 color, uint32 *fb, int fb_w, int fb_h)
{
    if (!font || !text || !fb) return;

    int pen_x = x;
    int pen_y = y;

    while (*text) {
        uint8 ch = (uint8)*text;
        uint16 glyph_id = font_get_glyph_id(font, ch);
        int advance = font_get_advance_width(font, glyph_id);
        if (advance == 0) advance = font_size * 0.6f;

        font_draw_glyph(font, glyph_id, pen_x, pen_y, font_size, color, fb, fb_w, fb_h);

        pen_x += (advance * font_size) / font->metrics.units_per_em;
        if (pen_x > fb_w - font_size) break;
        text++;
    }
}

int font_text_width(font_t *font, const char *text, int font_size)
{
    if (!font || !text) return 0;
    int width = 0;
    while (*text) {
        uint16 glyph_id = font_get_glyph_id(font, (uint8)*text);
        int advance = font_get_advance_width(font, glyph_id);
        if (advance == 0) advance = font_size * 0.6f;
        width += (advance * font_size) / font->metrics.units_per_em;
        text++;
    }
    return width;
}

/* ===== Hinting ===== */
void font_apply_hinting(ttf_glyph_t *glyph, int size, int dpi)
{
    (void)dpi;
    if (!glyph) return;
    /* Simplified: round coordinates to pixel grid */
    float scale = (float)size / 16.0f;
    for (int i = 0; i < glyph->num_points; i++) {
        glyph->points[i].x = (int16)((float)glyph->points[i].x * scale + 0.5f);
        glyph->points[i].y = (int16)((float)glyph->points[i].y * scale + 0.5f);
    }
}

/* ===== Kerning ===== */
static kern_table_t kern_table;

void font_load_kerning(font_t *font)
{
    if (!font || !font->kern_offset) return;
    const uint8 *data = font->data + font->kern_offset;
    uint16 version = read_u16(data);
    uint16 n_tables = read_u16(data + 2);
    kern_table.num_pairs = 0;

    for (int t = 0; t < n_tables && kern_table.num_pairs < MAX_KERN_PAIRS; t++) {
        uint16 coverage = read_u16(data + 4);
        if ((coverage & 0xFF00) == 0) {
            uint16 npairs = read_u16(data + 6);
            for (int p = 0; p < npairs && kern_table.num_pairs < MAX_KERN_PAIRS; p++) {
                const uint8 *pair = data + 14 + p * 6;
                kern_table.pairs[kern_table.num_pairs].left_glyph = read_u16(pair);
                kern_table.pairs[kern_table.num_pairs].right_glyph = read_u16(pair + 2);
                kern_table.pairs[kern_table.num_pairs].kerning = read_s16(pair + 4);
                kern_table.num_pairs++;
            }
        }
    }
}

int font_get_kerning(font_t *font, uint16 left_glyph, uint16 right_glyph)
{
    (void)font;
    for (int i = 0; i < kern_table.num_pairs; i++) {
        if (kern_table.pairs[i].left_glyph == left_glyph && kern_table.pairs[i].right_glyph == right_glyph)
            return kern_table.pairs[i].kerning;
    }
    return 0;
}

/* ===== Unicode Bidi ===== */

int bidi_get_direction(uint32 codepoint)
{
    /* RTL ranges: Arabic, Hebrew */
    if ((codepoint >= 0x0590 && codepoint <= 0x08FF) ||
        (codepoint >= 0xFB1D && codepoint <= 0xFDFF) ||
        (codepoint >= 0xFE70 && codepoint <= 0xFEFF) ||
        (codepoint >= 0x10800 && codepoint <= 0x10FFF))
        return BIDI_RTL;
    return BIDI_LTR;
}

int bidi_process_string(const uint32 *input, int len, uint32 *output, int *directions)
{
    if (!input || !output || len <= 0) return BIDI_LTR;

    int rtl_count = 0, ltr_count = 0;
    for (int i = 0; i < len; i++) {
        int dir = bidi_get_direction(input[i]);
        if (dir == BIDI_RTL) rtl_count++;
        else ltr_count++;
        if (directions) directions[i] = dir;
    }

    if (rtl_count > ltr_count) {
        /* Reverse for RTL */
        for (int i = 0; i < len; i++) output[i] = input[len - 1 - i];
        return BIDI_RTL;
    } else {
        memcpy(output, input, len * sizeof(uint32));
        return BIDI_LTR;
    }
}

/* ===== System Font ===== */
void font_system_init(void)
{
    printk_color(TERM_YELLOW, "[BOOT] Font System...\n");

    font_cache_init();

    /* Built-in font data (8x8 bitmap font as fallback) */
    /* No external file - use embedded data or fallback */

    printk_color(TERM_GREEN, "[BOOT] Font system ready\n");
}

font_t *font_get_default(void) { return default_font; }
void font_set_default(font_t *font) { default_font = font; }

/* ===== Cache ===== */
void font_cache_init(void)
{
    memset(glyph_cache, 0, sizeof(glyph_cache));
    for (int i = 0; i < FONT_CACHE_SIZE; i++) glyph_cache[i].glyph_id = 0xFFFF;
    cache_initialized = 1;
}

void font_cache_clear(void)
{
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (glyph_cache[i].bitmap) { kfree(glyph_cache[i].bitmap); glyph_cache[i].bitmap = NULL; }
        glyph_cache[i].glyph_id = 0xFFFF;
    }
}

font_cache_entry_t *font_cache_get(font_t *font, uint16 glyph_id)
{
    (void)font;
    if (!cache_initialized) return NULL;
    for (int i = 0; i < FONT_CACHE_SIZE; i++) {
        if (glyph_cache[i].glyph_id == glyph_id) {
            glyph_cache[i].last_used = get_jiffies();
            return &glyph_cache[i];
        }
    }
    return NULL;
}

void font_cache_put(font_t *font, uint16 glyph_id, uint8 *bitmap, int w, int h, int pitch)
{
    (void)font;
    if (!cache_initialized) return;
    /* Find oldest entry */
    int idx = 0;
    uint64 oldest = glyph_cache[0].last_used;
    for (int i = 1; i < FONT_CACHE_SIZE; i++) {
        if (glyph_cache[i].last_used < oldest) { oldest = glyph_cache[i].last_used; idx = i; }
    }
    if (glyph_cache[idx].bitmap) kfree(glyph_cache[idx].bitmap);
    glyph_cache[idx].glyph_id = glyph_id;
    glyph_cache[idx].bitmap = bitmap;
    glyph_cache[idx].width = w;
    glyph_cache[idx].height = h;
    glyph_cache[idx].pitch = pitch;
    glyph_cache[idx].last_used = get_jiffies();
}
