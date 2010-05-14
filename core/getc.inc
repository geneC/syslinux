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
;; getc.inc
;;
;; Simple file handling library (open, getc, ungetc)
;;
;; WARNING: This interface uses the real_mode_seg/comboot_seg.
;;

MAX_GETC_LG2	equ 4			; Max number of file nesting
MAX_GETC	equ (1 << MAX_GETC_LG2)
bytes_per_getc_lg2	equ 16-MAX_GETC_LG2
bytes_per_getc		equ (1 << bytes_per_getc_lg2)
MAX_UNGET	equ 9			; Max bytes that can be pushed back

		struc getc_file
gc_file		resw 1			; File pointer
gc_bufbytes	resw 1			; Bytes left in buffer
gc_bufdata	resw 1			; Pointer to data in buffer
gc_unget_cnt	resb 1			; Character pushed back count
gc_unget_buf	resb MAX_UNGET		; Character pushed back buffer
		endstruc
getc_file_lg2	equ 4			; Size of getc_file as a power of 2

%ifndef DEPEND
%if (getc_file_size != (1 << getc_file_lg2))
%error "getc_file_size != (1 << getc_file_lg2)"
%endif
%endif

;
; open,getc:	Load a file a character at a time for parsing in a manner
;		similar to the C library getc routine.
;		Up to MAX_GETC files can be open at the same time,
;		they are accessed in a stack-like fashion.
;
;		All routines assume CS == DS.
;
;		open:	Input:	mangled filename in DS:DI
;			Output: ZF set on file not found or zero length
;
;		openfd:	Input:	file handle in SI, file size in EAX
;			Output:	ZF set on getc stack overflow
;
;		getc:	Output: CF set on end of file
;				Character loaded in AL
;
;		close:	Output:	CF set if nothing open
;
		global core_open
core_open:
		pm_call pm_searchdir
		jz openfd.ret
openfd:
		push bx

		mov bx,[CurrentGetC]
		sub bx,getc_file_size
		cmp bx,GetCStack
		jb .stack_full		; Excessive nesting
		mov [CurrentGetC],bx

		mov [bx+gc_file],si	; File pointer
		xor ax,ax
		mov [bx+gc_bufbytes],ax		; Buffer empty
		mov [bx+gc_unget_cnt],al	; ungetc buffer empty

		inc ax			; ZF <- 0
		pop bx
.ret:		ret

.stack_full:
		pm_call pm_close_file
		xor ax,ax		; ZF <- 1
		pop bx
		ret
		
getc:
		push bx
		push si
		push di
		push es

		mov di,[CurrentGetC]
		movzx bx,byte [di+gc_unget_cnt]
		and bx,bx
		jnz .have_unget

		mov si,real_mode_seg	; Borrow the real_mode_seg
		mov es,si

.got_data:
		sub word [di+gc_bufbytes],1
		jc .get_data		; Was it zero already?
		mov si,[di+gc_bufdata]
		mov al,[es:si]
		inc si
		mov [di+gc_bufdata],si
.done:
		clc
.ret:
		pop es
		pop di
		pop si
		pop bx
		ret
.have_unget:
		dec bx
		mov al,[di+bx+gc_unget_buf]
		mov [di+gc_unget_cnt],bl
		jmp .done

.get_data:
		pushad
		; Compute start of buffer
		mov bx,di
		sub bx,GetCStack
		shl bx,bytes_per_getc_lg2-getc_file_lg2

		mov [di+gc_bufdata],bx
		mov si,[di+gc_file]
		and si,si
		mov [di+gc_bufbytes],si	; In case SI == 0
		jz .empty
		mov cx,bytes_per_getc
		pm_call getfsbytes
		mov [di+gc_bufbytes],cx
		mov [di+gc_file],si
		jcxz .empty
		popad
		TRACER 'd'
		jmp .got_data

.empty:
		TRACER 'e'
		; [di+gc_bufbytes] is zero already, thus we will continue
		; to get EOF on any further attempts to read the file.
		popad
		xor al,al		; Return a predictable zero
		stc
		jmp .ret

;
; This is similar to getc, except that we read up to CX bytes and
; store them in ES:DI.  Eventually this could get optimized...
;
; On return, CX and DI are adjusted by the number of bytes actually read.
;
readc:
		push ax
.loop:
		call getc
		jc .out
		stosb
		loop .loop
.out:
		pop ax
		ret

;
; close: close the top of the getc stack
;
close:
		push bx
		push si
		mov bx,[CurrentGetC]
		mov si,[bx+gc_file]
		pm_call pm_close_file
		add bx,getc_file_size
		mov [CurrentGetC],bx
		pop si
		pop bx
		ret

;
; ungetc:	Push a character (in AL) back into the getc buffer
;		Note: if more than MAX_UNGET bytes are pushed back, all
;		hell will break loose.
;
ungetc:
		push di
		push bx
		mov di,[CurrentGetC]
		movzx bx,[di+gc_unget_cnt]
		mov [bx+di+gc_unget_buf],al
		inc bx
		mov [di+gc_unget_cnt],bl
		pop bx
		pop di
		ret

;
; skipspace:	Skip leading whitespace using "getc".  If we hit end-of-line
;		or end-of-file, return with carry set; ZF = true of EOF
;		ZF = false for EOLN; otherwise CF = ZF = 0.
;
;		Otherwise AL = first character after whitespace
;
skipspace:
.loop:		call getc
		jc .eof
		cmp al,1Ah			; DOS EOF
		je .eof
		cmp al,0Ah
		je .eoln
		cmp al,' '
		jbe .loop
		ret				; CF = ZF = 0
