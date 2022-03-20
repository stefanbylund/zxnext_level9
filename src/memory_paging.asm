;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stefan Bylund 2021
;;
;; This module contains functions for memory paging in the Level 9 interpreter.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SECTION code_user

PUBLIC _page_in_rom
PUBLIC _page_in_game
PUBLIC _effective
PUBLIC _effective_ram_save

defc MEMORY_BASE_PAGE = 40
defc RAM_SAVE_BASE_PAGE = 36

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _PAGE_IN_ROM
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_page_in_rom:

   ; void page_in_rom(void);
   ;
   ; enter : none
   ; exit  : none
   ; uses  : af

   ld a,255
; ZXN_WRITE_MMU0(255);
   mmu0 a
; ZXN_WRITE_MMU1(255);
   mmu1 a
   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _PAGE_IN_GAME
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_page_in_game:

   ; void page_in_game(void);
   ;
   ; enter : none
   ; exit  : none
   ; uses  : af

   ld a,(_current_page)
; ZXN_WRITE_MMU0(current_page);
   mmu0 a
; ZXN_WRITE_MMU1(current_page + 1);
   inc a
   mmu1 a
   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _EFFECTIVE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_effective:

   ; uint8_t *effective(uint16_t ptr) __z88dk_fastcall;
   ;
   ; enter : hl = virtual pointer
   ; exit  : hl = effective pointer
   ; uses  : af, de, hl

	ex de,hl                   ; de = ptr

; uint8_t page = (uint8_t) (ptr / 0x2000);
	ld a,d
	rlca
	rlca
	rlca
	and a,0x07                 ; a = page

; uint8_t new_page = MEMORY_BASE_PAGE + page;
    add a,MEMORY_BASE_PAGE
    ld h,a                     ; h = new_page

; uint16_t addr = ptr % 0x2000;
    ld a,d
    and a,0x1F
    ld d,a                     ; de = addr

; if (current_page != new_page)
	ld a,(_current_page)
	cp a,h
	jr z,effective_end
; current_page = new_page;
	ld a,h
    ld (_current_page),a
; ZXN_WRITE_MMU0(current_page);
	mmu0 a
; ZXN_WRITE_MMU1(current_page + 1);
	inc a
	mmu1 a
; end-if

effective_end:
; return (uint8_t *) addr;
	ex de,hl
	ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _EFFECTIVE_RAM_SAVE
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

_effective_ram_save:

   ; uint8_t *effective_ram_save(uint8_t slot) __z88dk_fastcall;
   ;
   ; enter : l = slot
   ; exit  : hl = effective pointer
   ; uses  : af, de, hl

; uint16_t slot_addr = slot * sizeof(save_struct_t); // slot * 2560
; Note: The code below is tailored for this particular multiplication.
    ld h,0                     ; hl = slot
    ld e,l
    ld d,h
    add hl,hl
    add hl,hl
    add hl,de
    ld h,l
    ld l,0
    add hl,hl
    ex de,hl                   ; de = slot_addr

; uint8_t page = (uint8_t) (slot_addr / 0x2000);
    ld a,d
    rlca
    rlca
    rlca
    and a,0x07                 ; a = page

; uint8_t new_page = RAM_SAVE_BASE_PAGE + page;
    add a,RAM_SAVE_BASE_PAGE
    ld h,a                     ; h = new_page

; uint16_t addr = slot_addr % 0x2000;
    ld a,d
    and a,0x1F
    ld d,a                     ; de = addr

; if (current_page != new_page)
    ld a,(_current_page)
    cp a,h
    jr z,effective_ram_save_end
; current_page = new_page;
    ld a,h
    ld (_current_page),a
; ZXN_WRITE_MMU0(current_page);
    mmu0 a
; ZXN_WRITE_MMU1(current_page + 1);
    inc a
    mmu1 a
; end-if

effective_ram_save_end:
; return (uint8_t *) addr;
    ex de,hl
    ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; DATA
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SECTION data_user

PUBLIC _current_page

_current_page:
   DEFB 0
