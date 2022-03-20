;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; ZX_01_OUTPUT_FZX_CUSTOM
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This output terminal driver subclasses the zx_01_output_fzx
; driver.
;
; This driver overrides the scroll pause handling by showing a
; scroll prompt, waiting for a key press and finally hiding the
; scroll prompt. The scroll prompt is displayed using hardware
; sprites. The driver handles CAPS+6 and CAPS+7 key presses
; during the scroll pause period (which is otherwise handled by
; the input terminal driver) for increasing or decreasing the
; graphics window height.
;
; The driver also overrides the scroll limit computation so
; that the max scroll is reduced when graphics are present on
; screen.
;
; The driver also suppresses echoing of newline from the
; connected input terminal and sets the cursor to '_'  even if
; Caps Lock is on.
;
; The driver also translates tabs to spaces when printing on
; the output terminal.
;
; ;;;;;;;;;;;;;;;;;;;;
; DRIVER CLASS DIAGRAM
; ;;;;;;;;;;;;;;;;;;;;
;
; CONSOLE_01_OUTPUT_TERMINAL (root, abstract)
; CONSOLE_01_OUTPUT_TERMINAL_FZX (abstract)
; ZX_01_OUTPUT_FZX (concrete)
; ZX_01_OUTPUT_FZX_CUSTOM (concrete)

INCLUDE "zconfig.inc"

SECTION code_driver
SECTION code_driver_terminal_output

PUBLIC zx_01_output_fzx_custom

EXTERN zx_01_output_fzx
EXTERN console_01_output_fzx_oterm_msg_putc
EXTERN console_01_output_fzx_iterm_msg_putc
EXTERN console_01_output_fzx_iterm_msg_print_cursor
EXTERN asm_in_wait_nokey
EXTERN asm_in_inkey
EXTERN asm_z80_delay_ms

EXTERN _image_key_scroll
EXTERN _image_text_height_in_chars
EXTERN show_scroll_prompt

EXTERN OTERM_MSG_PUTC
EXTERN OTERM_MSG_PAUSE
EXTERN OTERM_MSG_SCROLL_LIMIT
EXTERN ITERM_MSG_READLINE_SCROLL_LIMIT
EXTERN ITERM_MSG_PUTC
EXTERN ITERM_MSG_PRINT_CURSOR

defc ASCII_CODE_TAB = 9
defc ASCII_CODE_LF = 10
defc ASCII_CODE_SPACE = 32

zx_01_output_fzx_custom:

   cp OTERM_MSG_PUTC
   jp z, oterm_msg_putc

   cp OTERM_MSG_SCROLL_LIMIT
   jr z, oterm_msg_scroll_limit

   cp ITERM_MSG_READLINE_SCROLL_LIMIT
   jr z, iterm_msg_readline_scroll_limit

   cp ITERM_MSG_PUTC
   jr z, iterm_msg_putc

   cp ITERM_MSG_PRINT_CURSOR
   jr z, iterm_msg_print_cursor

   cp OTERM_MSG_PAUSE
   ; let parent class handle other messages
   jp nz, zx_01_output_fzx

oterm_msg_pause:

   ; OTERM_MSG_PAUSE:
   ; The scroll count has reached zero so the driver should pause the output somehow.
   ;
   ; can use: af, bc, de, hl

   ; show the scroll prompt
   ld l,1
   call show_scroll_prompt

   ; wait for a key press and allow image scrolling during the scroll pause period

   call asm_in_wait_nokey

wait:

   call asm_in_inkey
   ld a,l                      ; a = ascii code keypress

   or a
   jr z, wait                  ; if no keypress

IF USE_GFX

   call _image_key_scroll      ; image scroll for up/down
   jr c, wait_done             ; if up/down not detected

   ld hl,KEY_REPEAT_RATE
   call asm_z80_delay_ms       ; scroll speed kept consistent with scrolling in input terminal

   jr wait

ENDIF

wait_done:

   call asm_in_wait_nokey

   ; hide the scroll prompt
   ld l,0
   call show_scroll_prompt

   ret

oterm_msg_putc:

   ; OTERM_MSG_PUTC:
   ;
   ; enter  :  c = char to output
   ; can use:  af, bc, de, hl
   ;
   ; Output given character and translate tab to space (otherwise '?' is printed).

   ld a,c
   cp ASCII_CODE_TAB
   jr nz, not_tab
   ld c,ASCII_CODE_SPACE
not_tab:
   jp console_01_output_fzx_oterm_msg_putc

oterm_msg_scroll_limit:

   ; OTERM_MSG_SCROLL_LIMIT (optional):
   ;
   ; enter  :  c = default
   ; exit   :  c = maximum scroll amount
   ; can use:  af, bc, de, hl
   ;
   ; Scroll has just paused. Return number of scrolls until next pause.
   ; Default is window height.

IF USE_GFX

   call _image_text_height_in_chars ; a = image height in text window in characters

   sub TEXT_WINDOW_HEIGHT
   neg                              ; a = TEXT_WINDOW_HEIGHT - image_text_height_in_chars

   ld c,a

ENDIF

   ret

iterm_msg_readline_scroll_limit:

   ; ITERM_MSG_READLINE_SCROLL_LIMIT (optional):
   ;
   ; enter  :  c = default
   ; exit   :  c = number of rows to scroll before pause
   ; can use:  af, bc, de, hl
   ;
   ; Return number of scrolls allowed before pause after
   ; a readline operation ends. Default is current y + 1.

IF USE_GFX

   call _image_text_height_in_chars ; a = image height in text window in characters

   ; Note: This assumes that the cursor position is never behind the image.
   sub c
   neg                              ; a = (y + 1) - image_text_height_in_chars

   ld c,a

ENDIF

   ret

iterm_msg_putc:

   ; ITERM_MSG_PUTC:
   ;
   ; enter  :  c = char to output
   ; can use:  af, bc, de, hl
   ;
   ; Output given character if not newline.

   ld a,c
   cp ASCII_CODE_LF
   call nz, console_01_output_fzx_iterm_msg_putc
   ret

iterm_msg_print_cursor:

   ; ITERM_MSG_PRINT_CURSOR:
   ;
   ; enter  :  c = cursor char (CHAR_CURSOR_LC or CHAR_CURSOR_UC)
   ; can use:  af, bc, de, hl, ix
   ;
   ; Input terminal is printing the given cursor.
   ; Set cursor to '_' (CHAR_CURSOR_LC) even if caps lock is on.

   ld c,'_'
   jp console_01_output_fzx_iterm_msg_print_cursor
