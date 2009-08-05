; "$Id: pxechain.asm,v 1.2 2007/12/16 08:15:39 jhutz Exp $"
; -*- fundamental -*- (asm-mode sucks)  vim:noet:com=\:;
; ****************************************************************************
;
;  pxechain.asm
;
;  A comboot program to chain from PXELINUX to another PXE network
;  bootstrap program (NBP).  This improves on PXELINUX's built-in PXE
;  chaining support by arranging for the server address and boot filename
;  reported by the PXE stack to be those from which the new NBP was
;  loaded, allowing PXELINUX to be used to select from multiple NBP's,
;  such as gPXE, another PXELINUX(*), Windows RIS, and so on.
;
;  (*) This seems unnecessary at first, but it is very helpful when
;  selecting from among self-contained network boot images.
;
;   Copyright (c) 2007 Carnegie Mellon University
;   Copyright (C) 1994-2007  H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;  Boston MA 02111-1307, USA; either version 2 of the License, or
;  (at your option) any later version; incorporated herein by reference.
;
; ****************************************************************************

;%define DEBUG
;%define NO_RUN

		absolute 0
pspInt20:	resw 1
pspNextP:	resw 1
		resb 124
pspCmdLen:	resb 1
pspCmdArg:	resb 127

		section .text
		org	0x100

%ifdef DEBUG
%macro MARK 1.nolist
		mov ah,0x02
		mov dl,%1&0xff
		int 0x21
%if (%1 >> 8) & 0xff
		mov dl,(%1 >> 8) & 0xff
		int 0x21
%if (%1 >> 16) & 0xff
		mov dl,(%1 >> 16) & 0xff
		int 0x21
%if (%1 >> 24) & 0xff
		mov dl,(%1 >> 24) & 0xff
		int 0x21
%endif
%endif
%endif
		mov dl,' '
		int 0x21
%endmacro
%macro SHOWD 1.nolist
		mov al,%1
		call print_dec
		mov ah,0x02
		mov dl,' '
		int 0x21
%endmacro
%macro SHOWX 1.nolist
		mov bx,%1
		call print_hex
		mov ah,0x02
		mov dl,' '
		int 0x21
%endmacro
%else
%macro MARK 1.nolist
%endmacro
%macro SHOWD 1.nolist
%endmacro
%macro SHOWX 1.nolist
%endmacro
%endif

_start:
		MARK 'INIT'

; There should be exactly one command-line argument, which is of the form
; [[ipaddress]::]tftp_filename, just like filenames given to PXELINUX.
; Too few or too many arguments is an error.
;
; This code is based on mangle_name in pxelinux.asm
parse_args:
		cld
		xor cx,cx
		mov cl,[pspCmdLen]
		dec cx
		mov si,pspCmdArg+1
		and cx,cx
		je near usage		; no args is bad
		add si,cx
		dec si
		std
.chomp:		lodsb
		cmp al,' '
		loopz .chomp
		inc cx
		cld
		mov [pspCmdLen],cl
		mov si,pspCmdArg+1
		cmp word [si],'::'	; Leading ::?
		je near gotprefix
		dec cx
		jz noip
		MARK 'SCAN'

.more:
		inc si
		cmp byte [si],' '
		je near usage
		cmp word [si],'::'
		je near parse_ip
		loop .more

noip:
		MARK 'NOIP'
		mov ax,0x0e		; get config file name
		int 0x22
		mov si,bx
%ifdef DEBUG
		mov ah,0x02
		mov dl,'['
		int 0x21
		mov ax,0x02
		int 0x22
		mov ah,0x02
		mov dl,']'
		int 0x21
		mov dl,' '
		int 0x21
%endif
		push ds
		push es
		pop ds
		pop es
		push es
.find_prefix:
		lodsb
		and al,al
		jnz .find_prefix
		dec si

		mov cx,si
		sub cx,bx
		MARK 'LEN'
		SHOWD cl		; assume it's <256 for debugging
		dec si
		std
.find_slash:
		lodsb
		cmp al,'/'
		je .slash
		loop .find_slash
.slash:
		cmp cx,127
		cld
		jna .copy_prefix
		pop ds
		jmp too_long

.copy_prefix:
		SHOWD cl
		MARK 'PFX'
		mov si,bx
		mov di,tftp_filename
		mov bx,128
		sub bx,cx
		rep movsb
		pop ds

		mov cl,[pspCmdLen]
		mov si,pspCmdArg+1
		jmp prefix_done

usage:
		xor cx,cx
		mov si,msg_usage
		jmp fail

too_long:
		xor cx,cx
		mov si,msg_too_long
		jmp fail

