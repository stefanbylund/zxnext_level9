; ===============================================================
; 2021
; ===============================================================
;
; void in_mouse_kempston(uint8_t *buttons, uint16_t *x, uint16_t *y)
;
; Returns mouse coordinates and button state.
;
; Based on the Kempston mouse driver by Chris Cowley (Copyright 2003).
;
; Extended to support Spectrum Next's 320x256 resolution.
;
; Replaces the original Kempston mouse driver in z88dk.
;
; ===============================================================

SECTION code_clib
SECTION code_input

PUBLIC asm_in_mouse_kempston

; uint16_t
EXTERN __input_kempston_mouse_x
EXTERN __input_kempston_mouse_y

; uint8_t
EXTERN __input_kempston_mouse_rawx
EXTERN __input_kempston_mouse_rawy

asm_in_mouse_kempston:

   ; exit  :  a = button state = 0000 0MLR active high
   ;         de = x-coordinate
   ;         bc = y-coordinate
   ;
   ; uses  : af, bc, de, hl

;mouse_x:
   ; Read Kempston x-coordinate
   ld a,(__input_kempston_mouse_rawx)
   ld d,a
   ld bc,64479
   in a,(c)
   ld (__input_kempston_mouse_rawx),a
   sub d                            ; a = signed x-displacement
   sra a                            ; scale it down by a factor of 2
   ld c,a                           ; c = a = scaled and signed x-displacement
   rla                              ; move sign bit of a into carry flag
   sbc a,a                          ; a = 0x00 or 0xFF depending on the carry
   ld b,a                           ; bc = sign extended 16-bit value

   ; Mouse left/right?
   cp $80
   jr nc, moving_left

;moving_right:
   ld hl,(__input_kempston_mouse_x)
   add hl,bc
   ex de,hl                         ; de = new x-coordinate = old x-coordinate + x-displacement
   ld hl,319
   xor a
   sbc hl,de
   jr c, right_edge                 ; if 319 < new x-coordinate then snap to right edge
   ld (__input_kempston_mouse_x),de
   jr mouse_y
right_edge:
   ld hl,319
   ld (__input_kempston_mouse_x),hl
   jr mouse_y

moving_left:
   ld hl,(__input_kempston_mouse_x)
   add hl,bc
   ex de,hl                         ; de = new x-coordinate
   ld hl,(__input_kempston_mouse_x) ; hl = old x-coordinate
   xor a
   sbc hl,de
   jr c, left_edge                  ; if old x-coordinate < new x-coordinate then snap to left edge
   ld (__input_kempston_mouse_x),de
   jr mouse_y
left_edge:
   ld hl,0
   ld (__input_kempston_mouse_x),hl

mouse_y:
   ; Read Kempston y-coordinate
   ; Note: Kempston mouse y-axis is flipped
   ld a,(__input_kempston_mouse_rawy)
   ld d,a
   ld bc,65503
   in a,(c)
   ld (__input_kempston_mouse_rawy),a
   sub d                            ; a = signed y-displacement
   neg                              ; reverse it
   sra a                            ; scale it down by a factor of 2
   ld b,a                           ; b = a = scaled, reversed and signed y-displacement

   ; Mouse up/down?
   cp $80
   jr nc, moving_up

;moving_down:
   ld a,(__input_kempston_mouse_y)
   add a,b                          ; a = new y-coordinate = old y-coordinate + y-displacement
   jr nc, not_bottom_edge           ; if new y-coordinate overflows then snap to bottom edge
;bottom_edge:
   ld a,255
not_bottom_edge:
   ld (__input_kempston_mouse_y),a
   jr mouse_btn

moving_up:
   ld a,(__input_kempston_mouse_y)
   add a,b                          ; a = new y-coordinate
   jr c, not_top_edge               ; if new y-coordinate doesn't overflow then snap to top edge
;top_edge:
   xor a
not_top_edge:
   ld (__input_kempston_mouse_y),a

mouse_btn:
   ; Read Kempston button state
   ld bc,64223
   in a,(c)
   xor $07                          ; Invert button bits
   and $07                          ; Only keep button bits
   ; a = buttons = 0000 0MLR active high

   ; Deliver mouse coordinates
   ld de,(__input_kempston_mouse_x) ; de = new x-coordinate
   ld bc,(__input_kempston_mouse_y) ; bc = new y-coordinate
   ret
