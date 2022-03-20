/*******************************************************************************
 *
 * Level 9 interpreter interface for ZX Spectrum Next.
 * Written by Stefan Bylund <stefan.bylund99@gmail.com>.
 * Copyright (C) 2021 Stefan Bylund.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111, USA.
 *
 ******************************************************************************/

/*******************************************************************************
 *
 * This program is configured in the configure.m4 file, which generates the
 * zconfig.h header file included by this program and the zpragma.inc file
 * containing the pragmas for this program.
 *
 ******************************************************************************/

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>
#include <z80.h>
#include <intrinsic.h>
#include <input.h>
#include <font/fzx.h>
#include <adt.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stropts.h>
#include <errno.h>

#include "zconfig.h"
#include "level9.h"
#include "memory_paging.h"
#include "sprite.h"
#include "layer2.h"
#include "text_color.h"
#include "mouse.h"
#include "image_scroll.h"
#include "ide_friendly.h"

#if USE_IMAGE_SLIDESHOW
#include "image_slideshow.h"
#endif

#define VERSION "v1.0.0"

#define SINGLE_GAME_FILE "gamedata.dat"
#define MULTI_GAME_FILE "gamedat1.dat"

#if USE_TIMEX_HIRES
#define BUFFER_MEMORY_START 0x5800
#else
#define BUFFER_MEMORY_START 0x5B00
#endif

#define OUT_BUFFER_SIZE 1152
#define HISTORY_BUFFER_SIZE 128

#define FD_STDIN 0
#define FD_STDOUT 1

#define SCROLL_PROMPT_SPRITE_START_SLOT 0

// Restart prompt message of the form:
// "Press [SPACE|space] to play again. "
#define RESTART_PROMPT_MSG "Press SPACE to play again. "
#define RESTART_PROMPT_MSG_LENGTH 27

// Save/restore prompt message of the form:
// "Remove the <game> dis[k|c], insert your formatted save position dis[k|c] and press a key. "
#define SAVE_RESTORE_PROMPT_MSG_MIN_LENGTH 80
#define SAVE_RESTORE_PROMPT_MSG_MAX_LENGTH 100
#define SAVE_RESTORE_PROMPT_MSG_END " and press a key. "
#define SAVE_RESTORE_PROMPT_MSG_END_LENGTH 18

extern uint8_t tmp_buffer[256];

static struct fzx_font *out_term_font;
static uint16_t out_term_line_width;

bool multiple_choice_game;
static uint8_t game_number = 1; // Only set for multiple choice games.

/*
 * Note: When using the Timex hi-res mode, we use the free 2 KB RAM between the
 * two Timex hi-res screen buffers (0x5800 - 0x5FFF) for storing the 1152 bytes
 * out_buffer and the 128 bytes history_buffer and when using the ULA mode we
 * use the 1.25 KB RAM after the ULA screen buffer (0x5B00 - 0x5FFF) for this.
 */

static uint8_t *out_buffer = (uint8_t *) BUFFER_MEMORY_START;
static uint16_t out_buffer_pos = 0;

static uint8_t *history_buffer = (uint8_t *) (BUFFER_MEMORY_START + OUT_BUFFER_SIZE);
uint16_t history_buffer_size = 0;
bool edit_pressed = false;

#if USE_GFX
bool gfx_on = false;

uint8_t max_image_height = 152;

uint8_t gfx_window_height = 0;

static uint8_t filename[MAX_PATH];
#endif

#if USE_GFX && USE_MOUSE
static void mouse_handler(uint16_t mouse_x, uint8_t mouse_y, uint8_t mouse_buttons, int8_t wheel_delta);
#endif

