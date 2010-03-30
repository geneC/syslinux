;; -*- fundamental -*- ---------------------------------------------------
;;
;;   Copyright 2008 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;;   Boston MA 02110-1301, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

		section .text16
TICKS_TO_IDLE	equ 4		; Also in idle.c

reset_idle:
		push eax
		mov eax,[cs:__jiffies]
		mov [cs:_IdleTimer],eax
		pop eax
		sti		; Guard against BIOS/PXE brokenness...
		ret

do_idle:
		push eax
		push ds
		push es
		mov ax,cs
		mov ds,ax
		mov es,ax
		pushf
		pop ax
		test ah,2
		jnz .ok
		push si
		push cx
		mov si,hlt_err
		call writestr
		mov si,sp
		add si,10
		mov cx,16
.errloop:
		ss lodsw
		call writehex4
		dec cx
		jz .endloop
		mov al,' '
		call writechr
		jmp .errloop
.endloop:
		call crlf
		pop cx
		pop si
		sti
.ok:
		; Don't spend time jumping to PM unless we're actually idle...

		mov eax,[__jiffies]
		sub eax,[_IdleTimer]
		cmp eax,TICKS_TO_IDLE
		jb .done

		extern __idle
		pm_call __idle
.done:
		pop es
		pop ds
		pop eax
.ret:		ret

		section .data16
		alignz 4
		global _IdleTimer
_IdleTimer	dd 0
		global NoHalt
NoHalt		dw 0

hlt_err		db 'ERROR: idle with IF=0', CR, LF, 0 

		section .text16
