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
; abort.inc
;
; Code to terminate a kernel load
;

		section .text
;
; abort_check: let the user abort with <ESC> or <Ctrl-C>
;
abort_check:
		call pollchar
		jz .ret1
		pusha
		call getchar
		cmp al,27			; <ESC>
		je .kill
		cmp al,3			; <Ctrl-C>
		je .kill
.ret2:		popa
.ret1:		ret

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
abort_load:
		RESET_STACK_AND_SEGS AX
                call cwritestr                  ; Expects SI -> error msg

		; Return to the command prompt
		jmp enter_command
