;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;;   Copyright 2010 Intel Corporation; author: H. Peter Anvin
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
; Detect old versions Xen HVM and disable halt
; Xen HVM older than version 3.3 might be using vmxassist, which breaks
; if HLT is executed in real mode.
;
; Note: in Syslinux 4, we should probably just execute the HLT in
; protected mode instead.
;
check_xen:
		pushfd
		pushfd
		pop eax
		mov edx,eax
		xor eax,(1 << 21)		; ID flag
		push eax
		popfd
		pushfd
		pop eax
		popfd
		xor eax,edx
		jz .not_xen			; No CPUID

		xor ebx,ebx
		xor ecx,ecx
		xor edx,edx
		mov eax,0x40000000
		cpuid
		cmp ebx,"XenV"
		jne .not_xen
		cmp ecx,"MMXe"
		jne .not_xen
		cmp edx,"nVMM"
		jne .not_xen

		; We're on Xen...
		mov eax,0x40000001
		cpuid
		cmp eax,0x00030003
		jae .not_xen			; Xen >= 3.3, not affected

		; We may be using vmxassist, so disable HLT
		mov byte [ForceNoHalt],1

.not_xen:

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
