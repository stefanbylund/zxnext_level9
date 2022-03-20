/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Limited C API for using the layer 2 screen of the ZX Spectrum Next for
 * displaying images for the Level 9 interpreter.
 *
 * Note: MMU slot 2 is temporarily used when writing to the layer 2 screen.
 ******************************************************************************/

#ifndef _LAYER2_H
#define _LAYER2_H

#include <arch/zxn.h>
#include <stdint.h>

#include "ide_friendly.h"

typedef enum layer2_screen
{
    MAIN_SCREEN = REG_LAYER_2_RAM_BANK,
    SHADOW_SCREEN = REG_LAYER_2_SHADOW_RAM_BANK
} layer2_screen_t;

typedef enum layer2_palette
{
    LAYER2_PALETTE_1 = RPC_SELECT_LAYER_2_PALETTE_0,
    LAYER2_PALETTE_2 = RPC_SELECT_LAYER_2_PALETTE_1
} layer2_palette_t;

#define layer2_config(visible) \
    (IO_LAYER_2_CONFIG = ((visible) ? IL2C_SHOW_LAYER_2 : 0))

#define layer2_set_main_screen_bank(bank) \
    ZXN_NEXTREGA(REG_LAYER_2_RAM_BANK, (bank))

#define layer2_get_main_screen_bank() \
    ZXN_READ_REG(REG_LAYER_2_RAM_BANK)

#define layer2_set_shadow_screen_bank(bank) \
    ZXN_NEXTREGA(REG_LAYER_2_SHADOW_RAM_BANK, (bank))

#define layer2_get_shadow_screen_bank() \
    ZXN_READ_REG(REG_LAYER_2_SHADOW_RAM_BANK)

void layer2_flip_main_shadow_screen(void);

void layer2_set_clip_window(uint8_t x, uint8_t y, uint16_t width, uint16_t height);

#define layer2_reset_clip_window() \
    ZXN_NEXTREG(REG_CLIP_WINDOW_CONTROL, RCWC_RESET_LAYER_2_CLIP_INDEX)

#define layer2_set_transparency_color(color) \
    ZXN_NEXTREGA(REG_GLOBAL_TRANSPARENCY_COLOR, (color))

#define layer2_get_transparency_color() \
    ZXN_READ_REG(REG_GLOBAL_TRANSPARENCY_COLOR)

void layer2_set_palette(layer2_palette_t palette,
                        const uint16_t *colors,
                        uint16_t length,
                        uint8_t palette_index);

layer2_palette_t layer2_get_unused_access_palette(void);

void layer2_flip_display_palettes(void);

void layer2_clear_screen(layer2_screen_t screen, uint8_t color);

void layer2_load_screen(layer2_screen_t screen,
                        layer2_palette_t palette,
                        const char *filename,
                        uint8_t *buf_256);

void wait_video_line(uint16_t line) __z88dk_fastcall;

#endif
