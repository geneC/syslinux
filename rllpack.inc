; -*- fundamental -*-
; -----------------------------------------------------------------------
;
;   Copyright 2004 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------
; $Id$

;
; rllpack.inc
;
; Very simple RLL compressor/decompressor, used to pack binary structures
; together.
;
; Format of leading byte
; 1-128		= x verbatim bytes follow
; 129-255 	= (x-126) times subsequent byte
; 0		= end of data
;

		section .text

;
; rllpack:
;	Pack CX bytes from DS:SI into ES:DI
;	Returns updated SI, DI and CX = number of bytes output
;
rllpack:
		push ax
		push bx
		push cx
		push bp
		push di
.startseq:
		xor ax,ax		; Zero byte
		xor bx,bx		; Run length zero
		mov bp,di		; Pointer to header byte
		stosb			; Store header byte (might be zero)
		jcxz .done_null
.stdbyte:
		lodsb
		stosb
		dec cx
		cmp ah,al
		je .same
.diff:
		mov ah,al
		xor bx,bx
.plainbyte:
		inc bx
		inc byte [es:bp]
		jcxz .done
		jns .stdbyte
		jmp .startseq
.same:
		cmp bl,2
		jb .plainbyte
		; 3 bytes or more in a row, time to convert sequence
		sub byte [es:bp],bl
		jnz .normal
		dec di			; We killed a whole stretch, remove start byte
.normal:
		inc bx
		sub di,bx
		mov bp,di
		mov al,bl
		add al,126
		stosb
		mov al,ah
		stosb
.getrun:
		jcxz .done
		cmp bl,255-126
		jae .startseq
		lodsb
		cmp al,ah
		jne .nomatch
		inc bx
		inc byte [es:bp]
		dec cx
		jmp .getrun
.nomatch:
		dec si
		jmp .startseq
.done:
		xor al,al
		stosb
.done_null:
		pop dx
		sub dx,di
		neg dx
		pop bp
		pop cx
		pop bx
		pop ax
		ret
;
; rllunpack:
;	Unpack bytes from DS:SI into ES:DI
;	On return SI, DI are updated and CX contains number of bytes output
;
rllunpack:
		push ax
		push di
		xor cx,cx
.header:
		lodsb
		and al,al
		jz .done
		cmp al,129
		jae .isrun
		; Not a run
		mov cl,al
		rep movsb
		jmp .header
.isrun:
		sub al,126
		mov cl,al
		lodsb
		rep stosb
		jmp .header
.done:
		pop cx
		sub cx,di
		neg cx
		pop ax
		ret
