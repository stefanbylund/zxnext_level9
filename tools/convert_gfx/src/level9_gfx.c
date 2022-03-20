/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Level 9 graphics interpreter for interpreting the line-drawn pictures in a
 * Level 9 graphics file. This module is basically the graphics part ripped out
 * from the level9.c file in the original Level 9 interpreter and then made self
 * contained.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "level9_gfx.h"

#define GFX_STACK_SIZE 100

/*
 * Graphics type    Resolution     Scale stack reset
 * -------------------------------------------------
 * GFX_V2           160 x 128            yes
 * GFX_V3A          160 x 96             yes
 * GFX_V3B          160 x 96             no
 * GFX_V3C          320 x 96             no
 */

static uint8_t *picture_address = NULL;
static uint8_t *picture_data = NULL;
static uint32_t picture_size;
static GfxTypes gfx_mode = GFX_V2;

static uint8_t *gfx_a5 = NULL;

static int reflect_flag;
static int scale;
static int colour;
static int option;
static int draw_x = 0;
static int draw_y = 0;

static uint8_t *gfx_a5_stack[GFX_STACK_SIZE];
static int gfx_a5_stack_pos = 0;

static int gfx_scale_stack[GFX_STACK_SIZE];
static int gfx_scale_stack_pos = 0;

/******************************************************************************
 * Helper Functions
 *****************************************************************************/

static uint32_t file_length(FILE *f)
{
    uint32_t pos;
    uint32_t file_size;

    pos = ftell(f);
    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    fseek(f, pos, SEEK_SET);
    return file_size;
}

static bool find_gfx_subs(uint8_t *test_ptr, uint32_t test_size, uint8_t **pic_data, uint32_t *pic_size)
{
    int count;
    int length;
    uint8_t *pic_ptr;
    uint8_t *start_ptr;

    if (test_size < 16)
    {
        return false;
    }

    /*
     * Try to traverse the graphics subroutines.
     *
     * Each subroutine starts with a 3 bytes long header: nn, nl, ll
     * nnn: the subroutine number (0x000 - 0x7ff)
     * lll: the subroutine length (0x004 - 0x3ff)
     *
     * The first subroutine usually has the number 0x000.
     * Each subroutine ends with 0xff.
     *
     * This function searches for the header of the second subroutine
     * (pattern: 0xff, nn, nl, ll) and then tries to find the first
     * and next subroutines by evaluating the length fields of the
     * subroutine headers.
     */

    for (int i = 4; i < (int) (test_size - 4); i++)
    {
        pic_ptr = test_ptr + i;
        if (*(pic_ptr - 1) != 0xff || (*pic_ptr & 0x80) || (*(pic_ptr + 1) & 0x0c) || (*(pic_ptr + 2) < 4))
        {
            continue;
        }

        count = 0;
        start_ptr = pic_ptr;

        while (true)
        {
            length = ((*(pic_ptr + 1) & 0x0f) << 8) + *(pic_ptr + 2);
            if (length > 0x3ff || pic_ptr + length + 4 > test_ptr + test_size)
            {
                break;
            }

            pic_ptr += length;
            if (*(pic_ptr - 1) != 0xff)
            {
                pic_ptr -= length;
                break;
            }
            if ((*pic_ptr & 0x80) || (*(pic_ptr + 1) & 0x0c) || (*(pic_ptr + 2) < 4))
            {
                break;
            }

            count++;
        }

        if (count > 10)
        {
            uint8_t *tmp_ptr;

            /* Search for the start of the first subroutine */
            for (int j = 4; j < 0x3ff; j++)
            {
                tmp_ptr = start_ptr - j;
                if (*tmp_ptr == 0xff || tmp_ptr < test_ptr)
                {
                    break;
                }

                length = ((*(tmp_ptr + 1) & 0x0f) << 8) + *(tmp_ptr + 2);
                if (tmp_ptr + length == start_ptr)
                {
                    start_ptr = tmp_ptr;
                    break;
                }
            }

            if (*tmp_ptr != 0xff)
            {
                *pic_data = start_ptr;
                *pic_size = pic_ptr - start_ptr;
                return true;
            }
        }
    }

    return false;
}

