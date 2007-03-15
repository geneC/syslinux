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
;; getc.inc
;;
;; Simple file handling library (open, getc, ungetc)
;;

;
; open,getc:	Load a file a character at a time for parsing in a manner
;		similar to the C library getc routine.	Only one simultaneous
;		use is supported.  Note: "open" trashes the trackbuf.
;
;		open:	Input:	mangled filename in DS:DI
;			Output: ZF set on file not found or zero length
;
;		openfd:	Input:	file handle in SI
;			Output:	none
;
;		getc:	Output: CF set on end of file
;				Character loaded in AL
;
open:
		call searchdir
		jz openfd.ret
openfd:
		pushf
		mov [FBytes],ax
		mov [FBytes+2],dx
		mov eax,[FBytes]
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT
		mov [FSectors],eax	; Number of sectors
		mov [FNextClust],si	; Cluster pointer
		mov ax,[EndOfGetCBuf]	; Pointer at end of buffer ->
		mov [FPtr],ax		;  nothing loaded yet
		popf			; Restore no ZF
.ret:		ret

getc:
		stc			; If we exit here -> EOF
		mov ecx,[FBytes]
		jecxz .ret
		mov si,[FPtr]
		cmp si,[EndOfGetCBuf]
		jb .loaded
		; Buffer empty -- load another set
		mov ecx,[FSectors]
		cmp ecx,trackbufsize >> SECTOR_SHIFT
		jna .oksize
		mov ecx,trackbufsize >> SECTOR_SHIFT
.oksize:	sub [FSectors],ecx	; Reduce remaining clusters
		mov si,[FNextClust]
		push es			; ES may be != DS, save old ES
		push ds
		pop es
		mov bx,getcbuf
		push bx
		call getfssec		; Load a trackbuf full of data
		mov [FNextClust],si	; Store new next pointer
		pop si			; SI -> newly loaded data
		pop es			; Restore ES
.loaded:	lodsb			; Load a byte, increment SI
		mov [FPtr],si		; Update next byte pointer
		dec dword [FBytes]	; Update bytes left counter
		clc			; Not EOF
.ret:		ret

;
; ungetc:	Push a character (in AL) back into the getc buffer
;		Note: if more than one byte is pushed back, this may cause
;		bytes to be written below the getc buffer boundary.  If there
;		is a risk for this to occur, the getcbuf base address should
;		be moved up.
;
ungetc:
		mov si,[FPtr]
		dec si
		mov [si],al
		mov [FPtr],si
		inc dword [FBytes]
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
;               Syntaxes accepted: [-]dec, [-]0+oct, [-]0x+hex, val+K, val+M
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
		jae .km		; Invalid for base
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
.isk:		shl ebx,10		; x 2^10
		jmp short .done
.ism:		shl ebx,20		; x 2^20
		jmp short .done


		section .bss
		alignb 4
NumBuf		resb 15			; Buffer to load number
NumBufEnd	resb 1			; Last byte in NumBuf
FBytes		resd 1			; Number of bytes left in getc file
FSectors	resd 1			; Number of sectors in getc file
FNextClust	resw 1			; Pointer to next cluster in d:o
FPtr		resw 1			; Pointer to next char in buffer

;
; unhexchar:    Convert a hexadecimal digit in AL to the equivalent number;
;               return CF=1 if not a hex digit
;
		section .text
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