static void init_hardware(void)
{
    // Set black border and clear the ULA screen.
    zx_border(INK_BLACK);
    zx_cls(INK_BLACK | PAPER_BLACK);

    // Make sure the Spectrum ROM is paged in initially.
    IO_7FFD = IO_7FFD_ROM0;

    // Put Z80 in 28 MHz turbo mode.
    ZXN_NEXTREG(REG_TURBO_MODE, 0x03); // Use RTM_28MHZ when included in z88dk

    // Disable RAM memory contention.
    ZXN_NEXTREGA(REG_PERIPHERAL_3, ZXN_READ_REG(REG_PERIPHERAL_3) | RP3_DISABLE_CONTENTION);

    // Reset display palette and r/w palette selections to their defaults.
    ZXN_NEXTREG(REG_PALETTE_CONTROL, 0);

    // Reset clip windows to their defaults.
    ZXN_NEXTREG(REG_CLIP_WINDOW_CONTROL, 0x07);

    // Initialize all hardware sprites to invisible.
    sprite_select_slot(0);
    for (uint8_t i = 0; i != 64; i++)
    {
        sprite_set_attributes(i, 0, 0, false);
    }

    // Set default sprite palette and sprite transparency palette index.
    sprite_set_default_palette();

    // Initialize hardware sprites and layers system:
    // Sprites over layer 2 screen over ULA screen.
    // Sprites visible and displayable on border.
    sprite_config(true, true);

#if USE_GFX
    // Set layer 2 screen in 320x256 mode.
    ZXN_NEXTREG(0x70, 0x10);
    // Set layer 2 clip window to 320x256 (x-coordinates are halved).
    layer2_set_clip_window(0, 0, 161, 256);
    // Set layer 2 main and shadow screen banks and clear and enable layer 2 screen.
    layer2_set_main_screen_bank(8);
    layer2_set_shadow_screen_bank(13);
    layer2_clear_screen(MAIN_SCREEN, 0x00);
    layer2_config(true);
#endif

    // Enable interrupts (initialized in interrupt.asm).
    intrinsic_ei();
}

static void wait_key(void)
{
    in_wait_nokey();
    in_wait_key();
    in_wait_nokey();
}

#if USE_GFX
static void show_title(void)
{
    // Load and show title image if available.
    layer2_load_screen(MAIN_SCREEN, LAYER2_PALETTE_1, "gfx/0.nxi", tmp_buffer);
    if (!errno)
    {
        // Wait for the user to press any key.
        wait_key();

        // Clear layer 2 screen.
        layer2_config(false);
        layer2_clear_screen(MAIN_SCREEN, 0x00);
        layer2_config(true);
    }
}
#endif

static void init_out_terminal(void)
{
    int fd;
    struct r_Rect16 paper;

    fd = fileno(stdout);
    out_term_font = (struct fzx_font *) ioctl(fd, IOCTL_OTERM_FONT, -1);
    ioctl(fd, IOCTL_OTERM_FZX_GET_PAPER_RECT, &paper);
    out_term_line_width = paper.width - ioctl(fd, IOCTL_OTERM_FZX_LEFT_MARGIN, -1) - 1;
}

static void create_screen(void)
{
    // We disable the layer 2 screen initially to make sure any error printouts during startup are displayed.
    layer2_config(false);

    // When graphics is enabled in os_graphics(), the clip window of the layer 2 screen
    // is set so that the ULA screen shows through and its text area is visible.

#if USE_TIMEX_HIRES
    // Clear the ULA screen in Timex hi-res mode.
    memset((void *) 0x4000, 0, 0x1800);
    memset((void *) 0x6000, 0, 0x1800);

    // Enable Timex hi-res mode for the ULA screen with white ink on black paper.
    ZXN_NEXTREGA(REG_PERIPHERAL_3, ZXN_READ_REG(REG_PERIPHERAL_3) | RP3_ENABLE_TIMEX);
    IO_FF = IO_TVM_HIRES_WHITE;
#endif

    // Set initial text font color.
    set_initial_text_color(TEXT_FONT_COLOR_INDEX);

    // Load and set scroll prompt hardware sprites using the default sprite palette.
    sprite_load_patterns("gfx/prompt.spr", tmp_buffer, 2, SCROLL_PROMPT_SPRITE_START_SLOT);

    // Init output terminal.
    init_out_terminal();

#if USE_GFX && USE_MOUSE
    // Init PS/2 Kempston mouse.
    // Note: From this point on, usage of the sprite slot port must be atomic (di/ei)
    // since the mouse interrupt handler will write to the sprite slot port each frame.
    init_mouse(tmp_buffer, mouse_handler);
#endif
}

