/*
 * keymap.c — Linux input event code -> ASCII (US QWERTY)
 * Faz 1.1: "Scan code translation: Linux input event codes → ASCII"
 */
#include <toros/types.h>

/* Özel karakter kodları (ASCII dışı kontroller) */
#define KC_NONE 0

/* Normal tablo (index = Linux KEY_* code) */
const u8 keymap_normal[128] = {
    [0] = KC_NONE,
    [1] = 0x1B,      /* KEY_ESC */
    [2] = '1', [3] = '2', [4] = '3', [5] = '4', [6] = '5',
    [7] = '6', [8] = '7', [9] = '8', [10] = '9', [11] = '0',
    [12] = '-', [13] = '=',
    [14] = '\b',     /* KEY_BACKSPACE */
    [15] = '\t',     /* KEY_TAB */
    [16] = 'q', [17] = 'w', [18] = 'e', [19] = 'r', [20] = 't',
    [21] = 'y', [22] = 'u', [23] = 'i', [24] = 'o', [25] = 'p',
    [26] = '[', [27] = ']',
    [28] = '\n',     /* KEY_ENTER */
    /* 29 = KEY_LEFTCTRL (modifier) */
    [30] = 'a', [31] = 's', [32] = 'd', [33] = 'f', [34] = 'g',
    [35] = 'h', [36] = 'j', [37] = 'k', [38] = 'l',
    [39] = ';', [40] = '\'', [41] = '`',
    /* 42 = KEY_LEFTSHIFT */
    [43] = '\\',
    [44] = 'z', [45] = 'x', [46] = 'c', [47] = 'v', [48] = 'b',
    [49] = 'n', [50] = 'm',
    [51] = ',', [52] = '.', [53] = '/',
    /* 54 = KEY_RIGHTSHIFT */
    [55] = '*',      /* KEY_KPASTERISK */
    /* 56 = KEY_LEFTALT */
    [57] = ' ',      /* KEY_SPACE */
    /* 58 = KEY_CAPSLOCK */
    [71] = '7', [72] = '8', [73] = '9', [74] = '-',   /* numpad */
    [75] = '4', [76] = '5', [77] = '6', [78] = '+',
    [79] = '1', [80] = '2', [81] = '3',
    [82] = '0', [83] = '.',
    [96] = '\n',     /* KEY_KPENTER */
};

/* Shift basılı tablo */
const u8 keymap_shift[128] = {
    [0] = KC_NONE,
    [1] = 0x1B,
    [2] = '!', [3] = '@', [4] = '#', [5] = '$', [6] = '%',
    [7] = '^', [8] = '&', [9] = '*', [10] = '(', [11] = ')',
    [12] = '_', [13] = '+',
    [14] = '\b',
    [15] = '\t',
    [16] = 'Q', [17] = 'W', [18] = 'E', [19] = 'R', [20] = 'T',
    [21] = 'Y', [22] = 'U', [23] = 'I', [24] = 'O', [25] = 'P',
    [26] = '{', [27] = '}',
    [28] = '\n',
    [30] = 'A', [31] = 'S', [32] = 'D', [33] = 'F', [34] = 'G',
    [35] = 'H', [36] = 'J', [37] = 'K', [38] = 'L',
    [39] = ':', [40] = '"', [41] = '~',
    [43] = '|',
    [44] = 'Z', [45] = 'X', [46] = 'C', [47] = 'V', [48] = 'B',
    [49] = 'N', [50] = 'M',
    [51] = '<', [52] = '>', [53] = '?',
    [55] = '*',
    [57] = ' ',
    [71] = '7', [72] = '8', [73] = '9', [74] = '-',
    [75] = '4', [76] = '5', [77] = '6', [78] = '+',
    [79] = '1', [80] = '2', [81] = '3',
    [82] = '0', [83] = '.',
    [96] = '\n',
};

/* Modifier key kodları */
#define KEY_LEFTCTRL   29
#define KEY_LEFTSHIFT  42
#define KEY_RIGHTSHIFT 54
#define KEY_LEFTALT    56
#define KEY_CAPSLOCK   58
#define KEY_RIGHTCTRL  97
#define KEY_RIGHTALT   100

/* Caps Lock harf dönüşümü */
char keymap_translate(u16 code, u8 modifiers, int *is_modifier, int *mod_press)
{
    *is_modifier = 0;
    *mod_press = 0;

    switch (code) {
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        *is_modifier = 1; *mod_press = 0; return 0;
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        *is_modifier = 1; *mod_press = 1; return 0;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        *is_modifier = 1; *mod_press = 2; return 0;
    case KEY_CAPSLOCK:
        *is_modifier = 1; *mod_press = 3; return 0;
    default:
        break;
    }

    if (code >= 128)
        return 0;

    int shift = modifiers & 0x1;
    int caps = (modifiers >> 3) & 1;

    char c = shift ? keymap_shift[code] : keymap_normal[code];

    /* Caps Lock: harfleri ters çevir */
    if (caps && !shift) {
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
    } else if (caps && shift) {
        if (c >= 'A' && c <= 'Z')
            c = c - 'A' + 'a';
    }
    return c;
}
