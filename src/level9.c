/*******************************************************************************
 *
 * Level 9 interpreter
 * Version 5.1
 * Copyright (c) 1996-2011 Glen Summers and contributors.
 * Contributions from David Kinder, Alan Staniforth, Simon Baldwin, Dieter Baron
 * and Andreas Scherrer.
 *
 * Ported to ZX Spectrum Next by Stefan Bylund <stefan.bylund99@gmail.com>.
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
 * The input routine will respond to the following 'hash' commands:
 *  #save         saves position file directly (bypasses any disk change prompts)
 *  #restore      restores position file directly (bypasses any protection code)
 *  #quit         terminates current game
 *  #picture <n>  show picture <n>
 *  #seed <n>     set the random number seed to the value <n>
 *  #play         plays back a script file as the input to the game
 *
 ******************************************************************************/

/*******************************************************************************
 *
 * Porting of the Level 9 interpreter to ZX Spectrum Next.
 *
 * Ideally, this file should be left unmodified when porting to a new target.
 * However, the original code in this file assumes a flat memory model and the
 * loaded game story file being kept in main memory. The Spectrum Next has only
 * 64 KB addressable memory and a banked memory model with 768 KB physical RAM
 * available as 8 KB pages that can be paged-in to the 64 KB address space.
 * The approach of this porting is to always have the interpreter in main memory
 * and loading the game story file into a contiguous set of 8 KB RAM pages that
 * are paged-in as needed to the bottom 16 KB of the address space.
 *
 * The interpreter has been stripped of the following features in order to
 * minimise its size:
 * - Removed L9_V1 support.
 * - Removed MSGT_V1 support for L9_V2 (only MSGT_V2 is supported).
 * - Removed scanning of game file for determining game version and offset.
 *   Instead a gamedata.txt file is used for storing the game version and the
 *   offset is then determined from that.
 * - Removed listing of the game's dictionary.
 * - Removed bypassing of copy protection in V3/V4 games by trying every word in
 *   the game's dictionary.
 * - Removed support for Level 9 line-drawn and bitmap images.
 *   Instead all image types are converted to Spectrum Next bitmap images by the
 *   convert_gfx and convert_bitmap tools.
 *
 * The game story file is loaded into a contiguous set of 8 KB RAM pages that
 * are paged-in as needed to the bottom 16 KB of the address space where the
 * Spectrum ROM is normally located. An effective() function has been added for
 * converting a virtual address in the game file to an effective address in the
 * Z80 address space by paging in the referenced page and the next page of the
 * game file into the bottom 16 KB. The code has been updated to call this
 * function whenever referencing an address in the game file.
 *
 * Files are accessed using ESXDOS whose API is different but similar to the
 * standard POSIX file I/O API. When using the ESXDOS API, the Spectrum ROM must
 * be temporarily paged in to the bottom 16 KB RAM. The memory area at 16K - 24K
 * is temporarily used for paging in the data that should be read/written
 * from/to a file.
 *
 ******************************************************************************/

#include <arch/zxn.h>
#include <arch/zxn/esxdos.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stropts.h>
#include <errno.h>

#include "level9.h"
#include "memory_paging.h"
#include "ide_friendly.h"

#define GAME_INFO_FILE "gamedata.txt"

#define L9_ID 0x4C393031

#define IN_BUFFER_SIZE 500

#define OUT_BUFFER_SIZE 34

#define RAM_SAVE_SLOTS 10

// Backup of workspace used when loading game state.
#define WORKSPACE_BACKUP_PAGE 2

#define MMU2_ADDRESS ((uint8_t *) 0x4000)

#define ESX_INVALID_FILE_HANDLE 0xFF

typedef struct save_struct
{
    uint16_t var_table[VAR_TABLE_SIZE];
    uint8_t list_area[LIST_AREA_SIZE];
} save_struct_t;

uint8_t tmp_buffer[256];

// Size of the actual paged memory used by the game (<= 64 KB).
static uint16_t memory_size;

// Note: Once initialized, the following variables stay constant.
static uint16_t l9_pointers[12];
static bool l9_pointers_in_ws[12];
static uint8_t *list2_ptr;       // workspace
static uint8_t *list3_ptr;       // workspace
static uint8_t *list9_start_ptr; // workspace
static uint16_t abs_data_block;  // memory
static uint16_t acode_ptr;       // memory
static uint16_t start_md;        // memory
static uint16_t start_md_v2;     // memory
static uint16_t end_md;          // memory
static uint16_t end_wdp5;        // memory
static uint16_t word_table;      // memory
static uint16_t def_dict;        // memory
static uint16_t dict_data;       // memory
static uint16_t dict_data_len;

static uint8_t *list9_ptr;       // workspace
static uint16_t dict_ptr;        // memory

/* Instruction codes */
static uint16_t code_ptr;        // memory
static uint8_t code;

static game_type_t game_type;
static uint8_t game_file[MAX_PATH];
static bool running = false;

static uint8_t in_buffer[IN_BUFFER_SIZE];
static uint8_t *in_buffer_ptr;
static uint8_t out_buffer[OUT_BUFFER_SIZE];
static uint8_t script_file = ESX_INVALID_FILE_HANDLE;

static uint16_t random_seed;
static uint16_t const_seed = 0;

static bool word_case;
static uint8_t unpack_count;
static uint8_t unpack_buf[8];
static uint8_t unpack_d3;
static uint8_t three_chars[34];

static uint8_t last_char = '.';
static uint8_t last_actual_char = 0;
static uint8_t d5 = 0;

static game_state_t workspace; // 2848 bytes

static uint8_t exit_reversal_table[16] =
{
    0x00, 0x04, 0x06, 0x07, 0x01, 0x08, 0x02, 0x03,
    0x05, 0x0a, 0x09, 0x0c, 0x0b, 0xff, 0xff, 0x0f
};

static uint16_t gno_stack[128];
static uint8_t gno_scratch[32];
static uint16_t object;
static uint8_t gno_sp;
static uint16_t num_object_found;
static uint16_t search_depth;
static uint16_t init_hi_search_pos;

#ifdef CODEFOLLOW
#define CODEFOLLOW_FILE "codefollow.txt"

static uint8_t cf_file = ESX_INVALID_FILE_HANDLE;
static uint8_t cf_buf[128];

static uint16_t *cf_var;
static uint16_t *cf_var2;

static uint8_t *codes[] =
{
    "goto",
    "gosub",
    "return",
    "print_number",
    "messagev",
    "messagec",
    "function",
    "input",
    "var_con",
    "var_var",
    "add",
    "sub",
    "ilins",
    "ilins",
    "jump",
    "exit",
    "if_eq_vt",
    "if_ne_vt",
    "if_lt_vt",
    "if_gt_vt",
    "screen",
    "clear_tg",
    "picture",
    "get_next_object",
    "if_eq_ct",
    "if_ne_ct",
    "if_lt_ct",
    "if_gt_ct",
    "print_input",
    "ilins",
    "ilins",
    "ilins"
};

static uint8_t *functions[] =
{
    "call_driver",
    "random",
    "save",
    "restore",
    "clear_workspace",
    "clear_stack"
};

static uint8_t *driver_calls[] =
{
    "init",
    "driver_calc_checksum",
    "driver_os_wr_ch",
    "driver_os_rd_ch",
    "driver_input_line",
    "driver_save_file",
    "driver_load_file",
    "set_text",
    "reset_task",
    "return_to_gem",
    "10 *",
    "load_game_data_file",
    "random_number",
    "13 *",
    "driver_14",
    "15 *",
    "driver_clg",
    "line",
    "fill",
    "driver_chg_col",
    "20 *",
    "21 *",
    "ram_save",
    "ram_load",
    "24 *",
    "lens_display",
    "26 *",
    "27 *",
    "28 *",
    "29 *",
    "alloc_space",
    "31 *",
    "show_bitmap",
    "33 *",
    "check_for_disc"
};
#endif

/* Prototypes */
static uint8_t get_long_code(void);

#ifdef CODEFOLLOW
static void cf_print(char *format, ...)
{
    int size;
    va_list args;

    va_start(args, format);
    size = vsnprintf(cf_buf, sizeof(cf_buf), format, args);
    va_end(args);

    page_in_rom();
    esx_f_write(cf_file, cf_buf, size);
    page_in_game();
}
#endif

static void error(uint8_t *format, ...)
{
    va_list args;

    va_start(args, format);
    vsprintf(tmp_buffer, format, args);
    va_end(args);

    for (uint16_t i = 0; i < strlen(tmp_buffer); i++)
    {
        os_print_char(tmp_buffer[i]);
    }
}