static void exit_handler(void)
{
    free_memory();

    // Trig a soft reset. The Next hardware registers and I/O ports will be reset by NextZXOS after a soft reset.
    ZXN_NEXTREG(REG_RESET, RR_SOFT_RESET);
}

static uint8_t *get_game_file(void)
{
    uint8_t *filename;
    struct esx_stat filestat;

    filename = SINGLE_GAME_FILE;
    if (esx_f_stat(filename, &filestat))
    {
        filename = MULTI_GAME_FILE;
        if (esx_f_stat(filename, &filestat))
        {
            filename = NULL;
        }
    }

    return filename;
}

static bool is_multiple_choice_game(uint8_t *game_file) __z88dk_fastcall
{
    return (get_game_type() == L9_V3) && (strcmp(game_file, MULTI_GAME_FILE) == 0);
}

/*
 * Word wrap the given string (i.e. replace space characters with newline
 * characters) so that it fits the width of the FZX output terminal on an
 * even word boundary. Any existing newline characters are respected.
 * The word wrapped string is returned.
 */
static uint8_t *str_word_wrap(uint8_t *str, uint16_t str_len)
{
    uint8_t *line;
    uint8_t *line_end;

    line = strtok(str, "\n");

    while (line != NULL)
    {
        line_end = fzx_string_partition_ww(out_term_font, line, out_term_line_width);
        if ((line_end - str) >= str_len)
        {
            break;
        }
        if ((*line_end == ' ') || (*line_end == '\0'))
        {
            *line_end = '\n';
        }
        line = strtok(line_end + 1, "\n");
    }

    return str;
}

// FIXME: Should be __z88dk_fastcall but it doesn't work here - compiler bug?
static void save_history(uint8_t *input_in)
{
    // Workaround: Manually transfer param to Z80 register.
    uint8_t *input = input_in;

    if (input != NULL)
    {
        // Copy entered input to history buffer.
        history_buffer_size = strlen(input);
        if (history_buffer_size > HISTORY_BUFFER_SIZE)
        {
            history_buffer_size = HISTORY_BUFFER_SIZE;
        }
        memcpy(history_buffer, input, history_buffer_size);
    }
}

static void load_history(void)
{
    b_array_t edit_buffer;

    // Copy history buffer to input terminal's edit buffer.
    fflush(stdin);
    ioctl(FD_STDIN, IOCTL_ITERM_GET_EDITBUF, &edit_buffer);
    memcpy(edit_buffer.data, history_buffer, history_buffer_size);
    edit_buffer.size = history_buffer_size;
    ioctl(FD_STDIN, IOCTL_ITERM_SET_EDITBUF, &edit_buffer);
}

static void read_filename(uint8_t *in_buf, uint16_t size)
{
    uint16_t in_buf_pos = 0;

    while (true)
    {
        int c = getchar();

        // We have to support the EDIT key here to avoid havoc if it's pressed.
        if ((c == EOF) && edit_pressed)
        {
            clearerr(stdin);
            ioctl(FD_STDIN, IOCTL_RESET);
            edit_pressed = false;
            load_history();
            in_buf_pos = 0;
            continue;
        }

        if ((c == '\n') || (c == EOF) || (in_buf_pos == size - 1))
        {
            c = '\0';
        }

        in_buf[in_buf_pos++] = c;

        if (c == '\0')
        {
            break;
        }
    }
}

static void clear_screen(void)
{
#if USE_GFX
    if (gfx_on)
    {
        uint16_t color = 0;
        layer2_set_palette(layer2_get_unused_access_palette(), &color, 1, 0);
        layer2_clear_screen(SHADOW_SCREEN, 0x00);
        wait_video_line(max_image_height);
        layer2_flip_main_shadow_screen();
        layer2_flip_display_palettes();
    }
#endif
}

