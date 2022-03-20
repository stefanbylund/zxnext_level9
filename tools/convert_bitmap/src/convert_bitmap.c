/*******************************************************************************
 * Stefan Bylund 2021
 *
 * Tool for converting Commodore Amiga and Atari ST bitmap image files from the
 * later Level 9 games to NXI image files for ZX Spectrum Next.
 *
 * The routines for parsing the bitmap images are based on the bitmap.c file
 * from the Level 9 interpreter.
 *
 * The bitmap pictures are numbered starting from 0. Picture #0 is the title
 * picture, which on Amiga is called "title" and on Atari ST is picture #30.
 * On Spectrum Next, picture #0 will be called just that. Picture #1 is the
 * first location picture that carries the frame. All Level 9 games with bitmap
 * pictures have a frame around their location pictures. Picture #2 and onwards
 * are location pictures that are stored without the frame. They were originally
 * drawn inside picture #1 at runtime to borrow its frame. On Spectrum Next, we
 * will put the frame on all location pictures statically using this tool to
 * speed up the runtime handling of them. Picture #30 is skipped as location
 * picture since it's used as title picture on Atari ST.
 ******************************************************************************/

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef MAX_PATH
#define MAX_PATH 256
#endif

#define MAX_BITMAP_WIDTH 320
#define MAX_BITMAP_HEIGHT 256

#define NXI_PALETTE_SIZE 512
#define NXI_IMAGE_WIDTH 320
#define NXI_IMAGE_HEIGHT 256

#define PICTURE_HEIGHT 152

#define GAME_KNIGHT_ORC "knight-orc"
#define GAME_GNOME_RANGER "gnome-ranger"
#define GAME_TIME_AND_MAGIK "time-and-magik"
#define GAME_LANCELOT "lancelot"
#define GAME_INGRIDS_BACK "ingrids-back"
#define GAME_SCAPEGHOST "scapeghost"

typedef enum
{
    AMIGA_BITMAPS,
    ST_BITMAPS,
    NO_BITMAPS
} BitmapType;

typedef enum
{
    FLOOR,
    CEIL,
    ROUND
} RoundingMode;

typedef struct
{
    uint8_t red;
    uint8_t green;
    uint8_t blue;
} Colour;

typedef struct
{
    uint16_t width;
    uint16_t height;
    uint8_t *bitmap;
    Colour palette[32];
    uint16_t num_palette_colours;
} Bitmap;

static uint8_t nxi_palette[NXI_PALETTE_SIZE];

static uint8_t nxi_image[NXI_IMAGE_WIDTH * NXI_IMAGE_HEIGHT];

static uint8_t nxi_frame_image[NXI_IMAGE_WIDTH * NXI_IMAGE_HEIGHT];

static uint16_t picture_top_margin = 0;

static void print_usage(void)
{
    printf("Usage: convert_bitmap <game> <directory>\n");
    printf("Convert Level 9 bitmap files to ZX Spectrum Next format for a given game located in a given directory.\n");
    printf("Only Amiga and Atari ST bitmap files are supported.\n");
    printf("\n");
    printf("The <game> argument can be one of:\n");
    printf(GAME_KNIGHT_ORC);
    printf("\n");
    printf(GAME_GNOME_RANGER);
    printf("\n");
    printf(GAME_TIME_AND_MAGIK);
    printf("\n");
    printf(GAME_LANCELOT);
    printf("\n");
    printf(GAME_INGRIDS_BACK);
    printf("\n");
    printf(GAME_SCAPEGHOST);
    printf("\n");
}

static void exit_with_msg(const char *format, ...)
{
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);

  exit(1);
}

static bool validate_game(char *game)
{
    if (stricmp(game, GAME_KNIGHT_ORC) == 0)
    {
        return true;
    }
    else if (stricmp(game, GAME_GNOME_RANGER) == 0)
    {
        return true;
    }
    else if (stricmp(game, GAME_TIME_AND_MAGIK) == 0)
    {
        return true;
    }
    else if (stricmp(game, GAME_LANCELOT) == 0)
    {
        return true;
    }
    else if (stricmp(game, GAME_INGRIDS_BACK) == 0)
    {
        return true;
    }
    else if (stricmp(game, GAME_SCAPEGHOST) == 0)
    {
        return true;
    }
    else
    {
        return false;
    }
}