static uint16_t seed(void) __preserves_regs(b,c,d,e) __naked
{
#ifdef IDE_FRIENDLY
    // Dummy statement to keep the IDE happy.
    return 1;
#else
    __asm

    ld a,r
    ld l,a
    cpl
    ld h,a

    ret

    __endasm;
#endif
}

static bool load_pages(uint8_t fh) __z88dk_fastcall
{
    uint8_t total_num_pages;
    uint8_t page;
    uint16_t rest;

    /*
     * The given file is loaded via MMU slot 2 (the ULA screen, page 10) into a
     * contiguous set of one or more 8 KB RAM pages. It is assumed that the ROM
     * is paged in.
     */

    errno = 0;
    total_num_pages = (uint8_t) (memory_size / 0x2000);

    for (page = MEMORY_BASE_PAGE; total_num_pages--; page++)
    {
        ZXN_WRITE_MMU2(page);
        esx_f_read(fh, MMU2_ADDRESS, 0x2000);
        if (errno)
        {
            goto end;
        }
    }

    // Load the last chunk (or the first and only chunk) if its size is less than 8 KB.

    rest = memory_size % 0x2000;

    if (rest > 0)
    {
        ZXN_WRITE_MMU2(page);
        esx_f_read(fh, MMU2_ADDRESS, rest);
    }

end:
    // Restore default page 10 in MMU slot 2.
    ZXN_WRITE_MMU2(10);
    return (errno == 0);
}

static uint32_t file_length(uint8_t fh) __z88dk_fastcall
{
    struct esx_stat filestat;
    esx_f_fstat(fh, &filestat);
    return filestat.size;
}

static bool load(uint8_t *filename) __z88dk_fastcall
{
    uint8_t fh;
    uint32_t file_size;
    bool status = false;

    // It is assumed that the ROM is paged in.

    errno = 0;
    fh = esx_f_open(filename, ESX_MODE_OPEN_EXIST | ESX_MODE_R);
    if (errno)
    {
        error("\rError opening game file\r");
        return false;
    }

    file_size = file_length(fh);
    if (file_size < 256 || file_size > 0xFFFF)
    {
        error("\rFile is not a valid Level 9 game\r");
        goto end;
    }

    memory_size = (uint16_t) file_size;
    current_page = 255;

    if (!load_pages(fh))
    {
        error("\rError loading game file\r");
        goto end;
    }

    status = true;
end:
    esx_f_close(fh);
    return status;
}

static game_type_t read_game_type(void)
{
    uint8_t fh;
    uint8_t game_version;

    // Read game version from game info file. It is assumed that the ROM is paged in.
    errno = 0;
    fh = esx_f_open(GAME_INFO_FILE, ESX_MODE_OPEN_EXIST | ESX_MODE_R);
    if (errno)
    {
        os_fatal_error("Error opening game info file gamedata.txt.");
    }

    if (esx_f_read(fh, &game_version, 1) != 1)
    {
        os_fatal_error("Error reading game info file gamedata.txt.");
    }

    esx_f_close(fh);

    // Convert from ASCII to integer value.
    game_version -= 48;

    switch (game_version)
    {
        case L9_V1:
            os_fatal_error("Unsupported game type L9_V1 in game info file gamedata.txt.");
            break; // Avoid compiler warning
        case L9_V2:
        case L9_V3:
        case L9_V4:
            break;
        default:
            os_fatal_error("Invalid game type %u in game info file gamedata.txt.", game_version);
    }

    return (game_type_t) game_version;
}

static bool init_game(uint8_t *filename) __z88dk_fastcall
{
    uint8_t hd_offset;
    uint8_t *header_ptr;

    page_in_rom();

    game_type = read_game_type();

    if (!load(filename))
    {
        error("\rUnable to load: %s\r", filename);
        return false;
    }

    hd_offset = (game_type == L9_V2) ? 0x04 : 0x12;

    header_ptr = effective(0);

    for (uint8_t i = 0; i < 12; i++)
    {
        uint16_t d0 = L9WORD(header_ptr + hd_offset + i * 2);
        bool in_ws = (i != 11 && d0 >= 0x8000 && d0 <= 0x9000);
        l9_pointers[i] = in_ws ? d0 - 0x8000 : d0;
        l9_pointers_in_ws[i] = in_ws;
    }

    abs_data_block = l9_pointers[0];                         // memory
    dict_data = l9_pointers[1];                              // memory
    list2_ptr = workspace.list_area + l9_pointers[3];        // workspace
    list3_ptr = workspace.list_area + l9_pointers[4];        // workspace
    list9_start_ptr = workspace.list_area + l9_pointers[10]; // workspace
    acode_ptr = l9_pointers[11];                             // memory

    switch (game_type)
    {
        case L9_V1:
            return false;
        case L9_V2:
            start_md = L9WORD(header_ptr + 0x0);
            start_md_v2 = L9WORD(header_ptr + 0x2);
            break;
        case L9_V3:
        case L9_V4:
            start_md = L9WORD(header_ptr + 0x2);
            end_md = start_md + L9WORD(header_ptr + 0x4);
            def_dict = L9WORD(header_ptr + 0x6);
            end_wdp5 = def_dict + 5 + L9WORD(header_ptr + 0x8);
            dict_data = L9WORD(header_ptr + 0x0a);
            dict_data_len = L9WORD(header_ptr + 0x0c);
            word_table = L9WORD(header_ptr + 0xe);
            break;
    }

    return true;
}

static bool int_load_game(uint8_t *filename) __z88dk_fastcall
{
#ifdef CODEFOLLOW
    // Append to the same code follow file for multi-part games.
    if (cf_file == ESX_INVALID_FILE_HANDLE)
    {
        errno = 0;
        cf_file = esx_f_open(CODEFOLLOW_FILE, ESX_MODE_OPEN_CREAT_TRUNC | ESX_MODE_W);
        if (errno)
        {
            printf("Warning: Error opening code follow file.\n");
        }
        else
        {
            cf_print("Code follow file for %s:\n", filename);
        }
    }
    else
    {
        cf_print("\nCode follow file for %s:", filename);
    }
#endif

    running = false;
    in_buffer_ptr = NULL;

    if (!init_game(filename))
    {
        return false;
    }

    code_ptr = acode_ptr;
    random_seed = const_seed ? const_seed : seed();
    strcpy(game_file, filename);

    return running = true;
}

static void init_dict(uint16_t ptr) __z88dk_fastcall
{
    dict_ptr = ptr;
    unpack_count = 8;
}

static uint8_t get_dictionary_code(void)
{
    if (unpack_count != 8)
    {
        return unpack_buf[unpack_count++];
    }
    else
    {
        /* unpackbytes */
        // Optimization: Minimize number of effective(dict_ptr++) calls.
        uint8_t *effective_ptr = effective(dict_ptr);
        uint8_t d1;
        uint8_t d2;

        d1 = *effective_ptr++;
        unpack_buf[0] = d1 >> 3;
        d2 = *effective_ptr++;
        unpack_buf[1] = ((d2 >> 6) + (d1 << 2)) & 0x1f;
        d1 = *effective_ptr++;
        unpack_buf[2] = (d2 >> 1) & 0x1f;
        unpack_buf[3] = ((d1 >> 4) + (d2 << 4)) & 0x1f;
        d2 = *effective_ptr++;
        unpack_buf[4] = ((d1 << 1) + (d2 >> 7)) & 0x1f;
        d1 = *effective_ptr++;
        unpack_buf[5] = (d2 >> 2) & 0x1f;
        unpack_buf[6] = ((d2 << 3) + (d1 >> 5)) & 0x1f;
        unpack_buf[7] = d1 & 0x1f;
        dict_ptr += 5;
        unpack_count = 1;
        return unpack_buf[0];
    }
}

static uint8_t get_dictionary(uint8_t d0) __z88dk_fastcall
{
    if (d0 >= 0x1a)
    {
        return get_long_code();
    }
    else
    {
        return d0 + 0x61;
    }
}

static uint8_t get_long_code(void)
{
    uint8_t d0;
    uint8_t d1;

    d0 = get_dictionary_code();
    if (d0 == 0x10)
    {
        word_case = true;
        d0 = get_dictionary_code();
        return get_dictionary(d0);
    }

    d1 = get_dictionary_code();

    return 0x80 | ((d0 << 5) & 0xe0) | (d1 & 0x1f);
}

static void print_char(uint8_t c) __z88dk_fastcall
{
    if (c & 128)
    {
        c &= 0x7f;
        last_char = c;
    }
    else if (c != 0x20 && c != 0x0d && (c < '\"' || c >= '.'))
    {
        if (last_char == '!' || last_char == '?' || last_char == '.')
        {
            c = toupper(c);
        }
        last_char = c;
    }

    /* eat multiple CRs */
    if (c != 0x0d || last_actual_char != 0x0d)
    {
        os_print_char(c);
    }

    last_actual_char = c;
}