static bool should_flush_in_read_char(void)
{
    if (!multiple_choice_game)
    {
        if (out_buffer_pos == RESTART_PROMPT_MSG_LENGTH)
        {
            return strnicmp(out_buffer, RESTART_PROMPT_MSG, RESTART_PROMPT_MSG_LENGTH) == 0;
        }

        if ((out_buffer_pos >= SAVE_RESTORE_PROMPT_MSG_MIN_LENGTH) &&
            (out_buffer_pos <= SAVE_RESTORE_PROMPT_MSG_MAX_LENGTH))
        {
            return strncmp(out_buffer + out_buffer_pos - SAVE_RESTORE_PROMPT_MSG_END_LENGTH,
                    SAVE_RESTORE_PROMPT_MSG_END, SAVE_RESTORE_PROMPT_MSG_END_LENGTH) == 0;
        }
    }

    return false;
}

static void handle_special_key(uint8_t c) __z88dk_fastcall
{
    switch (c)
    {
        case ASCII_CODE_TRUE_VIDEO:
            // Fall through
        case ASCII_CODE_INV_VIDEO:
            cycle_text_color(c == ASCII_CODE_INV_VIDEO);
            z80_delay_ms(1800);
            break;
        case ASCII_CODE_UP:
            // Fall through
        case ASCII_CODE_DOWN:
            image_key_scroll(c);
            z80_delay_ms(200);
            break;
        default:
            break;
    }
}

#if USE_MOUSE && USE_GFX
static void mouse_handler(uint16_t mouse_x, uint8_t mouse_y, uint8_t mouse_buttons, int8_t wheel_delta)
{
    static uint8_t last_mouse_drag_y = 0;

    if (wheel_delta != 0)
    {
        // MOUSE WHEEL EVENT

        int8_t image_height_change;
        uint8_t image_height_rest;

        // Translate one mouse wheel movement as 8 pixels, i.e. as one line of text.
        image_height_change = wheel_delta << 3;

        // If the image bottom is not aligned on a text line boundary, make it so for this mouse
        // wheel event by scrolling the image up or down the amount of pixels to make it aligned.
        image_height_rest = gfx_window_height % 8;
        if (image_height_rest != 0)
        {
            if (image_height_change < 0)
            {
                image_height_change = -image_height_rest;
            }
            else
            {
                image_height_change = 8 - image_height_rest;
            }
        }

        // Scroll image by the given amount.
        image_mouse_scroll(image_height_change);
    }
    else if (((mouse_buttons & MOUSE_BUTTON_LEFT) != 0) && ((last_mouse_drag_y != 0) ||
             (mouse_y < ((gfx_window_height != 0) ? gfx_window_height : 8))))
    {
        // MOUSE DRAG EVENT

        // Left mouse button pressed and mouse pointer inside image or a mouse drag
        // operation is ongoing. Allow some margin below the image when fully minimized.

        if (last_mouse_drag_y != 0)
        {
            int8_t image_height_change = (int8_t) (mouse_y - last_mouse_drag_y);

            // Make it easier to make the image fully minimized when dragging upwards and hitting the top border.
            if ((image_height_change < 0) && (gfx_window_height < 8))
            {
                image_height_change = -gfx_window_height;
            }

            // Scroll image by the given amount.
            image_mouse_scroll(image_height_change);
        }

        // Update last mouse drag y-coordinate.
        last_mouse_drag_y = mouse_y;
    }
    else
    {
        // Mark any ongoing mouse drag operation as ended.
        last_mouse_drag_y = 0;
    }
}
#endif

void os_print_char(uint8_t c) __z88dk_fastcall
{
    if (c == '\r')
    {
        os_flush();
        putchar('\n');
    }
    else if (isprint(c))
    {
        if (out_buffer_pos >= OUT_BUFFER_SIZE - 1)
        {
            os_flush();
        }
        out_buffer[out_buffer_pos++] = c;
    }
}

void os_flush(void)
{
    if (out_buffer_pos != 0)
    {
        out_buffer[out_buffer_pos] = '\0';
        str_word_wrap(out_buffer, out_buffer_pos);
        fputs(out_buffer, stdout);
        out_buffer_pos = 0;
    }
}