static void extend_dir_path(char *dir, char *out)
{
    strcpy(out, dir);
    int length = strlen(out);
    if ((out[length - 1] != '/') && (out[length - 1] != '\\'))
    {
        // Windows supports '/' as path separator nowadays.
        out[length] = '/';
        out[length + 1] = '\0';
    }
}

static bool is_st_title_bitmap(BitmapType type, int num)
{
    return (num == 30) && (type == ST_BITMAPS);
}

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

static bool bitmap_exists(char *file)
{
    FILE *f = fopen(file, "rb");
    if (f != NULL)
    {
        fclose(f);
        return true;
    }
    return false;
}

static uint8_t *bitmap_load(char *file, uint32_t *size)
{
    uint8_t *data = NULL;
    FILE *f = fopen(file, "rb");

    if (f != NULL)
    {
        *size = file_length(f);
        data = malloc(*size);
        if (data != NULL)
        {
            if (fread(data, 1, *size, f) != *size)
            {
                free(data);
                data = NULL;
            }
        }
        fclose(f);
    }

    return data;
}

static Bitmap *bitmap_alloc(uint16_t w, uint16_t h)
{
    Bitmap *bitmap = malloc(sizeof(Bitmap) + (w * h));
    if (bitmap != NULL)
    {
        bitmap->width = w;
        bitmap->height = h;
        bitmap->bitmap = ((uint8_t *) bitmap) + sizeof(Bitmap);
        bitmap->num_palette_colours = 0;
    }
    return bitmap;
}

static void bitmap_st_name(int num, char *dir, char *out)
{
    /* Title picture is #30 */
    if (num == 0)
    {
        num = 30;
    }

    sprintf(out, "%s%d.squ", dir, num);
}

/*
 * Atari ST Palette Colour
 * -----------------------
 *
 * An Atari ST palette colour is a 16-bit value in which the low three nibbles
 * hold the RGB colour values. The lowest nibble holds the blue value, the
 * second nibble the green value and the third nibble the red value. (The high
 * nibble is ignored). Within each nibble, only the low three bits are used,
 * i.e. the value can only be 0-7 not the full possible 0-15 and so the most
 * significant bit in each nibble is always 0.
 */

static Colour bitmap_st_colour(int big, int small)
{
    Colour col;
    uint32_t r = big & 0xF;
    uint32_t g = (small >> 4) & 0xF;
    uint32_t b = small & 0xF;

    r *= 0x49;
    r >>= 1;
    g *= 0x49;
    g >>= 1;
    b *= 0x49;
    b >>= 1;

    col.red = (uint8_t) (r & 0xFF);
    col.green = (uint8_t) (g & 0xFF);
    col.blue = (uint8_t) (b & 0xFF);
    return col;
}