.eof:		cmp al,al			; Set ZF
		stc				; Set CF
		ret
.eoln:		add al,0FFh			; Set CF, clear ZF
		ret

;
; getint:	Load an integer from the getc file.
;		Return CF if error; otherwise return integer in EBX
;
getint:
		mov di,NumBuf
.getnum:	cmp di,NumBufEnd	; Last byte in NumBuf
		jae .loaded
		push di
		call getc
		pop di
		jc .loaded
		stosb
		cmp al,'-'
		jnb .getnum
		call ungetc		; Unget non-numeric
.loaded:	mov byte [di],0
		mov si,NumBuf
		; Fall through to parseint
;
; parseint:	Convert an integer to a number in EBX
;		Get characters from string in DS:SI
;		Return CF on error
;		DS:SI points to first character after number
;
;               Syntaxes accepted: [-]dec, [-]0+oct, [-]0x+hex, val+[KMG]
;
parseint:
                push eax
                push ecx
		push bp
		xor eax,eax		; Current digit (keep eax == al)
		mov ebx,eax		; Accumulator
		mov ecx,ebx		; Base
                xor bp,bp               ; Used for negative flag
.begin:		lodsb
		cmp al,'-'
		jne .not_minus
		xor bp,1		; Set unary minus flag
		jmp short .begin
.not_minus:
		cmp al,'0'
		jb .err
		je .octhex
		cmp al,'9'
		ja .err
		mov cl,10		; Base = decimal
		jmp short .foundbase
.octhex:
		lodsb
		cmp al,'0'
		jb .km		; Value is zero
		or al,20h		; Downcase
		cmp al,'x'
		je .ishex
		cmp al,'7'
		ja .err
		mov cl,8		; Base = octal
		jmp short .foundbase
.ishex:
		mov al,'0'		; No numeric value accrued yet
		mov cl,16		; Base = hex
.foundbase:
                call unhexchar
                jc .km                ; Not a (hex) digit
                cmp al,cl
		jae .km			; Invalid for base
		imul ebx,ecx		; Multiply accumulated by base
                add ebx,eax             ; Add current digit
		lodsb
		jmp short .foundbase
.km:
		dec si			; Back up to last non-numeric
		lodsb
		or al,20h
		cmp al,'k'
		je .isk
		cmp al,'m'
		je .ism
		cmp al,'g'
		je .isg
		dec si			; Back up
.fini:		and bp,bp
		jz .ret		; CF=0!
		neg ebx			; Value was negative
.done:		clc
.ret:		pop bp
                pop ecx
                pop eax
		ret
.err:		stc
		jmp short .ret
.isg:		shl ebx,10		; * 2^30
.ism:		shl ebx,10		; * 2^20
.isk:		shl ebx,10		; * 2^10
		jmp .fini

		section .bss16
		alignb 4
NumBuf		resb 15			; Buffer to load number
NumBufEnd	resb 1			; Last byte in NumBuf

GetCStack	resb getc_file_size*MAX_GETC
.end		equ $

		section .data16
CurrentGetC	dw GetCStack.end	; GetCStack empty

;
; unhexchar:    Convert a hexadecimal digit in AL to the equivalent number;
;               return CF=1 if not a hex digit
;
		section .text16
unhexchar:
                cmp al,'0'
		jb .ret			; If failure, CF == 1 already
                cmp al,'9'
                ja .notdigit
		sub al,'0'		; CF <- 0
		ret
.notdigit:	or al,20h		; upper case -> lower case
		cmp al,'a'
                jb .ret			; If failure, CF == 1 already
                cmp al,'f'
                ja .err
                sub al,'a'-10           ; CF <- 0
                ret
.err:		stc
.ret:		ret

;
;
; getline:	Get a command line, converting control characters to spaces
;               and collapsing streches to one; a space is appended to the
;               end of the string, unless the line is empty.
;		The line is terminated by ^J, ^Z or EOF and is written
;		to ES:DI.  On return, DI points to first char after string.
;		CF is set if we hit EOF.
;
getline:
		call skipspace
                mov dl,1                ; Empty line -> empty string.
                jz .eof               ; eof
                jc .eoln              ; eoln
		call ungetc
.fillloop:	push dx
		push di
		call getc
		pop di
		pop dx
		jc .ret		; CF set!
		cmp al,' '
		jna .ctrl
		xor dx,dx
.store:		stosb
		jmp short .fillloop
.ctrl:		cmp al,10
		je .ret		; CF clear!
		cmp al,26
		je .eof
		and dl,dl
		jnz .fillloop		; Ignore multiple spaces
		mov al,' '		; Ctrl -> space
		inc dx
		jmp short .store
.eoln:		clc                     ; End of line is not end of file
                jmp short .ret
.eof:		stc
.ret:		pushf			; We want the last char to be space!
		and dl,dl
		jnz .xret
		mov al,' '
		stosb
.xret:		popf
		ret

;
; parseint_esdi:
;		Same as parseint, but takes the input in ES:DI
;
parseint_esdi:
		push ds
		push es
		pop ds
		xchg si,di
		call parseint
		xchg si,di
		pop ds
		ret