static bool valid_gfx_ptr(uint8_t *ptr)
{
    return (ptr >= picture_data) && (ptr < picture_data + picture_size);
}

/*
 * Find the graphics subroutine with number d0 and return its address in a5 if found.
 */
static bool find_gfx_sub(int d0, uint8_t **a5)
{
    int d1;
    int d2;
    int d3;
    int d4;

    d1 = d0 << 4;
    d2 = d1 >> 8;
    *a5 = picture_data;

    while (true)
    {
        d3 = *(*a5)++;
        if (!valid_gfx_ptr(*a5))
        {
            return false;
        }
        if (d3 & 0x80)
        {
            return false;
        }
        if (d2 == d3)
        {
            if ((d1 & 0xff) == (*(*a5) & 0xf0))
            {
                (*a5) += 2;
                return true;
            }
        }

        d3 = *(*a5)++ & 0x0f;
        if (!valid_gfx_ptr(*a5))
        {
            return false;
        }

        d4 = **a5;
        if ((d3 | d4) == 0)
        {
            return false;
        }

        (*a5) += (d3 << 8) + d4 - 2;
        if (!valid_gfx_ptr(*a5))
        {
            return false;
        }
    }
}

static void gosub_d0(int d0, uint8_t **a5)
{
    if (gfx_a5_stack_pos < GFX_STACK_SIZE)
    {
        gfx_a5_stack[gfx_a5_stack_pos] = *a5;
        gfx_a5_stack_pos++;

        gfx_scale_stack[gfx_scale_stack_pos] = scale;
        gfx_scale_stack_pos++;

        if (!find_gfx_sub(d0, a5))
        {
            gfx_a5_stack_pos--;
            *a5 = gfx_a5_stack[gfx_a5_stack_pos];

            gfx_scale_stack_pos--;
            scale = gfx_scale_stack[gfx_scale_stack_pos];
        }
    }
}

static int scale_x(int x)
{
    return (gfx_mode != GFX_V3C) ? (x >> 6) : (x >> 5);
}

static int scale_y(int y)
{
    return (gfx_mode == GFX_V2) ? 127 - (y >> 7) : 95 - (((y >> 5) + (y >> 6)) >> 3);
}

static void new_xy(int x, int y)
{
    draw_x += (x * scale) & ~7;
    draw_y += (y * scale) & ~7;
}

/******************************************************************************
 * Graphics Instructions
 *****************************************************************************/

/* sdraw instruction plus arguments are stored in an 8-bit word.
 *     76543210
 *     iixxxyyy
 * where i is instruction code
 *     x is x argument, high bit is sign
 *     y is y argument, high bit is sign
 */
static void sdraw(int d7)
{
    int x;
    int y;
    int x1;
    int y1;

    x = (d7 & 0x18) >> 3;
    if (d7 & 0x20)
    {
        x = (x | 0xfc) - 0x100;
    }

    y = (d7 & 0x3) << 2;
    if (d7 & 0x4)
    {
        y = (y | 0xf0) - 0x100;
    }

    if (reflect_flag & 2)
    {
        x = -x;
    }

    if (reflect_flag & 1)
    {
        y = -y;
    }

    x1 = draw_x;
    y1 = draw_y;
    new_xy(x, y);

    os_draw_line(scale_x(x1), scale_y(y1), scale_x(draw_x), scale_y(draw_y), colour & 3, option & 3);
}

/* smove instruction plus arguments are stored in an 8-bit word.
 *     76543210
 *     iixxxyyy
 * where i is instruction code
 *     x is x argument, high bit is sign
 *     y is y argument, high bit is sign
 */
static void smove(int d7)
{
    int x;
    int y;

    x = (d7 & 0x18) >> 3;
    if (d7 & 0x20)
    {
        x = (x | 0xfc) - 0x100;
    }

    y = (d7 & 0x3) << 2;
    if (d7 & 0x4)
    {
        y = (y | 0xf0) - 0x100;
    }

    if (reflect_flag & 2)
    {
        x = -x;
    }

    if (reflect_flag & 1)
    {
        y = -y;
    }

    new_xy(x, y);
}

