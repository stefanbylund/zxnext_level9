/*******************************************************************************
 *
 * Level 9 interpreter
 * Version 5.1
 * Copyright (c) 1996-2011 Glen Summers and contributors.
 * Contributions from David Kinder, Alan Staniforth, Simon Baldwin, Dieter Baron
 * and Andreas Scherrer.
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
 * NOTE:
 * This Level 9 interpreter header file has been slightly changed to suit the
 * ZX Spectrum Next porting better.
 ******************************************************************************/

#ifndef LEVEL9_H
#define LEVEL9_H

#include <stdint.h>
#include <stdbool.h>
#include "ide_friendly.h"

#define VAR_TABLE_SIZE 256

#define LIST_AREA_SIZE 2048

#define STACK_SIZE 128

#ifndef MAX_PATH
#define MAX_PATH 16
#endif

/*******************************************************************************
 * Enumeration of all Level 9 game versions.
 ******************************************************************************/
typedef enum game_type
{
    L9_V1 = 1,
    L9_V2 = 2,
    L9_V3 = 3,
    L9_V4 = 4
} game_type_t;

/*******************************************************************************
 * Data structure for keeping the game state.
 ******************************************************************************/
typedef struct game_state
{
    uint32_t id;
    uint16_t code_ptr;
    uint16_t stack_ptr;
    uint16_t list_area_size;
    uint16_t stack_size;
    uint16_t filename_size;
    uint16_t checksum;
    uint16_t var_table[VAR_TABLE_SIZE];
    uint8_t list_area[LIST_AREA_SIZE];
    uint16_t stack[STACK_SIZE];
    uint8_t filename[MAX_PATH];
} game_state_t;

#define L9WORD(x) (*(uint16_t *) (x))
#define L9SETWORD(x, val) (*(uint16_t *) (x) = (uint16_t) val)
#define L9SETDWORD(x, val) (*(uint32_t *) (x) = (uint32_t) val)

/*******************************************************************************
 * Routines provided by OS-dependent code
 ******************************************************************************/

/*******************************************************************************
 * Prints a character to the output. The interface can either buffer this
 * character or print it immediately, but if buffering is used then the
 * characters must all be sent to the output when the interpreter calls
 * os_flush(). A paragraph of text is output as one long stream of characters,
 * without line breaks, so the interface must provide its own word wrapping and
 * any other features that are desired, such as justification or a [MORE]
 * prompt. The carriage return character is always '\r', rather than '\n'.
 ******************************************************************************/
void os_print_char(uint8_t c) __z88dk_fastcall;

/*******************************************************************************
 * If the calls to os_print_char() are being buffered by the interface then the
 * buffered text must be printed when os_flush() is called.
 ******************************************************************************/
void os_flush(void);

/*******************************************************************************
 * Reads a line of text from the user, usually to accept the next command to be
 * sent to the game. The text input must be stored in in_buf with a terminating
 * zero, and be no longer than size characters. Normally os_input() should
 * return true, but may return false to cause the entire input so far to be
 * discarded. The reason for doing so is to allow the interpreter to load a new
 * game without exiting.
 ******************************************************************************/
bool os_input(uint8_t *in_buf, uint16_t size);

/*******************************************************************************
 * Looks to see if a key has been pressed, if one has, returns the character to
 * the interpreter immediately. If no key has been pressed, the interpreter
 * should wait for a key for at least the number of milliseconds given in the
 * argument. If after this period no key has been pressed, 0 should be returned.
 * This function is used in multiple choice games for accepting input. It is
 * also used in some text adventure games when they are restarted, saved or
 * restored, causing them to print a prompt message (e.g. "Press SPACE to play
 * again.") and then call os_read_char().
 ******************************************************************************/
uint8_t os_read_char(uint16_t millis) __z88dk_fastcall;

/*******************************************************************************
 * Should prompt the user in some way (with either text or a file requester) for
 * a filename to save the area of memory of size bytes pointed to by ptr. True
 * or false should be returned depending on whether or not the operation was
 * successful.
 ******************************************************************************/
bool os_save_file(uint8_t *ptr, uint16_t size);

/*******************************************************************************
 * Should prompt the user for the name of a file to load. At most max_size bytes
 * should be loaded into the memory pointed to by ptr, and the number of bytes
 * read should be placed into the variable pointed to by size. True or false
 * should be returned depending on whether or not the operation was successful.
 ******************************************************************************/
bool os_load_file(uint8_t *ptr, uint16_t *size, uint16_t max_size);

