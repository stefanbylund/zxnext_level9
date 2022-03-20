/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Limited C API for using the hardware sprites of the ZX Spectrum Next for
 * displaying a scroll prompt and a mouse pointer for the Level 9 interpreter.
 ******************************************************************************/

#ifndef _SPRITE_H
#define _SPRITE_H

#include <arch/zxn.h>
#include <z80.h>

#include <stdint.h>
#include <stdbool.h>

void sprite_config(bool sprites_visible, bool sprites_over_border);

void sprite_set_default_palette(void);

#define sprite_select_slot(sprite_slot) \
    (IO_SPRITE_SLOT = (sprite_slot) & 0x3F)

#define sprite_set_pattern(sprite_pattern) \
    z80_otir((void *) (sprite_pattern), __IO_SPRITE_PATTERN, 0)

void sprite_set_attributes(uint8_t sprite_pattern_slot,
                           uint16_t x,
                           uint16_t y,
                           bool visible);

void sprite_load_patterns(const char *filename,
                          const void *sprite_pattern_buf,
                          uint8_t num_sprite_patterns,
                          uint8_t start_sprite_pattern_slot);

#endif
