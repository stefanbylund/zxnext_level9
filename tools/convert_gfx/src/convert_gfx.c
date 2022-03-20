/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Tool for converting the pictures in a Level 9 graphics file to separate NXI
 * image files for ZX Spectrum Next.
 *
 * The routines for interpreting and drawing the pictures are based on the
 * level9.c and Lev9win.cpp files from the Level 9 interpreter.
 *
 * The tool uses an off-screen Windows bitmap for the Level 9 graphics
 * interpreter to draw the pictures on and another off-screen Windows bitmap for
 * stretching them to their final size. The stretched Windows bitmaps are then
 * converted to NXI images.
 ******************************************************************************/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "level9_gfx.h"

#define H_WND_MAIN NULL
#define L9_PALETTE_SIZE 4

#define NXI_PALETTE_SIZE 512
#define NXI_IMAGE_WIDTH 320
#define NXI_IMAGE_HEIGHT 256

/*
 * We want the pictures to have a 14 pixel top margin and a 2 pixel bottom
 * margin. The top margin is to compensate for that not all monitors can display
 * the full height of the layer 2 320x256 graphics mode and the bottom margin is
 * for having some space between the text and the picture.
 */
#define PICTURE_TOP_MARGIN 14

typedef struct
{
    BITMAPINFOHEADER bmi_header;
    RGBQUAD bmi_colors[L9_PALETTE_SIZE];
} L9_BITMAPINFO;

// Context for drawing the picture in its original size.
static HDC h_dc = NULL;
static HBITMAP h_bitmap = NULL;
static uint8_t *pixels = NULL;
static int pic_width = 0;
static int pic_height = 0;

// Context for stretching the picture to its final size.
static HDC h_draw_dc = NULL;
static HBITMAP h_draw_bitmap = NULL;
static uint8_t *draw_pixels = NULL;
static int draw_pic_width = 0;
static int draw_pic_height = 0;

// The Level 9 colour table is slightly changed to fit Spectrum Next better.
static RGBQUAD colours[8] =
{
    { 0x00, 0x00, 0x00, 0 }, // Black
    { 0x00, 0x00, 0xFF, 0 }, // Red
    { 0x24, 0xDB, 0x24, 0 }, // Green
    { 0x00, 0xFF, 0xFF, 0 }, // Yellow
    { 0xFF, 0x00, 0x00, 0 }, // Blue
    { 0x00, 0x6D, 0x92, 0 }, // Brown
    { 0xFF, 0xFF, 0x00, 0 }, // Cyan
    { 0xFF, 0xFF, 0xFF, 0 }  // White
};

static RGBQUAD palette[L9_PALETTE_SIZE] =
{
    { 0x00, 0x00, 0x00, 0 },
    { 0x00, 0x00, 0x00, 0 },
    { 0x00, 0x00, 0x00, 0 },
    { 0x00, 0x00, 0x00, 0 }
};

static COLORREF line_colour1 = 0;
static COLORREF line_colour2 = 0;

static uint8_t nxi_palette[NXI_PALETTE_SIZE];
static uint8_t nxi_image[NXI_IMAGE_WIDTH * NXI_IMAGE_HEIGHT];

/*******************************************************************************
 * Helper Functions
 ******************************************************************************/

static void print_usage(void)
{
    printf("Usage: convert_gfx <graphics-file> [<graphics-type>]\n");
    printf("Convert the pictures in a Level 9 graphics file of the given type to ZX Spectrum Next format.\n");
    printf("\n");
    printf("The <graphics-type> argument can be one of:\n");
    printf("GFX_V2");
    printf("\n");
    printf("GFX_V3A");
    printf("\n");
    printf("GFX_V3B");
    printf("\n");
    printf("GFX_V3C (default)");
    printf("\n");
}

static void exit_with_msg(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    exit(1);
}

static void get_dir(char *path)
{
    char *dir;

    dir = strrchr(path, '\\');
    if (dir == NULL)
    {
        dir = strrchr(path, '/');
        if (dir == NULL)
        {
            dir = path;
        }
    }

    *dir = '\0';
}

static GfxTypes get_gfx_type(char *type)
{
    if (stricmp(type, "GFX_V2") == 0)
    {
        return GFX_V2;
    }
    else if (stricmp(type, "GFX_V3A") == 0)
    {
        return GFX_V3A;
    }
    else if (stricmp(type, "GFX_V3B") == 0)
    {
        return GFX_V3B;
    }
    else if (stricmp(type, "GFX_V3C") == 0)
    {
        return GFX_V3C;
    }
    else
    {
        return GFX_UNKNOWN;
    }
}

/*
 * The set_index_palette() function sets the palette for a Windows bitmap with
 * four distinct RGB colours that are based on the four palette index values 0-3.
 *
 * The corresponding get_index_colour() function retrieves the RGB colour with
 * the given index from the palette of a Windows bitmap.
 *
 * The palette for a Windows bitmap is set when the bitmap is created and then
 * stays constant. Note that the actual RGB colours in the Windows bitmap palette
 * are not important in this case. The only important property is that they are
 * distinct. The actual colours used by the final picture is in the palette
 * managed by the os_set_colour() function. That palette can be updated while
 * a picture is being drawn.
 */

