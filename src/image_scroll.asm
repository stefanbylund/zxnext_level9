;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Stefan Bylund 2021
;;
;; Image scrolling functions. Only compiled if USE_GFX = 1.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

INCLUDE "zconfig.inc"

SECTION code_user

EXTERN __IO_LAYER_2_CONFIG
EXTERN __IL2C_SHOW_LAYER_2
EXTERN __REG_CLIP_WINDOW_LAYER_2
EXTERN __REG_LAYER_2_OFFSET_Y
EXTERN IOCTL_OTERM_SCROLL_LIMIT
EXTERN asm_ioctl

EXTERN _gfx_on
EXTERN _max_image_height
EXTERN _gfx_window_height

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _IMAGE_TEXT_HEIGHT_IN_CHARS
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC _image_text_height_in_chars

_image_text_height_in_chars:

   ; Return image height in text window in number of characters.
   ;
   ; enter : none
   ; exit  : l = a = image height in text window in number of characters
   ; uses  : af, l

   ld a,(_gfx_window_height)
   cp 32
   jr nc, image_in_text_window ; is image in text window?
   xor a                       ; a = image height in text window = 0
   ld l,a
   ret

image_in_text_window:
   sub 32                      ; remove top border from image height
   add a,7
   rra
   rra
   rra
   and $1f                     ; a = image height in text window in characters
   ld l,a
   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _IMAGE_MOUSE_SCROLL
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC _image_mouse_scroll

_image_mouse_scroll:

   ; Scroll image up/down by the given amount (provided by the mouse).
   ; Updates the _gfx_window_height variable.
   ;
   ; enter : l = image height change (positive if increased, negative if decreased)
   ; exit  : none
   ; uses  : af, bc, de, hl

   ld a,(_gfx_on)
   or a
   ret z                       ; return if graphics is off

   ld a,(_max_image_height)
   inc a
   ld c,a                      ; c = max_image_height + 1

   ; clip image height change if new image height would become < 0 or > max_image_height
   ld a,(_gfx_window_height)
   ld b,a                      ; b = image height
   add a,l                     ; a = new image height = image height + image height change
   bit 7,l
   jr z, positive_change
negative_change:
   ; is new image height < 0 (i.e. larger than 127 in 8-bit unsigned arithmetics)?
   bit 7,a
   jr z, valid_change
   ; special care if new image height is in range [128,max_image_height], which is ok
   cp c
   jr c, valid_change
   ; set image height change = -(image height)
   ld a,b
   neg
   ld l,a                      ; l = clipped image height change
   jr valid_change
positive_change:
   ; is new image height > max_image_height ?
   cp c
   jr c, valid_change
   ; set image height change = max_image_height - image height
   ld a,(_max_image_height)
   sub b
   ld l,a                      ; l = clipped image height change

valid_change:
   ld a,(_gfx_window_height)
   ld e,a                      ; e = old window height
   add a,l                     ; a = new window height

   ; hide layer 2 screen if new window height is 0
   or a
   jr nz,show_layer2_screen
   ld bc,__IO_LAYER_2_CONFIG
   out (c),0
   jr adjust_clip_window

show_layer2_screen:
   ; show layer 2 screen if new window height is growing from 0
   ld b,a
   ld a,e
   or a
   ld a,b
   jr nz,adjust_clip_window
   ld h,__IL2C_SHOW_LAYER_2
   ld bc,__IO_LAYER_2_CONFIG
   out (c),h
   jr adjust_clip_window

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; _IMAGE_KEY_SCROLL
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

PUBLIC _image_key_scroll

_image_key_scroll:

   ; Scroll image up/down if up/down arrow key is pressed.
   ; Updates the _gfx_window_height variable.
   ;
   ; enter : l = ascii code
   ; exit  : carry reset if up/down arrow keypress detected
   ; uses  : af, bc, de, hl

   ld a,l
   cp ASCII_CODE_UP
   jr z, image_smaller

   cp ASCII_CODE_DOWN
   jr z, image_bigger

   scf
   ret

