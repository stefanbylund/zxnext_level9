;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ZX_01_INPUT_KBD_INKEY_CUSTOM
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;
; This input terminal driver subclasses the zx_01_input_kbd_inkey
; driver. It intercepts the ITERM_MSG_REJECT message to pick up
; CAPS+6 (down) and CAPS+7 (up) typed on the input line. These
; are used to increase or decrease the graphics window height
; by one pixel if graphics is enabled. CAPS+5 (left) and CAPS+8
; (right) are rejected. Allows enter at end of input when buffer
; is full.
;
; Intercepts the ITERM_MSG_GETC message to detect if EDIT (CAPS+1)
; is typed on the input line. If EDIT is pressed, the input
; terminal is cleared (in preparation for the calling program
; to insert from history buffer) and the edit_pressed variable
; is set and getc() is made to return error.
;
; The colour of the text can be changed by cycling downwards or
; upwards through a palette of 32 colours by pressing TRUE_VIDEO
; (CAPS+3) and INV_VIDEO (CAPS+4), respectively.
;
; ;;;;;;;;;;;;;;;;;;;;
; DRIVER CLASS DIAGRAM
; ;;;;;;;;;;;;;;;;;;;;
;
; CONSOLE_01_INPUT_TERMINAL (root, abstract)
; ZX_01_INPUT_KBD_INKEY (concrete)
; ZX_01_INPUT_KBD_INKEY_CUSTOM (concrete)

INCLUDE "zconfig.inc"

SECTION code_driver
SECTION code_driver_terminal_input

PUBLIC zx_01_input_kbd_inkey_custom

EXTERN ITERM_MSG_REJECT
EXTERN ITERM_MSG_GETC
EXTERN ITERM_MSG_PUTC
EXTERN ITERM_MSG_ERASE_CURSOR

EXTERN zx_01_input_kbd_inkey
EXTERN zx_01_input_inkey_iterm_msg_getc
EXTERN l_jpix
EXTERN _image_key_scroll
EXTERN _cycle_text_color

EXTERN _history_buffer_size
EXTERN _edit_pressed

defc ASCII_CODE_EDIT = 7
defc ASCII_CODE_LEFT = 8
defc ASCII_CODE_RIGHT = 9

zx_01_input_kbd_inkey_custom:

   cp ITERM_MSG_GETC
   jp z, iterm_msg_getc

   ; let parent class handle other messages
   cp ITERM_MSG_REJECT
   jp nz, zx_01_input_kbd_inkey

iterm_msg_reject:

   ; ITERM_MSG_REJECT:
   ; Indicate whether typed character should be rejected.
   ;
   ; enter   : ix = FDSTRUCT.JP *input_terminal
   ;           c = ascii code
   ; exit    : carry reset indicates the character should be rejected
   ; can use : af, bc, de, hl

   ; unconditionally accept enter
   ld a,c
   cp '\n'
   scf
   ret z

   ; stop at next-to-last index in input buffer to always allow a final enter
   ; hl = input buffer size prior to accepting/rejecting this character (i.e.
   ;      index of this character), offset 21 in FDSTRUCT.JP
   ; de = input buffer capacity, offset 23 in FDSTRUCT.JP
IF __SDCC_IY
   ld l,(iy+21)
   ld h,(iy+22)
   ld e,(iy+23)
   ld d,(iy+24)
ELSE
   ld l,(ix+21)
   ld h,(ix+22)
   ld e,(ix+23)
   ld d,(ix+24)
ENDIF
   dec de
   dec de ; de = index of next-to-last character in input buffer
   xor a  ; clear carry flag
   sbc hl,de
   jr nz, continue
   xor a
   ret

continue:
   xor a
   ld a,c

   cp ASCII_CODE_TRUE_VIDEO
   ld l,0
   jp z, _cycle_text_color

   cp ASCII_CODE_INV_VIDEO
   ld l,1
   jp z, _cycle_text_color

   cp ASCII_CODE_EDIT
   ret z

   cp ASCII_CODE_LEFT
   ret z

   cp ASCII_CODE_RIGHT
   ret z

IF USE_GFX = 0
   cp ASCII_CODE_UP
   ret z

   cp ASCII_CODE_DOWN
   ret z

   scf
   ret
ELSE
   ld l,a
   jp _image_key_scroll         ; sets carry flag correctly for up/down rejection
ENDIF

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ITERM_MSG_GETC
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

iterm_msg_getc:

   ; ITERM_MSG_GETC:
   ; Read the keyboard device and return the character read.
   ;
   ; enter   : ix = FDSTRUCT.JP *input_terminal
   ; exit    : a = keyboard character after character set translation
   ;           carry set on error, hl = 0 (stream error) or -1 (eof)
   ; can use : af, bc, de, hl

   ; call parent
   call zx_01_input_inkey_iterm_msg_getc
   ; a = ascii code
   ret c                       ; return if error
   cp ASCII_CODE_EDIT
   jr z, history_check
   or a                        ; clear carry flag
   ret

history_check:
   ; check if history buffer is empty
   ld b,a
   ld de,(_history_buffer_size)
   ld a,d
   or e
   jr nz, signal_edit_pressed
   ld a,b
   or a                        ; clear carry flag
   ret

