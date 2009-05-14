; -*- fundamental -*-
; -----------------------------------------------------------------------
;
;   Copyright 2004-2008 H. Peter Anvin - All Rights Reserved
;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; init.inc
;
; Common initialization code (inline)
;

		section .text16
common_init:
		; Zero bss sections (but not .earlybss, since it may
		; contain already-live data.)
		xor eax,eax
		mov di,__bss16_start
		mov cx,__bss16_dwords
		rep stosd
		mov di,__uibss_start
		mov cx,__uibss_dwords
		rep stosd

		; Now set up screen parameters
		call adjust_screen

;
; Initialize configuration information
;
		call reset_config

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
		section .text16			; This is an inline file...
