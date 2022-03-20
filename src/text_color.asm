;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stefan Bylund 2021
;;
;; Support for setting the initial text color and changing the text color by
;; cycling upwards or downwards through a palette of 32 RGB332 colors.
;; The default text color (light gray) is a 9-bit RGB333 color. It is imho the
;; best neutral text color on a black background which justifies the special
;; treatment.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SECTION code_user

EXTERN __IO_NEXTREG_REG
EXTERN __REG_PALETTE_CONTROL
EXTERN __REG_PALETTE_INDEX
EXTERN __REG_PALETTE_VALUE_8
EXTERN __REG_PALETTE_VALUE_16

DEFC TEXT_PALETTE_SIZE = 32
DEFC DEFAULT_TEXT_PALETTE_INDEX = 1

; RGB333 color 110 110 110 = RGB332 0xDB and B1 0x00
DEFC DEFAULT_TEXT_COLOR_RGB332 = 0xDB
DEFC DEFAULT_TEXT_COLOR_B1 = 0x00

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _SET_INITIAL_TEXT_COLOR
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC _set_initial_text_color

_set_initial_text_color:

   ; Set the initial text color from the given text palette index (0 - 31).
   ;
   ; enter : l = text palette index
   ; exit  : carry reset
   ; uses  : af, bc, hl

   ld a,l
   cp TEXT_PALETTE_SIZE
   jr c, valid_text_palette_index
   xor a
valid_text_palette_index:
   ld (text_palette_index),a
   jr set_text_color

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _CYCLE_TEXT_COLOR
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC _cycle_text_color

_cycle_text_color:

   ; Cycle the text color in the given direction (up or down).
   ;
   ; enter : l = direction (1 = up, 0 = down)
   ; exit  : carry reset
   ; uses  : af, bc, hl

   ; check direction
   bit 0,l
   ld hl,text_palette_index     ; hl = address of text_palette_index variable
   jr z, decrement_text_palette_index

   ; increment text palette index
   inc (hl)
   ld a,(hl)
   cp TEXT_PALETTE_SIZE
   jr nz, set_text_color
   ld (hl),0
   jr set_text_color

decrement_text_palette_index:
   dec (hl)
   ld a,(hl)
   cp 0xff
   jr nz, set_text_color
   ld (hl),TEXT_PALETTE_SIZE-1

set_text_color:
   ; update text color at index 15 in the primary ula
   ; palette while preserving the palette control register
   ; note: default text color is 9-bit while the others are 8-bit
   ld a,__REG_PALETTE_CONTROL
   ld bc,__IO_NEXTREG_REG
   out (c),a
   inc b
   in a,(c)
   and 0x8f
   nextreg __REG_PALETTE_CONTROL,a
   nextreg __REG_PALETTE_INDEX,15
   ld hl,text_palette
   ld a,(text_palette_index)
   ; check if default text color
   cp DEFAULT_TEXT_PALETTE_INDEX
   jr z, set_default_text_color
   ; not default text color
   add hl,a
   ld a,(hl)
   nextreg __REG_PALETTE_VALUE_8,a
   or a                        ; clear carry flag
   ret

set_default_text_color:
   nextreg __REG_PALETTE_VALUE_16,DEFAULT_TEXT_COLOR_RGB332
   nextreg __REG_PALETTE_VALUE_16,DEFAULT_TEXT_COLOR_B1
   or a                        ; clear carry flag
   ret

SECTION data_user

text_palette_index:
   DEFB 0

SECTION rodata_user

text_palette:
   ; white/gray
   DEFB 0xFF ; 255
   DEFB 0xDB ; 219 DEFAULT_TEXT_COLOR
   DEFB 0xB6 ; 182
   DEFB 0x92 ; 146
   ; yellow
   DEFB 0xFE ; 254
   DEFB 0xFD ; 253
   DEFB 0xFC ; 252
   ; green
   DEFB 0x1C ; 28
   DEFB 0x18 ; 24
   DEFB 0x14 ; 20
   ; cyan
   DEFB 0xBF ; 191
   DEFB 0x9F ; 159
   DEFB 0x1F ; 31
   ; red
   DEFB 0xE0 ; 224
   DEFB 0xC0 ; 192
   DEFB 0xA4 ; 164
   ; magenta
   DEFB 0xF3 ; 243
   DEFB 0xEF ; 239
   DEFB 0xE7 ; 231
   ; blue
   DEFB 0x17 ; 23
   DEFB 0x13 ; 19
   DEFB 0x0F ; 15
   ; earth
   DEFB 0xFA ; 250
   DEFB 0xF6 ; 246
   DEFB 0xF5 ; 245
   DEFB 0xF4 ; 244
   DEFB 0xF1 ; 241
   DEFB 0xD6 ; 214
   DEFB 0xD5 ; 213
   DEFB 0xD1 ; 209
   DEFB 0xD0 ; 208
   DEFB 0xB1 ; 177
