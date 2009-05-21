;; -*- fundamental -*- ---------------------------------------------------
;;
;;   Copyright 2008 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;;   Boston MA 02110-1301, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

		section .text
TICKS_TO_IDLE	equ 4

reset_idle:
		push ax
		mov ax,[cs:BIOS_timer]
		mov [cs:IdleTimer],ax
		pop ax
		sti		; Guard against BIOS/PXE brokenness...
		ret

do_idle:
		push ax
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
		mov ax,[BIOS_timer]
		sub ax,[IdleTimer]
		cmp ax,TICKS_TO_IDLE
		jb .done
		call [IdleHook]
		cmp word [NoHalt],0
		jne .done
		hlt
.done:
		pop es
		pop ds
		pop ax
.ret:		ret

		section .data
IdleHook	dw do_idle.ret
NoHalt		dw 0

hlt_err		db 'ERROR: idle with IF=0', CR, LF, 0 

		section .bss
IdleTimer	resw 1
