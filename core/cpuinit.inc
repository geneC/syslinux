;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; cpuinit.inc
;;
;; CPU-dependent initialization and related checks.
;;

check_escapes:
		mov ah,02h			; Check keyboard flags
		int 16h
		mov [KbdFlags],al		; Save for boot prompt check
		test al,04h			; Ctrl->skip 386 check
		jnz skip_checks

;
; Now check that there is sufficient low (DOS) memory
;
; NOTE: Linux doesn't use all of real_mode_seg, but we use the same
; segment for COMBOOT images, which can use all 64K
;
dosram_k	equ (real_mode_seg+0x1000) >> 6	; Minimum DOS memory (K)
		int 12h
		cmp ax,dosram_k
		jae enough_ram
		mov si,err_noram
		call writestr_early
		jmp kaboom
enough_ram:
skip_checks:

;
; Initialize the bcopy32 code in low memory
;
		mov si,__bcopy32_lma
		mov di,__bcopy32_start
		mov cx,__bcopy32_dwords
		rep movsd

;
; Check if we're 386 (as opposed to 486+); if so we need to blank out
; the WBINVD instruction
;
; We check for 486 by setting EFLAGS.AC
;
%if DO_WBINVD
		pushfd				; Save the good flags
		pushfd
		pop eax
		mov ebx,eax
		xor eax,(1 << 18)		; AC bit
		push eax
		popfd
		pushfd
		pop eax
		popfd				; Restore the original flags
		xor eax,ebx
		jnz is_486
;
; 386 - Looks like we better blot out the WBINVD instruction
;
		mov byte [try_wbinvd],0c3h		; Near RET
is_486:
%endif	; DO_WBINVD

		section .data
err_noram	db 'It appears your computer has less than '
		asciidec dosram_k
		db 'K of low ("DOS")'
		db CR, LF
		db 'RAM.  Linux needs at least this amount to boot.  If you get'
		db CR, LF
		db 'this message in error, hold down the Ctrl key while'
		db CR, LF
		db 'booting, and I will take your word for it.', CR, LF, 0
		section .text
