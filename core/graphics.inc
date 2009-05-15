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

; ----------------------------------------------------------------------------
;  VGA splash screen code
; ----------------------------------------------------------------------------

;
; vgadisplayfile:
;	Display a graphical splash screen.
;	The file is already opened on the top of the getc stack.
;
;	Assumes CS == DS == ES.
;
		section .text

vgadisplayfile:
		; This is a cheap and easy way to make sure the screen is
		; cleared in case we were in graphics mode already
		call vgaclearmode
		call vgasetmode
		jnz .error_nz

.graphalready:
		; Load the header.
		mov cx,4+2*2+16*3
		mov di,LSSHeader
.gethdr:
		call getc
		stosb
		loop .gethdr
		jc .error

		; The header WILL be in the first chunk.
		cmp dword [LSSMagic],0x1413f33d	; Magic number
.error_nz:	jne .error

		mov dx,GraphColorMap		; Color map offset
		mov ax,1012h			; Set RGB registers
		xor bx,bx			; First register number
		mov cx,16			; 16 registers
		int 10h

.movecursor:
		mov ax,[GraphYSize]		; Number of pixel rows
		mov dx,[VGAFontSize]
		add ax,dx
		dec ax
		div dl
		xor dx,dx			; Set column to 0
		cmp al,[VidRows]
		jb .rowsok
		mov al,[VidRows]
		dec al
.rowsok:
		mov dh,al
		mov ah,2
		xor bx,bx
		int 10h				; Set cursor below image

		mov cx,[GraphYSize]		; Number of graphics rows
		mov word [VGAPos],0

.drawpixelrow:
		push cx
		mov di,VGARowBuffer
		; Pre-clear the row buffer
		push di
		mov cx,640/4
		xor eax,eax
		rep stosd
		pop di
		push di
		mov cx,[GraphXSize]
		call rledecode			; Decode one row
		pop si
		push es
		mov di,0A000h			; VGA segment
		mov es,di
		mov di,[VGAPos]
		mov bp,640
		call packedpixel2vga
		add word [VGAPos],80
		pop es
		pop cx
		loop .drawpixelrow

.error:
		jmp close			; Tailcall!

;
; rledecode:
;	Decode a pixel row in RLE16 format.
;
; getc stack	-> input
; CX		-> pixel count
; ES:DI		-> output (packed pixel)
;
rledecode:
		xor dx,dx		; DL = last pixel, DH = nybble buffer
.loop:
		call .getnybble
		cmp al,dl
		je .run			; Start of run sequence
		stosb
		mov dl,al
		dec cx
		jnz .loop
.done:
		ret
.run:
		xor bx,bx
		call .getnybble
		or bl,al
		jz .longrun
.dorun:
		push cx
		mov cx,bx
		mov al,dl
		rep stosb
		pop cx
		sub cx,bx
		ja .loop
		jmp short .done
.longrun:
		call .getnybble
		mov bl,al
		call .getnybble
		shl al,4
		or bl,al
		add bx,16
		jmp short .dorun

.getnybble:
		test dh,10h
		jz .low
		and dh,0Fh
		mov al,dh
		ret
.low:
		call getc
		mov dh,al
		shr dh,4
		or dh,10h		; Nybble already read
		and al,0Fh
		ret

;
; packedpixel2vga:
;	Convert packed-pixel to VGA bitplanes
;
; DS:SI -> packed pixel string
; BP    -> pixel count (multiple of 8)
; ES:DI -> output
;
packedpixel2vga:
		mov dx,3C4h	; VGA Sequencer Register select port
		mov al,2	; Sequencer mask
		out dx,al	; Select the sequencer mask
		inc dx		; VGA Sequencer Register data port
		mov al,1
		mov bl,al
.planeloop:
		pusha
		out dx,al
.loop1:
		mov cx,8
