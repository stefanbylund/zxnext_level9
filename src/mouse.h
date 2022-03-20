/*******************************************************************************
 * Stefan Bylund 2021
 *
 * C API for PS/2 Kempston mouse support.
 ******************************************************************************/

#ifndef _MOUSE_H
#define _MOUSE_H

#include <stdint.h>

// FIXME: Remove when the corresponding defines in input.h have been updated.
#define MOUSE_BUTTON_RIGHT  0x01
#define MOUSE_BUTTON_LEFT   0x02
#define MOUSE_BUTTON_MIDDLE 0x04

typedef void (*MOUSE_LISTENER)(uint16_t mouse_x, uint8_t mouse_y, uint8_t mouse_buttons, int8_t wheel_delta);

void init_mouse(const void *mouse_sprite_buf, MOUSE_LISTENER mouse_listener);

#endif
