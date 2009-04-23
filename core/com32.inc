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

		; We need to make this a proper section rather
		; than using absolute numbers, in order to work
		; around a bug in GNU ld 2.17, which is still in
		; use as of this writing in the form of Debian
		; 4.0 (etch).
		bits 32
		section .com32 exec write nobits align=16
pm_idt		equ 0x100000		; Needs to be absolute...
		resb 4096
pm_entry:				; Needs to not be...

		bits 16
		section .data
		alignz 2
com32_pmidt:
		dw 8*256		; Limit
		dd pm_idt		; Address

com32_rmidt:
		dw 0ffffh		; Limit
		dd 0			; Address

		section .text
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
		mov ebx,com32_call_start	; Where to go in PM

com32_enter_pm:
		cli
		mov ax,cs
		mov ds,ax
		mov [RealModeSSSP],sp
		mov [RealModeSSSP+2],ss
		cld
		call a20_test
		jnz .a20ok
		call enable_a20

.a20ok:
		mov byte [bcopy_gdt.TSS+5],89h	; Mark TSS unbusy

		lgdt [bcopy_gdt]	; We can use the same GDT just fine
		lidt [com32_pmidt]	; Set up the IDT
		mov eax,cr0
		or al,1
		mov cr0,eax		; Enter protected mode
		jmp PM_CS32:.in_pm

		bits 32
.in_pm:
		xor eax,eax		; Available for future use...
		mov fs,eax
		mov gs,eax
		lldt ax

		mov al,PM_DS32		; Set up data segments
		mov es,eax
		mov ds,eax
		mov ss,eax

		mov al,PM_TSS		; Be nice to Intel's VT by
		ltr ax			; giving it a valid TR

		mov esp,[PMESP]		; Load protmode %esp if available
		jmp ebx			; Go to where we need to go

;
; This is invoked right before the actually starting the COM32
; progam, in 32-bit mode...
;
com32_call_start:
		;
		; Point the stack to the end of (permitted) high memory
		;
		mov esp,[word HighMemRsvd]
		xor sp,sp		; Align to a 64K boundary

		;
		; Set up the protmode IDT and the interrupt jump buffers
		; We set these up in the system area at 0x100000,
		; but we could also put them beyond the stack.
		;
		mov edi,pm_idt

		; Form an interrupt gate descriptor
		mov eax,0x00200000+((pm_idt+8*256)&0x0000ffff)
		mov ebx,0x0000ee00+((pm_idt+8*256)&0xffff0000)
		xor ecx,ecx
		inc ch				; ecx <- 256

		push ecx
.make_idt:
		stosd
		add eax,8
		xchg eax,ebx
		stosd
		xchg eax,ebx
		loop .make_idt

		pop ecx

		; Each entry in the interrupt jump buffer contains
		; the following instructions:
		;
		; 00000000 60                pushad
		; 00000001 B0xx              mov al,<interrupt#>
		; 00000003 E9xxxxxxxx        jmp com32_handle_interrupt

		mov eax,0e900b060h
		mov ebx,com32_handle_interrupt-(pm_idt+8*256+8)

.make_ijb:
		stosd
		sub [edi-2],cl			; Interrupt #
		xchg eax,ebx
		stosd
		sub eax,8
		xchg eax,ebx
		loop .make_ijb

		; Now everything is set up for interrupts...

		push dword [HighMemSize]	; Memory managed by Syslinux
		push dword com32_cfarcall	; Cfarcall entry point
		push dword com32_farcall	; Farcall entry point
		push dword (1 << 16)		; 64K bounce buffer
		push dword (xfer_buf_seg << 4)	; Bounce buffer address
		push dword com32_intcall	; Intcall entry point
		push dword command_line		; Command line pointer
		push dword 7			; Argument count
		sti				; Interrupts OK now
		call pm_entry			; Run the program...
		; ... on return, fall through to com32_exit ...

com32_exit:
		mov bx,com32_done	; Return to command loop

com32_enter_rm:
		cli
		cld
		mov [PMESP],esp		; Save exit %esp
		xor esp,esp		; Make sure the high bits are zero
		jmp PM_CS16:.in_pm16	; Return to 16-bit mode first

		bits 16
.in_pm16:
		mov ax,PM_DS16		; Real-mode-like segment
		mov es,ax
		mov ds,ax
		mov ss,ax
		mov fs,ax
		mov gs,ax

		lidt [com32_rmidt]	; Real-mode IDT (rm needs no GDT)
		mov eax,cr0
		and al,~1
		mov cr0,eax
		jmp 0:.in_rm

.in_rm:					; Back in real mode
		mov ax,cs		; Set up sane segments
		mov ds,ax
		mov es,ax
		mov fs,ax
		mov gs,ax
		lss sp,[RealModeSSSP]	; Restore stack
		jmp bx			; Go to whereever we need to go...

com32_done:
		sti
		jmp enter_command

;
; 16-bit support code
;
		bits 16

;
; 16-bit interrupt-handling code
;
com32_int_rm:
		pushf				; Flags on stack
		push cs				; Return segment
		push word .cont			; Return address
		push dword edx			; Segment:offset of IVT entry
		retf				; Invoke IVT routine
.cont:		; ... on resume ...
		mov ebx,com32_int_resume
		jmp com32_enter_pm		; Go back to PM

;
; 16-bit intcall/farcall handling code
;
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
		mov ebx,com32_syscall.resume
		jmp com32_enter_pm

;
; 16-bit cfarcall handing code
;
com32_cfar_rm:
		retf
.return:
		mov sp,[cs:Com32SysSP]
		mov [cs:RealModeEAX],eax
		mov ebx,com32_cfarcall.resume
		jmp com32_enter_pm

;
; 32-bit support code
;
		bits 32

;
; This is invoked on getting an interrupt in protected mode.  At
; this point, we need to context-switch to real mode and invoke
; the interrupt routine.
;
; When this gets invoked, the registers are saved on the stack and
; AL contains the register number.
;
com32_handle_interrupt:
		movzx eax,al
		xor ebx,ebx		; Actually makes the code smaller
		mov edx,[ebx+eax*4]	; Get the segment:offset of the routine
		mov bx,com32_int_rm
		jmp com32_enter_rm	; Go to real mode

com32_int_resume:
		popad
		iret

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
com32_farcall:
		pushfd				; Save IF among other things...
		pushad				; We only need to save some, but...

		mov eax,[esp+10*4]		; CS:IP
		jmp com32_syscall


com32_intcall:
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
		jmp com32_enter_rm	; Go to real mode

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

		add dword [word RealModeSSSP],54	; Remove from stack

		popad
		popfd
		ret			; Return to 32-bit program

;
; Cfarcall invocation.  We copy the stack frame to the real-mode stack,
; followed by the return CS:IP and the CS:IP of the target function.
;
com32_cfarcall:
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
		jmp com32_enter_rm

.resume:
		popad
		mov eax,[word RealModeEAX]
		popfd
		ret

		bits 16

		section .bss1
		alignb 4
RealModeSSSP	resd 1			; Real-mode SS:SP
RealModeEAX	resd 1			; Real mode EAX
PMESP		resd 1			; Protected-mode ESP
Com32SysSP	resw 1			; SP saved during COM32 syscall

		section .text