/*
 * Atari ST Bitmap Format
 * ----------------------
 *
 * On the Atari ST, different graphics file formats were used for the early V4
 * games (Knight Orc and Gnome Ranger) and the later V4 games (Time & Magik,
 * Lancelot, Ingrid's Back and Scapeghost). This tool only supports the later
 * format, which was also used by the later Level 9 games for PC.
 *
 * An Atari ST v2 image file has the following format. It consists of a 570
 * byte header followed by the image data.
 *
 * The header has the following format:
 * Bytes 0-1: "datalen": length of file - 1 as a big-endian word.
 * Bytes 2-3: "flagbyte1 & flagbyte2": unknown, possibly type identifiers.
 *     Usually 0xFF or 0xFE followed by 0x84, 0x72, 0xFF, 0xFE or
 *     some other (of a fairly small range of possibles) byte.
 * Bytes 4-35: "colour_index[]": 16 entries palette. Basically an ST
 *     palette (even if in a PC image file). Each entry is a 16-bit
 *     value in which the low three nibbles hold the RGB colour
 *     values. The lowest nibble holds the blue value, the second
 *     nibble the green value and the third nibble the red value. (The
 *     high nibble is ignored). Within each nibble, only the low
 *     three bits are used, i.e. the value can only be 0-7 not the full
 *     possible 0-15 and so the most significant bit in each nibble is
 *     always 0.
 * Bytes 36-37: "width": image width in pixels as a big-endian word.
 * Bytes 38-39: "numrows": image height in pixel rows as a big-endian word.
 * Byte 40: "seedByte": seed byte to start picture decoding.
 * Byte 41: "padByte": unknown. Possibly padding to word align the next element?
 * Bytes 42-297: "pixelTable": an array of 0x100 bytes used as a lookup table
 *     for pixel values.
 * Bytes 298-313: "bitStripTable": an array of 0x10 bytes used as a lookup table
 *     for the number of bytes to strip from the bit stream for the pixel being
 *     decoded.
 * Bytes 314-569: "indexByteTable": an array of 0x100 bytes used as a lookup
 *     table to index into bitStripTable and pixelTable.
 *
 * The encoded image data then follows ending in a 0x00 at the file length stored
 * in the first two bytes of the file. There is then one extra byte holding a
 * checksum produced by the addition of all the bytes in the file (except the first
 * two and itself).
 *
 * As this file format is intended for two very different platforms the decoded
 * image data is in a neutral, intermediate form. Each pixel is extracted as a
 * byte with only the low four bits significant. The pixel value is an index into
 * the 16 entry palette.
 *
 * The pixel data is compressed, presumably to allow a greater number of images
 * to be distributed on the (rather small) default ST & PC floppy disks (in both
 * cases about 370 KB).
 *
 * Here's how to decode the data. The image data is actually a contiguous bit
 * stream with the byte structure on disk having almost no relevance to the
 * encoding. We access the bit stream via a two-byte buffer arranged as a word.
 * See the original bitmap.c file from the Level 9 interpreter for more detailed
 * information.
 */

