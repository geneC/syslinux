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
; Initialize configuration information
;
		call reset_config

;
; Initialize Files structures (other than zeroing)
;
%if IS_PXELINUX
		mov di,Files+tftp_pktbuf
		mov cx,MAX_OPEN
.setbufptr:
		mov [di],ax
		add di,open_file_t_size
		add ax,PKTBUF_SIZE
		loop .setbufptr
%endif

;
; Set up the COMBOOT APIs
;
		call comboot_setup_api

;
; Now set up screen parameters
;
		call adjust_screen

;
; The code to decompress the PM code.  We copy it temporarily to 1 MB;
; we will probably eventually actually simply run the PM code out of
; high memory, at which point the extra copy can be dropped.
;
		extern _lzo1x_decompress_asm_fast
		extern __uibss_auxseg_dwords

		section .textnr
		bits 32
pm_decompress:
		mov esi,__pm_code_lma
		mov edi,0x100000
		mov ecx,[lzo_data_size]
		call pm_bcopy

		push 0				; Space for decompressed size
		push esp			; Pointer to previous word
		push __pm_code_start		; Target address
		push dword [lzo_data_size]	; Compressed size
		push dword 0x100000
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
		mov edi,__uibss_start
		mov ecx,__uibss_auxseg_dwords
		rep stosd

		ret

		section .data16
lzo_data_size	dd 0				; filled in by compressor

		section .text16
		bits 16
