;; $Id$
;; -----------------------------------------------------------------------
;;   
;;   Copyright 1994-2002 H. Peter Anvin - All Rights Reserved
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

		section .text

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

getcommand:
.find:
		call skipspace		; Skip leading whitespace
		jz .eof			; End of file
		jc .find		; End of line: try again

		or al,20h		; Convert to lower case
		movzx ebx,al		; Hash for a one-char keyword
.read_loop:
		push ebx
		call getc
		pop ebx
		jc .eof
		cmp al,' '		; Whitespace
		jbe .done
		or al,20h
		rol ebx,5
		xor bl,al
		jmp short .read_loop
.done:		call ungetc
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
		jmp short .error

		; No parameter
.noparm:
		mov si,err_noparm
		mov al,10		; Already at EOL
.error:
		call cwritestr
		jmp short .skipline

.found_keywd:	lodsw			; Load argument into ax
		call [si]
		clc
		ret

.eof:		stc
		ret

.skipline:	cmp al,10		; Search for LF
		je .find
		call getc
		jc .eof
		jmp short .skipline

		section .bss
		alignb 4
vk_size		equ (vk_end + 3) & ~3
VKernelBuf:	resb vk_size		; "Current" vkernel
AppendBuf       resb max_cmd_len+1	; append=
Ontimeout	resb max_cmd_len+1	; ontimeout
Onerror		resb max_cmd_len+1	; onerror
KbdMap		resb 256		; Keyboard map
FKeyName	resb 10*FILENAME_MAX	; File names for F-key help
KernelCNameLen	resw 1			; Length of unmangled kernel name
InitRDCNameLen	resw 1			; Length of unmangled initrd name
%if IS_SYSLINUX
KernelName      resb FILENAME_MAX+1	; Mangled name for kernel
KernelCName     resb FILENAME_MAX+2	; Unmangled kernel name
InitRDCName     resb FILENAME_MAX+2	; Unmangled initrd name
%else
KernelName      resb FILENAME_MAX	; Mangled name for kernel
KernelCName     resb FILENAME_MAX	; Unmangled kernel name
InitRDCName     resb FILENAME_MAX	; Unmangled initrd name
%endif
MNameBuf        resb FILENAME_MAX
InitRD          resb FILENAME_MAX
