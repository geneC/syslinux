; -*- fundamental -*-
; -----------------------------------------------------------------------
;   
;   Copyright 2004 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; dnsresolv.inc
;
; Very simple DNS resolver (assumes recursion-enabled DNS server;
; this should be the normal thing for client-serving DNS servers.)
;

		section .text

;
; Turn a string in DS:SI into a DNS "label set" in ES:DI.
; On return, DI points to the first byte after the label set.
;
mangle_dnsname:
		push ax
		push cx
		xor cx,cx
.getbyte:
		jcxz .nostart	
.gotstart:
		lodsb
		and al,al
		jz .endstring
		cmp al,'.'
		je .isdot
		inc byte [es:cx]
		stosb
		jmp .getbyte
.nostart:
		xor al,al
		mov cx,di
		stosb
		jmp .gotstart
.isdot:
		xor cx,cx
		jmp .getbyte
.endstring:
		mov al,[es:cx]
		and al,al
		jz .done
		xor al,al
		stosb
.done:
		pop cx
		pop ax
		ret

;
; Compare two sets of DNS labels, in DS:SI and DS:DI; the one in SI
; is allowed pointers relative to a packet address in BX.
;
; Assumes DS == ES.  ZF = 1 if same; no registers changed.
;
dns_compare:
		pusha

.label:
		lodsb
		cmp al,0C0h
		jb .noptr
		and al,03Fh
		mov ah,al
		lodsb
		mov si,bx
		add si,ax
		jmp .label
.noptr:
		cmp al,[di]
		jne .done			; Mismatch
		inc di
		movzx cx,al			; End label?
		and cx,cx			; ZF = 1 if match
		jz .done
		
		; We have a string of bytes that need to match now
		repe cmpsb
		jz .label

.done:
		popa
		ret

;
; Skip past a DNS label set in DS:SI.
;
dns_skiplabel:
		push ax
		xor ax,ax			; AH == 0
.loop:
		lodsb
		cmp al,0C0h
		jae .ptr
		and al,al
		jz .done
		add si,ax
		jmp .loop
.ptr:
		inc si
.done:
		pop ax
		ret

