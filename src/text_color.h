/*******************************************************************************
 * Stefan Bylund 2021
 *
 * C API for functions in text_color.asm.
 ******************************************************************************/

#ifndef _TEXT_COLOR_H
#define _TEXT_COLOR_H

#include <stdint.h>
#include "ide_friendly.h"

extern void set_initial_text_color(uint8_t text_palette_index) __z88dk_fastcall __preserves_regs(d,e);

extern void cycle_text_color(uint8_t direction) __z88dk_fastcall __preserves_regs(d,e);

#endif
