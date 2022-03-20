;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Phoebus Dokos 2018
;;
;; Proportional 7pt FZX font inspired by the Sinclair QL system font.
;;
;; Note: The Level 9 interpreter uses standard ASCII so the ZX Spectrum
;; character codes 94 and 96 representing up-arrow and £ are replaced with
;; their ASCII counterparts ^ and BACKQUOTE.
;;
;; There are two versions of the font: One for the standard 256 x 192 pixel mode
;; and another that has been adapted for the Timex hi-res 512 x 192 pixel mode.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

INCLUDE "zconfig.inc"

SECTION rodata_font
SECTION rodata_font_fzx

PUBLIC _ff_pd_QLStyle

_ff_pd_QLStyle:

IF USE_TIMEX_HIRES
BINARY "QLStyle_tshr.fzx"
ELSE
BINARY "QLStyle.fzx"
ENDIF
