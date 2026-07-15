/******************************************************************************
 * torOS - Terminal Operating System
 * Widget Views - ListView & TreeView
 *
 * Copyright (c) 2025 torOS Contributors
 * License: MIT
 ******************************************************************************/

#include "../include/toros.h"
#include "../include/widget.h"
#include "../include/font.h"
#include "../include/window.h"

/* ===== ListView ===== */

void listview_init(listview_t *lv, int x, int y, int w, int h)
{
    if (!lv) return;
    memset(lv, 0, sizeof(listview_t));
    lv->widget.type = WIDGET_LISTVIEW;
    lv->widget.x = x;
    lv->widget.y = y;
    lv->widget.width = w;
    lv->widget.height = h;
    lv->widget.visible = 1;
    lv->selection_mode = LV_SELECT_SINGLE;
    lv->scroll_offset = 0;
    lv->item_height = 20;
}

void listview_clear(listview_t *lv)
{
    if (!lv) return;
    for (int i = 0; i < lv->item_count; i++) {
        lv->items[i].text[0] = '\0';
        lv->items[i].icon = -1;
    }
    lv->item_count = 0;
    lv->selected_index = -1;
    lv->scroll_offset = 0;
}

int listview_add_item(listview_t *lv, const char *text, int icon)
{
    if (!lv || !text || lv->item_count >= LV_MAX_ITEMS) return -1;
    listview_item_t *item = &lv->items[lv->item_count];
    strncpy(item->text, text, LV_MAX_TEXT - 1);
    item->text[LV_MAX_TEXT - 1] = '\0';
    item->icon = icon;
    item->selected = 0;
    return lv->item_count++;
}

void listview_remove_item(listview_t *lv, int index)
{
    if (!lv || index < 0 || index >= lv->item_count) return;
    for (int i = index; i < lv->item_count - 1; i++)
        lv->items[i] = lv->items[i + 1];
    lv->item_count--;
    if (lv->selected_index >= lv->item_count)
        lv->selected_index = lv->item_count - 1;
}

void listview_set_selected(listview_t *lv, int index)
{
    if (!lv || index < 0 || index >= lv->item_count) return;
    if (lv->selection_mode == LV_SELECT_SINGLE) {
        for (int i = 0; i < lv->item_count; i++)
            lv->items[i].selected = 0;
    }
    lv->items[index].selected = 1;
    lv->selected_index = index;
}

int listview_get_selected(listview_t *lv)
{
    return lv ? lv->selected_index : -1;
}

const char *listview_get_item_text(listview_t *lv, int index)
{
    if (!lv || index < 0 || index >= lv->item_count) return NULL;
    return lv->items[index].text;
}

void listview_sort(listview_t *lv, int (*cmp)(const void*, const void*))
{
    if (!lv || lv->item_count < 2) return;
    /* Simple bubble sort */
    for (int i = 0; i < lv->item_count - 1; i++) {
        for (int j = 0; j < lv->item_count - i - 1; j++) {
            if (cmp(lv->items[j].text, lv->items[j + 1].text) > 0) {
                listview_item_t tmp = lv->items[j];
                lv->items[j] = lv->items[j + 1];
                lv->items[j + 1] = tmp;
            }
        }
    }
}

void listview_handle_click(listview_t *lv, int mouse_x, int mouse_y)
{
    if (!lv) return;
    int rel_y = mouse_y - lv->widget.y;
    int idx = lv->scroll_offset + (rel_y / lv->item_height);
    if (idx >= 0 && idx < lv->item_count)
        listview_set_selected(lv, idx);
}

