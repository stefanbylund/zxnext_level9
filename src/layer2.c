/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Implementation of layer2.h; a limited C API for using the layer 2 screen
 * of the ZX Spectrum Next for displaying images for the Level 9 interpreter.
 *
 * Note: MMU slot 2 is temporarily used when writing to the layer 2 screen.
 ******************************************************************************/

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "zconfig.h"
#include "layer2.h"
#include "ide_friendly.h"

#define SCREEN_ADDRESS ((uint8_t *) 0x4000)

#define GET_SCREEN_BASE_PAGE(screen)  (ZXN_READ_REG(screen) << 1)

extern uint8_t max_image_height;

void layer2_flip_main_shadow_screen(void)
{
    uint8_t main_screen_bank = ZXN_READ_REG(REG_LAYER_2_RAM_BANK);
    uint8_t shadow_screen_bank = ZXN_READ_REG(REG_LAYER_2_SHADOW_RAM_BANK);

    ZXN_NEXTREGA(REG_LAYER_2_RAM_BANK, shadow_screen_bank);
    ZXN_NEXTREGA(REG_LAYER_2_SHADOW_RAM_BANK, main_screen_bank);
}

void layer2_set_clip_window(uint8_t x, uint8_t y, uint16_t width, uint16_t height)
{
    // Skip validations of arguments.

    IO_NEXTREG_REG = REG_CLIP_WINDOW_LAYER_2;
    IO_NEXTREG_DAT = x;
    IO_NEXTREG_DAT = (width != 0) ? (x + width - 1) : x;
    IO_NEXTREG_DAT = y;
    IO_NEXTREG_DAT = (height != 0) ? (y + height - 1) : y;

    // Scroll the layer 2 screen vertically so that the bottom-most part of its
    // contents is always visible in the bottom part of the layer 2 clip window.
    ZXN_NEXTREGA(REG_LAYER_2_OFFSET_Y, (height < max_image_height) ? (max_image_height - height) : 0);
}

void layer2_set_palette(layer2_palette_t palette,
                        const uint16_t *colors,
                        uint16_t length,
                        uint8_t palette_index)
{
    uint8_t palette_control;
    uint8_t *color_bytes = (uint8_t *) colors;

    // Skip parameter checking to save memory.

    palette_control = ZXN_READ_REG(REG_PALETTE_CONTROL);
    palette_control = (palette_control & 0x8F) | palette;
    ZXN_NEXTREGA(REG_PALETTE_CONTROL, palette_control);

    IO_NEXTREG_REG = REG_PALETTE_INDEX;
    IO_NEXTREG_DAT = palette_index;

    IO_NEXTREG_REG = REG_PALETTE_VALUE_16;
    for (uint16_t i = 0; i < (length << 1); i++)
    {
        IO_NEXTREG_DAT = color_bytes[i];
    }
}

/*
 * If the primary layer 2 display palette is currently used, return the
 * secondary layer 2 access palette and vice versa.
 */
layer2_palette_t layer2_get_unused_access_palette(void)
{
    uint8_t display_palette = ZXN_READ_REG(REG_PALETTE_CONTROL) & 0x04;
    return (display_palette == 0) ? LAYER2_PALETTE_2 : LAYER2_PALETTE_1;
}

/*
 * Flip the layer 2 display palettes, i.e. if the primary layer 2 display
 * palette is currently used, select the secondary layer 2 display palette
 * and vice versa.
 */
void layer2_flip_display_palettes(void)
{
    uint8_t palette_control = ZXN_READ_REG(REG_PALETTE_CONTROL) ^ 0x04;
    ZXN_NEXTREGA(REG_PALETTE_CONTROL, palette_control);
}

void layer2_clear_screen(layer2_screen_t screen, uint8_t color)
{
    uint8_t screen_base_page = GET_SCREEN_BASE_PAGE(screen);

    for (uint8_t page = screen_base_page; page < screen_base_page + 10; page++)
    {
        ZXN_WRITE_MMU2(page);
        memset(SCREEN_ADDRESS, color, 0x2000);
    }

    // Restore original page in MMU slot 2.
    ZXN_WRITE_MMU2(10);
}

void layer2_load_screen(layer2_screen_t screen,
                        layer2_palette_t palette,
                        const char *filename,
                        uint8_t *buf_256)
{
    uint8_t filehandle;
    uint8_t screen_base_page;

    // Skip parameter checking to save memory.

    // Note: Caller must ensure that the Spectrum ROM is in place.

    errno = 0;
    filehandle = esx_f_open(filename, ESX_MODE_R | ESX_MODE_OPEN_EXIST);
    if (errno)
    {
        return;
    }

    // Load palette.

    esx_f_read(filehandle, buf_256, 256);
    if (errno)
    {
        goto end;
    }
    layer2_set_palette(palette, (uint16_t *) buf_256, 128, 0);
    esx_f_read(filehandle, buf_256, 256);
    if (errno)
    {
        goto end;
    }
    layer2_set_palette(palette, (uint16_t *) buf_256, 128, 128);

    // Load screen in 8 KB chunks using MMU slot 2 at address 0x4000.

    screen_base_page = GET_SCREEN_BASE_PAGE(screen);

    for (uint8_t page = screen_base_page; page < screen_base_page + 10; page++)
    {
        ZXN_WRITE_MMU2(page);

        esx_f_read(filehandle, SCREEN_ADDRESS, 0x2000);
        if (errno)
        {
            goto end;
        }
    }

end:
    // Restore original page in MMU slot 2.
    ZXN_WRITE_MMU2(10);
    esx_f_close(filehandle);
}

void wait_video_line(uint16_t line) __z88dk_fastcall
{
    uint8_t line_l = (uint8_t) line;
    uint8_t line_h = (uint8_t) (line >> 8);

    do
    {
        while (ZXN_READ_REG(REG_ACTIVE_VIDEO_LINE_L) != line_l);
    }
    while (ZXN_READ_REG(REG_ACTIVE_VIDEO_LINE_H) != line_h);
}
