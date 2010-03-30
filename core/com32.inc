;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; com32.inc
;;
;; Common code for running a COM32 image
;;

		extern pm_api_vector

;
; Load a COM32 image.  A COM32 image is the 32-bit analogue to a DOS
; .com file.  A COM32 image is loaded at address 0x101000, with %esp
; set to the high end of usable memory.
;
; A COM32 image should begin with the magic bytes:
; B8 FF 4C CD 21, which is "mov eax,0x21cd4cff" in 32-bit mode and
; "mov ax,0x4cff; int 0x21" in 16-bit mode.  This will abort the
; program with an error if run in 16-bit mode.
;
com32_entry	equ	free_high_memory

		section .text16
is_com32_image:
		push si			; Save file handle
		push eax		; Save file length

		call make_plain_cmdline
		; Copy the command line into the low cmdline buffer
		mov ax,real_mode_seg
		mov fs,ax
		mov si,cmd_line_here
		mov di,command_line
		mov cx,[CmdLinePtr]
		inc cx			; Include final null
		sub cx,si
		fs rep movsb

		mov si,KernelName
		mov di,Com32Name
		call strcpy

		call comboot_setup_api	; Set up the COMBOOT-style API

		mov edi,com32_entry	; Load address
		pop eax			; File length
		pop si			; File handle
		xor dx,dx		; No padding
		mov bx,abort_check	; Don't print dots, but allow abort
		call load_high

		mov esi,com32_entry
		mov edi,trackbuf
		mov ecx,5
		call bcopy
		cmp dword [trackbuf],0xcd4cfeb8
		jne not_com32r
		cmp byte [trackbuf+4],0x21
		jne not_com32r

com32_start:
		;
		; Point the stack to the end of (permitted) high memory
		;
		mov eax,[HighMemRsvd]
		xor ax,ax			; Align to a 64K boundary
		mov [PMESP],eax
		mov ebx,.pm			; Where to go in PM
		jmp enter_pm

;
; This is invoked right before the actually starting the COM32
; progam, in 32-bit mode...
;
		bits 32
		section .text
.pm:
		; Set up the calling stack frame

		push dword pm_api_vector
		push dword Com32Name		; Module filename
		push dword [HighMemSize]	; Memory managed by Syslinux
		push dword core_cfarcall	; Cfarcall entry point
		push dword core_farcall		; Farcall entry point
		push dword (1 << 16)		; 64K bounce buffer
		push dword core_real_mode	; Bounce buffer address
		push dword core_intcall		; Intcall entry point
		push dword command_line		; Command line pointer
		push dword 9			; Argument count
		sti				; Interrupts OK now
		call com32_entry		; Run the program...
		; ... on return, fall through to com32_exit ...
com32_exit:
		mov bx,comboot_return
		jmp enter_rm

		bits 16
		section .text16
not_com32r:
		mov si,KernelName
		call writestr
		mov si,not_com32r_msg
		call writestr
		jmp enter_command

		section .data16
not_com32r_msg	db ': not a COM32R image', CR, LF, 0

		; Ersatz com32 invocation structure, to make libcom32
		; code run the same if linked to the core.  This is in
		; the .data16 segment so HighMemSize can live here.
		;
		; Danger, Will Robinson: it's not clear the use of
		; core_xfer_buf is safe here.
		global __entry_esp, __com32
		alignz 4
__entry_esp:
		dd 0				; Dummy to avoid _exit issues
__com32:
		dd 9				; Argument count
		dd 0				; No command line
		dd core_intcall			; Intcall entry point
		dd 0				; Bounce buffer address
		dd 0				; 64K bounce buffer
		dd core_farcall			; Farcall entry point
		dd core_cfarcall		; Cfarcall entry point
HighMemSize	dd 0				; End of memory pointer (bytes)
		dd 0				; No module name
		dd pm_api_vector		; Protected mode functions

		section .uibss
Com32Name	resb FILENAME_MAX

		section .text16