static void set_index_palette(RGBQUAD *pal)
{
    for (int i = 0; i < L9_PALETTE_SIZE; i++)
    {
        pal[i].rgbBlue = 8 * i;
        pal[i].rgbGreen = 8 * i;
        pal[i].rgbRed = 8 * i;
        pal[i].rgbReserved = 0;
    }
}

// index: 0-3
static COLORREF get_index_colour(int index)
{
    int i = 8 * index;
    return RGB(i, i, i);
}

static void create_bitmap_info(L9_BITMAPINFO *bitmap_info, int width, int height)
{
    // Create info for an 8 bits-per-pixel top-down bitmap.
    memset(bitmap_info, 0, sizeof(L9_BITMAPINFO));
    bitmap_info->bmi_header.biSize = sizeof(BITMAPINFOHEADER);
    bitmap_info->bmi_header.biPlanes = 1;
    bitmap_info->bmi_header.biBitCount = 8;
    bitmap_info->bmi_header.biCompression = BI_RGB;
    bitmap_info->bmi_header.biWidth = width;
    bitmap_info->bmi_header.biHeight = height * -1;
    bitmap_info->bmi_header.biClrUsed = L9_PALETTE_SIZE;
    bitmap_info->bmi_header.biClrImportant = L9_PALETTE_SIZE;
    set_index_palette(bitmap_info->bmi_colors);
}

static bool is_blank_picture()
{
    uint8_t *image_ptr = pixels;
    uint8_t first_pixel = *image_ptr;

    for (int y = 0; y < pic_height; y++)
    {
        for (int x = 0; x < pic_width; x++)
        {
            if (image_ptr[x] != first_pixel)
            {
                return false;
            }
        }
        image_ptr += pic_width;
    }

    return true;
}

static void draw_picture(void)
{
    RECT rect;
    rect.left = 0;
    rect.right = draw_pic_width;
    rect.top = 0;
    rect.bottom = draw_pic_height;
    FillRect(h_draw_dc, &rect, (HBRUSH) GetStockObject(BLACK_BRUSH));

    // Blit and (if needed) stretch the picture to its final size.
    StretchBlt(h_draw_dc, 0, 0, draw_pic_width, draw_pic_height,
        h_dc, 0, 0, pic_width, pic_height, SRCCOPY);
}

static void free_resources(void)
{
    DeleteObject(h_bitmap);
    DeleteDC(h_dc);

    DeleteObject(h_draw_bitmap);
    DeleteDC(h_draw_dc);
}

static void nxi_name(int num, char *out)
{
    sprintf(out, "%d.nxi", num);
}

static uint8_t c8_to_c3(uint8_t c8)
{
    return (uint8_t) round((c8 * 7.0) / 255.0);
}

static void create_nxi_palette(void)
{
    memset(nxi_palette, 0, sizeof(nxi_palette));

    // The RGB888 colors in the Windows bitmap palette are converted
    // to RGB333 colors, which are then split in RGB332 and B1 parts.
    for (int i = 0; i < L9_PALETTE_SIZE; i++)
    {
        RGBQUAD *colour = &(palette[i]);

        uint8_t r3 = c8_to_c3(colour->rgbRed);
        uint8_t g3 = c8_to_c3(colour->rgbGreen);
        uint8_t b3 = c8_to_c3(colour->rgbBlue);

        uint16_t rgb333 = (r3 << 6) | (g3 << 3) | (b3 << 0);
        uint8_t rgb332 = (uint8_t) (rgb333 >> 1);
        uint8_t b1 = (uint8_t) (rgb333 & 0x01);

        nxi_palette[i * 2 + 0] = rgb332;
        nxi_palette[i * 2 + 1] = b1;
    }
}

static void create_nxi_image(void)
{
    uint8_t *image_ptr = draw_pixels;

    /*
     * We want the unused parts of the image to be black. Since black is not
     * guaranteed to be included in the 4 colour Level 9 palette, we choose
     * one of the other 252 colours in the layer 2 palette, which are all
     * initilaized to black.
     */
    memset(nxi_image, 255, sizeof(nxi_image));

    for (int y = 0; y < draw_pic_height; y++)
    {
        for (int x = 0; x < draw_pic_width; x++)
        {
            nxi_image[(PICTURE_TOP_MARGIN + y) + x * NXI_IMAGE_HEIGHT] = image_ptr[x];
        }
        image_ptr += draw_pic_width;
    }
}

static void convert_nxi(int num)
{
    char nxi_filename[MAX_PATH];

    create_nxi_palette();
    create_nxi_image();

    nxi_name(num, nxi_filename);
    FILE *nxi_file = fopen(nxi_filename, "wb");
    if (nxi_file == NULL)
    {
        exit_with_msg("Error creating image file %s.\n", nxi_filename);
    }

    if (fwrite(nxi_palette, 1, sizeof(nxi_palette), nxi_file) != sizeof(nxi_palette))
    {
        exit_with_msg("Error writing palette to file %s.\n", nxi_filename);
    }

    if (fwrite(nxi_image, 1, sizeof(nxi_image), nxi_file) != sizeof(nxi_image))
    {
        exit_with_msg("Error writing image data to file %s.\n", nxi_filename);
    }

    fclose(nxi_file);
}