// FIXME: Should be __z88dk_fastcall but it doesn't work here - compiler bug?
static void print_string(uint8_t *str)
{
    for (uint16_t i = 0; i < strlen(str); i++)
    {
        print_char(str[i]);
    }
}

static void print_decimal(uint16_t d0) __z88dk_fastcall
{
    uint8_t temp[12];
    sprintf(temp, "%u", d0);
    print_string(temp);
}

static void print_auto_case(uint8_t d0) __z88dk_fastcall
{
    if (d0 & 128)
    {
        print_char(d0);
    }
    else
    {
        if (word_case)
        {
            print_char(toupper(d0));
        }
        else if (d5 < 6)
        {
            print_char(d0);
        }
        else
        {
            word_case = false;
            print_char(toupper(d0));
        }
    }
}

static void display_word_ref(uint16_t off) __z88dk_fastcall
{
    static uint8_t mdt_mode = 0;

    word_case = false;
    d5 = (off >> 12) & 7;
    off &= 0xfff;

    if (off < 0xf80)
    {
    /* dwr01 */
        uint16_t a0;
        uint16_t a0_org;
        uint8_t *a3;
        uint8_t d0;
        uint16_t d2;

        if (mdt_mode == 1)
        {
            print_char(0x20);
        }
        mdt_mode = 1;

        /* setindex */
        a0 = dict_data;
        d2 = dict_data_len;

    /* dwr02 */
        a0_org = a0;
        while (d2 && off >= L9WORD(effective(a0 + 2)))
        {
            a0 += 4;
            d2--;
        }

    /* dwr04 */
        if (a0 == a0_org)
        {
            a0 = def_dict;
        }
        else
        {
            a0 -= 4;
            off -= L9WORD(effective(a0 + 2));
            a0 = L9WORD(effective(a0));
        }

    /* dwr04b */
        off++;
        init_dict(a0);
        a3 = three_chars; /* a3 not set in original, prevent possible spam */

        /* dwr05 */
        while (true)
        {
            d0 = get_dictionary_code();
            if (d0 < 0x1c)
            {
                /* dwr06 */
                if (d0 >= 0x1a)
                {
                    d0 = get_long_code();
                }
                else
                {
                    d0 += 0x61;
                }
                *a3++ = d0;
            }
            else
            {
                d0 &= 3;
                a3 = (uint8_t *) three_chars + d0;
                if (--off == 0)
                {
                    break;
                }
            }
        }

        for (uint8_t i = 0; i < d0; i++)
        {
            print_auto_case(three_chars[i]);
        }

        /* dwr10 */
        while (true)
        {
            d0 = get_dictionary_code();
            if (d0 >= 0x1b)
            {
                return;
            }
            print_auto_case(get_dictionary(d0));
        }
    }
    else
    {
        if (d5 & 2)
        {
            print_char(0x20); /* prespace */
        }
        mdt_mode = 2;
        off &= 0x7f;
        if (off != 0x7e)
        {
            print_char((uint8_t) off);
        }
        if (d5 & 1)
        {
            print_char(0x20); /* postspace */
        }
    }
}

static uint16_t get_md_length(uint16_t *ptr) __z88dk_fastcall
{
    uint16_t tot = 0;
    uint16_t len;

    do
    {
        len = (*effective((*ptr)++) - 1) & 0x3f;
        tot += len;
    } while (len == 0x3f);

    return tot;
}

static void print_message(uint16_t msg) __z88dk_fastcall
{
    uint16_t msg_ptr = start_md;
    uint8_t data;
    uint16_t len;
    uint16_t off;

    while (msg > 0 && msg < 0x8000 && msg_ptr <= end_md)
    {
        data = *effective(msg_ptr);
        if (data & 128)
        {
            msg_ptr++;
            msg -= data & 0x7f;
        }
        else
        {
            len = get_md_length(&msg_ptr);
            msg_ptr += len;
        }
        msg--;
    }

    if ((msg & 0x8000) || (*effective(msg_ptr) & 128)) // msg < 0
    {
        return;
    }

    len = get_md_length(&msg_ptr);

    while (len)
    {
        data = *effective(msg_ptr++);
        len--;
        if (data & 128)
        {
            /* long form (reverse word) */
            off = (((uint16_t) data) << 8) + *effective(msg_ptr++);
            len--;
        }
        else
        {
            uint8_t *word_table_ptr = effective(word_table);
            uint8_t word_table_index = data * 2;
            off = (((uint16_t) word_table_ptr[word_table_index]) << 8) + word_table_ptr[word_table_index + 1];
        }
        if (off == 0x8f80)
        {
            break;
        }
        display_word_ref(off);
    }
}

static uint16_t msg_len_v2(uint16_t *ptr) __z88dk_fastcall
{
    uint16_t i = 0;
    uint8_t a;

    /* catch berzerking code */
    if (*ptr >= memory_size)
    {
        return 0;
    }

    while ((a = *effective(*ptr)) == 0)
    {
        (*ptr)++;

        if (*ptr >= memory_size)
        {
            return 0;
        }

        i += 255;
    }

    i += a;
    return i;
}

static void print_char_v2(uint8_t c) __z88dk_fastcall
{
    if (c == 0x25)
    {
        c = 0xd;
    }
    else if (c == 0x5f)
    {
        c = 0x20;
    }

    print_auto_case(c);
}

static void display_word_v2(uint16_t ptr, uint16_t msg)
{
    uint16_t n;
    uint8_t a;

    if (msg == 0)
    {
        return;
    }

    while (--msg)
    {
        ptr += msg_len_v2(&ptr);
    }

    n = msg_len_v2(&ptr);
    if (n == 0)
    {
        return;
    }

    while (--n)
    {
        a = *effective(++ptr);
        if (a < 3)
        {
            return;
        }
        else if (a >= 0x5e)
        {
            display_word_v2(start_md_v2 - 1, a - 0x5d);
        }
        else
        {
            print_char_v2(a + 0x1d);
        }
    }
}

static void print_message_v2(uint16_t msg) __z88dk_fastcall
{
    display_word_v2(start_md, msg);
}

static uint16_t get_addr(void)
{
    if (code & 0x20)
    {
        /* get_addr_short */
        int8_t diff = *effective(code_ptr++);
        return code_ptr + diff - 1;
    }
    else
    {
        // Optimization: Manually inlined move_wa5d0() function.
        uint16_t ret = L9WORD(effective(code_ptr));
        code_ptr += 2;
        return acode_ptr + ret;
    }
}

static uint16_t get_con(void)
{
    if (code & 64)
    {
        /* get_con_small */
        return *effective(code_ptr++);
    }
    else
    {
        // Optimization: Manually inlined move_wa5d0() function.
        uint16_t ret = L9WORD(effective(code_ptr));
        code_ptr += 2;
        return ret;
    }
}

static uint16_t *get_var(void)
{
#ifndef CODEFOLLOW
    return workspace.var_table + *effective(code_ptr++);
#else
    cf_var2 = cf_var;
    cf_var = workspace.var_table + *effective(code_ptr++);
    return cf_var;
#endif
}

/*
 * Optimization: Added a variant of get_var() that returns the value of the
 * variable instead of the variable itself. The compiler will then generate
 * more effective assembly code for C code that use get_var_val() instead of
 * *get_var() when reading the value of a variable.
 */
static uint16_t get_var_val(void)
{
#ifndef CODEFOLLOW
    return workspace.var_table[*effective(code_ptr++)];
#else
    cf_var2 = cf_var;
    cf_var = workspace.var_table + *effective(code_ptr++);
    return *cf_var;
#endif
}

static void int_goto(void)
{
    uint16_t target = get_addr();

#ifdef L9DEBUG
    if (target == code_ptr - 2)
    {
        error("\rBad goto address: endless loop\r");
        running = false;
        return;
    }
#endif

    code_ptr = target;

#ifdef CODEFOLLOW
    cf_print(" %u", target - acode_ptr);
#endif
}

static void int_gosub(void)
{
    uint16_t new_code_ptr;

#ifdef L9DEBUG
    if (workspace.stack_ptr == STACK_SIZE)
    {
        error("\rStack overflow error\r");
        running = false;
        return;
    }
#endif

    new_code_ptr = get_addr();
    workspace.stack[workspace.stack_ptr++] = code_ptr;
    code_ptr = new_code_ptr;

#ifdef CODEFOLLOW
    cf_print(" %u", new_code_ptr - acode_ptr);
#endif
}

static void int_return(void)
{
#ifdef L9DEBUG
    if (workspace.stack_ptr == 0)
    {
        error("\rStack underflow error\r");
        running = false;
        return;
    }
#endif

    code_ptr = workspace.stack[--workspace.stack_ptr];
}

