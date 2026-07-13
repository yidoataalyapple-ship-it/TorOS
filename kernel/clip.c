/*
 * torOS Clipping & Region Management (FAZ 3.3)
 * y-x banded rectangle regions: union, subtract, intersect test
 * Clip-aware drawing primitives for the compositor
 */

#include "../include/toros.h"
#include "../include/window.h"

/* ===== Region lifecycle ===== */

void clip_region_init(clip_region_t *region)
{
    if (!region)
        return;
    region->rects = NULL;
    region->num_rects = 0;
    region->x = region->y = region->w = region->h = 0;
}

void clip_region_clear(clip_region_t *region)
{
    if (!region)
        return;
    region_rect_t *r = region->rects;
    while (r) {
        region_rect_t *next = r->next;
        kfree(r);
        r = next;
    }
    region->rects = NULL;
    region->num_rects = 0;
    region->x = region->y = region->w = region->h = 0;
}

void clip_region_free(clip_region_t *region)
{
    clip_region_clear(region);
}

/* Recompute bounding box from rect list */
static void region_update_bbox(clip_region_t *region)
{
    if (!region->rects) {
        region->x = region->y = region->w = region->h = 0;
        return;
    }

    int min_x = 0x7FFFFFFF, min_y = 0x7FFFFFFF;
    int max_x = -0x7FFFFFFF, max_y = -0x7FFFFFFF;

    for (region_rect_t *r = region->rects; r; r = r->next) {
        if (r->x < min_x) min_x = r->x;
        if (r->y < min_y) min_y = r->y;
        if (r->x + r->w > max_x) max_x = r->x + r->w;
        if (r->y + r->h > max_y) max_y = r->y + r->h;
    }

    region->x = min_x;
    region->y = min_y;
    region->w = max_x - min_x;
    region->h = max_y - min_y;
}

static int rects_overlap(const region_rect_t *a, const region_rect_t *b)
{
    return !(a->x + a->w <= b->x || b->x + b->w <= a->x ||
             a->y + a->h <= b->y || b->y + b->h <= a->y);
}

