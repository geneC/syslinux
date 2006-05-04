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
; init.inc
;
; Common initialization code (inline)
;

		section .text
common_init:
		; Now set up screen parameters
		call adjust_screen

		; Wipe the F-key area
		mov al,NULLFILE
		mov di,FKeyName
		mov cx,10*(1 << FILENAME_MAX_LG2)
		rep stosb

		mov si,linuxauto_cmd		; Default command: "linux auto"
		mov di,default_cmd
                mov cx,linuxauto_len
		rep movsb

		mov di,KbdMap			; Default keymap 1:1
		xor al,al
		inc ch				; CX <- 256
mkkeymap:	stosb
		inc al
		loop mkkeymap

;
; Clear Files structures
;
		mov di,Files
		mov cx,(MAX_OPEN*open_file_t_size)/4
		xor eax,eax
		rep stosd

%if IS_PXELINUX
		mov di,Files+tftp_pktbuf
		mov cx,MAX_OPEN
.setbufptr:
		mov [di],ax
		add di,open_file_t_size
		add ax,PKTBUF_SIZE
		loop .setbufptr
%endif

		section .data
linuxauto_cmd	db 'linux auto',0
linuxauto_len   equ $-linuxauto_cmd

		section .text			; This is an inline file...
