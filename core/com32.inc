;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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

pm_entry	equ 101000h

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

		call comboot_setup_api	; Set up the COMBOOT-style API

		mov edi,pm_entry	; Load address
		pop eax			; File length
		pop si			; File handle
		xor dx,dx		; No padding
		mov bx,abort_check	; Don't print dots, but allow abort
		call load_high

com32_start:
		;
		; Point the stack to the end of (permitted) high memory
		;
		mov eax,[HighMemRsvd]
		xor ax,ax			; Align to a 64K boundary
		mov [PMESP],eax
		mov bx,.pm			; Where to go in PM
		jmp enter_pm

;
; This is invoked right before the actually starting the COM32
; progam, in 32-bit mode...
;
		bits 32
.pm:
		; Set up the calling stack frame

		push dword [HighMemSize]	; Memory managed by Syslinux
		push dword __com32_cfarcall	; Cfarcall entry point
		push dword __com32_farcall	; Farcall entry point
		push dword (1 << 16)		; 64K bounce buffer
		push dword (xfer_buf_seg << 4)	; Bounce buffer address
		push dword __com32_intcall	; Intcall entry point
		push dword command_line		; Command line pointer
		push dword 7			; Argument count
		sti				; Interrupts OK now
		call pm_entry			; Run the program...
		; ... on return, fall through to com32_exit ...

com32_exit:
		mov bx,.rm
		jmp enter_rm

		bits 16
.rm:
		mov dword [PMESP],__stack_end	; Stop use of COM32 stack
		sti
		jmp enter_command

;
; 16-bit intcall/farcall handling code
;
		bits 16

com32_sys_rm:
		pop gs
		pop fs
		pop es
		pop ds
		popad
		popfd
		mov [cs:Com32SysSP],sp
		retf				; Invoke routine
.return:
		; We clean up SP here because we don't know if the
		; routine returned with RET, RETF or IRET
		mov sp,[cs:Com32SysSP]
		pushfd
		pushad
		push ds
		push es
		push fs
		push gs
		mov bx,com32_syscall.resume
		jmp enter_pm

;
; 16-bit cfarcall handing code
;
com32_cfar_rm:
		retf
.return:
		mov sp,[cs:Com32SysSP]
		mov [cs:RealModeEAX],eax
		mov bx,__com32_cfarcall.resume
		jmp enter_pm

;
; 32-bit support code
;
		bits 32

;
; Intcall/farcall invocation.  We manifest a structure on the real-mode stack,
; containing the com32sys_t structure from <com32.h> as well as
; the following entries (from low to high address):
; - Target offset
; - Target segment
; - Return offset
; - Return segment (== real mode cs == 0)
; - Return flags
;
		global __com32_farcall
__com32_farcall:
		pushfd				; Save IF among other things...
		pushad				; We only need to save some, but...

		mov eax,[esp+10*4]		; CS:IP
		jmp com32_syscall

		global __com32_intcall
__com32_intcall:
		pushfd				; Save IF among other things...
		pushad				; We only need to save some, but...

		movzx eax,byte [esp+10*4]	; INT number
		mov eax,[eax*4]			; Get CS:IP from low memory

com32_syscall:
		cld

		movzx edi,word [word RealModeSSSP]
		movzx ebx,word [word RealModeSSSP+2]
		sub edi,54		; Allocate 54 bytes
		mov [word RealModeSSSP],di
		shl ebx,4
		add edi,ebx		; Create linear address

		mov esi,[esp+11*4]	; Source regs
		xor ecx,ecx
		mov cl,11		; 44 bytes to copy
		rep movsd

		; EAX is already set up to be CS:IP
		stosd			; Save in stack frame
		mov eax,com32_sys_rm.return	; Return seg:offs
		stosd			; Save in stack frame
		mov eax,[edi-12]	; Return flags
		and eax,0x200cd7	; Mask (potentially) unsafe flags
		mov [edi-12],eax	; Primary flags entry
		stosw			; Return flags

		mov bx,com32_sys_rm
		jmp enter_rm	; Go to real mode

		; On return, the 44-byte return structure is on the
		; real-mode stack, plus the 10 additional bytes used
		; by the target address (see above.)
.resume:
		movzx esi,word [word RealModeSSSP]
		movzx eax,word [word RealModeSSSP+2]
		mov edi,[esp+12*4]	; Dest regs
		shl eax,4
		add esi,eax		; Create linear address
		and edi,edi		; NULL pointer?
		jnz .do_copy
.no_copy:	mov edi,esi		; Do a dummy copy-to-self
.do_copy:	xor ecx,ecx
		mov cl,11		; 44 bytes
		rep movsd		; Copy register block

		add dword [word RealModeSSSP],54
					; Remove from stack

		popad
		popfd
		ret			; Return to 32-bit program

;
; Cfarcall invocation.  We copy the stack frame to the real-mode stack,
; followed by the return CS:IP and the CS:IP of the target function.
;
		global __com32_cfarcall
__com32_cfarcall:
		pushfd
		pushad

		cld
		mov ecx,[esp+12*4]		; Size of stack frame

		movzx edi,word [word RealModeSSSP]
		movzx ebx,word [word RealModeSSSP+2]
		mov [word Com32SysSP],di
		sub edi,ecx		; Allocate space for stack frame
		and edi,~3		; Round
		sub edi,4*2		; Return pointer, return value
		mov [word RealModeSSSP],di
		shl ebx,4
		add edi,ebx		; Create linear address

		mov eax,[esp+10*4]	; CS:IP
		stosd			; Save to stack frame
		mov eax,com32_cfar_rm.return	; Return seg:off
		stosd
		mov esi,[esp+11*4]	; Stack frame
		mov eax,ecx		; Copy the stack frame
		shr ecx,2
		rep movsd
		mov ecx,eax
		and ecx,3
		rep movsb

		mov bx,com32_cfar_rm
		jmp enter_rm

.resume:
		popad
		mov eax,[word RealModeEAX]
		popfd
		ret

		bits 16

		section .bss16
		alignb 4
RealModeEAX	resd 1			; Real mode EAX
Com32SysSP	resw 1			; SP saved during COM32 syscall

		section .text16