static Bitmap *bitmap_st_decode(char *file)
{
    uint8_t *data = NULL;
    uint32_t size;

    Bitmap *bitmap;
    int max_x;
    int max_y;

    uint8_t newPixel;
    uint8_t newPixelIndex;
    uint8_t bufferBitCounter;
    uint8_t newPixelIndexSelector;
    uint8_t bufferBitStripCount;
    uint16_t bitStreamBuffer;
    uint16_t imageDataIndex;
    uint8_t *imageFileData;

    data = bitmap_load(file, &size);
    if (data == NULL)
    {
        return NULL;
    }

    max_x = data[37] + data[36] * 256;
    max_y = data[39] + data[38] * 256;
    if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
    {
        free(data);
        return NULL;
    }

    bitmap = bitmap_alloc(max_x, max_y);
    if (bitmap == NULL)
    {
        free(data);
        return NULL;
    }

    if (max_x > bitmap->width)
    {
        max_x = bitmap->width;
    }
    if (max_y > bitmap->height)
    {
        max_y = bitmap->height;
    }

    /* prime the new pixel variable with the seed byte */
    newPixel = data[40];
    /* initialise the index to the image data */
    imageDataIndex = 0;
    /* prime the bit stream buffer */
    imageFileData = data + 570;
    bitStreamBuffer = imageFileData[imageDataIndex++];
    bitStreamBuffer = bitStreamBuffer + (0x100 * imageFileData[imageDataIndex++]);
    /* initialise the bit stream buffer bit counter */
    bufferBitCounter = 8;

    for (int y = 0; y < max_y; y++)
    {
        for (int x = 0; x < max_x; x++)
        {
            newPixelIndexSelector = bitStreamBuffer & 0x00FF;
            if (newPixelIndexSelector != 0xFF)
            {
                /* get index for new pixel and bit strip count */
                newPixelIndex = (data + 314)[newPixelIndexSelector];
                /* get the bit strip count */
                bufferBitStripCount = (data + 298)[newPixelIndex];

                /* strip bufferBitStripCount bits from bitStreamBuffer */
                while (bufferBitStripCount > 0)
                {
                    bitStreamBuffer = bitStreamBuffer >> 1;
                    bufferBitStripCount--;
                    bufferBitCounter--;
                    if (bufferBitCounter == 0)
                    {
                        /* need to refill the bitStreamBuffer high byte */
                        bitStreamBuffer = bitStreamBuffer + (0x100 * imageFileData[imageDataIndex++]);
                        /* re-initialise the bit stream buffer bit counter */
                        bufferBitCounter = 8;
                    }
                }
            }
            else
            {
                /* strip the 8 bits holding 0xFF from bitStreamBuffer */
                bufferBitStripCount = 8;
                while (bufferBitStripCount > 0)
                {
                    bitStreamBuffer = bitStreamBuffer >> 1;
                    bufferBitStripCount--;
                    bufferBitCounter--;
                    if (bufferBitCounter == 0)
                    {
                        /* need to refill the bitStreamBuffer high byte */
                        bitStreamBuffer = bitStreamBuffer + (0x100 * imageFileData[imageDataIndex++]);
                        /* re-initialise the bit stream buffer bit counter */
                        bufferBitCounter = 8;
                    }
                }

                /* get the literal pixel index value from the bit stream */
                newPixelIndex = 0x000F & bitStreamBuffer;
                bufferBitStripCount = 4;

                /* strip 4 bits from bitStreamBuffer */
                while (bufferBitStripCount > 0)
                {
                    bitStreamBuffer = bitStreamBuffer >> 1;
                    bufferBitStripCount--;
                    bufferBitCounter--;
                    if (bufferBitCounter == 0)
                    {
                        /* need to refill the bitStreamBuffer high byte */
                        bitStreamBuffer = bitStreamBuffer + (0x100 * imageFileData[imageDataIndex++]);
                        /* re-initialise the bit stream buffer bit counter */
                        bufferBitCounter = 8;
                    }
                }
            }

            /* shift the previous pixel into the high four bits of newPixel */
            newPixel = 0xF0 & (newPixel << 4);
            /* add the index to the new pixel to newPixel */
            newPixel = newPixel + newPixelIndex;
            /* extract the nex pixel from the table */
            newPixel = (data + 42)[newPixel];
            /* store new pixel in the bitmap */
            bitmap->bitmap[(bitmap->width * y) + x] = newPixel;
        }
    }

    bitmap->num_palette_colours = 16;
    for (int i = 0; i < 16; i++)
    {
        bitmap->palette[i] = bitmap_st_colour(data[4 + (i * 2)], data[5 + (i * 2)]);
    }

    free(data);
    return bitmap;
}

static void bitmap_amiga_name(int num, char *dir, char *out)
{
    if (num == 0)
    {
        FILE *f;

        sprintf(out, "%stitle", dir);
        f = fopen(out, "rb");
        if (f != NULL)
        {
            fclose(f);
            return;
        }
        else
        {
            num = 30;
        }
    }

    sprintf(out, "%s%d", dir, num);
}

static BitmapType bitmap_amiga_type(char *file)
{
    FILE *f = fopen(file, "rb");

    if (f != NULL)
    {
        uint8_t data[72];
        int x;
        int y;

        fread(data, 1, sizeof(data), f);
        fclose(f);

        x = data[67] + data[66] * 256;
        y = data[71] + data[70] * 256;

        if ((x == 0x0140) && (y == 0x0088))
        {
            return AMIGA_BITMAPS;
        }
        if ((x == 0x0140) && (y == 0x0087))
        {
            return AMIGA_BITMAPS;
        }
        if ((x == 0x00E0) && (y == 0x0075))
        {
            return AMIGA_BITMAPS;
        }
        if ((x == 0x00E4) && (y == 0x0075))
        {
            return AMIGA_BITMAPS;
        }
        if ((x == 0x00E0) && (y == 0x0076))
        {
            return AMIGA_BITMAPS;
        }
        if ((x == 0x00DB) && (y == 0x0076))
        {
            return AMIGA_BITMAPS;
        }
    }

    return NO_BITMAPS;
}

static int bitmap_amiga_intensity(int col)
{
    return (int) (pow((double) col / 15, 1.0 / 0.8) * 0xff);
}