/*******************************************************************************
 * Should prompt the user for a new game file, to be stored in new_name, which
 * can take a maximum name of size characters. When this function is called the
 * new_name array contains the name of the currently loaded game, which can be
 * used to derive a name to prompt the user with. True or false should be
 * returned depending on whether or not the operation was successful.
 *
 * This is used by the multiple choice games, which load in the next part of the
 * game after the part currently being played has been completed. These games
 * were originally written for tape-based systems where the call was simply
 * "load the next game from the tape".
 ******************************************************************************/
bool os_get_game_file(uint8_t *new_name, uint16_t size);

/*******************************************************************************
 * This function is for multi-part games originally written for disk-based
 * systems, which used game filenames such as gamedat1.dat, gamedat2.dat etc.
 * The function should take the full filename in new_name (of maximum size size)
 * and modify it to reflect the given number num, e.g.
 * os_set_file_number("gamedat1.dat", MAX_PATH, 2) should return "gamedat2.dat"
 * in new_name.
 ******************************************************************************/
void os_set_file_number(uint8_t *new_name, uint16_t size, uint8_t num);

/*******************************************************************************
 * Called when graphics are turned on or off, either by the game or by the user
 * entering "graphics" or "text" as a command. If graphics_on is false then
 * graphics should be turned off. If graphics_on is true then line-drawn or
 * bitmap graphics will follow, so graphics should be turned on. After an
 * os_graphics(false) call all the other graphics functions should do nothing.
 *
 * NOTE: Both line-drawn and bitmap graphics are converted off-line to Spectrum
 * Next bitmap pictures by the convert_gfx and convert_bitmap custom tools.
 ******************************************************************************/
void os_graphics(bool graphics_on) __z88dk_fastcall;

/*******************************************************************************
 * Clears the current graphics bitmap by filling the entire bitmap with
 * colour 0.
 ******************************************************************************/
void os_clear_graphics(void);

/*******************************************************************************
 * Shows the bitmap picture given by the picture number pic.
 *
 * Note that the game may request the same picture several times in a row:
 * it is a good idea for ports to record the last picture number and check it
 * against any new requests.
 *
 * NOTE: Both line-drawn and bitmap pictures are converted off-line to Spectrum
 * Next bitmap pictures by the convert_gfx and convert_bitmap custom tools for
 * later display by this function.
 ******************************************************************************/
void os_show_bitmap(uint16_t pic) __z88dk_fastcall;

/*******************************************************************************
 * Should prompt the user for the name of a script file, from which input will
 * be read until the end of the script file is reached, and should return a
 * handle to the opened script file or -1 if an error occurred. This function is
 * called in response to the player entering the "#play" interpreter command.
 ******************************************************************************/
uint8_t os_open_script_file(void);

/*******************************************************************************
 * Called if a fatal error occurs. Should display a formatted error message and
 * then exit the interpreter.
 ******************************************************************************/
void os_fatal_error(uint8_t *format, ...);

/*******************************************************************************
 * Routines provided by Level 9 interpreter
 ******************************************************************************/

/*******************************************************************************
 * Attempts to load a Level 9 game file with the given filename. If the
 * operation is successful, true is returned otherwise false. Any previous game
 * in memory will be overwritten if the file filename can be loaded, even if it
 * does not contain a Level 9 game, so even if load_game() returns false it must
 * be assumed that the game memory has changed.
 ******************************************************************************/
bool load_game(uint8_t *filename) __z88dk_fastcall;

/*******************************************************************************
 * Returns the version of the loaded Level 9 game file.
 ******************************************************************************/
game_type_t get_game_type(void);

/*******************************************************************************
 * Returns the width and height of the bitmap that graphics should be drawn
 * into. This is constant for any particular game.
 ******************************************************************************/
void get_picture_size(uint16_t *width, uint8_t *height);

/*******************************************************************************
 * If load_game() has been successful, run_game() can be called to run the
 * loaded Level 9 game. Each call to run_game() executes a single opcode of the
 * game. In pre-emptive multitasking systems or systems without any multitasking
 * it is enough to sit in a loop calling run_game(), e.g.
 *
 * while (run_game());
 *
 * This function returns true if an opcode code was executed and false if the
 * game is stopped, either by an error or by a call to stop_game().
 ******************************************************************************/
bool run_game(void);

/*******************************************************************************
 * Stops the current game from playing.
 ******************************************************************************/
void stop_game(void);

/*******************************************************************************
 * Frees memory and any resources used to store the game. This function should
 * be called when exiting the interpreter.
 ******************************************************************************/
void free_memory(void);

#endif