static void print_number(void)
{
#ifndef CODEFOLLOW
    print_decimal(get_var_val());
#else
    uint16_t number = get_var_val();
    print_decimal(number);
    cf_print(" %u", number);
#endif
}

static void messagev(void)
{
    if (game_type <= L9_V2)
    {
        print_message_v2(get_var_val());
    }
    else
    {
        print_message(get_var_val());
    }
}

static void messagec(void)
{
    if (game_type <= L9_V2)
    {
        print_message_v2(get_con());
    }
    else
    {
        print_message(get_con());
    }
}

static void random_number(uint8_t *a6) __z88dk_fastcall
{
    L9SETWORD(a6, rand());
}

static void driver_os_rd_ch(uint8_t *a6) __z88dk_fastcall
{
    /* max delay of 1/50 sec */
    *a6 = os_read_char(20);
}

static void lens_display(uint8_t *a6) __z88dk_fastcall
{
    print_string("\rLenslok code is ");
    print_char(*a6);
    print_char(*(a6 + 1));
    print_char('\r');
}

static void driver_14(uint8_t *a6) __z88dk_fastcall
{
    *a6 = 0;
}

static void show_bitmap(uint8_t *a6) __z88dk_fastcall
{
    os_show_bitmap(a6[1]);

#ifdef CODEFOLLOW
    cf_print(" %u", a6[1]);
#endif
}

static void check_for_disc(uint8_t *a6) __z88dk_fastcall
{
    *a6 = 0;
    list9_start_ptr[2] = 0;
}

static void driver(uint8_t d0_in, uint8_t *a6)
{
    uint8_t d0 = d0_in; // transfer param to Z80 register

    switch (d0)
    {
        //case 0:    init(a6); break;
        case 0x0c: random_number(a6); break;
        //case 0x10: driver_clg(a6); break;
        //case 0x11: line(a6); break;
        //case 0x12: fill(a6); break;
        //case 0x13: driver_chg_col(a6); break;
        //case 0x01: driver_calc_checksum(a6); break;
        //case 0x02: driver_os_wr_ch(a6); break;
        case 0x03: driver_os_rd_ch(a6); break;
        //case 0x05: driver_save_file(a6); break;
        //case 0x06: driver_load_file(a6); break;
        //case 0x07: set_text(a6); break;
        //case 0x08: reset_task(a6); break;
        //case 0x04: driver_input_line(a6); break;
        //case 0x09: return_to_gem(a6); break;
        //case 0x16: ram_save(a6); break;
        //case 0x17: ram_load(a6); break;
        case 0x19: lens_display(a6); break;
        //case 0x1e: alloc_space(a6); break;
/* v4 */
        case 0x0e: driver_14(a6); break;
        case 0x20: show_bitmap(a6); break;
        case 0x22: check_for_disc(a6); break;
    }
}

/* The RAM SAVE area is RAM_SAVE_SLOTS x sizeof(save_struct_t) = 10 x 2560 =
 * 25600 bytes. The RAM SAVE command saves the current position and the RAM
 * RESTORE/LOAD command restores it. Implicit RAM SAVEs are done automatically
 * by the game and can be restored with UNDO/OOPS in multiple steps.
 */

static void ram_save(uint8_t i) __z88dk_fastcall
{
#ifdef CODEFOLLOW
    cf_print(" %u", i);
#endif

    uint8_t memory_page = current_page;
    uint8_t *ram_save_slot = effective_ram_save(i);
    memcpy(ram_save_slot, workspace.var_table, sizeof(save_struct_t));
    current_page = memory_page;
    page_in_game();
}

static void ram_load(uint8_t i) __z88dk_fastcall
{
#ifdef CODEFOLLOW
    cf_print(" %u", i);
#endif

    uint8_t memory_page = current_page;
    uint8_t *ram_save_slot = effective_ram_save(i);
    memcpy(workspace.var_table, ram_save_slot, sizeof(save_struct_t));
    current_page = memory_page;
    page_in_game();
}

static void call_driver(void)
{
    uint8_t *a6 = list9_start_ptr;
    uint8_t d0 = *a6++;

#ifdef CODEFOLLOW
    cf_print(" %s", driver_calls[d0]);
#endif

    if (d0 == 0x16 || d0 == 0x17)
    {
        uint8_t d1 = *a6;

        if (d1 > 0xfa)
        {
            *a6 = 1;
        }
        else if (d1 + 1 >= RAM_SAVE_SLOTS)
        {
            *a6 = 0xff;
        }
        else
        {
            *a6 = 0;
            if (d0 == 0x16)
            {
                ram_save(d1 + 1);
            }
            else
            {
                ram_load(d1 + 1);
            }
        }

        *list9_start_ptr = *a6;
    }
    else if (d0 == 0x0b)
    {
        if (*a6 == 0)
        {
            print_string("\rSearching for next game part.\r");
            if (!os_get_game_file(game_file, sizeof(game_file)))
            {
                print_string("\rFailed to load game.\r");
                return;
            }
        }
        else
        {
            os_set_file_number(game_file, sizeof(game_file), *a6);
        }

#ifdef CODEFOLLOW
        cf_print(" %s", game_file);
#endif

        int_load_game(game_file);
    }
    else
    {
        driver(d0, a6);
    }
}

static void random(void)
{
#ifdef CODEFOLLOW
    cf_print(" %u", random_seed);
#endif
    random_seed = (((random_seed << 8) + 0x0a - random_seed) << 2) + random_seed + 1;
    *get_var() = random_seed & 0xff;
#ifdef CODEFOLLOW
    cf_print(" %u", random_seed);
#endif
}

static void save(void)
{
    uint16_t checksum = 0;

    /* does a full save of the workpace */
    workspace.id = L9_ID;
    workspace.code_ptr = code_ptr;
    workspace.list_area_size = LIST_AREA_SIZE;
    workspace.stack_size = STACK_SIZE;
    workspace.filename_size = MAX_PATH;
    workspace.checksum = 0;
    strcpy(workspace.filename, game_file);

    for (uint16_t i = 0; i < sizeof(game_state_t); i++)
    {
        checksum += ((uint8_t *) &workspace)[i];
    }
    workspace.checksum = checksum;

    if (os_save_file((uint8_t *) &workspace, sizeof(game_state_t)))
    {
        print_string("\rGame saved.\r");
    }
    else
    {
        print_string("\rUnable to save game.\r");
    }
}

static bool check_file(game_state_t *gs) __z88dk_fastcall
{
    uint16_t checksum;
    uint8_t c = 'Y';

    if (gs->id != L9_ID)
    {
        return false;
    }

    checksum = gs->checksum;
    gs->checksum = 0;
    for (uint16_t i = 0; i < sizeof(game_state_t); i++)
    {
        checksum -= ((uint8_t *) gs)[i];
    }
    if (checksum)
    {
        return false;
    }

    if (stricmp(gs->filename, game_file))
    {
        print_string("\rWarning: Game path name does not match, "
            "you may be about to load this position file into the wrong story file.\r");
        print_string("Are you sure you want to restore? (Y/N)");
        os_flush();

        c = '\0';
        while ((c != 'y') && (c != 'Y') && (c != 'n') && (c != 'N'))
        {
            c = os_read_char(20);
        }
    }

    return (c == 'y') || (c == 'Y');
}

static void normal_restore(void)
{
    bool restore_succeeded = false;
    uint16_t size;

    // Backup current workspace if loading of new workspace fails.
    ZXN_WRITE_MMU2(WORKSPACE_BACKUP_PAGE);
    memcpy(MMU2_ADDRESS, &workspace, sizeof(game_state_t));
    ZXN_WRITE_MMU2(10);

    if (os_load_file((uint8_t *) &workspace, &size, sizeof(game_state_t)))
    {
        if (check_file(&workspace))
        {
            print_string("\rGame restored.\r");
            /* only restore var_table and list_area */
            ZXN_WRITE_MMU2(WORKSPACE_BACKUP_PAGE);
            memcpy(((game_state_t *) MMU2_ADDRESS)->var_table, workspace.var_table, sizeof(save_struct_t));
            memcpy(&workspace, MMU2_ADDRESS, sizeof(game_state_t));
            ZXN_WRITE_MMU2(10);
            restore_succeeded = true;
        }
        else
        {
            print_string("\rSorry, unrecognised format. Unable to restore.\r");
        }
    }
    else
    {
        print_string("\rUnable to restore game.\r");
    }

    // Restore backed-up workspace if loading of new workspace failed.
    if (!restore_succeeded)
    {
        ZXN_WRITE_MMU2(WORKSPACE_BACKUP_PAGE);
        memcpy(&workspace, MMU2_ADDRESS, sizeof(game_state_t));
        ZXN_WRITE_MMU2(10);
    }
}