static void sgosub(int d7, uint8_t **a5)
{
    int d0 = d7 & 0x3f;
    gosub_d0(d0, a5);
}

/* draw instruction plus arguments are stored in a 16-bit word.
 *     FEDCBA9876543210
 *     iiiiixxxxxxyyyyy
 * where i is instruction code
 *     x is x argument, high bit is sign
 *     y is y argument, high bit is sign
 */
static void draw(int d7, uint8_t **a5)
{
    int xy;
    int x;
    int y;
    int x1;
    int y1;

    xy = (d7 << 8) + (*(*a5)++);

    x = (xy & 0x3e0) >> 5;
    if (xy & 0x400)
    {
        x = (x | 0xe0) - 0x100;
    }

    y = (xy & 0xf) << 2;
    if (xy & 0x10)
    {
        y = (y | 0xc0) - 0x100;
    }

    if (reflect_flag & 2)
    {
        x = -x;
    }

    if (reflect_flag & 1)
    {
        y = -y;
    }

    x1 = draw_x;
    y1 = draw_y;
    new_xy(x, y);

    os_draw_line(scale_x(x1), scale_y(y1), scale_x(draw_x), scale_y(draw_y), colour & 3, option & 3);
}

/* move instruction plus arguments are stored in a 16-bit word.
 *     FEDCBA9876543210
 *     iiiiixxxxxxyyyyy
 * where i is instruction code
 *     x is x argument, high bit is sign
 *     y is y argument, high bit is sign
 */
static void move(int d7, uint8_t **a5)
{
    int xy;
    int x;
    int y;

    xy = (d7 << 8) + (*(*a5)++);

    x = (xy & 0x3e0) >> 5;
    if (xy & 0x400)
    {
        x = (x | 0xe0) - 0x100;
    }

    y = (xy & 0xf) << 2;
    if (xy & 0x10)
    {
        y = (y | 0xc0) - 0x100;
    }

    if (reflect_flag & 2)
    {
        x = -x;
    }

    if (reflect_flag & 1)
    {
        y = -y;
    }

    new_xy(x, y);
}

static void icolour(int d7)
{
    colour = d7 & 3;
}

static void size(int d7)
{
    static int size_table[7] = { 0x02, 0x04, 0x06, 0x07, 0x09, 0x0c, 0x10 };

    d7 &= 7;

    if (d7)
    {
        int d0 = (scale * size_table[d7 - 1]) >> 3;
        scale = (d0 < 0x100) ? d0 : 0xff;
    }
    else
    {
        /* size reset */
        scale = 0x80;
        if (gfx_mode == GFX_V2 || gfx_mode == GFX_V3A)
        {
            gfx_scale_stack_pos = 0;
        }
    }
}

static void fill(int d7)
{
    if ((d7 & 7) == 0)
    {
        /* fill_a */
        d7 = colour;
    }
    else
    {
        /* fill_b */
        d7 &= 3;
    }

    os_fill(scale_x(draw_x), scale_y(draw_y), d7 & 3, option & 3);
}

static void gosub(int d7, uint8_t **a5)
{
    int d0 = ((d7 & 7) << 8) + (*(*a5)++);
    gosub_d0(d0, a5);
}

static void reflect(int d7)
{
    if (d7 & 4)
    {
        d7 &= 3;
        d7 ^= reflect_flag;
    }

    reflect_flag = d7;
}

static void not_imp(void)
{
    // Not implemented
}

static void change_colour(uint8_t **a5)
{
    int d0 = *(*a5)++;
    os_set_colour((d0 >> 3) & 3, d0 & 7);
}

static void amove(uint8_t **a5)
{
    draw_x = 0x40 * (*(*a5)++);
    draw_y = 0x40 * (*(*a5)++);
}

static void opt(uint8_t **a5)
{
    int d0 = *(*a5)++;

    if (d0)
    {
        d0 = (d0 & 3) | 0x80;
    }

    option = d0;
}

static void restore_scale(void)
{
    if (gfx_scale_stack_pos > 0)
    {
        scale = gfx_scale_stack[gfx_scale_stack_pos - 1];
    }
}

