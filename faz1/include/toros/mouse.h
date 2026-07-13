/*
 * mouse.h — Fare sürücüsü API'si (Faz 1.2)
 */
#ifndef TOROS_MOUSE_H
#define TOROS_MOUSE_H

#include <toros/types.h>

/* Ekran sınırları (plan: 1024x768 framebuffer) */
#define MOUSE_SCREEN_W 1024
#define MOUSE_SCREEN_H 768

void mouse_init(void);

/* Plan API'si: void mouse_get_state(int32_t *x, int32_t *y, uint8_t *buttons) */
void mouse_get_state(s32 *x, s32 *y, u8 *buttons);
s32  mouse_wheel_delta(void);     /* son okumadan beri biriken wheel */
u64  mouse_event_count(void);

/* virtio-input sürücüsünden çağrılan ham olay girişleri */
void mouse_handle_rel(u16 code, s32 value);
void mouse_handle_button(u16 code, s32 value);

#endif