static void restore(void)
{
    bool restore_succeeded = false;
    uint16_t size;

    // Backup current workspace if loading of new workspace fails.
    ZXN_WRITE_MMU2(WORKSPACE_BACKUP_PAGE);
    memcpy(MMU2_ADDRESS, &workspace, sizeof(game_state_t));
    ZXN_WRITE_MMU2(10);

    if (os_load_file((uint8_t *) &workspace, &size, sizeof(game_state_t)))
    {
        if (check_file(&workspace))
        {
            print_string("\rGame restored.\r");
            /* full restore */
            code_ptr = workspace.code_ptr;
            restore_succeeded = true;
        }
        else
        {
            print_string("\rSorry, unrecognised format. Unable to restore.\r");
        }
    }
    else
    {
        print_string("\rUnable to restore game.\r");
    }

    // Restore backed-up workspace if loading of new workspace failed.
    if (!restore_succeeded)
    {
        ZXN_WRITE_MMU2(WORKSPACE_BACKUP_PAGE);
        memcpy(&workspace, MMU2_ADDRESS, sizeof(game_state_t));
        ZXN_WRITE_MMU2(10);
    }
}

static void playback(void)
{
    if (script_file != ESX_INVALID_FILE_HANDLE)
    {
        page_in_rom();
        esx_f_close(script_file);
        // os_open_script_file() calls page_in_game()
    }
    script_file = os_open_script_file();
    if (errno)
    {
        script_file = ESX_INVALID_FILE_HANDLE;
        print_string("\rUnable to play back script file.\r");
    }
    else
    {
        // Disable scroll pause during script play.
        ioctl(1, IOCTL_OTERM_PAUSE, 0);
        print_string("\rPlaying back input from script file.\r");
    }
}

static void clear_workspace(void)
{
    memset(workspace.var_table, 0, sizeof(workspace.var_table));
}

static void clear_stack(void)
{
    workspace.stack_ptr = 0;
}

static void print_string_and_advance(void)
{
    print_string(effective(code_ptr));
    while (*effective(code_ptr++));
}

static void ilins(uint8_t d0) __z88dk_fastcall
{
    error("\rIllegal instruction: %u\r", d0);
    running = false;
}

static void function(void)
{
    uint8_t d0 = *effective(code_ptr++);

#ifdef CODEFOLLOW
    cf_print(" %s", (d0 == 250) ? "print_str" : (char *) functions[d0 - 1]);
#endif

    switch (d0)
    {
        case 1: call_driver(); break;
        case 2: random(); break;
        case 3: save(); break;
        case 4: normal_restore(); break;
        case 5: clear_workspace(); break;
        case 6: clear_stack(); break;
        case 250: print_string_and_advance(); break;
        default: ilins(d0);
    }
}

static int script_getc(void)
{
    uint16_t num_read;
    int c = 0;

    errno = 0;
    num_read = esx_f_read(script_file, &c, 1);
    return (errno || (num_read != 1)) ? EOF : c;
}

static bool script_gets(uint8_t *s, uint16_t n)
{
    int c = '\0';
    uint16_t count = 0;

    while ((c != '\n') && (c != '\r') && (c != EOF) && (count < n - 1))
    {
        c = script_getc();
        *s++ = c;
        count++;
    }

    *s = '\0';

    if (c == EOF)
    {
        s--;
        *s = '\n';
    }
    else if (c == '\r')
    {
        s--;
        *s = '\n';

        c = script_getc();
        if ((c != '\r') && (c != EOF))
        {
            esx_f_seek(script_file, 1, ESX_SEEK_BWD);
        }
    }

    return (c == EOF);
}

static bool script_input(uint8_t *in_buf, uint16_t size)
{
    while (script_file != ESX_INVALID_FILE_HANDLE)
    {
        uint8_t *p = in_buf;
        bool is_eof;

        *p = '\0';

        page_in_rom();
        is_eof = script_gets(in_buf, size);
        if (is_eof)
        {
            esx_f_close(script_file);
            script_file = ESX_INVALID_FILE_HANDLE;
            // Enable scroll pause handling again.
            ioctl(1, IOCTL_OTERM_PAUSE, 1);
        }
        page_in_game();

        while (*p != '\0')
        {
            switch (*p)
            {
                case '\n':
                case '\r':
                case '[':
                case ';':
                    *p = '\0';
                    break;
                case '#':
                    if ((p == in_buf) && (strnicmp(p, "#seed ", 6) == 0))
                    {
                        p++;
                    }
                    else
                    {
                        *p = '\0';
                    }
                    break;
                default:
                    p++;
                    break;
            }
        }

        if (*in_buf != '\0')
        {
            print_string(in_buf);
            last_char = '.';
            last_actual_char = '.';
            return true;
        }
    }

    return false;
}

static void find_msg_equiv(uint16_t d7) __z88dk_fastcall
{
    uint16_t d4 = 0xffff; // -1
    uint16_t d0;
    uint16_t a2 = start_md;

    do
    {
        if (a2 > end_md)
        {
            return;
        }

        d4++;
        d0 = *effective(a2);

        if (d0 & 0x80)
        {
            a2++;
            d4 += d0 & 0x7f;
        }
        else if (d0 & 0x40)
        {
            uint16_t d6 = get_md_length(&a2);

            do
            {
                uint16_t d1;

                if (d6 == 0)
                {
                    break;
                }

                d1 = *effective(a2++);
                d6--;
                if (d1 & 0x80)
                {
                    if (d1 < 0x90)
                    {
                        a2++;
                        d6--;
                    }
                    else
                    {
                        d0 = (d1 << 8) + *effective(a2++);
                        d6--;
                        if (d7 == (d0 & 0xfff))
                        {
                            d0 = ((d0 << 1) & 0xe000) | d4;
                            list9_ptr[1] = (uint8_t) d0;
                            list9_ptr[0] = (uint8_t) (d0 >> 8);
                            list9_ptr += 2;
                            if (list9_ptr >= list9_start_ptr + 0x20)
                            {
                                return;
                            }
                        }
                    }
                }
            } while (true);
        }
        else
        {
            uint16_t len = get_md_length(&a2);
            a2 += len;
        }
    } while (true);
}

static bool unpack_word(void)
{
    uint8_t *a3;

    if (unpack_d3 == 0x1b)
    {
        return true;
    }

    a3 = (uint8_t *) three_chars + (unpack_d3 & 3);

    while (true)
    {
        uint8_t d0 = get_dictionary_code();
        if (dict_ptr >= end_wdp5)
        {
            return true;
        }
        if (d0 >= 0x1b)
        {
            *a3 = 0;
            unpack_d3 = d0;
            return false;
        }
        *a3++ = get_dictionary(d0);
    }
}

static bool init_unpack(uint16_t ptr) __z88dk_fastcall
{
    init_dict(ptr);
    unpack_d3 = 0x1c;
    return unpack_word();
}

static bool part_word(uint8_t c) __z88dk_fastcall
{
    c = tolower(c);

    if (c == 0x27 || c == 0x2d)
    {
        return false;
    }
    else if (c < 0x30)
    {
        return true;
    }
    else if (c < 0x3a)
    {
        return false;
    }
    else if (c < 0x61)
    {
        return true;
    }
    else if (c < 0x7b)
    {
        return false;
    }
    else
    {
        return true;
    }
}

static void check_number(void)
{
    if (*out_buffer >= 0x30 && *out_buffer < 0x3a)
    {
        if (game_type == L9_V4)
        {
            *list9_ptr = 1;
            L9SETWORD(list9_ptr + 1, atol(out_buffer));
            L9SETWORD(list9_ptr + 3, 0);
        }
        else
        {
            L9SETDWORD(list9_ptr, atol(out_buffer));
            L9SETWORD(list9_ptr + 4, 0);
        }
    }
    else
    {
        L9SETWORD(list9_ptr, 0x8000);
        L9SETWORD(list9_ptr + 2, 0);
    }
}

static bool strcmp_hash(uint8_t *command) __z88dk_fastcall
{
    uint8_t length;

    // Allow both exact match and relaxed match with trailing whitespace (and anything subsequent) ignored.

    if (stricmp(in_buffer, command) == 0)
    {
        return true;
    }

    length = strlen(command);
    if ((strnicmp(in_buffer, command, length) == 0) && (in_buffer[length] == ' '))
    {
        return true;
    }

    return false;
}