/*******************************************************************************
 * Graphics Routines
 ******************************************************************************/

void os_init_graphics(void)
{
    // Setup context for drawing the picture in its original size.

    HDC h_dc_main = GetDC(H_WND_MAIN);
    h_dc = CreateCompatibleDC(h_dc_main);
    ReleaseDC(H_WND_MAIN, h_dc_main);

    get_picture_size(&pic_width, &pic_height);

    L9_BITMAPINFO bitmap_info;
    create_bitmap_info(&bitmap_info, pic_width, pic_height);

    h_bitmap = CreateDIBSection(h_dc, (BITMAPINFO *) &bitmap_info,
        DIB_RGB_COLORS, (VOID **) &pixels, NULL, 0);
    SelectObject(h_dc, h_bitmap);

    // Setup context for stretching the picture to its final size.

    draw_pic_width = pic_width;
    draw_pic_height = pic_height;

    // Widen GFX_V2/GFX_V3A/GFX_V3B pictures from 160 to 320 pixels (as used by GFX_V3C).
    if (draw_pic_width < 320)
    {
        draw_pic_width = 320;
    }

    h_draw_dc = CreateCompatibleDC(h_dc);
    SetStretchBltMode(h_draw_dc, COLORONCOLOR);

    L9_BITMAPINFO draw_bitmap_info;
    create_bitmap_info(&draw_bitmap_info, draw_pic_width, draw_pic_height);

    h_draw_bitmap = CreateDIBSection(h_draw_dc, (BITMAPINFO *) &draw_bitmap_info,
        DIB_RGB_COLORS, (VOID **) &draw_pixels, NULL, 0);
    SelectObject(h_draw_dc, h_draw_bitmap);
}

void os_clear_graphics(void)
{
    RECT rect;
    rect.left = 0;
    rect.right = pic_width;
    rect.top = 0;
    rect.bottom = pic_height;

    HBRUSH brush = CreateSolidBrush(get_index_colour(0));
    HGDIOBJ old = SelectObject(h_dc, brush);
    FillRect(h_dc, &rect, brush);
    SelectObject(h_dc, old);
    DeleteObject(brush);
}

// colour: 0-3, index: 0-7
void os_set_colour(int colour, int index)
{
    palette[colour] = colours[index];
}

void CALLBACK line_proc(int x, int y, LPARAM p)
{
    if (GetPixel(h_dc, x, y) == line_colour2)
    {
        SetPixel(h_dc, x, y, line_colour1);
    }
}

// colour: 0-3
void os_draw_line(int x1, int y1, int x2, int y2, int colour1, int colour2)
{
    line_colour1 = get_index_colour(colour1);
    line_colour2 = get_index_colour(colour2);
    LineDDA(x1, y1, x2, y2, line_proc, (LPARAM) NULL);
    line_proc(x2, y2, (LPARAM) NULL);
}

// colour: 0-3
void os_fill(int x, int y, int colour1, int colour2)
{
    COLORREF colour = get_index_colour(colour2);
    if (GetPixel(h_dc, x, y) == colour)
    {
        HBRUSH brush = CreateSolidBrush(get_index_colour(colour1));
        HGDIOBJ old = SelectObject(h_dc, brush);
        ExtFloodFill(h_dc, x, y, colour, FLOODFILLSURFACE);
        SelectObject(h_dc, old);
        DeleteObject(brush);
    }
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(int argc, char *argv[])
{
    char *filename;
    char *type;
    char dir[MAX_PATH];
    GfxTypes gfx_type;

    if (argc < 2)
    {
        print_usage();
        return 1;
    }

    filename = argv[1];
    type = (argc > 2) ? argv[2] : "GFX_V3C";

    strcpy(dir, filename);
    get_dir(dir);

    gfx_type = get_gfx_type(type);
    if (gfx_type == GFX_UNKNOWN)
    {
        fprintf(stderr, "Error: Unknown graphics type %s.\n", type);
        return 1;
    }

    if (!load_graphics(filename, gfx_type))
    {
        fprintf(stderr, "Error: Unable to load graphics file %s.\n", filename);
        return 1;
    }

    printf("Converting graphics file %s of type %s\n", filename, type);

    /*
     * It seems that all complete pictures use graphics subroutines in the range
     * 500 to 800, although not all consecutive numbers in this range are used.
     * The graphics subroutines below 500 seem to be for sub-images.
     */
    for (int i = 500; i < 800; i++)
    {
        if (show_picture(i))
        {
            while (run_graphics());
            if (!is_blank_picture())
            {
                draw_picture();
                convert_nxi(i);
                printf("Converted picture %d\n", i);
            }
        }
    }

    free_memory();
    free_resources();
    return 0;
}