/*
 * Amiga Palette Colour
 * --------------------
 *
 * An Amiga palette colour is a 16-bit structure with the red, green and blue
 * values stored in the second, third and lowest nibbles respectively. The
 * high nibble is always zero.
 */

static Colour bitmap_amiga_colour(int i1, int i2)
{
    Colour col;
    col.red = bitmap_amiga_intensity(i1 & 0xf);
    col.green = bitmap_amiga_intensity(i2 >> 4);
    col.blue = bitmap_amiga_intensity(i2 & 0xf);
    return col;
}

/*
 * Amiga Bitmap Format
 * -------------------
 *
 * An Amiga image file has the following format. It consists of a 72 byte
 * header followed by the image data.
 *
 * The header has the following format:
 * Bytes 0-63:  32 entries Amiga palette
 * Bytes 64-65: padding
 * Bytes 66-67: big-endian word holding picture width in pixels*
 * Bytes 68-69: padding
 * Bytes 70-71: big-endian word holding number of pixel rows in the image*
 *
 * [*] These are probably big-endian unsigned longs but I have designated
 * the upper two bytes as padding because (a) Level 9 does not need them
 * as longs and (b) using unsigned shorts reduces byte order juggling.
 *
 * The images are designed for an Amiga low-res mode screen - that is they
 * assume a 320 * 256 (or 320 * 200 if NTSC display) screen with a palette of
 * 32 colours from the possible 4096.
 *
 * The image data is organised the same way that Amiga video memory is. The
 * entire data block is divided into five equal length bit planes with the
 * first bit plane holding the low bit of each 5-bit pixel, the second bitplane
 * the second bit of the pixel and so on up to the fifth bit plane holding the
 * high bit of the 5-bit pixel.
 */

static Bitmap *bitmap_amiga_decode(char *file)
{
    uint8_t *data = NULL;
    uint32_t size;

    Bitmap *bitmap;
    int max_x;
    int max_y;

    data = bitmap_load(file, &size);
    if (data == NULL)
    {
        return NULL;
    }

    max_x = (((((data[64] << 8) | data[65]) << 8) | data[66]) << 8) | data[67];
    max_y = (((((data[68] << 8) | data[69]) << 8) | data[70]) << 8) | data[71];
    if (max_x > MAX_BITMAP_WIDTH || max_y > MAX_BITMAP_HEIGHT)
    {
        free(data);
        return NULL;
    }

    bitmap = bitmap_alloc(max_x, max_y);
    if (bitmap == NULL)
    {
        free(data);
        return NULL;
    }

    if (max_x > bitmap->width)
    {
        max_x = bitmap->width;
    }
    if (max_y > bitmap->height)
    {
        max_y = bitmap->height;
    }

    for (int y = 0; y < max_y; y++)
    {
        for (int x = 0; x < max_x; x++)
        {
            int p = 0;
            for (int b = 0; b < 5; b++)
            {
                p |= ((data[72 + (max_x / 8) * (max_y * b + y) + x / 8] >> (7 - (x % 8))) & 1) << b;
            }
            bitmap->bitmap[(bitmap->width * y) + x] = p;
        }
    }

    bitmap->num_palette_colours = 32;
    for (int i = 0; i < 32; i++)
    {
        bitmap->palette[i] = bitmap_amiga_colour(data[i * 2], data[i * 2 + 1]);
    }

    free(data);
    return bitmap;
}

static void nxi_name(int num, char *out)
{
    sprintf(out, "%d.nxi", num);
}

static uint8_t c8_to_c3(uint8_t c8, RoundingMode rounding_mode)
{
    double c3 = (c8 * 7.0) / 255.0;

    switch (rounding_mode)
    {
        case FLOOR:
            return (uint8_t) floor(c3);
        case CEIL:
            return (uint8_t) ceil(c3);
        case ROUND:
            // Fall through
        default:
            return (uint8_t) round(c3);
    }
}