static bool check_hash(void)
{
    if (strcmp_hash("#save"))
    {
        putchar('\n');
        save();
        return true;
    }
    else if (strcmp_hash("#restore"))
    {
        putchar('\n');
        restore();
        return true;
    }
    else if (strcmp_hash("#quit"))
    {
        stop_game();
        print_string("\rGame Terminated\r");
        return true;
    }
    else if (strcmp_hash("#play"))
    {
        playback();
        return true;
    }
    else if (strnicmp(in_buffer, "#picture ", 9) == 0)
    {
        uint16_t pic = 0;
        if (sscanf(in_buffer + 9, "%u", &pic) == 1)
        {
            os_show_bitmap(pic);
        }
        last_actual_char = 0;
        print_char('\r');
        return true;
    }
    else if (strnicmp(in_buffer, "#seed ", 6) == 0)
    {
        uint16_t seed = 0;
        if (sscanf(in_buffer + 6, "%u", &seed) == 1)
        {
            const_seed = seed;
            random_seed = seed;
        }
        last_actual_char = 0;
        print_char('\r');
        return true;
    }

    return false;
}

static bool is_input_char(uint8_t c) __z88dk_fastcall
{
    if (c == '-' || c == '\'')
    {
        return true;
    }

    if ((game_type >= L9_V3) && (c == '.' || c == ','))
    {
        return true;
    }

    return isalnum(c);
}

static bool corrupting_input(void)
{
    uint8_t *a0;
    uint8_t *a2;
    uint8_t *a6;
    uint16_t d0;
    uint16_t d1;
    uint16_t d2;
    uint16_t abrev_word;
    uint16_t dict_addr;

    list9_ptr = list9_start_ptr;

    if (in_buffer_ptr == NULL)
    {
        /* flush */
        os_flush();
        last_char = '.';
        last_actual_char = '.';

        /* get input */
        if (!script_input(in_buffer, IN_BUFFER_SIZE))
        {
            if (!os_input(in_buffer, IN_BUFFER_SIZE))
            {
                return false; /* fall through */
            }
        }
        if (check_hash())
        {
            return false;
        }

        /* check for invalid chars */
        for (uint8_t *iptr = in_buffer; *iptr != 0; iptr++)
        {
            if (!is_input_char(*iptr))
            {
                *iptr = ' ';
            }
        }

        /* force CR but prevent others */
        last_actual_char = '\r';
        os_print_char(last_actual_char);

        in_buffer_ptr = in_buffer;
    }

    a2 = out_buffer;
    a6 = in_buffer_ptr;

    /* ip05 */
    while (true)
    {
        d0 = *a6++;
        if (d0 == 0)
        {
            in_buffer_ptr = NULL;
            L9SETWORD(list9_ptr, 0);
            return true;
        }
        if (!part_word((uint8_t) d0))
        {
            break;
        }
        if (d0 != 0x20)
        {
            in_buffer_ptr = a6;
            L9SETWORD(list9_ptr, 0);
            L9SETWORD(list9_ptr + 2, 0);
            list9_ptr[1] = (uint8_t) d0;
            *a2 = 0x20;
            return true;
        }
    }

    a6--;
    /* ip06loop */
    do
    {
        d0 = *a6++;
        if (part_word((uint8_t) d0))
        {
            break;
        }
        d0 = tolower(d0);
        *a2++ = (uint8_t) d0;
    } while (a2 < (uint8_t *) out_buffer + 0x1f);

    /* ip06a */
    *a2 = 0x20;
    a6--;
    in_buffer_ptr = a6;
    abrev_word = 0xffff; // -1
    list9_ptr = list9_start_ptr;
    /* setindex */
    a0 = effective(dict_data);
    d2 = dict_data_len;
    d0 = *out_buffer - 0x61;

    if (d0 & 0x8000) // d0 < 0
    {
        dict_addr = def_dict;
        d1 = 0;
    }
    else
    {
        /* ip10 */
        d1 = 0x67;
        if (d0 < 0x1a)
        {
            d1 = d0 << 2;
            d0 = out_buffer[1];
            if (d0 != 0x20)
            {
                d1 += ((d0 - 0x61) >> 3) & 3;
            }
        }
        /*ip13 */
        if (d1 >= d2)
        {
            check_number();
            return true;
        }
        a0 += d1 << 2;
        dict_addr = L9WORD(a0);
        d1 = L9WORD(a0 + 2);
    }

    /* ip13gotwordnumber */
    init_unpack(dict_addr);
    /* ip14 */
    d1--;

    do
    {
        d1++;
        if (unpack_word())
        {
            /* ip21b */
            if (abrev_word == 0xffff) // -1
            {
                break; /* goto ip22 */
            }
            else
            {
                d0 = abrev_word; /* goto ip18b */
            }
        }
        else
        {
            uint8_t *a1 = three_chars;
            uint16_t d6 = 0xffff; // -1

            a0 = out_buffer;
            /* ip15 */
            do
            {
                d6++;
                d0 = tolower(*a1++ & 0x7f);
                d2 = *a0++;
            } while (d0 == d2);

            if (d2 != 0x20)
            {
                /* ip17 */
                if (abrev_word == 0xffff) // -1
                {
                    continue;
                }
                else
                {
                    d0 = 0xffff; // -1
                }
            }
            else if (d0 == 0)
            {
                d0 = d1;
            }
            else if (abrev_word != 0xffff) // -1
            {
                break;
            }
            else if (d6 >= 4)
            {
                d0 = d1;
            }
            else
            {
                abrev_word = d1;
                continue;
            }
        }

        /* ip18b */
        find_msg_equiv(d1);
        abrev_word = 0xffff; // -1
        if (list9_ptr != list9_start_ptr)
        {
            L9SETWORD(list9_ptr, 0);
            return true;
        }
    } while (true);

    /* ip22 */
    check_number();
    return true;
}

static bool is_dictionary_char(uint8_t c) __z88dk_fastcall
{
    switch (c)
    {
        case '?':
        case '-':
        case '\'':
        case '/':
        case '!':
        case '.':
        case ',':
            return true;
    }

    return isupper(c) || isdigit(c);
}

static bool input_v2(uint16_t *word_count) __z88dk_fastcall
{
    uint8_t a;
    uint8_t x;
    uint8_t *in_buf_ptr;
    uint8_t *out_buf_ptr;
    uint8_t *ptr;
    uint8_t *list0_ptr;

    /* flush */
    os_flush();
    last_char = '.';
    last_actual_char = '.';

    /* get input */
    if (!script_input(in_buffer, IN_BUFFER_SIZE))
    {
        if (!os_input(in_buffer, IN_BUFFER_SIZE))
        {
            return false; /* fall through */
        }
    }
    if (check_hash())
    {
        return false;
    }

    /* check for invalid chars */
    for (uint8_t *iptr = in_buffer; *iptr != 0; iptr++)
    {
        if (!is_input_char(*iptr))
        {
            *iptr = ' ';
        }
    }

    /* force CR but prevent others */
    last_actual_char = '\r';
    os_print_char(last_actual_char);

    /* add space onto end */
    in_buf_ptr = (uint8_t *) strchr(in_buffer, 0);
    *in_buf_ptr++ = 32;
    *in_buf_ptr = 0;

    *word_count = 0;
    in_buf_ptr = in_buffer;
    out_buf_ptr = out_buffer;
    list0_ptr = effective(dict_data);

    while (*in_buf_ptr == 32)
    {
        ++in_buf_ptr;
    }

    ptr = in_buf_ptr;
    do
    {
        while (*ptr == 32)
        {
            ++ptr;
        }
        if (*ptr == 0)
        {
            break;
        }
        (*word_count)++;
        do
        {
            a = *++ptr;
        } while (a != 32 && a != 0);
    } while (*ptr != 0);

    while (true)
    {
        ptr = in_buf_ptr;
        while (*in_buf_ptr == 32)
        {
            ++in_buf_ptr;
        }

        while (true)
        {
            a = *in_buf_ptr;
            x = *list0_ptr++;

            if (a == 32)
            {
                break;
            }
            if (a == 0)
            {
                *out_buf_ptr++ = 0;
                return true;
            }

            ++in_buf_ptr;
            if (!is_dictionary_char(x & 0x7f))
            {
                x = 0;
            }

            if (tolower(x & 0x7f) != tolower(a))
            {
                while (x > 0 && x < 0x7f)
                {
                    x = *list0_ptr++;
                }

                if (x == 0)
                {
                    do
                    {
                        a = *in_buf_ptr++;
                        if (a == 0)
                        {
                            *out_buf_ptr = 0;
                            return true;
                        }
                    } while (a != 32);

                    while (*in_buf_ptr == 32)
                    {
                        ++in_buf_ptr;
                    }

                    list0_ptr = effective(dict_data);
                    ptr = in_buf_ptr;
                }
                else
                {
                    list0_ptr++;
                    in_buf_ptr = ptr;
                }
            }
            else if (x >= 0x7f)
            {
                break;
            }
        }

        a = *in_buf_ptr;
        if (a != 32)
        {
            in_buf_ptr = ptr;
            list0_ptr += 2;
            continue;
        }

        while (*in_buf_ptr == 32)
        {
            ++in_buf_ptr;
        }

        --list0_ptr;
        while (*list0_ptr++ < 0x7e);
        *out_buf_ptr++ = *list0_ptr;
        list0_ptr = effective(dict_data);
    }
}

