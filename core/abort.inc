; -----------------------------------------------------------------------
;
;   Copyright 2005-2009 H. Peter Anvin - All Rights Reserved
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
; abort.inc
;
; Code to terminate a kernel load
;

		section .text16

;
; dot_pause: same as abort_check, except prints a dot, too
;            assumes CS == DS
;
dot_pause:
		push si
		mov si,dot_msg
		call writestr_qchk
		pop si
		; fall through

;
; abort_check: let the user abort with <ESC> or <Ctrl-C>
;
abort_check:
		call reset_idle			; Not idle despite pollchar
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
		mov bx,enter_command
		jmp abort_load_chain

;
; abort_load: Called by various routines which wants to print a fatal
;             error message and return to the command prompt.  Since this
;             may happen at just about any stage of the boot process, assume
;             our state is messed up, and just reset the segment registers
;             and the stack forcibly.
;
;             SI    = offset (in _text) of error message to print
;	      BX    = future entry point (abort_load_chain)
;
abort_load:
		mov bx,error_or_command
abort_load_chain:
		RESET_STACK_AND_SEGS AX
                call writestr                  ; Expects SI -> error msg

		; Return to the command prompt
		jmp bx

;
; error_or_command: Execute ONERROR if appropriate, otherwise enter_command
;
error_or_command:
		mov cx,[OnerrorLen]
		and cx,cx
		jnz on_error
		jmp enter_command

		section .data16
aborted_msg	db ' aborted.', CR, LF, 0

		section .text16