.loop2:
		xchg cx,bx
		lodsb
		shr al,cl
		rcl ch,1	; VGA is bigendian.  Sigh.
		xchg cx,bx
		loop .loop2
		mov al,bh
		stosb
		sub bp,byte 8
		ja .loop1
		popa
		inc bl
		shl al,1
		cmp bl,4
		jbe .planeloop
		ret

;
; vgasetmode:
;	Enable VGA graphics, if possible; return ZF=1 on success
;	DS must be set to the base segment; ES is set to DS.
;
vgasetmode:
		push ds
		pop es
		mov al,[UsingVGA]
		cmp al,01h
		je .success		; Nothing to do...
		test al,04h
		jz .notvesa
		; We're in a VESA mode, which means VGA; use VESA call
		; to revert the mode, and then call the conventional
		; mode-setting for good measure...
		mov ax,4F02h
		mov bx,0012h
		int 10h
		jmp .setmode
.notvesa:
		mov ax,1A00h		; Get video card and monitor
		xor bx,bx
		int 10h
		sub bl, 7		; BL=07h and BL=08h OK
		cmp bl, 1
		ja .error		; ZF=0
;		mov bx,TextColorReg
;		mov dx,1009h		; Read color registers
;		int 10h
.setmode:
		mov ax,0012h		; Set mode = 640x480 VGA 16 colors
		int 10h
		mov dx,linear_color
		mov ax,1002h		; Write color registers
		int 10h
		mov [UsingVGA], byte 1

		; Set GXPixCols and GXPixRows
		mov dword [GXPixCols],640+(480 << 16)

		call use_font		; Set graphics font/data
		mov byte [ScrollAttribute], 00h

.success:
		xor ax,ax		; Set ZF
.error:
		ret

;
; vgaclearmode:
;	Disable VGA graphics.  It is not safe to assume any value
;	for DS or ES.
;
vgaclearmode:
		push ds
		push es
		pushad
		mov ax,cs
		mov ds,ax
		mov es,ax
		mov al,[UsingVGA]
		and al,al		; Already in text mode?
		jz .done
		test al,04h
		jz .notvesa
		mov ax,4F02h		; VESA return to normal video mode
		mov bx,0003h
		int 10h
.notvesa:
		mov ax,0003h		; Return to normal video mode
		int 10h
;		mov dx,TextColorReg	; Restore color registers
;		mov ax,1002h
;		int 10h
		mov [UsingVGA], byte 0

		mov byte [ScrollAttribute], 07h
		call use_font		; Restore text font/data
.done:
		popad
		pop es
		pop ds
		ret

;
; vgashowcursor/vgahidecursor:
;	If VGA graphics is enabled, draw a cursor/clear a cursor
;
vgashowcursor:
		pushad
		mov al,'_'
		jmp short vgacursorcommon
vgahidecursor:
		pushad
		mov al,' '
vgacursorcommon:
		cmp [UsingVGA], byte 1
		jne .done
		mov ah,09h
		mov bx,0007h
		mov cx,1
		int 10h
.done:
		popad
		ret


		section .data
		; Map colors to consecutive DAC registers
linear_color	db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0

		; See comboot.doc, INT 22h AX=0017h for the semantics
		; of this byte.
UsingVGA	db 0

		section .bss2
		alignb 4
LSSHeader	equ $
LSSMagic	resd 1			; Magic number
GraphXSize	resw 1			; Width of splash screen file
GraphYSize	resw 1			; Height of splash screen file
GraphColorMap	resb 3*16
VGAPos		resw 1			; Pointer into VGA memory
VGAFilePtr	resw 1			; Pointer into VGAFileBuf
; TextColorReg	resb 17			; VGA color registers for text mode
%if IS_SYSLINUX
VGAFileBuf	resb FILENAME_MAX+2	; Unmangled VGA image name
%else
VGAFileBuf	resb FILENAME_MAX	; Unmangled VGA image name
%endif
VGAFileBufEnd	equ $
VGAFileMBuf	resb FILENAME_MAX	; Mangled VGA image name

		alignb 4
VGARowBuffer	resb 640+80		; Decompression buffer
