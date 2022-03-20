/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Implementation of sprite.h; a limited C API for using the hardware sprites
 * of the ZX Spectrum Next for displaying a scroll prompt and a mouse pointer
 * for the Level 9 interpreter.
 ******************************************************************************/

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#include "sprite.h"
#include "ide_friendly.h"

#define SPRITE_SLOT_MASK 0x3F
#define SPRITE_VISIBLE_MASK 0x80
#define SPRITE_EXTENDED_MASK 0x40

#define LSB(x) ((uint8_t) ((x) & 0x00FF))
#define MSB(x) ((uint8_t) (((x) & 0x0100) >> 8))

void sprite_config(bool sprites_visible, bool sprites_over_border)
{
    uint8_t value = RSLS_LAYER_PRIORITY_SLU;

    if (sprites_visible)
    {
        value |= RSLS_SPRITES_VISIBLE;
    }

    if (sprites_over_border)
    {
        value |= RSLS_SPRITES_OVER_BORDER;
    }

    ZXN_NEXTREGA(REG_SPRITE_LAYER_SYSTEM, value);
}

void sprite_set_default_palette(void)
{
    uint8_t palette_control;

    ZXN_NEXTREG(REG_SPRITE_TRANSPARENCY_INDEX, 0xE3);

    palette_control = ZXN_READ_REG(REG_PALETTE_CONTROL);
    palette_control = (palette_control & 0x8F) | RPC_SELECT_SPRITES_PALETTE_0;
    ZXN_NEXTREGA(REG_PALETTE_CONTROL, palette_control);

    IO_NEXTREG_REG = REG_PALETTE_INDEX;
    IO_NEXTREG_DAT = 0;

    IO_NEXTREG_REG = REG_PALETTE_VALUE_8;
    for (uint16_t i = 0; i < 256; ++i)
    {
        IO_NEXTREG_DAT = (uint8_t) i;
    }
}

void sprite_set_attributes(uint8_t sprite_pattern_slot,
                           uint16_t x,
                           uint16_t y,
                           bool visible)
{
    uint8_t pattern_slot = sprite_pattern_slot & SPRITE_SLOT_MASK;

    pattern_slot |= SPRITE_EXTENDED_MASK;

    if (visible)
    {
        pattern_slot |= SPRITE_VISIBLE_MASK;
    }

    IO_SPRITE_ATTRIBUTE = LSB(x);
    IO_SPRITE_ATTRIBUTE = LSB(y);
    IO_SPRITE_ATTRIBUTE = MSB(x);
    IO_SPRITE_ATTRIBUTE = pattern_slot;
    IO_SPRITE_ATTRIBUTE = MSB(y);
}

void sprite_load_patterns(const char *filename,
                          const void *sprite_pattern_buf,
                          uint8_t num_sprite_patterns,
                          uint8_t start_sprite_pattern_slot)
{
    uint8_t filehandle;

    // Skip parameter checking to save memory.

    // Note: Caller must ensure that the Spectrum ROM is in place.

    errno = 0;
    filehandle = esx_f_open(filename, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (errno)
    {
        return;
    }

    sprite_select_slot(start_sprite_pattern_slot);

    while (num_sprite_patterns--)
    {
        esx_f_read(filehandle, (void *) sprite_pattern_buf, 256);
        if (errno)
        {
            break;
        }
        sprite_set_pattern(sprite_pattern_buf);
    }

    esx_f_close(filehandle);
}
