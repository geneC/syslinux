;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; parsecmd.inc
;;
;; Command line parser code
;;

		section .text16

; -------------------------------------------------------------------------
;  getcommand:	Get a keyword from the current "getc" file and match it
;		against a list of keywords (keywd_table).  Each entry in
;		that table should have the following form:
;		<32 bit hash value> <16 bit handler offset>
;
;               The handler is called, and upon return this function
;               returns with CF = 0.  On EOF, this function returns
;		with CF = 1.
; -------------------------------------------------------------------------

getcommand.skipline:
		call skipline

getcommand:
.find:
		call skipspace		; Skip leading whitespace
		jz .eof			; End of file
		jc .find		; End of line: try again

		; Do this explicitly so #foo is treated as a comment
		cmp al,'#'		; Leading hash mark -> comment
		je .skipline

		; Abuse the trackbuf by putting the keyword there for
		; possible error messaging...
		mov di,trackbuf
		stosb
		or al,20h		; Convert to lower case
		movzx ebx,al		; Hash for a one-char keyword
.read_loop:
		call getc
		jc .eof
		cmp al,' '		; Whitespace
		jbe .done
		stosb
		or al,20h
		rol ebx,5
		xor bl,al
		jmp short .read_loop
.done:		call ungetc
		xor ax,ax
		stosb			; Null-terminate the trackbuf
		call skipspace
		jz .eof
		jc .noparm
		call ungetc		; Return nonwhitespace char to buf
		mov si,keywd_table
		mov cx,keywd_count
.table_search:
		lodsd
		cmp ebx,eax
		je .found_keywd
		lodsd			; Skip entrypoint/argument
		loop .table_search

		; Otherwise unrecognized keyword
		mov si,err_badcfg
		call writestr
		mov si,trackbuf
		call writestr
		call crlf
		jmp .skipline

		; No parameter
.noparm:
		mov si,err_noparm
		call writestr
		mov si,trackbuf
		call writestr
		call crlf
		jmp .find

.found_keywd:	lodsw			; Load argument into ax
		call [si]
		clc
		ret

.eof:		stc
		ret

skipline:	cmp al,10		; Search for LF
		je .end
		call getc
		jnc skipline
.end:		ret

		section .data16
err_badcfg      db 'Unknown keyword in configuration file: ',0
err_noparm      db 'Missing parameter in configuration file. Keyword: ',0

		section .uibss
		alignb 4
vk_size		equ (vk_end + 3) & ~3
VKernelBuf:	resb vk_size		; "Current" vkernel
AppendBuf       resb max_cmd_len+1	; append=
Ontimeout	resb max_cmd_len+1	; ontimeout
Onerror		resb max_cmd_len+1	; onerror
		; This could be in .uibss but that makes PXELINUX overflow
		section .bss16
KbdMap		resb 256		; Keyboard map
FKeyName	resb MAX_FKEYS*FILENAME_MAX	; File names for F-key help
                global KernelName
KernelName      resb FILENAME_MAX	; Mangled name for kernel
MNameBuf        resb FILENAME_MAX
InitRD          resb FILENAME_MAX

		section .text16