parse_ip:
		MARK 'PIP'
		mov di,si
		mov si,pspCmdArg+1
		call parse_dotquad
		jc .notdq
		cmp si,di		; is it the same place?
		jne .notdq
		mov [tftp_siaddr],eax
		jmp gotprefix
.notdq:
		MARK 'NDQ'
		mov si,di
		mov bx,pspCmdArg+1
		mov ax,0x0010		; DNS resolve
		int 0x22
		and eax,eax
		jz noip
		mov [tftp_siaddr],eax
gotprefix:
		MARK 'GOTP'
		dec cx			; skip the ::
		dec cx
		inc si
		inc si
		mov di,tftp_filename
		mov bx,128

prefix_done:
		SHOWD bl
		MARK 'LEFT'

; SI points at the filename, plus remaining arguments,
; CX contains their combined length.
; DI points to where the filename should be stored
; BX says how much space is left for the filename and NUL

		and cx,cx
		jz usage		; no args is bad
.copy_filename:
		lodsb
%ifdef DEBUG
		mov dl,al
		mov ah,0x2
		int 0x21
%endif
		cmp al,' '
		je usage
		dec bx
		jz too_long
		stosb
		loop .copy_filename
		xor eax,eax
		stosb

; get PXE cached data
		MARK 'GCI'
		mov ax,0x0009		; call PXE stack
		mov bx,0x0071		; PXENV_GET_CACHED_INFO
		mov di,PXECacheParms
		int 0x22
		and eax,eax
		jz .fix_siaddr
		mov cx,[gci_status]
		mov si,msg_get_cache
		jmp fail

.fix_siaddr:
		mov bx,[gci_bufferseg]
		mov es,bx
		mov bx,[gci_buffer]
		mov eax,[es:bx+12]	; save our address (ciaddr)
		mov [open_ciaddr],eax	; ... in case we have to do UDP open
		mov eax,[tftp_siaddr]
		and eax,eax
		jnz .replace_addr
		MARK 'ADDR'
		mov eax,[es:bx+20]	; siaddr
		mov [tftp_siaddr],eax
		jmp .addr_done
.replace_addr:
		mov [es:bx+20],eax
.addr_done:
		mov si,tftp_filename	; copy the new filename...
		lea di,[es:bx+108]	; to the "cached DHCP response"
		mov cx,128
		rep movsb
		mov bx,ds		; restore es before proceeding
		mov es,bx

; print out what we are doing
%ifdef DEBUG
		mov ah,0x02		; write character
		mov dl,0x0d		; print a CRLF first
		int 0x21
		mov dl,0x0a
		int 0x21
%endif
		mov ax,0x0002		; write string
		mov bx,msg_booting
		int 0x22
		mov ebx,[tftp_siaddr]
		call print_dotquad
		mov ah,0x02		; write character
		mov dl,' '
		int 0x21
		mov ax,0x0002		; write string
		mov bx,tftp_filename
		int 0x22
		mov ah,0x02		; write character
		mov dl,0x0d
		int 0x21
		mov dl,0x0a
		int 0x21

%ifndef NO_RUN
		mov ax,0x0009		; call PXE stack
		mov bx,0x0031		; PXENV_UDP_CLOSE
		mov di,PXECloseParms
		int 0x22
		mov cx,[close_status]
		mov si,msg_udp_close
		and ax,ax
		jnz fail

		mov ax,0x0009		; call PXE stack
		mov bx,0x0073		; PXENV_RESTART_TFTP
		mov di,PXERestartTFTPParms
		int 0x22
		mov cx,[tftp_status]
		mov si,msg_rst_tftp
		call fail

		mov ax,0x0009		; call PXE stack
		mov bx,0x0030		; PXENV_UDP_OPEN
		mov di,PXEOpenParms
		int 0x22
		mov cx,[open_status]
		mov si,msg_udp_open
		and ax,ax
		jnz fail
		ret
%endif

fail:
		MARK 'FAIL'
		SHOWX cs
		SHOWX ds
		SHOWX es
		SHOWX si
%ifdef DEBUG
		mov ah,0x02		; write character
		mov dl,0x0d		; print a CRLF first
		int 0x21
		mov dl,0x0a
		int 0x21
%endif
		mov ax,0x0002		; write string
		mov bx,msg_progname	; print our name
		int 0x22
		mov bx,si		; ... the error message
		int 0x22
		mov ah,0x02		; write character
		jcxz .done
		mov dl,' '		; ... and the error code, in []
		int 0x21
		mov dl,'['
		int 0x21
		mov bx,cx
		call print_hex
		mov ah,0x02		; write character
		mov dl,']'
		int 0x21
.done:
		mov dl,0x0d		; and finally a CRLF
		int 0x21
		mov dl,0x0a
		int 0x21
		ret