static void input(void)
{
    /* If corrupting_input() returns false then input() will be called again next
     * time around in the instruction loop, this is used when save() and restore()
     * are called out of line.
     */
    code_ptr--;

    if (game_type <= L9_V2)
    {
        uint16_t word_count;
        if (input_v2(&word_count))
        {
            uint8_t *obuffptr = out_buffer;
            code_ptr++;
            *get_var() = *obuffptr++;
            *get_var() = *obuffptr++;
            *get_var() = *obuffptr;
            *get_var() = word_count;
        }
    }
    else if (corrupting_input())
    {
        code_ptr += 5;
    }
}

static void var_con(void)
{
    uint16_t d6 = get_con();
    *get_var() = d6;

#ifdef CODEFOLLOW
    cf_print(" var[%u] = %u", cf_var - workspace.var_table, *cf_var);
#endif
}

static void var_var(void)
{
    uint16_t d6 = get_var_val();
    *get_var() = d6;

#ifdef CODEFOLLOW
    cf_print(" var[%u] = var[%u] (=%u)", cf_var - workspace.var_table, cf_var2 - workspace.var_table, d6);
#endif
}

static void add(void)
{
    uint16_t d0 = get_var_val();
    *get_var() += d0;

#ifdef CODEFOLLOW
    cf_print(" var[%u] += var[%u] (+=%u)", cf_var - workspace.var_table, cf_var2 - workspace.var_table, d0);
#endif
}

static void sub(void)
{
    uint16_t d0 = get_var_val();
    *get_var() -= d0;

#ifdef CODEFOLLOW
    cf_print(" var[%u] -= var[%u] (-=%u)", cf_var - workspace.var_table, cf_var2 - workspace.var_table, d0);
#endif
}

static void jump(void)
{
    uint16_t d0 = L9WORD(effective(code_ptr));
    uint16_t a0;

    code_ptr += 2;
    a0 = acode_ptr + ((d0 + (get_var_val() << 1)) & 0xffff);
    code_ptr = acode_ptr + L9WORD(effective(a0));

#ifdef CODEFOLLOW
    cf_print(" %u", code_ptr - acode_ptr);
#endif
}

static void do_exit(uint8_t *d4, uint8_t *d5, uint8_t d6, uint8_t d7)
{
    uint8_t *a0 = effective(abs_data_block);
    uint8_t d1 = d7;
    uint8_t d0;

    // Assumption: a0 stays within the bottom 16K.

    if (--d1)
    {
        do
        {
            d0 = *a0;
            if (game_type == L9_V4)
            {
                if ((d0 == 0) && (*(a0 + 1) == 0))
                {
                    goto notfn4;
                }
            }
            a0 += 2;
        } while ((d0 & 0x80) == 0 || --d1);
    }

    do
    {
        *d4 = *a0++;
        if (((*d4) & 0xf) == d6)
        {
            *d5 = *a0;
            return;
        }
        a0++;
    } while (((*d4) & 0x80) == 0);

    /* notfn4 */
notfn4:
    d6 = exit_reversal_table[d6];
    a0 = effective(abs_data_block);
    *d5 = 1;

    do
    {
        *d4 = *a0++;
        if (((*d4) & 0x10) == 0 || ((*d4) & 0xf) != d6)
        {
            a0++;
        }
        else if (*a0++ == d7)
        {
            return;
        }
        /* exit6noinc */
        if ((*d4) & 0x80)
        {
            (*d5)++;
        }
    } while (*d4);

    *d5 = 0;
}

static void int_exit(void)
{
    uint8_t d4;
    uint8_t d5;
    uint8_t d7 = (uint8_t) get_var_val();
    uint8_t d6 = (uint8_t) get_var_val();

#ifdef CODEFOLLOW
    cf_print(" (d7=%u) (d6=%u)", d7, d6);
#endif

    do_exit(&d4, &d5, d6, d7);
    *get_var() = (d4 & 0x70) >> 4;
    *get_var() = d5;

#ifdef CODEFOLLOW
    cf_print(" var[%u] = %u (d4=%u) var[%u] = %u",
        cf_var2 - workspace.var_table, (d4 & 0x70) >> 4, d4, cf_var - workspace.var_table, d5);
#endif
}

static void if_eq_vt(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_var_val();
    uint16_t a0 = get_addr();

    if (d0 == d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] = var[%u] goto %u (%s)",
        cf_var2 - workspace.var_table, cf_var - workspace.var_table, a0 - acode_ptr, (d0 == d1) ? "Yes" : "No");
#endif
}

static void if_ne_vt(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_var_val();
    uint16_t a0 = get_addr();

    if (d0 != d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] != var[%u] goto %u (%s)",
        cf_var2 - workspace.var_table, cf_var - workspace.var_table, a0 - acode_ptr, (d0 != d1) ? "Yes" : "No");
#endif
}

static void if_lt_vt(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_var_val();
    uint16_t a0 = get_addr();

    if (d0 < d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] < var[%u] goto %u (%s)",
        cf_var2 - workspace.var_table, cf_var - workspace.var_table, a0 - acode_ptr, (d0 < d1) ? "Yes" : "No");
#endif
}

static void if_gt_vt(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_var_val();
    uint16_t a0 = get_addr();

    if (d0 > d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] > var[%u] goto %u (%s)",
        cf_var2 - workspace.var_table, cf_var - workspace.var_table, a0 - acode_ptr, (d0 > d1) ? "Yes" : "No");
#endif
}

static void screen(void)
{
    bool graphics_on = *effective(code_ptr++);
    os_graphics(graphics_on);
    if (graphics_on)
    {
        code_ptr++;
    }

#ifdef CODEFOLLOW
    cf_print(" %s", graphics_on ? "graphics" : "text");
#endif
}

static void clear_tg(void)
{
    uint8_t d0 = *effective(code_ptr++);

#ifdef CODEFOLLOW
    cf_print(" %s", d0 ? "graphics" : "text");
#endif

    if (d0)
    {
        os_clear_graphics();
    }
}

static void picture(void)
{
    // Line-drawn images are preconverted to bitmap images by a separate tool.
#ifndef CODEFOLLOW
    os_show_bitmap(get_var_val());
#else
    uint16_t pic = get_var_val();
    os_show_bitmap(pic);
    cf_print(" %u", pic);
#endif
}

static void init_get_obj(void)
{
    num_object_found = 0;
    object = 0;
    memset(gno_scratch, 0, sizeof(gno_scratch));
}

static void get_next_object(void)
{
    uint16_t d2;
    uint16_t d3;
    uint16_t d4;
    uint16_t *hi_search_pos_var;
    uint16_t *search_pos_var;

    d2 = get_var_val();
    hi_search_pos_var = get_var();
    search_pos_var = get_var();
    d3 = *hi_search_pos_var;
    d4 = *search_pos_var;

    /* gnoabs */
    do
    {
        if ((d3 | d4) == 0)
        {
            /* initgetobjsp */
            gno_sp = 128;
            search_depth = 0;
            init_get_obj();
            break;
        }

        if (num_object_found == 0)
        {
            init_hi_search_pos = d3;
        }

        /* gnonext */
        do
        {
            if (d4 == list2_ptr[++object])
            {
                /* gnomaybefound */
                uint8_t d6 = list3_ptr[object] & 0x1f;
                if (d6 != d3)
                {
                    if (d6 == 0 || d3 == 0)
                    {
                        continue;
                    }
                    if (d3 != 0x1f)
                    {
                        gno_scratch[d6] = d6;
                        continue;
                    }
                    d3 = d6;
                }

                /* gnofound */
                num_object_found++;
                gno_stack[--gno_sp] = object;
                gno_stack[--gno_sp] = 0x1f;

                *hi_search_pos_var = d3;
                *search_pos_var = d4;
                *get_var() = object;
                *get_var() = num_object_found;
                *get_var() = search_depth;
                return;
            }
        } while (object <= d2);

        if (init_hi_search_pos == 0x1f)
        {
            gno_scratch[d3] = 0;
            d3 = 0;

            /* gnoloop */
            do
            {
                if (gno_scratch[d3])
                {
                    gno_stack[--gno_sp] = d4;
                    gno_stack[--gno_sp] = d3;
                }
            } while (++d3 < 0x1f);
        }

        /* gnonewlevel */
        if (gno_sp != 128)
        {
            d3 = gno_stack[gno_sp++];
            d4 = gno_stack[gno_sp++];
        }
        else
        {
            d3 = 0;
            d4 = 0;
        }

        num_object_found = 0;
        if (d3 == 0x1f)
        {
            search_depth++;
        }

        init_get_obj();
    } while (d4);

    /* gnofinish */
    /* gnoreturnargs */
    *hi_search_pos_var = 0;
    *search_pos_var = 0;
    object = 0;
    *get_var() = object;
    *get_var() = num_object_found;
    *get_var() = search_depth;
}

