; -----------------------------------------------------------------------
;
;   Copyright 2005-2006 H. Peter Anvin - All Rights Reserved
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
; This macro resets the stack pointer.  It assumes CS == DS and AX == 0.
;
%macro RESET_STACK 0
%if IS_SYSLINUX || IS_EXTLINUX
                mov ss,ax                       ; Just in case...
                mov sp,StackBuf-2*3    		; Reset stack
%elif IS_PXELINUX
		lss esp,[BaseStack]
%elif IS_ISOLINUX
		lss sp,[StackPtr]
%else
		NEED TO KNOW HOW TO RESET STACK
%endif
%endmacro

%endif ; _STACK_INC
