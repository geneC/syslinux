;; $Id$
;; -----------------------------------------------------------------------
;;   
;;   Copyright 1994-2002 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; cpuinit.inc
;; 
;; CPU-dependent initialization and related checks.
;;

;
; Check that no moron is trying to boot Linux on a 286 or so.  According
; to Intel, the way to check is to see if the high 4 bits of the FLAGS
; register are either all stuck at 1 (8086/8088) or all stuck at 0
; (286 in real mode), if not it is a 386 or higher.  They didn't
; say how to check for a 186/188, so I *hope* it falls out as a 8086
; or 286 in this test.
;
; Also, provide an escape route in case it doesn't work.
;
check_escapes:
		mov ah,02h			; Check keyboard flags
		int 16h
		mov [KbdFlags],al		; Save for boot prompt check
		test al,04h			; Ctrl->skip 386 check
		jnz skip_checks
test_8086:
		pushf				; Get flags
		pop ax
		and ax,0FFFh			; Clear top 4 bits
		push ax				; Load into FLAGS
		popf
		pushf				; And load back
		pop ax
		and ax,0F000h			; Get top 4 bits
		cmp ax,0F000h			; If set -> 8086/8088
		je not_386
test_286:
		pushf				; Get flags
		pop ax
		or ax,0F000h			; Set top 4 bits
		push ax
		popf
		pushf
		pop ax
		and ax,0F000h			; Get top 4 bits
		jnz is_386			; If not clear -> 386
not_386:
		mov si,err_not386
		call writestr
		jmp kaboom
is_386:
		; Now we know it's a 386 or higher
;
; Now check that there is sufficient low (DOS) memory
;
		int 12h
		cmp ax,(real_mode_seg+0xa00) >> 6
		jae enough_ram
		mov si,err_noram
		call writestr
		jmp kaboom
enough_ram:
skip_checks:

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
;
; Mark A20 type as unknown.  This particular word needs to be in BSS,
; so it needs to be initialized.
;
		mov word [A20Type],0


