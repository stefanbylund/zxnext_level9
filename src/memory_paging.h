/*******************************************************************************
 * Stefan Bylund 2021
 *
 * This module contains functions for memory paging in the Level 9 interpreter.
 ******************************************************************************/

#ifndef _MEMORY_PAGING_H
#define _MEMORY_PAGING_H

#include <stdint.h>
#include "ide_friendly.h"

// The 64 KB memory space of the A-machine.
#define MEMORY_BASE_PAGE 40
#define NUM_MEMORY_PAGES 8

// The 32 KB RAM SAVE area (actual size = 10 x 2560 = 25600 bytes).
#define RAM_SAVE_BASE_PAGE 36
#define NUM_RAM_SAVE_PAGES 4

/*
 * Current page in MMU slot 0.
 */
extern uint8_t current_page;

/*
 * Page in the ROM to MMU slots 0 and 1.
 */
void page_in_rom(void) __preserves_regs(b,c,d,e,h,l);

/*
 * Page in the current and next game pages to MMU slots 0 and 1.
 */
void page_in_game(void) __preserves_regs(b,c,d,e,h,l);

/*
 * Convert the given virtual pointer to an effective pointer.
 * The returned pointer will point into a game page in MMU slot 0 with the next
 * game page in MMU slot 1 and update the current_page global variable to the
 * game page in MMU slot 0.
 */
uint8_t *effective(uint16_t ptr) __preserves_regs(b,c) __z88dk_fastcall;

/*
 * Convert the given RAM SAVE slot number to an effective pointer.
 * The returned pointer will point into a RAM SAVE page in MMU slot 0 with the
 * next RAM SAVE page in MMU slot 1 and update the current_page global variable
 * to the RAM SAVE page in MMU slot 0.
 */
uint8_t *effective_ram_save(uint8_t slot) __preserves_regs(b,c) __z88dk_fastcall;

#endif