bool os_input(uint8_t *in_buf, uint16_t size)
{
    uint16_t in_buf_pos = 0;

    os_flush();

    while (true)
    {
        int c = getchar();

        if ((c == EOF) && edit_pressed)
        {
            clearerr(stdin);
            ioctl(FD_STDIN, IOCTL_RESET);
            edit_pressed = false;
            load_history();
            in_buf_pos = 0;
            continue;
        }

        if ((c == '\n') || (c == EOF) || (in_buf_pos == size - 1))
        {
            c = '\0';
        }

        in_buf[in_buf_pos++] = c;

        if (c == '\0')
        {
            break;
        }
    }

    if (in_buf_pos > 1)
    {
        save_history(in_buf);
    }

    return true;
}

uint8_t os_read_char(uint16_t millis) __z88dk_fastcall
{
    int c;

    /*
     * The multiple choice games call os_read_char() to read the input choice.
     * The text adventure games should really only call os_read_char() when the
     * game is about to be restarted, saved or restored and a prompt message
     * (e.g. "Press SPACE to play again.") is printed. However, some of the text
     * adventure games call os_read_char() seemingly spuriously. If os_flush()
     * is called in these cases, any ongoing buffered output may be formatted
     * badly. To avoid this, we have to check if flushing is really needed, i.e.
     * if the game is a text adventure game (not a multiple choice game) and is
     * about to be restarted, saved or restored and thus has the corresponding
     * prompt text unflushed in its output buffer.
     */
    if (should_flush_in_read_char())
    {
        os_flush();
    }

    c = in_inkey();
    handle_special_key(c);
    if (c || (millis == 0))
    {
        return c;
    }

    in_pause(millis);
    c = in_inkey();
    handle_special_key(c);
    return c;
}

bool os_save_file(uint8_t *ptr, uint16_t size)
{
    uint8_t fh;

    os_flush();
    fputs("Save file: ", stdout);
    read_filename(tmp_buffer, sizeof(tmp_buffer));
    putchar('\n');

    page_in_rom();
    errno = 0;
    fh = esx_f_open(tmp_buffer, ESX_MODE_OPEN_CREAT_TRUNC | ESX_MODE_W);
    if (errno)
    {
        goto end;
    }
    esx_f_write(fh, ptr, size);
    esx_f_close(fh);

end:
    page_in_game();
    return (errno == 0);
}

bool os_load_file(uint8_t *ptr, uint16_t *size, uint16_t max_size)
{
    uint8_t fh;

    os_flush();
    fputs("Load file: ", stdout);
    read_filename(tmp_buffer, sizeof(tmp_buffer));
    putchar('\n');

    page_in_rom();
    errno = 0;
    fh = esx_f_open(tmp_buffer, ESX_MODE_OPEN_EXIST | ESX_MODE_R);
    if (errno)
    {
        goto end;
    }
    *size = esx_f_read(fh, ptr, max_size);
    esx_f_close(fh);

end:
    page_in_game();
    return (errno == 0);
}

bool os_get_game_file(uint8_t *new_name, uint16_t size)
{
    for (uint16_t i = strlen(new_name) - 1; i > 0; i--)
    {
        if (isdigit(new_name[i]))
        {
            game_number++;
            new_name[i] = '0' + game_number;
            return true;
        }
    }

    return false;
}

void os_set_file_number(uint8_t *new_name, uint16_t size, uint8_t num)
{
    for (uint16_t i = strlen(new_name) - 1; i > 0; i--)
    {
        if (isdigit(new_name[i]))
        {
            new_name[i] = '0' + num;
            return;
        }
    }
}

void os_graphics(bool graphics_on) __z88dk_fastcall
{
#if USE_GFX
    if (graphics_on)
    {
        // Turn on graphics if off.
        if (!gfx_on)
        {
            gfx_on = true;
            gfx_window_height = max_image_height;
            layer2_set_clip_window(0, 0, 161, max_image_height);
            layer2_config(true);
            ioctl(FD_STDOUT, IOCTL_OTERM_SCROLL_LIMIT, (TEXT_WINDOW_HEIGHT - image_text_height_in_chars()));
        }
    }
    else
    {
        // Turn off graphics if on.
        if (gfx_on)
        {
            gfx_on = false;
            gfx_window_height = 0;
            layer2_config(false);
            ioctl(FD_STDOUT, IOCTL_OTERM_SCROLL_LIMIT, TEXT_WINDOW_HEIGHT);
        }
    }
#endif
}

