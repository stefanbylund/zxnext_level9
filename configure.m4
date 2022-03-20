divert(-1)

################################################################################
# CONFIGURE THE COMPILE HERE
#
# Compile-time configuration of the Level 9 interpreter for ZX Spectrum Next.
#
# This M4 configuration file generates the following files:
# - zconfig.h    (TARGET=1): C include file
# - zconfig.inc  (TARGET=2): Assembly include file
# - zconfig.m4   (TARGET=3): M4 include file
# - zpragma.inc  (TARGET=4): Pragma file
# - zproject.lst (TARGET=5): List of source files to compile
#
# The following definitions are also configurable from the M4 command-line:
# - USE_TIMEX_HIRES
# - USE_GFX
# - USE_MOUSE
# - USE_CODEFOLLOW
# - USE_IMAGE_SLIDESHOW
################################################################################

# Text input

# Time in ms @ 3.5 MHz before a key starts repeating. Default is 500 ms @ 3.5 MHz.
# At 28 MHz, we multiply with a factor of 8.
define(`KEY_REPEAT_START', 4000)

# Time in ms @ 3.5 MHz between key repeats. Default is 15 ms @ 3.5 MHz.
# At 28 MHz, we multiply with a factor of 8.
define(`KEY_REPEAT_RATE', 120)

# ASCII code associated with CAPS SHIFT + 3 (TRUE VIDEO key).
define(`ASCII_CODE_TRUE_VIDEO', 128)

# ASCII code associated with CAPS SHIFT + 4 (INV VIDEO key).
define(`ASCII_CODE_INV_VIDEO', 129)

# ASCII code associated with CAPS SHIFT + 7 (up arrow key).
define(`ASCII_CODE_UP', 253)

# ASCII code associated with CAPS SHIFT + 6 (down arrow key).
define(`ASCII_CODE_DOWN', 254)

# Text output

# Non-zero to enable Timex hi-res mode for text, default is ULA mode.
ifdef(`USE_TIMEX_HIRES',, `define(`USE_TIMEX_HIRES', 0)')

# Height of text window in characters.
define(`TEXT_WINDOW_HEIGHT', 24)

# FZX font used for text window.
define(`TEXT_FONT', _ff_pd_QLStyle)

# Text font color index (0 - 31), default is 1 (light gray).
define(`TEXT_FONT_COLOR_INDEX', 1)

# Graphics

# Non-zero to enable title and location graphics, default is no graphics.
ifdef(`USE_GFX',, `define(`USE_GFX', 0)')

# Mouse

# Non-zero to enable mouse support, default is no mouse support.
ifdef(`USE_MOUSE',, `define(`USE_MOUSE', 0)')

# Level 9 interpreter

# Non-zero to log interpreter code execution to a file, default is off.
ifdef(`USE_CODEFOLLOW',, `define(`USE_CODEFOLLOW', 0)')

# Image slideshow

# Non-zero to enable image slideshow, default is off.
# Used for debugging and testing.
ifdef(`USE_IMAGE_SLIDESHOW',, `define(`USE_IMAGE_SLIDESHOW', 0)')

################################################################################
# FILE: zconfig.h
################################################################################

ifelse(TARGET, 1,
`
divert
`#ifndef' _ZCONFIG_H
`#define' _ZCONFIG_H

`#define' `ASCII_CODE_TRUE_VIDEO' ASCII_CODE_TRUE_VIDEO
`#define' `ASCII_CODE_INV_VIDEO' ASCII_CODE_INV_VIDEO
`#define' `ASCII_CODE_UP' ASCII_CODE_UP
`#define' `ASCII_CODE_DOWN' ASCII_CODE_DOWN

`#define' `USE_TIMEX_HIRES' USE_TIMEX_HIRES
`#define' `TEXT_WINDOW_HEIGHT' TEXT_WINDOW_HEIGHT
`#define' `TEXT_FONT_COLOR_INDEX' TEXT_FONT_COLOR_INDEX

`#define' `USE_GFX' USE_GFX

`#define' `USE_MOUSE' USE_MOUSE

ifelse(USE_CODEFOLLOW, 0,,
`
`#define' `CODEFOLLOW'
')dnl

`#define' `USE_IMAGE_SLIDESHOW' USE_IMAGE_SLIDESHOW

`#endif'
divert(-1)
')

################################################################################
# FILE: zconfig.inc
################################################################################

ifelse(TARGET, 2,
`
divert
IFNDEF _ZCONFIG_INC
DEFC _ZCONFIG_INC = 1

defc `KEY_REPEAT_RATE' = KEY_REPEAT_RATE
defc `ASCII_CODE_TRUE_VIDEO' = ASCII_CODE_TRUE_VIDEO
defc `ASCII_CODE_INV_VIDEO' = ASCII_CODE_INV_VIDEO
defc `ASCII_CODE_UP' = ASCII_CODE_UP
defc `ASCII_CODE_DOWN' = ASCII_CODE_DOWN

defc `USE_TIMEX_HIRES' = USE_TIMEX_HIRES
defc `TEXT_WINDOW_HEIGHT' = TEXT_WINDOW_HEIGHT

defc `USE_GFX' = USE_GFX

ENDIF
divert(-1)
')

################################################################################
# FILE: zconfig.m4
################################################################################

ifelse(TARGET, 3,
`
divert
`define'(`KEY_REPEAT_START', KEY_REPEAT_START)

`define'(`KEY_REPEAT_RATE', KEY_REPEAT_RATE)

`define'(`USE_TIMEX_HIRES', USE_TIMEX_HIRES)

`define'(`TEXT_WINDOW_HEIGHT', TEXT_WINDOW_HEIGHT)

`define'(`TEXT_FONT', TEXT_FONT)
divert(-1)
')

################################################################################
# FILE: zpragma.inc
################################################################################

ifelse(TARGET, 4,
`
divert
ifelse(USE_TIMEX_HIRES, 0,
`
// Program org for ULA mode.
`#pragma' output CRT_ORG_CODE = 0x6000
',
`
// Program org for Timex hi-res mode.
`#pragma' output CRT_ORG_CODE = 0x7800
')dnl

// Set stack location to top of memory.
`#pragma' output REGISTER_SP = 0

// No malloc heap.
`#pragma' output CLIB_MALLOC_HEAP_SIZE = 0

// No stdio heap.
`#pragma' output CLIB_STDIO_HEAP_SIZE = 0

// Space to register atexit functions.
`#pragma' output CLIB_EXIT_STACK_SIZE = 1

// Reduce the size of printf to what is needed.
`#pragma' printf = "%s %u %X"

// Include crt_driver_instantiation.asm.m4 in driver instantiation section.
`#pragma' output CRT_INCLUDE_DRIVER_INSTANTIATION = 1
divert(-1)
')

################################################################################
# FILE: zproject.lst
################################################################################

ifelse(TARGET, 5,
`
divert
src/level9.c
src/memory_paging.asm
src/main.c
src/sprite.c
src/interrupt.asm
src/in_key_translation_table.asm
src/zx_01_input_kbd_inkey_custom.asm
src/scroll_prompt.asm
src/text_color.asm
ifelse(TEXT_FONT, `_ff_pd_QLStyle',
`
src/_ff_pd_QLStyle.asm
')dnl
ifelse(USE_TIMEX_HIRES, 0,
`
src/zx_01_output_fzx_custom.asm
',
`
src/tshr_01_output_fzx_custom.asm
')dnl
ifelse(USE_GFX, 0,,
`
src/layer2.c
src/image_scroll.asm
')dnl
ifelse(USE_MOUSE, 0,,
`
src/asm_in_mouse_kempston.asm
src/asm_in_mouse_kempston_wheel.asm
src/mouse.c
')dnl
ifelse(USE_IMAGE_SLIDESHOW, 0,,
`
src/image_slideshow.c
')dnl
divert(-1)
')

divert(0)dnl