/* Merge two overlapping/touching rects into their bounding rect */
static void rect_merge(region_rect_t *a, const region_rect_t *b)
{
    int x1 = a->x < b->x ? a->x : b->x;
    int y1 = a->y < b->y ? a->y : b->y;
    int x2 = (a->x + a->w) > (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
    int y2 = (a->y + a->h) > (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
    a->x = x1; a->y = y1;
    a->w = x2 - x1; a->h = y2 - y1;
}

/* Touching test for y-x banding: same y-band and horizontally adjacent,
 * or same x-span and vertically adjacent */
static int rects_touch(const region_rect_t *a, const region_rect_t *b)
{
    if (a->y == b->y && a->h == b->h) {
        if (a->x + a->w == b->x || b->x + b->w == a->x)
            return 1;
    }
    if (a->x == b->x && a->w == b->w) {
        if (a->y + a->h == b->y || b->y + b->h == a->y)
            return 1;
    }
    return 0;
}

void clip_region_add_rect(clip_region_t *region, int x, int y, int w, int h)
{
    if (!region || w <= 0 || h <= 0)
        return;

    region_rect_t *nr = (region_rect_t *)kmalloc(sizeof(region_rect_t));
    if (!nr)
        return;
    nr->x = x; nr->y = y; nr->w = w; nr->h = h;
    nr->next = NULL;

    if (!region->rects) {
        region->rects = nr;
        region->num_rects = 1;
        region_update_bbox(region);
        return;
    }

    /* Union: iteratively merge with any overlapping/touching rect.
     * Overlaps merge into bounding boxes (conservative union: result may
     * cover slightly more area but never less — safe for clipping). */
    region_rect_t **pp = &region->rects;
    int merged;
    do {
        merged = 0;
        pp = &region->rects;
        while (*pp) {
            region_rect_t *r = *pp;
            if (r != nr && (rects_overlap(r, nr) || rects_touch(r, nr))) {
                rect_merge(nr, r);
                *pp = r->next;
                kfree(r);
                region->num_rects--;
                merged = 1;
                break;  /* restart scan */
            }
            pp = &r->next;
        }
    } while (merged);

    /* Append merged rect (keep list sorted by y band for cache friendliness) */
    pp = &region->rects;
    while (*pp && (*pp)->y <= nr->y)
        pp = &(*pp)->next;
    nr->next = *pp;
    *pp = nr;
    region->num_rects++;

    region_update_bbox(region);
}

/* Subtract rect b from rect a, producing up to 4 fragment rects.
 * Returns fragment count; fragments written into out[] (may be empty). */
static int rect_subtract(const region_rect_t *a, const region_rect_t *b,
                         region_rect_t out[4])
{
    /* No overlap: a survives whole */
    if (!rects_overlap(a, b)) {
        out[0] = *a;
        out[0].next = NULL;
        return 1;
    }

    int ax1 = a->x, ay1 = a->y, ax2 = a->x + a->w, ay2 = a->y + a->h;
    int bx1 = b->x > ax1 ? b->x : ax1;
    int by1 = b->y > ay1 ? b->y : ay1;
    int bx2 = (b->x + b->w) < ax2 ? (b->x + b->w) : ax2;
    int by2 = (b->y + b->h) < ay2 ? (b->y + b->h) : ay2;

    int n = 0;

    /* Top fragment */
    if (by1 > ay1) {
        out[n].x = ax1; out[n].y = ay1;
        out[n].w = ax2 - ax1; out[n].h = by1 - ay1;
        out[n].next = NULL; n++;
    }
    /* Bottom fragment */
    if (by2 < ay2) {
        out[n].x = ax1; out[n].y = by2;
        out[n].w = ax2 - ax1; out[n].h = ay2 - by2;
        out[n].next = NULL; n++;
    }
    /* Left fragment (between top/bottom) */
    if (bx1 > ax1) {
        out[n].x = ax1; out[n].y = by1;
        out[n].w = bx1 - ax1; out[n].h = by2 - by1;
        out[n].next = NULL; n++;
    }
    /* Right fragment (between top/bottom) */
    if (bx2 < ax2) {
        out[n].x = bx2; out[n].y = by1;
        out[n].w = ax2 - bx2; out[n].h = by2 - by1;
        out[n].next = NULL; n++;
    }

    return n;
}

void clip_region_subtract_rect(clip_region_t *region, int x, int y, int w, int h)
{
    if (!region || !region->rects || w <= 0 || h <= 0)
        return;

    region_rect_t sub = { x, y, w, h, NULL };
    region_rect_t *new_list = NULL;
    int new_count = 0;

    region_rect_t *r = region->rects;
    while (r) {
        region_rect_t *next = r->next;
        region_rect_t frags[4];
        int nf = rect_subtract(r, &sub, frags);

        if (nf == 1 && frags[0].x == r->x && frags[0].y == r->y &&
            frags[0].w == r->w && frags[0].h == r->h) {
            /* Unchanged: keep node */
            r->next = new_list;
            new_list = r;
            new_count++;
        } else {
            kfree(r);
            for (int i = 0; i < nf; i++) {
                region_rect_t *fr = (region_rect_t *)kmalloc(sizeof(region_rect_t));
                if (!fr)
                    continue;
                *fr = frags[i];
                fr->next = new_list;
                new_list = fr;
                new_count++;
            }
        }
        r = next;
    }

    region->rects = new_list;
    region->num_rects = new_count;
    region_update_bbox(region);
}

int clip_region_intersects(const clip_region_t *region, int x, int y, int w, int h)
{
    if (!region || !region->rects)
        return 0;

    region_rect_t q = { x, y, w, h, NULL };
    for (const region_rect_t *r = region->rects; r; r = r->next) {
        if (rects_overlap(r, &q))
            return 1;
    }
    return 0;
}

void clip_region_set_rect(clip_region_t *region, int x, int y, int w, int h)
{
    if (!region)
        return;
    clip_region_clear(region);
    clip_region_add_rect(region, x, y, w, h);
}

void clip_region_dump(const clip_region_t *region)
{
    if (!region) {
        printk("[CLIP] (null region)\n");
        return;
    }
    printk("[CLIP] %d rects, bbox=(%d,%d %dx%d)\n",
           region->num_rects, region->x, region->y, region->w, region->h);
    int i = 0;
    for (const region_rect_t *r = region->rects; r; r = r->next, i++) {
        printk("  [%d] (%d,%d) %dx%d\n", i, r->x, r->y, r->w, r->h);
    }
}

/* ===== Clip-aware drawing ===== */

/* Fill a rect, clipped against region (NULL region = no clipping) */
void clip_draw_rect(clip_region_t *clip, uint32 *fb, int fb_w, int fb_h,
                    int x, int y, int w, int h, uint32 color)
{
    if (!fb || w <= 0 || h <= 0)
        return;

    if (!clip) {
        /* Unclipped fill with screen bounds only */
        for (int ry = y; ry < y + h; ry++) {
            if (ry < 0 || ry >= fb_h) continue;
            for (int rx = x; rx < x + w; rx++) {
                if (rx < 0 || rx >= fb_w) continue;
                fb[ry * fb_w + rx] = color;
            }
        }
        return;
    }

    /* Draw intersection of (x,y,w,h) with each region rect */
    for (region_rect_t *r = clip->rects; r; r = r->next) {
        int ix1 = x > r->x ? x : r->x;
        int iy1 = y > r->y ? y : r->y;
        int ix2 = (x + w) < (r->x + r->w) ? (x + w) : (r->x + r->w);
        int iy2 = (y + h) < (r->y + r->h) ? (y + h) : (r->y + r->h);

        if (ix1 < 0) ix1 = 0;
        if (iy1 < 0) iy1 = 0;
        if (ix2 > fb_w) ix2 = fb_w;
        if (iy2 > fb_h) iy2 = fb_h;
        if (ix2 <= ix1 || iy2 <= iy1)
            continue;

        for (int ry = iy1; ry < iy2; ry++) {
            uint32 *row = fb + ry * fb_w;
            for (int rx = ix1; rx < ix2; rx++)
                row[rx] = color;
        }
    }
}

/* Blit src(sx,sy,w,h) to dst(dx,dy), clipped against region */
void clip_blit(clip_region_t *clip, uint32 *dst, int dst_w, int dst_h,
               uint32 *src, int src_w, int src_h,
               int dx, int dy, int sx, int sy, int w, int h)
{
    if (!dst || !src || w <= 0 || h <= 0)
        return;
    if (sx < 0 || sy < 0 || sx + w > src_w || sy + h > src_h)
        return;

    if (!clip) {
        for (int ry = 0; ry < h; ry++) {
            int dyy = dy + ry;
            if (dyy < 0 || dyy >= dst_h) continue;
            for (int rx = 0; rx < w; rx++) {
                int dxx = dx + rx;
                if (dxx < 0 || dxx >= dst_w) continue;
                dst[dyy * dst_w + dxx] = src[(sy + ry) * src_w + (sx + rx)];
            }
        }
        return;
    }

    for (region_rect_t *r = clip->rects; r; r = r->next) {
        int ix1 = dx > r->x ? dx : r->x;
        int iy1 = dy > r->y ? dy : r->y;
        int ix2 = (dx + w) < (r->x + r->w) ? (dx + w) : (r->x + r->w);
        int iy2 = (dy + h) < (r->y + r->h) ? (dy + h) : (r->y + r->h);

        if (ix1 < 0) ix1 = 0;
        if (iy1 < 0) iy1 = 0;
        if (ix2 > dst_w) ix2 = dst_w;
        if (iy2 > dst_h) iy2 = dst_h;
        if (ix2 <= ix1 || iy2 <= iy1)
            continue;

        for (int dyy = iy1; dyy < iy2; dyy++) {
            for (int dxx = ix1; dxx < ix2; dxx++) {
                int sxx = sx + (dxx - dx);
                int syy = sy + (dyy - dy);
                dst[dyy * dst_w + dxx] = src[syy * src_w + sxx];
            }
        }
    }
}

/* ===== Visible region computation (occlusion culling helper) =====
 * Computes the part of `win` not covered by higher z-order windows.
 * The WM list is sorted back-to-front; windows after `win` in the list
 * are on top of it. */
void wm_compute_visible_region(window_t *win, clip_region_t *out)
{
    if (!win || !out)
        return;

    clip_region_init(out);

    if (win->state == WSTATE_MINIMIZED || win->state == WSTATE_HIDDEN ||
        !(win->flags & WS_VISIBLE))
        return;

    /* Start with the full window rect */
    clip_region_add_rect(out, win->x, win->y, win->full_width, win->full_height);

    extern window_manager_t wm;
    int past_self = 0;
    for (window_t *w = wm.window_list; w; w = w->next) {
        if (w == win) {
            past_self = 1;
            continue;
        }
        if (!past_self)
            continue;

        /* Higher windows occlude */
        if (w->state == WSTATE_MINIMIZED || w->state == WSTATE_HIDDEN ||
            !(w->flags & WS_VISIBLE))
            continue;

        clip_region_subtract_rect(out, w->x, w->y, w->full_width, w->full_height);
        if (out->num_rects == 0)
            break;  /* Fully occluded */
    }
}
