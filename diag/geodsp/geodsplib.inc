; -----------------------------------------------------------------------
;
;   Copyright 2010-2015 Gene Cumm
;
;   Portions from diskstart.inc:
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;   Boston MA 02110-1301, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; geodsplib.inc
;
; Library file for geodsp*.asm
;

		; ES:BX points to the buffer with address
		; DX,CX as they should be for INT13h,AH=02
		; Should work for C>=256
write_chs_lba:
; 		pushad
		mov si,s_atchs
		call writestr_early
		call write_chs
		call write_col_val_crlf
; 		popad
		ret

		; DX,CX as they should be for INT13h,AH=02
		; Should work for C>=256
write_chs:
		pushad
		mov al,ch
		mov ah,cl
		shr ah,6
		call writehex4
		mov al,','
		call writechr
		mov al,dh
		call writehex2
		mov al,','
		call writechr
		mov al,cl
		and al,3Fh
		call writehex2
		popad
		ret

%ifdef TEST_EDD
	write_edd_lba:
		pushad
		mov si,s_atchs
		call writestr_early
		call writehex8
		call write_col_val_crlf
		popad
		ret
%endif	; TEST_EDD

write_col_val_crlf:
		mov al,':'
		call writechr
		mov eax,[es:bx]
		call writehex8
		mov si,s_crlf
		call writestr_early
		ret

; write_exc:
; ; 		push ax
; 		mov al,'!'
; 		call writechr
; ; 		pop ax
; 		ret

; crlf:
; 		push si
; 		mov si,s_crlf
; 		call writestr_early
; 		pop si
; 		ret

;
;
; writestr_early: write a null-terminated string to the console
;	    This assumes we're on page 0.  This is only used for early
;           messages, so it should be OK.
;
writestr_early:
		pushad
.loop:		lodsb
		and al,al
                jz .return
		call writechr
		jmp short .loop
.return:	popad
		ret

writechr:
writechr_early:
		pushad
		mov ah,0Eh		; Write to screen as TTY
		mov bx,0007h		; Attribute
		int 10h
		popad
		ret

%include "writehex.inc"

s_atchs:	db '@'
s_chs:		db 'CHS'
s_space:	db ' ', 0
s_typespec:	db 'D='
s_type:		db 'CHS', 0
s_end:		db 0Dh, 0Ah, 'end'
s_crlf:		db 0Dh, 0Ah, 0

; This indicates the general format of the last few bytes in the boot sector
BS_MAGIC_VER	equ 0x1b << 9
