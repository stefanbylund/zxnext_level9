; ===============================================================
; 2021
; ===============================================================
;
; uint16_t in_mouse_kempston_wheel(void)
;
; Report position of mouse wheel as a 4-bit unsigned value from
; 0 to 15.
;
; Corrected version of the original Kempston mouse wheel driver
; in z88dk.
;
; ===============================================================

SECTION code_clib
SECTION code_input

PUBLIC asm_in_mouse_kempston_wheel

asm_in_mouse_kempston_wheel:

   ; exit : success
   ;           hl = track wheel position
   ;           carry reset
   ;
   ;        fail
   ;           hl = 0
   ;           carry set
   ;
   ; uses : af, hl

   ld a,$fa
   in a,($df)

   and $f0
   swapnib

   ld h,0
   ld l,a

   ret
