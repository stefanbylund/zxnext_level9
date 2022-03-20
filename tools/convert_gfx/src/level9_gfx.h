/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Routines for interpreting and drawing the pictures in a Level 9 graphics file.
 * Based on the level9.h file from the Level 9 interpreter.
 ******************************************************************************/

#include <stdbool.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

typedef enum
{
    GFX_V2,
    GFX_V3A,
    GFX_V3B,
    GFX_V3C,
    GFX_UNKNOWN
} GfxTypes;

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * Routines provided by OS-dependent code
 ******************************************************************************/

void os_init_graphics(void);

void os_clear_graphics(void);

void os_set_colour(int colour, int index);

void os_draw_line(int x1, int y1, int x2, int y2, int colour1, int colour2);

void os_fill(int x, int y, int colour1, int colour2);

/*******************************************************************************
 * Routines provided by Level 9 graphics interpreter
 ******************************************************************************/

bool load_graphics(char *filename, GfxTypes gfx_type);

void get_picture_size(int *width, int *height);

bool show_picture(int pic);

bool run_graphics(void);

void free_memory(void);

#ifdef __cplusplus
}
#endif
