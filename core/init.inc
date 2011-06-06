; -*- fundamental -*-
; -----------------------------------------------------------------------
;
;   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; init.inc
;
; Common initialization code (inline)
;

		section .text16
common_init:
		; Initialize PM invocation framework
		call pm_init

		; Decompress PM code to its target location
		pm_call pm_decompress
		cmp eax,__pm_code_len
		jne kaboom

;
; Initialize timer
;
		call timer_init

;
; Initialize configuration information
;
		call reset_config

;
; Set up the COMBOOT APIs
;
		call comboot_setup_api

;
; Now set up screen parameters
;
		call adjust_screen

;
; CPU-dependent initialization and related checks.
;
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
		int 12h
		mov edx,__lowmem_heap + min_lowmem_heap + 1023
		shr edx,10
		cmp ax,dx
		jae enough_ram
		mov si,err_noram
		mov cl,10
		push dx
		div cl
		add [si+err_noram.need-err_noram+2],ah
		cbw
		div cl
		add [si+err_noram.need-err_noram],ax
		pop ax
		div cl
		add [si+err_noram.size-err_noram+2],ah
		cbw
		div cl
		add [si+err_noram.size-err_noram],ax
		call writestr_early
		jmp kaboom
enough_ram:
skip_checks:

		section .data16
err_noram	db 'It appears your computer has only '
.size		db '000'
		db 'K of low ("DOS") RAM.', CR, LF
		db 'This version of Syslinux needs '
.need		db '000'
		db 'K to boot.  If you get this', CR, LF
		db 'message in error, hold down the Ctrl key while'
		db 'booting, and I', CR, LF
		db 'will take your word for it.', CR, LF, 0

		section .text16
;
; The code to decompress the PM code and initialize other segments.
;
		extern _lzo1x_decompress_asm_fast

		section .textnr
		bits 32
pm_decompress:
		push 0				; Space for decompressed size
		push esp			; Pointer to previous word
		push __pm_code_start		; Target address
		push dword [lzo_data_size]	; Compressed size
		push dword __pm_code_lma
		call _lzo1x_decompress_asm_fast
		add esp,16
		pop RM_EAX			; Decompressed size

		; Zero bss sections (but not .earlybss, since it may
		; contain already-live data.)
		xor eax,eax
		mov edi,__bss_start
		mov ecx,__bss_dwords
		rep stosd
		mov edi,__bss16_start
		mov ecx,__bss16_dwords
		rep stosd
		mov edi,__high_clear_start	; .uibss, .auxseg, .lowmem
		mov ecx,__high_clear_dwords
		rep stosd

		ret

		section .data16
lzo_data_size	dd 0				; filled in by compressor

		section .text16
		bits 16
