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

		extern init
		pm_call init
;
; Initialize configuration information
;
		call reset_config

;
; Set up the COMBOOT APIs
;
		call comboot_setup_api

;
; Inite the memmory subsystem
;
;		pm_call mem_init
		mov eax,[HighMemSize]
		mov [VKernelEnd],eax

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
