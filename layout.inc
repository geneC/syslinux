; $Id$
; -----------------------------------------------------------------------
;   
;   Copyright 1994-2004 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; layout.inc
;
; Memory layout of segments
;


; Memory below 0800h is reserved for the BIOS and the MBR
BSS_START	equ 0800h

; PXELINUX needs lots of BSS, so it relocates itself on startup
%if IS_PXELINUX
TEXT_START	equ 0A000h
%else
TEXT_START	equ 7C00h
%endif

%ifdef MAP
		[map all MAP]
%endif

;
; The various sections and their relationship
;
		org TEXT_START

		; NASM BUG: refers to hacks to handle NASM 0.98.38 bugs; might need
		; conditional compilation
	
		section .earlybss nobits start=BSS_START
		section .bcopy32  align=4 valign=16 follows=.data vfollows=.earlybss
		; NASM BUG: follows= here should be vfollows=
		section .bss      nobits align=256 follows=.bcopy32

		section .text	  start=TEXT_START
		; NASM BUG: follows=.text not accepted here
		section .data	  align=16 ; follows=.text

		; NASM BUG: We would like to do follows=.bcopy32
		section .latebss  nobits align=16 start=0E000h


