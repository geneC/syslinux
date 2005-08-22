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


; Memory below 0800h is reserved for the BIOS and the MBR.
BSS_START	equ 0800h

; Text starts at the load address of 07C00h.
TEXT_START	equ 7C00h

; The secondary BSS section, above the text; we really wish we could
; just make it follow .bcopy32 or hang off the end,
; but it doesn't seem to work that way.
LATEBSS_START	equ 0B000h

; Reserve memory for the stack.  This causes checkov to abort the
; compile if we violate this space.
STACK_SIZE	equ 4096
STACK_START	equ TEXT_START-STACK_SIZE

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
		section .latebss  nobits align=16 start=LATEBSS_START

		; Reserve space for stack
		section .stack	  nobits align=16 start=STACK_START
Stack		resb	STACK_SIZE

