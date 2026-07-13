/*
 * torOS Font & Text Rendering Header
 * TTF/OTF parser, rasterizer, hinting, kerning, Unicode Bidi, text layout
 */

#ifndef _FONT_H
#define _FONT_H

#include "toros.h"

/* ===== TTF/OTF Table Tags ===== */
#define TAG_CMAP 0x636D6170
#define TAG_GLYF 0x676C7966
#define TAG_HEAD 0x68656164
#define TAG_HHEA 0x68686561
#define TAG_HMTX 0x686D7478
#define TAG_MAXP 0x6D617870
#define TAG_NAME 0x6E616D65
#define TAG_POST 0x706F7374
#define TAG_OS2  0x4F532F32
#define TAG_LOCA 0x6C6F6361
#define TAG_KERN 0x6B65726E

/* ===== TTF Offsets ===== */
typedef struct {
    uint32 tag;
    uint32 checksum;
    uint32 offset;
    uint32 length;
} ttf_table_t;

typedef struct {
    uint32 sfnt_version;
    uint16 num_tables;
    uint16 search_range;
    uint16 entry_selector;
    uint16 range_shift;
} ttf_header_t;

/* Glyph header */
typedef struct {
    int16 num_contours;
    int16 x_min;
    int16 y_min;
    int16 x_max;
    int16 y_max;
} ttf_glyph_header_t;

/* Point in glyph outline */
typedef struct {
    int16 x, y;
    uint8 on_curve;
    uint8 flags;
} ttf_point_t;

/* Glyph */
typedef struct {
    uint16 glyph_id;
    int16 num_contours;
    int16 x_min, y_min, x_max, y_max;
    ttf_point_t *points;
    int num_points;
    uint16 *contour_ends;
    int advance_width;
    int left_side_bearing;
    uint8 *bitmap;      /* Rasterized bitmap */
    int bmp_width;
    int bmp_height;
    int bmp_pitch;
} ttf_glyph_t;

/* Font metrics */
typedef struct {
    int16 ascent;
    int16 descent;
    int16 line_gap;
    uint16 advance_width_max;
    int16 x_min, y_min, x_max, y_max;
    uint16 units_per_em;
    uint16 num_glyphs;
    int16 *advance_widths;
    int16 *left_side_bearings;
} ttf_metrics_t;

/* Font handle */
typedef struct {
    uint8 *data;
    uint32 size;
    ttf_header_t header;
    ttf_table_t tables[32];
    int num_tables;
    ttf_metrics_t metrics;
    uint16 *cmap;       /* Unicode -> glyph mapping */
    uint32 cmap_size;
    uint32 loca_offset;
    uint32 glyf_offset;
    uint32 head_offset;
    uint32 hhea_offset;
    uint32 hmtx_offset;
    uint32 kern_offset;
    int initialized;
} font_t;

/* ===== Kerning ===== */
typedef struct {
    uint16 left_glyph;
    uint16 right_glyph;
    int16 kerning;
} kern_pair_t;

#define MAX_KERN_PAIRS 4096

typedef struct {
    kern_pair_t pairs[MAX_KERN_PAIRS];
    int num_pairs;
} kern_table_t;

/* ===== Unicode Bidi ===== */
#define BIDI_LTR    0
#define BIDI_RTL    1
#define BIDI_NEUTRAL 2

int bidi_get_direction(uint32 codepoint);
int bidi_process_string(const uint32 *input, int len, uint32 *output, int *directions);

/* ===== Text Layout ===== */
typedef struct {
    uint32 codepoint;
    uint16 glyph_id;
    int16 x, y;         /* Position */
    int16 advance_x;
    int16 advance_y;
    uint32 color;
} glyph_position_t;

#define MAX_LINE_GLYPHS 256
#define MAX_TEXT_LINES  64

typedef struct {
    glyph_position_t glyphs[MAX_LINE_GLYPHS];
    int num_glyphs;
    int line_width;
    int line_height;
    int baseline;
} text_line_t;

typedef struct {
    text_line_t lines[MAX_TEXT_LINES];
    int num_lines;
    int total_width;
    int total_height;
    int alignment;      /* 0=left, 1=center, 2=right */
} text_layout_t;

/* ===== Font Cache ===== */
#define FONT_CACHE_SIZE 256

typedef struct {
    uint16 glyph_id;
    uint8 *bitmap;
    int width, height;
    int pitch;
    uint64 last_used;
} font_cache_entry_t;

/* ===== Font API ===== */
font_t *font_load(const uint8 *data, uint32 size);
void font_free(font_t *font);
int font_load_from_file(const char *filename, font_t **out);
int font_get_glyph_id(font_t *font, uint32 codepoint);
ttf_glyph_t *font_load_glyph(font_t *font, uint16 glyph_id);
void font_free_glyph(ttf_glyph_t *glyph);
int font_get_advance_width(font_t *font, uint16 glyph_id);

/* ===== Rasterizer API ===== */
void font_rasterize_glyph(ttf_glyph_t *glyph, int size);
void font_draw_glyph(font_t *font, uint16 glyph_id, int x, int y, int size, uint32 color, uint32 *fb, int fb_w, int fb_h);

/* ===== Hinting API ===== */
void font_apply_hinting(ttf_glyph_t *glyph, int size, int dpi);

/* ===== Kerning API ===== */
int font_get_kerning(font_t *font, uint16 left_glyph, uint16 right_glyph);
void font_load_kerning(font_t *font);

/* ===== Text Layout API ===== */
void font_layout_text(font_t *font, const char *text, int font_size, int max_width, text_layout_t *layout);
void font_layout_line(font_t *font, const uint32 *codepoints, int len, int font_size, text_line_t *line);
void font_draw_text(font_t *font, const char *text, int x, int y, int font_size, uint32 color, uint32 *fb, int fb_w, int fb_h);
void font_draw_text_wrapped(font_t *font, const char *text, int x, int y, int font_size, int max_width, uint32 color, uint32 *fb, int fb_w, int fb_h);
int font_text_width(font_t *font, const char *text, int font_size);

/* ===== Cache API ===== */
void font_cache_init(void);
void font_cache_clear(void);
font_cache_entry_t *font_cache_get(font_t *font, uint16 glyph_id);
void font_cache_put(font_t *font, uint16 glyph_id, uint8 *bitmap, int w, int h, int pitch);

/* ===== System Font ===== */
#define DEFAULT_FONT_SIZE   16
#define DEFAULT_FONT_DPI    96

font_t *font_get_default(void);
void font_set_default(font_t *font);
void font_system_init(void);

#endif