static void create_nxi_palette(char *game, Bitmap *bitmap, BitmapType type, int num)
{
    RoundingMode rounding_mode = ROUND;

    /*
     * Hack for Amiga version of Knight Orc to make pictures darker
     * and for making the darkest colour in the title picture pure black.
     */
    if ((stricmp(game, GAME_KNIGHT_ORC) == 0) && (type == AMIGA_BITMAPS))
    {
        rounding_mode = FLOOR;

        if (num == 0)
        {
            Colour *colour = &(bitmap->palette[0]);
            colour->red = 0;
            colour->green = 0;
            colour->blue = 0;
        }
    }

    memset(nxi_palette, 0, sizeof(nxi_palette));

    // Create the NXI palette.
    // The RGB888 colors in the bitmap palette are converted to
    // RGB333 colors, which are then split in RGB332 and B1 parts.
    for (int i = 0; i < bitmap->num_palette_colours; i++)
    {
        Colour *colour = &(bitmap->palette[i]);

        uint8_t r3 = c8_to_c3(colour->red, rounding_mode);
        uint8_t g3 = c8_to_c3(colour->green, rounding_mode);
        uint8_t b3 = c8_to_c3(colour->blue, rounding_mode);

        uint16_t rgb333 = (r3 << 6) | (g3 << 3) | (b3 << 0);
        uint8_t rgb332 = (uint8_t) (rgb333 >> 1);
        uint8_t b1 = (uint8_t) (rgb333 & 0x01);

        nxi_palette[i * 2 + 0] = rgb332;
        nxi_palette[i * 2 + 1] = b1;
    }
}

static void get_bitmap_position(char *game, Bitmap *bitmap, int num, uint16_t *x, uint16_t *y)
{
    if (num == 0)
    {
        // title bitmap (centered)
        *x = (NXI_IMAGE_WIDTH - bitmap->width) / 2;
        *y = (NXI_IMAGE_HEIGHT - bitmap->height) / 2;

        if (stricmp(game, GAME_GNOME_RANGER) == 0)
        {
            *y += 2;
        }
        else if (stricmp(game, GAME_TIME_AND_MAGIK) == 0)
        {
            *y += 6;
        }
        else if (stricmp(game, GAME_LANCELOT) == 0)
        {
            *y += 8;
        }
        else if (stricmp(game, GAME_SCAPEGHOST) == 0)
        {
            *y += 10;
        }
    }
    else if (num == 1)
    {
        // frame bitmap
        *x = 0;
        *y = 0;
    }
    else if (stricmp(game, GAME_KNIGHT_ORC) == 0)
    {
        *x = 48;
        *y = 10;
    }
    else if (stricmp(game, GAME_GNOME_RANGER) == 0)
    {
        *x = 48;
        *y = 8;
    }
    else if (stricmp(game, GAME_TIME_AND_MAGIK) == 0)
    {
        *x = 48;
        *y = 9;
    }
    else if (stricmp(game, GAME_LANCELOT) == 0)
    {
        *x = 48;
        *y = 10;

        if (num == 8)
        {
            *y = 9;
        }
        else if ((num == 5) || (num == 13))
        {
            *y = 0;
        }
    }
    else if (stricmp(game, GAME_INGRIDS_BACK) == 0)
    {
        *x = 49;
        *y = 10;
    }
    else if (stricmp(game, GAME_SCAPEGHOST) == 0)
    {
        *x = 49;
        *y = 10;
    }
    else
    {
        *x = 0;
        *y = 0;
    }
}

