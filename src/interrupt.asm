;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stefan Bylund 2021
;;
;; Put Z80 in IM2 mode with a 257-byte interrupt vector table located at 0xFE00
;; (after the program) filled with 0xFD bytes. Install an empty interrupt
;; service routine at the ISR entry at address 0xFDFD, which aligns perfectly
;; below the interrupt vector table.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; create 257-byte im2 vector table

SECTION VECTOR_TABLE
org 0xFE00

defs 257, 0xFD

; create empty interrupt service routine
; real isr can be installed later

SECTION VECTOR_TABLE_JP
org 0xFDFD

ei
reti

; initialize im2 mode inside crt before main is called
; leave interrupts disabled to allow the program to enable them when ready

SECTION code_crt_init

EXTERN __VECTOR_TABLE_head

ld a,__VECTOR_TABLE_head / 256
ld i,a
im 2