signal_edit_pressed:
   ; clear input terminal in preparation for calling program to insert history buffer
   call clear_input_terminal

   ; signal that EDIT was pressed by setting edit_pressed variable and returning error
   ld a,1
   ld (_edit_pressed),a
   ld hl,0
   scf
   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; CLEAR_INPUT_TERMINAL
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

clear_input_terminal:

   ; Clear the input terminal; delete any typed characters on its connected
   ; output terminal (an fzx output terminal) and empty the input terminal's
   ; edit buffer.
   ;
   ; enter   : ix = FDSTRUCT.JP *input_terminal
   ; exit    : none
   ; can use : af, bc, de, hl

IF __SDCC_IY
   ld c,(iy+14)
   ld b,(iy+15)                ; bc = FDSTRUCT *output_terminal
ELSE
   ld c,(ix+14)
   ld b,(ix+15)                ; bc = FDSTRUCT *output_terminal
ENDIF

   ; return if no output terminal is connected
   ld a,b
   or c
   ret z

IF __SDCC_IY
   ld e,(iy+21)
   ld d,(iy+22)                ; de = edit buffer size
ELSE
   ld e,(ix+21)
   ld d,(ix+22)                ; de = edit buffer size
ENDIF

   ; return if edit buffer is empty
   ld a,d
   or e
   ret z

IF __SDCC_IY
   ld l,(iy+19)
   ld h,(iy+20)                ; hl = edit buffer data address

   push iy                     ; save FDSTRUCT *input_terminal
   push de                     ; save edit buffer size
   push hl                     ; save edit buffer data address

   push bc
   pop iy                      ; ix = FDSTRUCT *output_terminal
ELSE
   ld l,(ix+19)
   ld h,(ix+20)                ; hl = edit buffer data address

   push ix                     ; save FDSTRUCT *input_terminal
   push de                     ; save edit buffer size
   push hl                     ; save edit buffer data address

   push bc
   pop ix                      ; ix = FDSTRUCT *output_terminal
ENDIF

   ; ix now points to the output terminal

   ; erase the cursor character in the output terminal (not stored in the edit buffer)
   pop de                      ; de = edit buffer data address
   pop bc                      ; bc = edit buffer size
   push bc                     ; save edit buffer size
   push de                     ; save edit buffer data address
   ld a,ITERM_MSG_ERASE_CURSOR
   call l_jpix                 ; erase the cursor

   ; reset cursor position in output terminal to start of edit buffer's display location
   call reset_output_terminal_cursor

   pop hl                      ; hl = edit buffer data address
   pop de                      ; de = edit buffer size
   push hl                     ; save edit buffer data address

   ; print all characters in edit buffer from the start of its display location
   ; in the output terminal; since drawing is done in XOR mode, this will delete
   ; all typed characters by overwriting them
print_loop:
   push de
   push hl
   ld c,(hl)                   ; c = char to overwrite
   ld a,ITERM_MSG_PUTC
   call l_jpix                 ; print the char
   pop hl
   pop de
   inc hl                      ; goto next char
   dec de                      ; decrement edit buffer count
   ld a,d
   or e
   jr nz, print_loop

   ; reset cursor position in output terminal to start of edit buffer's display location
   call reset_output_terminal_cursor

   ; erase the cursor character in the output terminal (not stored in the edit buffer)
   pop de                      ; de = edit buffer data address
   ld bc,0                     ; bc = edit buffer size
   ld a,ITERM_MSG_ERASE_CURSOR
   call l_jpix                 ; erase the cursor

IF __SDCC_IY
   pop iy                      ; ix = FDSTRUCT *input_terminal

   ; set input terminal's edit buffer size to 0
   ld a,0
   ld (iy+21),a
   ld (iy+22),a
ELSE
   pop ix                      ; ix = FDSTRUCT *input_terminal

   ; set input terminal's edit buffer size to 0
   ld a,0
   ld (ix+21),a
   ld (ix+22),a
ENDIF

   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; RESET_OUTPUT_TERMINAL_CURSOR
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

reset_output_terminal_cursor:

   ; Reset cursor position in output terminal to start of edit buffer's display
   ; location.
   ;
   ; enter : ix = FDSTRUCT.JP *output_terminal
   ; exit  : none
   ; uses  : hl

   ; offsets in FDSTRUCT *output_terminal must be +3 when accessed via input terminal

   ; copy edit buffer x,y into current x,y
IF __SDCC_IY
   ld l,(iy+25+3)
   ld h,(iy+26+3)
   ld (iy+35+3),l
   ld (iy+36+3),h              ; x = edit_x

   ld l,(iy+27+3)
   ld h,(iy+28+3)
   ld (iy+37+3),l
   ld (iy+38+3),h              ; y = edit_y
ELSE
   ld l,(ix+25+3)
   ld h,(ix+26+3)
   ld (ix+35+3),l
   ld (ix+36+3),h              ; x = edit_x

   ld l,(ix+27+3)
   ld h,(ix+28+3)
   ld (ix+37+3),l
   ld (ix+38+3),h              ; y = edit_y
ENDIF

   ret