void listview_draw(listview_t *lv, uint32 *fb, int fb_w, int fb_h)
{
    if (!lv || !lv->widget.visible || !fb) return;
    int x = lv->widget.x;
    int y = lv->widget.y;
    int w = lv->widget.width;
    int h = lv->widget.height;

    if (x + w > fb_w || y + h > fb_h) return;

    /* Background */
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            fb[(y + row) * fb_w + x + col] = 0xFFFFFFFF;

    /* Border */
    for (int col = 0; col < w; col++) {
        fb[y * fb_w + x + col] = 0xFF808080;
        fb[(y + h - 1) * fb_w + x + col] = 0xFF808080;
    }
    for (int row = 0; row < h; row++) {
        fb[(y + row) * fb_w + x] = 0xFF808080;
        fb[(y + row) * fb_w + x + w - 1] = 0xFF808080;
    }

    /* Items */
    int visible_items = (h - 4) / lv->item_height;
    for (int i = 0; i < visible_items && (lv->scroll_offset + i) < lv->item_count; i++) {
        int item_idx = lv->scroll_offset + i;
        int item_y = y + 2 + i * lv->item_height;
        listview_item_t *item = &lv->items[item_idx];

        if (item->selected) {
            for (int row = 0; row < lv->item_height && (item_y + row) < (y + h - 1); row++)
                for (int col = 2; col < w - 2; col++)
                    fb[(item_y + row) * fb_w + x + col] = 0xFF0078D7;
            fb_set_color(0xFFFFFFFF);
        } else {
            fb_set_color(0xFF000000);
        }
        fb_draw_string(x + 4, item_y + 2, item->text);
    }

    /* Scrollbar */
    if (lv->item_count > visible_items) {
        int sb_x = x + w - 14;
        int sb_h = h - 4;
        int thumb_h = (visible_items * sb_h) / lv->item_count;
        if (thumb_h < 16) thumb_h = 16;
        int thumb_y = y + 2 + (lv->scroll_offset * (sb_h - thumb_h)) / (lv->item_count - visible_items);

        for (int row = 0; row < sb_h; row++)
            for (int col = 0; col < 12; col++)
                fb[(y + 2 + row) * fb_w + sb_x + col] = 0xFFF0F0F0;

        for (int row = 0; row < thumb_h && (thumb_y + row) < (y + h - 2); row++)
            for (int col = 0; col < 12; col++)
                fb[(thumb_y + row) * fb_w + sb_x + col] = 0xFFC0C0C0;
    }
}

/* ===== TreeView ===== */

void treeview_init(treeview_t *tv, int x, int y, int w, int h)
{
    if (!tv) return;
    memset(tv, 0, sizeof(treeview_t));
    tv->widget.type = WIDGET_TREEVIEW;
    tv->widget.x = x;
    tv->widget.y = y;
    tv->widget.width = w;
    tv->widget.height = h;
    tv->widget.visible = 1;
    tv->root = NULL;
    tv->selected_node = NULL;
    tv->scroll_offset = 0;
    tv->node_height = 18;
}

static tree_node_t *tree_node_create(const char *text, void *data)
{
    tree_node_t *node = (tree_node_t *)kmalloc(sizeof(tree_node_t));
    if (!node) return NULL;
    memset(node, 0, sizeof(tree_node_t));
    strncpy(node->text, text ? text : "", TV_MAX_TEXT - 1);
    node->text[TV_MAX_TEXT - 1] = '\0';
    node->data = data;
    node->expanded = 0;
    node->has_children = 0;
    node->parent = NULL;
    node->next_sibling = NULL;
    node->first_child = NULL;
    return node;
}

tree_node_t *treeview_add_root(treeview_t *tv, const char *text, void *data)
{
    if (!tv) return NULL;
    tree_node_t *node = tree_node_create(text, data);
    if (!node) return NULL;

    if (!tv->root) {
        tv->root = node;
    } else {
        tree_node_t *s = tv->root;
        while (s->next_sibling) s = s->next_sibling;
        s->next_sibling = node;
    }
    tv->node_count++;
    return node;
}

tree_node_t *treeview_add_child(treeview_t *tv, tree_node_t *parent, const char *text, void *data)
{
    if (!tv || !parent) return NULL;
    tree_node_t *node = tree_node_create(text, data);
    if (!node) return NULL;

    node->parent = parent;
    parent->has_children = 1;

    if (!parent->first_child) {
        parent->first_child = node;
    } else {
        tree_node_t *s = parent->first_child;
        while (s->next_sibling) s = s->next_sibling;
        s->next_sibling = node;
    }
    tv->node_count++;
    return node;
}

void treeview_remove_node(treeview_t *tv, tree_node_t *node)
{
    if (!tv || !node) return;
    /* Remove children first */
    while (node->first_child)
        treeview_remove_node(tv, node->first_child);

    /* Unlink from parent or sibling list */
    if (node->parent && node->parent->first_child == node) {
        node->parent->first_child = node->next_sibling;
    } else if (tv->root == node) {
        tv->root = node->next_sibling;
    } else {
        tree_node_t *s = tv->root;
        while (s && s->next_sibling != node) s = s->next_sibling;
        if (s) s->next_sibling = node->next_sibling;
    }
    kfree(node);
    tv->node_count--;
}

void treeview_expand(treeview_t *tv, tree_node_t *node)
{
    (void)tv;
    if (node) node->expanded = 1;
}

void treeview_collapse(treeview_t *tv, tree_node_t *node)
{
    (void)tv;
    if (node) node->expanded = 0;
}

void treeview_toggle(treeview_t *tv, tree_node_t *node)
{
    if (!tv || !node) return;
    node->expanded = !node->expanded;
}

