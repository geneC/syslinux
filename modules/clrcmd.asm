; ****************************************************************************
;
;  clrcmd.asm
;
;  A comboot program to clear the screen then run an arbitrary Syslinux
;  commandline.
;
;  Uses BIOS calls for the standard screen as I assume that serial ports are
;  too slow and too restricted for this to be relevant.
;
;   Copyright (C) 2010  Gene Cumm
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;  Boston MA 02111-1307, USA; either version 2 of the License, or
;  (at your option) any later version; incorporated herein by reference.
;
; ****************************************************************************

cpu 8086

		absolute 0
pspInt20:	resw 1
pspNextP:	resw 1
		resb 124
pspCmdLen:	resb 1
pspCmdArg:	resb 127

		section .text
		org	0x100

_start:

clrbios:		; Clear normal BIOS console with BIOS call
.curoff:	mov ah,01
		mov cx,2001h
		int 10h
.getwin:	; INT 10h AH=11h doesn't appear to work; oversize dimensions
		mov cx,0
; 		mov dx,1950h		; 25r x 80c
; 		mov dx,2c84h		; 44r x 132c
		mov dx,64a0h		; 50r x 160c
.clear:		mov bh,0
		mov ax,0600h		; 0 lines should do window
		int 10h
.home:		mov dx,0
		mov bh,0
		mov ah,02h
		int 10h
.done:

runcmd:
		xor bx,bx
		mov bl,[pspCmdLen]
		xor ax,ax
		mov [bx+pspCmdArg],al
		mov bx,pspCmdArg+1
		mov al,[bx]
		cmp al,0
		je .norun	; Don't run an empty command
		mov ax,0003h	; Run command [es:bx] null-terminated
		int 22h		; No return
.norun:
		ret
