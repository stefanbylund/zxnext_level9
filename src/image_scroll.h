/*******************************************************************************
 * Stefan Bylund 2021
 *
 * C API for image scrolling functions in image_scroll.asm.
 ******************************************************************************/

#ifndef _IMAGE_SCROLL_H
#define _IMAGE_SCROLL_H

#include <stdint.h>
#include "ide_friendly.h"

extern uint8_t image_text_height_in_chars(void) __preserves_regs(b,c,d,e);

extern void image_mouse_scroll(int8_t image_height_change) __z88dk_fastcall;

extern void image_key_scroll(uint8_t c) __z88dk_fastcall;

#endif
