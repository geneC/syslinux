; -----------------------------------------------------------------------
;
;   Copyright 2005-2008 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; stack.inc
;
; How to reset the stack pointer
;

%ifndef _STACK_INC
%define _STACK_INC

;
; This macro resets the stack pointer (including SS), and sets
; DS == ES == 0, interrupts on, DF = 0.
;
; It takes a 16-bit register that can be safely clobbered as parameter.
;
%macro RESET_STACK_AND_SEGS 1
		xor %1,%1
		mov ds,%1
		mov es,%1
%if IS_SYSLINUX || IS_EXTLINUX
                mov ss,%1                       ; Just in case...
                mov sp,StackBuf-2*5		; Reset stack
%elif IS_PXELINUX
		lss esp,[BaseStack]
%elif IS_ISOLINUX
		mov ss,%1
		mov sp,StackBuf-2*2
%else
		NEED TO KNOW HOW TO RESET STACK
%endif
		sti
		cld
%endmacro

%endif ; _STACK_INC