static bool rts(uint8_t **a5)
{
    if (gfx_a5_stack_pos > 0)
    {
        gfx_a5_stack_pos--;
        *a5 = gfx_a5_stack[gfx_a5_stack_pos];

        if (gfx_scale_stack_pos > 0)
        {
            gfx_scale_stack_pos--;
            scale = gfx_scale_stack[gfx_scale_stack_pos];
        }

        return true;
    }

    return false;
}

static bool run_instruction(uint8_t **a5)
{
    int d7 = *(*a5)++;

    if ((d7 & 0xc0) != 0xc0)
    {
        switch ((d7 >> 6) & 3)
        {
            case 0: sdraw(d7); break;
            case 1: smove(d7); break;
            case 2: sgosub(d7, a5); break;
        }
    }
    else if ((d7 & 0x38) != 0x38)
    {
        switch ((d7 >> 3) & 7)
        {
            case 0: draw(d7, a5); break;
            case 1: move(d7, a5); break;
            case 2: icolour(d7); break;
            case 3: size(d7); break;
            case 4: fill(d7); break;
            case 5: gosub(d7, a5); break;
            case 6: reflect(d7); break;
        }
    }
    else
    {
        switch (d7 & 7)
        {
            case 0: not_imp(); break;
            case 1: change_colour(a5); break;
            case 2: not_imp(); break;
            case 3: amove(a5); break;
            case 4: opt(a5); break;
            case 5: restore_scale(); break;
            case 6: not_imp(); break;
            case 7: return rts(a5);
        }
    }

    return true;
}

static void abs_run_gfx_sub(int d0)
{
    uint8_t *a5;

    if (!find_gfx_sub(d0, &a5))
    {
        return;
    }

    while (run_instruction(&a5));
}

/******************************************************************************
 * Public Functions
 *****************************************************************************/

bool load_graphics(char *filename, GfxTypes gfx_type)
{
    FILE *f;

    picture_address = NULL;
    picture_data = NULL;
    picture_size = 0;

    gfx_mode = gfx_type;
    gfx_a5 = NULL;

    f = fopen(filename, "rb");
    if (f == NULL)
    {
        fprintf(stderr, "Error opening graphics file.\n");
        return false;
    }

    picture_size = file_length(f);
    picture_address = malloc(picture_size);
    if (picture_address == NULL)
    {
        fclose(f);
        fprintf(stderr, "Unable to allocate memory for graphics file.\n");
        return false;
    }

    if (fread(picture_address, 1, picture_size, f) != picture_size)
    {
        free(picture_address);
        fclose(f);
        fprintf(stderr, "Error reading graphics file.\n");
        return false;
    }
    fclose(f);

    if (!find_gfx_subs(picture_address, picture_size, &picture_data, &picture_size))
    {
        free(picture_address);
        fprintf(stderr, "Error processing graphics file.\n");
        return false;
    }

    os_init_graphics();

    return true;
}

void get_picture_size(int *width, int *height)
{
    if (width != NULL)
    {
        *width = (gfx_mode != GFX_V3C) ? 160 : 320;
    }

    if (height != NULL)
    {
        *height = (gfx_mode == GFX_V2) ? 128 : 96;
    }
}

bool show_picture(int pic)
{
    os_clear_graphics();

    reflect_flag = 0;
    scale = 0x80;
    colour = 3;
    option = 0x80;
    draw_x = 0x1400;
    draw_y = 0x1400;

    gfx_a5_stack_pos = 0;
    gfx_scale_stack_pos = 0;

    // Run graphics subroutine #0.
    abs_run_gfx_sub(0);

    // Find graphics subroutine #pic.
    if (!find_gfx_sub(pic, &gfx_a5))
    {
        gfx_a5 = NULL;
        return false;
    }

    return true;
}

bool run_graphics(void)
{
    if (gfx_a5)
    {
        if (!run_instruction(&gfx_a5))
        {
            gfx_a5 = NULL;
        }

        return true;
    }

    return false;
}

void free_memory(void)
{
    if (picture_address)
    {
        free(picture_address);
        picture_address = NULL;
    }
}
