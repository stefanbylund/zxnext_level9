   ;# DRIVER INSTANTIATION
   ;#
   ;# Sets up an input terminal (fd=0, stdin) and two output terminals; one for
   ;# stdout (fd=1) and a duplicate for stderr (fd=2). The stdin terminal is
   ;# connected to the stdout terminal.
   ;#
   ;# The stdout terminal is an FZX terminal that uses either standard ULA mode
   ;# (256 x 192) or Timex hi-res mode (512 x 192). By default, it is displayed
   ;# on the whole screen with an adjustable scroll limit to accomodate any
   ;# graphics above it. The cursor position of the stdout terminal is set to
   ;# the bottom row to avoid the cursor from ever being located behind the
   ;# graphics.

divert(-1)
   include(`zconfig.m4')

   ifelse(USE_TIMEX_HIRES, 0, `define(`SCREEN_MODE', 0)', `define(`SCREEN_MODE', 1)')
   define(`WINDOW_HEIGHT', TEXT_WINDOW_HEIGHT)
   define(`WINDOW_Y', eval(24 - WINDOW_HEIGHT))
   define(`PAPER_HEIGHT', eval(WINDOW_HEIGHT * 8))
   define(`PAPER_Y', eval(192 - PAPER_HEIGHT))
   define(`CURSOR_Y', eval(PAPER_HEIGHT - 8))
divert

   EXTERN TEXT_FONT

   ;# Input terminal for stdin (fd=0) with 128 characters edit buffer
   include(`src/zx_01_input_kbd_inkey_custom.m4')dnl
   m4_zx_01_input_kbd_inkey_custom(_stdin, __i_fcntl_fdstruct_1, CRT_ITERM_TERMINAL_FLAGS, 128, CRT_ITERM_INKEY_DEBOUNCE, KEY_REPEAT_START, KEY_REPEAT_RATE)dnl

ifelse(SCREEN_MODE, 0,
`
   ;# ULA stdout terminal (fd=1)
   include(`src/zx_01_output_fzx_custom.m4')dnl
   m4_zx_01_output_fzx_custom(_stdout, 0x2070, 0, CURSOR_Y, 0, 32, WINDOW_Y, WINDOW_HEIGHT, WINDOW_HEIGHT, TEXT_FONT, 0x47, 0, 0x47, 0, 256, PAPER_Y, PAPER_HEIGHT, M4__CRT_OTERM_FZX_DRAW_MODE, CRT_OTERM_FZX_LINE_SPACING, CRT_OTERM_FZX_LEFT_MARGIN, CRT_OTERM_FZX_SPACE_EXPAND)dnl
')dnl

ifelse(SCREEN_MODE, 1,
`
   ;# Timex hi-res stdout terminal (fd=1)
   include(`src/tshr_01_output_fzx_custom.m4')dnl
   m4_tshr_01_output_fzx_custom(_stdout, 0x2070, 0, CURSOR_Y, 0, 64, WINDOW_Y, WINDOW_HEIGHT, WINDOW_HEIGHT, TEXT_FONT, 0, 512, PAPER_Y, PAPER_HEIGHT, M4__CRT_OTERM_FZX_DRAW_MODE, CRT_OTERM_FZX_LINE_SPACING, CRT_OTERM_FZX_LEFT_MARGIN, CRT_OTERM_FZX_SPACE_EXPAND)dnl
')dnl

   ;# Output terminal for stderr (fd=2), duplicate of stdout terminal
   include(`../m4_file_dup.m4')dnl
   m4_file_dup(_stderr, 0x80, __i_fcntl_fdstruct_1)dnl