image_smaller:

   ld a,(_gfx_on)
   or a
   ret z                       ; if graphics is off, just reject the character

   ld a,(_gfx_window_height)
   dec a                       ; window height decreases by 1

   ld d,0
   jr z, config_layer2_screen  ; hide layer 2 screen when window height decrements to 0

   cp 0xff
   jr nz, adjust_clip_window

   xor a                       ; window height is 0
   jr config_layer2_screen     ; hide layer 2 screen when window height is 0

image_bigger:

   ld a,(_gfx_on)
   or a
   ret z                       ; if graphics is off, just reject the character

   ld a,(_max_image_height)
   inc a
   ld b,a                      ; b = max_image_height + 1

   ld a,(_gfx_window_height)
   inc a                       ; window height increases by 1

   cp 1
   ld d,__IL2C_SHOW_LAYER_2
   jr z, config_layer2_screen  ; show layer 2 screen when window height increments from 0

   cp b
   jr c, adjust_clip_window

   ld a,(_max_image_height)    ; window height is max_image_height
   jr adjust_clip_window

config_layer2_screen:

   ; d = layer 2 screen config value

   ld bc,__IO_LAYER_2_CONFIG
   out (c),d

   ; continue to adjust_clip_window below

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; ADJUST_CLIP_WINDOW
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

adjust_clip_window:

   ; Adjust the layer 2 clip window height, scroll the layer 2 screen vertically
   ; and update the scroll limit of the output terminal.
   ;
   ; Updates the _gfx_window_height variable.
   ;
   ; enter : a = new window height
   ; exit  : none
   ; uses  : af, bc, hl

   ld (_gfx_window_height),a

   ; set new layer 2 clip window height

   ld b,a                      ; b = stored window height
   or a
   jr z, set_clip_reg          ; y2 = 0
   dec a                       ; y2 = window height - 1
set_clip_reg:
   nextreg __REG_CLIP_WINDOW_LAYER_2,0    ; x1
   nextreg __REG_CLIP_WINDOW_LAYER_2,255  ; x2
   nextreg __REG_CLIP_WINDOW_LAYER_2,0    ; y1
   nextreg __REG_CLIP_WINDOW_LAYER_2,a    ; y2

   ; scroll the layer 2 screen vertically so that the bottom-most part of its
   ; contents is always visible in the bottom part of the layer 2 clip window

   ld a,(_max_image_height)
   sub b
   nextreg __REG_LAYER_2_OFFSET_Y,a  ; max_image_height - height

   ; update the scroll limit of the output terminal

IF __SDCC_IY
   push iy
   call update_scroll_limit
   pop iy
ELSE
   push ix
   call update_scroll_limit
   pop ix
ENDIF

   or a                        ; reject the character
   ret

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; UPDATE_SCROLL_LIMIT
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

update_scroll_limit:

   ; Update the scroll limit of the output terminal.
   ; Note that the _gfx_window_height variable has
   ; already been updated when this function is called.
   ;
   ; enter : none
   ; exit  : none
   ; uses  : af, bc, hl

   call _image_text_height_in_chars ; a = image height in text window in characters
   sub TEXT_WINDOW_HEIGHT
   neg                         ; a = new_scroll_limit = TEXT_WINDOW_HEIGHT - image_text_height_in_chars

   ; ioctl(1, IOCTL_OTERM_SCROLL_LIMIT, new_scroll_limit)
IF __SDCC
   ld h,0
   ld l,a
   push hl
   ld hl,IOCTL_OTERM_SCROLL_LIMIT
   push hl
   ld hl,1
   push hl
ELSE
   ld hl,1
   push hl
   ld hl,IOCTL_OTERM_SCROLL_LIMIT
   push hl
   ld h,0
   ld l,a
   push hl
ENDIF
   call asm_ioctl
   pop af
   pop af
   pop af

   ret
