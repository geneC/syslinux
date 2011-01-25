; -----------------------------------------------------------------------
;
;   Copyright 1999-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;   Boston MA 02110-1301, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; localboot.inc
;
; Boot from a local disk, or invoke INT 18h.
;

;
; Boot a specified local disk.  AX specifies the BIOS disk number; or
; -1 in case we should execute INT 18h ("next device.")
;
		section .text16

local_boot:
		call vgaclearmode
		RESET_STACK_AND_SEGS dx		; dx <- 0
		mov fs,dx
		mov gs,dx
		mov si,localboot_msg
		call writestr
		call cleanup_hardware
		cmp ax,-1
		je .int18

		; Load boot sector from the specified BIOS device and jump to it.
		mov dl,al
		xor dh,dh
		push dx
		xor ax,ax			; Reset drive
		int 13h
		mov ax,0201h			; Read one sector
		mov cx,0001h			; C/H/S = 0/0/1 (first sector)
		mov bx,trackbuf
		mov bp,retry_count
.again:
		pusha
		int 13h
		popa
		jnc .ok
		dec bp
		jnz .again
		jmp kaboom			; Failure...
.ok:
		pop dx
		cli				; Abandon hope, ye who enter here
		mov si,trackbuf
		mov di,07C00h
		mov cx,512			; Probably overkill, but should be safe
		rep movsd
		mov ss,cx			; SS <- 0
		mov sp,7C00h
		jmp 0:07C00h			; Jump to new boot sector

.int18:
		int 18h				; Hope this does the right thing...
		jmp kaboom			; If we returned, oh boy...

		section .data16
localboot_msg	db 'Booting from local disk...', CR, LF, 0

		section .text16

