; -*- fundamental -*- ---------------------------------------------------
;
;   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; rllpack.inc
;
; Very simple RLL compressor/decompressor, used to pack binary structures
; together.
;
; Format of leading byte
; 1-128		= x verbatim bytes follow
; 129-255	= (x-126) times subsequent byte
; 0		= end of data
;
; These structures are stored *in reverse order* in high memory.
; High memory pointers point to one byte beyond the end.
;

		section .text

;
; rllpack:
;	Pack CX bytes from SI into EDI.
;	Returns updated SI and EDI.
;
rllpack:
		push word .pmentry
		call simple_pm_call
		ret

.pmentry:
		push cx
		push ebx
		push edx
.startseq:
		xor ax,ax		; Zero byte
		xor ebx,ebx		; Run length zero
		dec edi
		mov edx,edi		; Pointer to header byte
		mov [edi],al
		jcxz .done_null
.stdbyte:
		lodsb
		dec edi
		mov [edi],al
		dec cx
		cmp ah,al
		je .same
.diff:
		mov ah,al
		xor bx,bx
.plainbyte:
		inc bx
		inc byte [edx]
		jcxz .done
		jns .stdbyte
		jmp .startseq
.same:
		cmp bl,2
		jb .plainbyte
		; 3 bytes or more in a row, time to convert sequence
		sub [edx],bl
		jnz .normal
		inc edi			; We killed a whole stretch,
					; drop start byte
.normal:
		inc bx
		add edi,ebx
		mov al,bl
		add al,126
		dec edi
		mov edx,edi
		mov [edi],al
		dec edi
		mov [edi],ah
.getrun:
		jcxz .done
		cmp bl,255-126
		jae .startseq
		lodsb
		cmp al,ah
		jne .nomatch
		inc bx
		inc byte [edx]
		dec cx
		jmp .getrun
.nomatch:
		dec si
		jmp .startseq
.done:
		dec edi
		mov [edi],cl		; CX = 0 here
.done_null:
		pop edx
		pop ebx
		pop cx
		ret
;
; rllunpack:
;	Unpack bytes from ESI into DI
;	On return ESI, DI are updated and CX contains number of bytes output.
;
rllunpack:
		push word .pmentry
		call simple_pm_call
		ret

.pmentry:
		push di
		xor cx,cx
.header:
		dec esi
		mov al,[esi]
		and al,al
		jz .done
		cmp al,129
		jae .isrun
		; Not a run
		mov cl,al
.copy:
		dec esi
		mov al,[esi]
		stosb
		loop .copy
		jmp .header
.isrun:
		sub al,126
		mov cl,al
		dec esi
		mov al,[esi]
		rep stosb
		jmp .header
.done:
		pop cx
		sub cx,di
		neg cx
		ret