tree_node_t *treeview_get_selected(treeview_t *tv)
{
    return tv ? tv->selected_node : NULL;
}

const char *treeview_node_get_text(tree_node_t *node)
{
    return node ? node->text : NULL;
}

void *treeview_node_get_data(tree_node_t *node)
{
    return node ? node->data : NULL;
}

static void treeview_draw_node(treeview_t *tv, tree_node_t *node, uint32 *fb, int fb_w, int fb_h,
                                int x, int *y, int level, int max_y)
{
    if (!node || !y || *y >= max_y) return;

    int indent = level * TV_INDENT;
    int node_y = *y;

    if (node_y >= tv->widget.y && node_y < max_y) {
        /* Expand/collapse button */
        if (node->has_children) {
            int btn_x = x + indent;
            int btn_y = node_y + 4;
            fb_set_color(0xFF808080);
            for (int row = 0; row < 9; row++)
                for (int col = 0; col < 9; col++)
                    fb[(btn_y + row) * fb_w + btn_x + col] = 0xFFFFFFFF;
            fb[(btn_y + 4) * fb_w + btn_x + 2] = 0xFF000000;
            fb[(btn_y + 4) * fb_w + btn_x + 3] = 0xFF000000;
            fb[(btn_y + 4) * fb_w + btn_x + 4] = 0xFF000000;
            fb[(btn_y + 4) * fb_w + btn_x + 5] = 0xFF000000;
            fb[(btn_y + 4) * fb_w + btn_x + 6] = 0xFF000000;
            if (!node->expanded) {
                fb[(btn_y + 2) * fb_w + btn_x + 4] = 0xFF000000;
                fb[(btn_y + 3) * fb_w + btn_x + 4] = 0xFF000000;
                fb[(btn_y + 5) * fb_w + btn_x + 4] = 0xFF000000;
                fb[(btn_y + 6) * fb_w + btn_x + 4] = 0xFF000000;
            }
        }

        /* Selection highlight */
        if (node == tv->selected_node) {
            for (int row = 0; row < tv->node_height && (node_y + row) < max_y; row++)
                for (int col = indent + 12; col < tv->widget.width - 2; col++)
                    if ((x + col) < fb_w)
                        fb[(node_y + row) * fb_w + x + col] = 0xFF0078D7;
            fb_set_color(0xFFFFFFFF);
        } else {
            fb_set_color(0xFF000000);
        }

        fb_draw_string(x + indent + 14, node_y + 2, node->text);
    }

    *y += tv->node_height;

    /* Draw children if expanded */
    if (node->expanded && node->first_child) {
        treeview_draw_node(tv, node->first_child, fb, fb_w, fb_h, x, y, level + 1, max_y);
    }

    /* Draw siblings */
    if (node->next_sibling) {
        treeview_draw_node(tv, node->next_sibling, fb, fb_w, fb_h, x, y, level, max_y);
    }
}

void treeview_draw(treeview_t *tv, uint32 *fb, int fb_w, int fb_h)
{
    if (!tv || !tv->widget.visible || !fb) return;
    int x = tv->widget.x;
    int y = tv->widget.y;
    int w = tv->widget.width;
    int h = tv->widget.height;

    if (x + w > fb_w || y + h > fb_h) return;

    /* Background */
    for (int row = 0; row < h; row++)
        for (int col = 0; col < w; col++)
            fb[(y + row) * fb_w + x + col] = 0xFFFFFFFF;

    /* Border */
    for (int col = 0; col < w; col++) {
        fb[y * fb_w + x + col] = 0xFF808080;
        fb[(y + h - 1) * fb_w + x + col] = 0xFF808080;
    }
    for (int row = 0; row < h; row++) {
        fb[(y + row) * fb_w + x] = 0xFF808080;
        fb[(y + row) * fb_w + x + w - 1] = 0xFF808080;
    }

    /* Draw tree */
    if (tv->root) {
        int draw_y = y + 2;
        treeview_draw_node(tv, tv->root, fb, fb_w, fb_h, x, &draw_y, 0, y + h - 1);
    }
}

tree_node_t *treeview_hit_test(treeview_t *tv, int mouse_x, int mouse_y)
{
    if (!tv || !tv->root) return NULL;
    /* Simplified: return root for now */
    (void)mouse_x;
    (void)mouse_y;
    return tv->root;
}

void treeview_clear(treeview_t *tv)
{
    if (!tv) return;
    while (tv->root)
        treeview_remove_node(tv, tv->root);
    tv->selected_node = NULL;
    tv->node_count = 0;
}
