;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stefan Bylund 2021
;;
;; Module for showing/hiding the scroll prompt.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SECTION code_user

EXTERN __IO_SPRITE_SLOT
EXTERN __IO_SPRITE_ATTRIBUTE

defc SCROLL_PROMPT_SPRITE_START_SLOT = 0

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; SHOW_SCROLL_PROMPT
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC show_scroll_prompt

show_scroll_prompt:

   ; Show or hide the scroll prompt.
   ;
   ; enter : l = scroll prompt visibility flag (0 = hidden, 1 = visible)
   ; exit  : none
   ; uses  : af, bc

   di

   ; select sprite slot SCROLL_PROMPT_SPRITE_START_SLOT
   ld a,SCROLL_PROMPT_SPRITE_START_SLOT
   ld bc,__IO_SPRITE_SLOT
   out (c),a

   ; set sprite attributes for first scroll prompt sprite
   ; sprite slot SCROLL_PROMPT_SPRITE_START_SLOT at (x,y) = (33,226)
   ld a,33
   out (__IO_SPRITE_ATTRIBUTE),a  ; x lsb
   ld a,226
   out (__IO_SPRITE_ATTRIBUTE),a  ; y
   xor a
   out (__IO_SPRITE_ATTRIBUTE),a  ; x msb, rotate, mirror, palette offset
   ld a,SCROLL_PROMPT_SPRITE_START_SLOT
   bit 0,l
   jr z, end_1
   set 7,a
end_1:
   out (__IO_SPRITE_ATTRIBUTE),a  ; pattern slot and visibility flag

   ; set sprite attributes for second scroll prompt sprite
   ; sprite slot SCROLL_PROMPT_SPRITE_START_SLOT + 1 at (x,y) = (49,226)
   ld a,49
   out (__IO_SPRITE_ATTRIBUTE),a  ; x lsb
   ld a,226
   out (__IO_SPRITE_ATTRIBUTE),a  ; y
   xor a
   out (__IO_SPRITE_ATTRIBUTE),a  ; x msb, rotate, mirror, palette offset
   ld a,SCROLL_PROMPT_SPRITE_START_SLOT + 1
   bit 0,l
   jr z, end_2
   set 7,a
end_2:
   out (__IO_SPRITE_ATTRIBUTE),a  ; pattern slot and visibility flag

   ei
   ret