static void create_nxi_image(char *game, Bitmap *bitmap, BitmapType type, int num)
{
    uint16_t x_start = 0;
    uint16_t y_start = 0;
    uint16_t width = bitmap->width;
    uint16_t height = bitmap->height;
    uint8_t *image_ptr = bitmap->bitmap;

    memset(nxi_image, 0, sizeof(nxi_image));
    if (num > 1)
    {
        memcpy(nxi_image, nxi_frame_image, sizeof(nxi_frame_image));
    }

    get_bitmap_position(game, bitmap, num, &x_start, &y_start);

    /*
     * We want the frame picture and all following pictures to have a 2 pixel
     * bottom margin and a top margin that makes their total height PICTURE_HEIGHT.
     * The bottom margin is for having some space between the text and the
     * picture and the top margin is to compensate for that not all monitors
     * can display the full height of the layer 2 320x256 graphics mode.
     */
    if (num == 1)
    {
        picture_top_margin = PICTURE_HEIGHT - bitmap->height - 2;
    }

    /*
     * Hack for Amiga version of Knight Orc to fix that the pictures are four
     * pixels too wide and the extra columns contain the first four columns
     * repeated. The picture height is also reduced by one pixel to make them
     * fit better in the frame.
     */
    if ((stricmp(game, GAME_KNIGHT_ORC) == 0) && (type == AMIGA_BITMAPS) && (num > 1))
    {
        width = width - 4;
        height = height - 1;
    }

    /*
     * Hack for Amiga version of Gnome Ranger to reduce picture number five's
     * width by one pixel.
     */
    if ((stricmp(game, GAME_GNOME_RANGER) == 0) && (type == AMIGA_BITMAPS) && (num == 5))
    {
        width = width - 1;
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            nxi_image[(picture_top_margin + y_start + y) + (x_start + x) * NXI_IMAGE_HEIGHT] = image_ptr[x];
        }
        image_ptr += bitmap->width;
    }

    if (num == 1)
    {
        memcpy(nxi_frame_image, nxi_image, sizeof(nxi_image));
    }
}

void convert_nxi(char *game, Bitmap *bitmap, BitmapType type, int num)
{
    char nxi_filename[MAX_PATH];

    create_nxi_palette(game, bitmap, type, num);
    create_nxi_image(game, bitmap, type, num);

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

BitmapType detect_bitmaps(char *dir)
{
    char file[MAX_PATH];

    bitmap_amiga_name(2, dir, file);
    if (bitmap_exists(file))
    {
        return bitmap_amiga_type(file);
    }

    bitmap_st_name(2, dir, file);
    if (bitmap_exists(file))
    {
        return ST_BITMAPS;
    }

    return NO_BITMAPS;
}

bool exist_bitmap(char *dir, int num)
{
    char file[MAX_PATH];

    bitmap_amiga_name(num, dir, file);
    if (bitmap_exists(file))
    {
        return true;
    }

    bitmap_st_name(num, dir, file);
    if (bitmap_exists(file))
    {
        return true;
    }

    return false;
}

Bitmap *decode_bitmap(char *dir, BitmapType type, int num)
{
    char file[MAX_PATH];

    switch (type)
    {
        case AMIGA_BITMAPS:
            bitmap_amiga_name(num, dir, file);
            return bitmap_amiga_decode(file);
        case ST_BITMAPS:
            bitmap_st_name(num, dir, file);
            return bitmap_st_decode(file);
        case NO_BITMAPS:
            // Fall through
        default:
            return NULL;
    }
}

int main(int argc, char *argv[])
{
    char dir[MAX_PATH];

    if (argc <= 2)
    {
        print_usage();
        return 1;
    }

    char *game = argv[1];
    char *path = argv[2];

    if (!validate_game(game))
    {
        fprintf(stderr, "Unsupported game: %s\n", game);
        return 1;
    }

    extend_dir_path(path, dir);

    BitmapType bitmap_type = detect_bitmaps(dir);
    if (bitmap_type == NO_BITMAPS)
    {
        fprintf(stderr, "Cannot find any bitmap files in directory %s\n", dir);
        return 1;
    }

    printf("Converting game %s located in directory %s\n", game, dir);

    // Picture numbers start at 0 and never exceeds 100.
    for (int i = 0; i < 100; i++)
    {
        if (exist_bitmap(dir, i) && !is_st_title_bitmap(bitmap_type, i))
        {
            Bitmap *bitmap = decode_bitmap(dir, bitmap_type, i);
            if (bitmap == NULL)
            {
                fprintf(stderr, "Error decoding bitmap file %d.\n", i);
                return 1;
            }

            convert_nxi(game, bitmap, bitmap_type, i);
            printf("Converted image %2.1d (width: %d, height: %d)\n", i, bitmap->width, bitmap->height);
            free(bitmap);
        }
    }

    return 0;
}