static void if_eq_ct(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_con();
    uint16_t a0 = get_addr();

    if (d0 == d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] = %u goto %u (%s)",
        cf_var - workspace.var_table, d1, a0 - acode_ptr, (d0 == d1) ? "Yes" : "No");
#endif
}

static void if_ne_ct(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_con();
    uint16_t a0 = get_addr();

    if (d0 != d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] != %u goto %u (%s)",
        cf_var - workspace.var_table, d1, a0 - acode_ptr, (d0 != d1) ? "Yes" : "No");
#endif
}

static void if_lt_ct(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_con();
    uint16_t a0 = get_addr();

    if (d0 < d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] < %u goto %u (%s)",
        cf_var - workspace.var_table, d1, a0 - acode_ptr, (d0 < d1) ? "Yes" : "No");
#endif
}

static void if_gt_ct(void)
{
    uint16_t d0 = get_var_val();
    uint16_t d1 = get_con();
    uint16_t a0 = get_addr();

    if (d0 > d1)
    {
        code_ptr = a0;
    }

#ifdef CODEFOLLOW
    cf_print(" if var[%u] > %u goto %u (%s)",
        cf_var - workspace.var_table, d1, a0 - acode_ptr, (d0 > d1) ? "Yes" : "No");
#endif
}

static void print_input(void)
{
    uint8_t *ptr = out_buffer;
    uint8_t c;

    while ((c = *ptr++) != ' ')
    {
        print_char(c);
    }
}

static void list_handler(void)
{
    uint8_t a4_index;
    bool a4_in_ws;
    uint16_t a4;
    uint16_t max_access;
    uint16_t val;
    uint16_t *var;
#ifdef CODEFOLLOW
    uint16_t offset;
#endif

#ifdef L9DEBUG
    if ((code & 0x1f) > 0xa)
    {
        error("\rIllegal list access %u\r", code & 0x1f);
        running = false;
        return;
    }
#endif

    a4_index = (code + 1) & 0x1f;
    a4_in_ws = l9_pointers_in_ws[a4_index];
    a4 = l9_pointers[a4_index];
    max_access = a4_in_ws ? LIST_AREA_SIZE : memory_size;

    if (code >= 0xe0)
    {
#ifndef CODEFOLLOW
        a4 += get_var_val();
        val = get_var_val();
#else
        offset = get_var_val();
        a4 += offset;
        var = get_var();
        val = *var;
        cf_print(" list_%u[%u] = var[%u] (=%u)", code & 0x1f, offset, var - workspace.var_table, val);
#endif

        if (a4 < max_access)
        {
            if (a4_in_ws)
            {
                *(workspace.list_area + a4) = (uint8_t) val;
            }
            else
            {
                *effective(a4) = (uint8_t) val;
            }
        }
    }
    else if (code >= 0xc0)
    {
#ifndef CODEFOLLOW
        a4 += *effective(code_ptr++);
        var = get_var();
#else
        offset = *effective(code_ptr++);
        a4 += offset;
        var = get_var();
        cf_print(" var[%u] = list_%u[%u]", var - workspace.var_table, code & 0x1f, offset);
        if (a4 < max_access)
        {
            uint8_t a4_value = a4_in_ws ? *(workspace.list_area + a4) : *effective(a4);
            cf_print(" (=%u)", a4_value);
        }
#endif

        if (a4 < max_access)
        {
            *var = a4_in_ws ? *(workspace.list_area + a4) : *effective(a4);
        }
        else
        {
            *var = 0;
        }
    }
    else if (code >= 0xa0)
    {
#ifndef CODEFOLLOW
        a4 += get_var_val();
        var = get_var();
#else
        offset = get_var_val();
        a4 += offset;
        var = get_var();
        cf_print(" var[%u] = list_%u[%u]", var - workspace.var_table, code & 0x1f, offset);
        if (a4 < max_access)
        {
            uint8_t a4_value = a4_in_ws ? *(workspace.list_area + a4) : *effective(a4);
            cf_print(" (=%u)", a4_value);
        }
#endif

        if (a4 < max_access)
        {
            *var = a4_in_ws ? *(workspace.list_area + a4) : *effective(a4);
        }
        else
        {
            *var = 0;
        }
    }
    else
    {
#ifndef CODEFOLLOW
        a4 += *effective(code_ptr++);
        val = get_var_val();
#else
        offset = *effective(code_ptr++);
        a4 += offset;
        var = get_var();
        val = *var;
        cf_print(" list_%u[%u] = var[%u] (=%u)", code & 0x1f, offset, var - workspace.var_table, val);
#endif

        if (a4 < max_access)
        {
            if (a4_in_ws)
            {
                *(workspace.list_area + a4) = (uint8_t) val;
            }
            else
            {
                *effective(a4) = (uint8_t) val;
            }
        }
    }
}

static void execute_instruction(void)
{
#ifdef CODEFOLLOW
    cf_print("%5u (s:%2u) %2.2X", code_ptr - acode_ptr - 1, workspace.stack_ptr, code);
    if (!(code & 0x80))
    {
        cf_print(" = %s", codes[code & 0x1f]);
    }
#endif

    if (code & 0x80)
    {
        list_handler();
    }
    else
    {
        switch (code & 0x1f)
        {
            case 0:  int_goto(); break;
            case 1:  int_gosub(); break;
            case 2:  int_return(); break;
            case 3:  print_number(); break;
            case 4:  messagev(); break;
            case 5:  messagec(); break;
            case 6:  function(); break;
            case 7:  input(); break;
            case 8:  var_con(); break;
            case 9:  var_var(); break;
            case 10: add(); break;
            case 11: sub(); break;
            case 12: // Fall through
            case 13: ilins(code & 0x1f); break;
            case 14: jump(); break;
            case 15: int_exit(); break;
            case 16: if_eq_vt(); break;
            case 17: if_ne_vt(); break;
            case 18: if_lt_vt(); break;
            case 19: if_gt_vt(); break;
            case 20: screen(); break;
            case 21: clear_tg(); break;
            case 22: picture(); break;
            case 23: get_next_object(); break;
            case 24: if_eq_ct(); break;
            case 25: if_ne_ct(); break;
            case 26: if_lt_ct(); break;
            case 27: if_gt_ct(); break;
            case 28: print_input(); break;
            case 29: // Fall through
            case 30: // Fall through
            case 31: ilins(code & 0x1f); break;
        }
    }

#ifdef CODEFOLLOW
    cf_print("\n");
#endif
}

bool load_game(uint8_t *filename) __z88dk_fastcall
{
    bool ret;

    ret = int_load_game(filename);
    clear_workspace();
    clear_stack();
    memset(workspace.list_area, 0, LIST_AREA_SIZE);
    return ret;
}

game_type_t get_game_type(void)
{
    return game_type;
}

void get_picture_size(uint16_t *width, uint8_t *height)
{
    /*
     * V2 games: 160 x 128 (stretched to 320 x 128)
     * V3 games: 320 x 96 or 160 x 96 (stretched to 320 x 96)
     * V4 games: 320 x 136
     */

    if (width != NULL)
    {
        *width = 320;
    }

    if (height != NULL)
    {
        // Add top and bottom margins
        switch (game_type)
        {
            case L9_V2:
                *height = 144; // 14 + 128 + 2
                break;
            case L9_V3:
                *height = 112; // 14 + 96 + 2
                break;
            case L9_V4:
                // Fall through
            default:
                *height = 152; // 14 + 136 + 2
                break;
        }
    }
}

bool run_game(void)
{
    code = *effective(code_ptr++);
    execute_instruction();
    return running;
}

void stop_game(void)
{
    running = false;
}

void free_memory(void)
{
    if (script_file != ESX_INVALID_FILE_HANDLE)
    {
        esx_f_close(script_file);
        script_file = ESX_INVALID_FILE_HANDLE;
    }

#ifdef CODEFOLLOW
    if (cf_file != ESX_INVALID_FILE_HANDLE)
    {
        esx_f_close(cf_file);
        cf_file = ESX_INVALID_FILE_HANDLE;
    }
#endif
}