; print_hex
;
; Take a 16-bit integer in BX and print it as 2 hex digits.
; Destroys AX and DL.
;
print_hex:
		mov al,bh
		aam 16
		cmp ah,10
		jb .lt_a000
		add ah,'A'-'0'-10
.lt_a000:	add ah,'0'
		mov dl,ah
		mov ah,0x02		; write character
		int 0x21

		cmp al,10
		jb .lt_a00
		add al,'A'-'0'-10
.lt_a00:	add al,'0'
		mov dl,al
		mov ah,0x02		; write character
		int 0x21

		mov al,bl
		aam 16
		cmp ah,10
		jb .lt_a0
		add ah,'A'-'0'-10
.lt_a0:		add ah,'0'
		mov dl,ah
		mov ah,0x02		; write character
		int 0x21

		cmp al,10
		jb .lt_a
		add al,'A'-'0'-10
.lt_a:		add al,'0'
		mov dl,al
		mov ah,0x02		; write character
		int 0x21
		ret


; print_dec
;
; Take an 8-bit integer in AL and print it in decimal.
; Destroys AX and DL.
;
print_dec:
		cmp al,10		; < 10?
		jb .lt10		; If so, skip first 2 digits

		cmp al,100		; < 100
		jb .lt100		; If so, skip first digit

		aam 100
		; Now AH = 100-digit; AL = remainder
		add ah,'0'
		mov dl,ah
		mov ah,0x02
		int 0x21

.lt100:
		aam 10
		; Now AH = 10-digit; AL = remainder
		add ah,'0'
		mov dl,ah
		mov ah,0x02
		int 0x21

.lt10:
		add al,'0'
		mov dl,al
		mov ah,0x02
		int 0x21
		ret


; print_dotquad
;
; Take an IP address (in network byte order) in EBX and print it
; as a dotted quad.
; Destroys EAX, EBX, ECX, EDX
;
print_dotquad:
		mov cx,3
.octet:
		mov al,bl
		call print_dec
		jcxz .done
		mov ah,0x02
		mov dl,'.'
		int 0x21
		ror ebx,8	; Move next char into LSB
		dec cx
		jmp .octet
.done:
		ret


; parse_dotquad:
;		Read a dot-quad pathname in DS:SI and output an IP
;		address in EAX, with SI pointing to the first
;		nonmatching character.
;
;		Return CF=1 on error.
;
;		No segment assumptions permitted.
;
parse_dotquad:
		push cx
		mov cx,4
		xor eax,eax
.parseloop:
		mov ch,ah
		mov ah,al
		lodsb
		sub al,'0'
		jb .notnumeric
		cmp al,9
		ja .notnumeric
		aad				; AL += 10 * AH; AH = 0;
		xchg ah,ch
		jmp .parseloop
.notnumeric:
		cmp al,'.'-'0'
		pushf
		mov al,ah
		mov ah,ch
		xor ch,ch
		ror eax,8
		popf
		jne .error
		loop .parseloop
		jmp .done
.error:
		loop .realerror			; If CX := 1 then we're done
		clc
		jmp .done
.realerror:
		stc
.done:
		dec si				; CF unchanged!
		pop cx
		ret

		section .data
msg_booting:	db 'TFTP boot: ',0
msg_progname:	db 'pxechain: ',0
msg_usage:	db 'usage: pxechain.cbt [[ipaddress]::]filename',0dh,0ah,0
msg_too_long:	db 'pxechain: filename is too long (max 127)',0dh,0ah,0
msg_get_cache:	db 'PXENV_GET_CACHED_INFO',0
msg_rst_tftp:	db 'PXENV_RESTART_TFTP',0
msg_udp_close:	db 'PXENV_UDP_CLOSE',0
msg_udp_open:	db 'PXENV_UDP_OPEN',0

PXECacheParms:
gci_status:	dw 0
gci_packettype:	dw 3			; PXENV_PACKET_TYPE_CACHED_REPLY
gci_buffersize:	dw 0
gci_buffer:	dw 0
gci_bufferseg:	dw 0
gci_bufferlim:	dw 0

PXERestartTFTPParms:
tftp_status:	dw 0
tftp_filename:	times 128 db 0
tftp_bufsize:	dd 0x00090000		; available memory for NBP
tftp_bufaddr:	dd 0x00007c00		; PXE NBP load address
tftp_siaddr:	dd 0
tftp_giaddr:	dd 0
tftp_mcaddr:	dd 0
tftp_mcport:	dw 0
tftp_msport:	dw 0
tftp_timeout:	dw 0
tftp_reopendly:	dw 0

PXECloseParms:
close_status:	dw 0

PXEOpenParms:
open_status:	dw 0
open_ciaddr:    dd 0