void os_clear_graphics(void)
{
/*
 * Some of the V2/V3 games (Emerald Isle, Dungeon Adventure, Snowball) use
 * os_clear_graphics() in a few cases for showing a black picture when the
 * room is dark. However, all of the V2/V3 games also use os_clear_graphics()
 * for clearing the screen before drawing the next picture.
 *
 * On Spectrum Next, where the V2/V3 pictures are bitmap pictures and not
 * line-drawn pictures drawn step-by-step at runtime, this will add flicker
 * when switching between pictures. So for now we ignore os_clear_graphics()
 * to reduce flicker for the common case.
 */
#if 0 && USE_GFX
    clear_screen();
#endif
}

void os_show_bitmap(uint16_t pic) __z88dk_fastcall
{
#if USE_GFX
    // Some of the V3 games (Colossal Adventure and Adventure Quest) use the
    // non-existent image #0 for showing a black picture when the room is dark.
    if (pic == 0)
    {
        clear_screen();
        return;
    }

    // Load and display the given location image. In order to minimize flicker
    // when switching to a new image, the new image and its palette is first
    // loaded into the layer 2 shadow screen and the layer 2 palette (primary or
    // secondary) not currently used. Then the layer 2 main/shadow screen and
    // the primary/secondary palettes are flipped to show the new image.

    if (multiple_choice_game)
    {
        sprintf(filename, "gfx/%u/%u.nxi", game_number, pic);
    }
    else
    {
        sprintf(filename, "gfx/%u.nxi", pic);
    }

    errno = 0;
    page_in_rom();

    layer2_load_screen(SHADOW_SCREEN, layer2_get_unused_access_palette(), filename, tmp_buffer);
    // Skip reporting image loading problems since some Level 9 games sometimes
    // issue loading of non-existent images and expect silent failure.
    if (!errno)
    {
        wait_video_line(max_image_height);
        layer2_flip_main_shadow_screen();
        layer2_flip_display_palettes();
    }

    page_in_game();
#endif
}

uint8_t os_open_script_file(void)
{
    uint8_t fh;

    os_flush();
    fputs("\nScript file: ", stdout);
    read_filename(tmp_buffer, sizeof(tmp_buffer));

    page_in_rom();
    errno = 0;
    fh = esx_f_open(tmp_buffer, ESX_MODE_OPEN_EXIST | ESX_MODE_R);
    page_in_game();
    return fh;
}

void os_fatal_error(uint8_t *format, ...)
{
    va_list args;

    printf("\nFatal error: ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    wait_key();
    exit(1);
}

int main(void)
{
    uint8_t *game_file;

    init_hardware();

#if USE_GFX
    show_title();
#endif

    create_screen();

    atexit(exit_handler);

    fputs("Level 9 Interpreter for ZX Spectrum Next " VERSION "\n\n", stdout);

    game_file = get_game_file();
    if (game_file == NULL)
    {
        os_fatal_error("Unable to find game file.");
    }

    if (!load_game(game_file))
    {
        os_fatal_error("Unable to load game file.");
    }

    multiple_choice_game = is_multiple_choice_game(game_file);

#if USE_GFX
    get_picture_size(NULL, &max_image_height);
#endif

#if USE_IMAGE_SLIDESHOW
    run_image_slideshow();
#endif

#if USE_GFX
    // Make sure that graphics is enabled (Snowball) and the
    // start location image is displayed immediately (Knight Orc).
    // Image #1 is the start location image in V4 games and a
    // non-existent image in V2/V3 games (with a silent failure).
    os_graphics(true);
    os_show_bitmap(1);
#endif

    while (run_game());

    return 0;
}
