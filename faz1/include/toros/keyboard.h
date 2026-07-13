/*
 * keyboard.h — Klavye sürücüsü API'si (Faz 1.1)
 * Linux input event code -> ASCII çevrimi, modifier takibi
 */
#ifndef TOROS_KEYBOARD_H
#define TOROS_KEYBOARD_H

#include <toros/types.h>

void keyboard_init(void);

/* Plan API'si */
char keyboard_getchar(void);          /* blocking */
int  keyboard_poll(char *out);        /* non-blocking: 1=karakter var */

/* İstatistik / durum */
u32  keyboard_event_count(void);
u8   keyboard_modifiers(void);        /* bit0 shift, 1 ctrl, 2 alt, 3 caps */

/* virtio-input sürücüsünden çağrılan ham olay girişi */
void keyboard_handle_key(u16 code, s32 value);

#endif
