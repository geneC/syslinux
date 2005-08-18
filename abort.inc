; $Id$
; -----------------------------------------------------------------------
;   
;   Copyright 2005 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; abort.inc
;
; Code to terminate a kernel load
;

;
; abort_check: let the user abort with <ESC> or <Ctrl-C>
;
abort_load	equ abort_check.do_abort

abort_check:
		call pollchar
		jz .ret1
		pusha
		call getchar
		cmp al,27			; <ESC>
		je .kill
		cmp al,3			; <Ctrl-C>
		jne .ret2
.kill:		mov si,aborted_msg

		; ... fall through ...

;
; abort_load: Called by various routines which wants to print a fatal
;             error message and return to the command prompt.  Since this
;             may happen at just about any stage of the boot process, assume
;             our state is messed up, and just reset the segment registers
;             and the stack forcibly.
;
;             SI    = offset (in _text) of error message to print
;
.do_abort:
                mov ax,cs                       ; Restore CS = DS = ES
                mov ds,ax
                mov es,ax
%if IS_SYSLINUX || IS_EXTLINUX
                mov ss,ax                       ; Just in case...
                mov sp,StackBuf-2*3    		; Reset stack
%elif IS_PXELINUX
		lss esp,[BaseStack]
%elif IS_ISOLINUX
		lss sp,[cs:Stack]
%else
		NEED TO KNOW HOW TO RESET STACK
%endif
		sti
                call cwritestr                  ; Expects SI -> error msg

		; If onerror is set, jump to onerror, otherwise return
		; to the command prompt
		mov cx,[OnerrorLen]
		and cx,cx
		jnz on_error
          	jmp enter_command               ; Return to command prompt

;
; End of abort_check
;
.ret2:		popa
.ret1:		ret
