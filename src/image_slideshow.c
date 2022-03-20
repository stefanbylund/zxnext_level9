/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Test module for displaying all location images. Press the O key to show the
 * previous image and the P key to show the next image. For multi-part multiple
 * choice games, press the A key to switch to the previous game part and the S
 * key to switch to the next game part. Press any other key to exit the
 * slideshow.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <input.h>
#include <errno.h>

#include "zconfig.h"
#include "image_slideshow.h"
#include "layer2.h"
#include "ide_friendly.h"

#define MIN_IMAGE 1
#define MAX_IMAGE 799

#define MIN_GAME_PART 1
#define MAX_GAME_PART 4

extern uint8_t tmp_buffer[256];

extern bool multiple_choice_game;

extern uint8_t max_image_height;

static uint16_t image = 0;

static uint8_t game_part = MIN_GAME_PART;

static uint8_t filename[16];

static bool show_image(uint16_t image_number) __z88dk_fastcall
{
    if (multiple_choice_game)
    {
        sprintf(filename, "gfx/%u/%u.nxi", game_part, image_number);
    }
    else
    {
        sprintf(filename, "gfx/%u.nxi", image_number);
    }

    errno = 0;
    layer2_load_screen(SHADOW_SCREEN, layer2_get_unused_access_palette(), filename, tmp_buffer);
    if (!errno)
    {
        wait_video_line(max_image_height);
        layer2_flip_main_shadow_screen();
        layer2_flip_display_palettes();
    }

    return !errno;
}

static void toggle_image(bool next) __z88dk_fastcall
{
    if (next)
    {
        image++;
        if (image > MAX_IMAGE)
        {
            image = MIN_IMAGE;
        }
    }
    else
    {
        image--;
        if ((image < MIN_IMAGE) || (image > MAX_IMAGE))
        {
            image = MAX_IMAGE;
        }
    }

    if (!show_image(image))
    {
        toggle_image(next);
    }
}

static void toggle_game_part(bool next) __z88dk_fastcall
{
    if (next)
    {
        game_part++;
        if (game_part > MAX_GAME_PART)
        {
            game_part = MIN_GAME_PART;
        }
    }
    else
    {
        game_part--;
        if (game_part < MIN_GAME_PART)
        {
            game_part = MAX_GAME_PART;
        }
    }

    image = 0;
    toggle_image(true);
}

void run_image_slideshow(void)
{
    ZXN_WRITE_MMU0(255);
    ZXN_WRITE_MMU1(255);

    layer2_set_clip_window(0, 0, 161, max_image_height);
    layer2_config(true);

    toggle_image(true);

    while (true)
    {
        uint8_t ascii_code = (uint8_t) in_inkey();

        switch (ascii_code)
        {
            case 0:
                continue;
            case 'o':
                toggle_image(false);
                break;
            case 'p':
                toggle_image(true);
                break;
            case 'a':
                toggle_game_part(false);
                break;
            case 's':
                toggle_game_part(true);
                break;
            default:
                return;
        }

        in_wait_nokey();
    }
}
