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
;; font.inc
;;
;; VGA font handling code
;;

		section .text16

;
; loadfont:	Load a .psf font file and install it onto the VGA console
;		(if we're not on a VGA screen then ignore.)
;		The font is on top of the getc stack.
;
loadfont.err:	jmp close			; Tailcall the close routine

loadfont:
		mov di,trackbuf
		mov cx,4
		call readc			; Read header
		jc .err

		mov ax,[trackbuf]		; Magic number
		cmp ax,0436h
		jne .err

		mov al,[trackbuf+2]		; File mode
		cmp al,5			; Font modes 0-5 supported
		ja .err

		xor bx,bx
		mov bh,[trackbuf+3]		; Height of font
		cmp bh,2			; VGA minimum
		jb .err
		cmp bh,32			; VGA maximum
		ja .err

		; Load the actual font
		mov di,trackbuf
		mov cx,bx			; Bytes = font height * 256
		call readc
		jc .err

		call close

		; Copy to font buffer
		mov si,trackbuf			; Start of font data
		mov [VGAFontSize],bh
		push es
		mov cx,aux_seg
		mov es,cx
		mov di,aux.fontbuf
		mov cx,bx
		shr cx,2
		rep movsd
		pop es

		mov [UserFont], byte 1		; Set font flag

;
; use_font:
;	This routine activates whatever font happens to be in the
;	vgafontbuf, and updates the adjust_screen data.
;       Must be called with CS = DS
;
use_font:
		test byte [UsingVGA], ~03h	; Nonstandard mode?
		jz .modeok
		call vgaclearmode

.modeok:
		test [UserFont], byte 1		; Are we using a user-specified font?
		jz adjust_screen		; If not, just do the normal stuff

		push es
		mov bp,aux_seg
		mov es,bp

		mov bp,aux.fontbuf		; ES:BP -> font
		mov bh,[VGAFontSize]
		xor bl,bl			; Needed by both INT 10h calls

		test byte [UsingVGA], 01h	; Are we in graphics mode?
		jz .text

.graphics:
		xor cx,cx
		mov cl,bh			; CX = bytes/character
		mov ax,[GXPixRows]
		div cl				; Compute char rows per screen
		mov dl,al
		dec ax
		mov [VidRows],al
		mov ax,1121h			; Set user character table
		int 10h
		mov ax,[GXPixCols]
		shr ax,3			; 8 pixels/character
		dec ax
		mov [VidCols],al
		pop es
		ret				; No need to call adjust_screen

.text:
		mov cx,256
		xor dx,dx
		mov ax,1110h
		int 10h				; Load into VGA RAM
		pop es

		xor bl,bl
		mov ax,1103h			; Select page 0
		int 10h

;
; adjust_screen: Set the internal variables associated with the screen size.
;		This is a subroutine in case we're loading a custom font.
;
adjust_screen:
		pusha
                mov al,[BIOS_vidrows]
                and al,al
                jnz vidrows_ok
                mov al,24                       ; No vidrows in BIOS, assume 25
						; (Remember: vidrows == rows-1)
vidrows_ok:	mov [VidRows],al
                mov ah,0fh
                int 10h                         ; Read video state
                dec ah                          ; Store count-1 (same as rows)
                mov [VidCols],ah
		popa
		ret

		section .data16
		alignz 2
VGAFontSize	dw 16			; Defaults to 16 byte font
UserFont	db 0			; Using a user-specified font

		section .bss16
		alignb 4
GXPixCols	resw 1			; Graphics mode pixel columns
GXPixRows	resw 1			; Graphics mode pixel rows
