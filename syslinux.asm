; -*- fundamental -*- (asm-mode sucks)
; $Id$
; -----------------------------------------------------------------------
;   
;   Copyright 1998 H. Peter Anvin - All Rights Reserved
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;   USA; either version 2 of the License, or (at your option) any later
;   version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; syslinux.asm
;
;	DOS installer for SYSLINUX
;

		absolute 0080h
psp_cmdlen:	resb 1
psp_cmdline:	resb 127

		section .text
		org 0100h
_start:		
		; BUG: check DOS version

;
; Scan command line for a drive letter followed by a colon
;
		xor cx,cx
		mov si,psp_cmdline
		mov cl,[psp_cmdlen]
		
cmdscan1:	jcxz bad_usage			; End of command line?
		lodsb				; Load character
		dec cx
		cmp al,' '			; White space
		jbe cmdscan1
		or al,020h			; -> lower case
		cmp al,'a'			; Check for letter
		jb bad_usage
		cmp al,'z'
		ja bad_usage
		sub al,'a'			; Convert to zero-based index
		mov [DriveNo],al		; Save away drive index

		section .bss
DriveNo:	resb 1

		section .text
;
; Got the leading letter, now the next character must be a colon
;
got_letter:	jcxz bad_usage
		lodsb
		dec cx
		cmp al,':'
		jne bad_usage
;
; Got the colon; the rest better be whitespace
;
got_colon:	jcxz got_cmdline
		lodsb
		dec cx
		cmp al,' '
		jbe got_colon
;
; We end up here if the command line doesn't parse
;
bad_usage:	mov dx,msg_unfair
		mov ah,09h
		int 21h

		mov ax,4C01h			; Exit with error code
		int 21h

		section .data
msg_unfair:	db 'Usage: syslinux <drive>:', 0Dh, 0Ah, '$'

		section .text
;
; Parsed the command line OK, now get to work
;
got_cmdline:	; BUG: check sector size == 512
		; Use INT 21h:32h

		mov bx,SectorBuffer
		mov al,[DriveNo]
		mov cx,1			; One sector
		xor dx,dx			; Absolute sector 0
		int 25h				; DOS absolute disk read
		add sp,byte 2			; Remove flags from stack
		jc disk_read_error
		
		; BUG: check FAT12, clustersize < 128 sectors

		mov si,SectorBuffer+11		; Offset of superblock
		mov di,BootSector+11
		mov cx,51			; Superblock = 51 bytes
		rep movsb			; Copy the superblock
;
; Writing LDLINUX.SYS
;
		; 1. If the file exists, strip its attributes and delete

		xor cx,cx			; Clear attributes
		mov dx,ldlinux_sys_str
		mov ax,4301h			; Set file attributes
		int 21h

		mov dx,ldlinux_sys_str
		mov ah,41h			; Delete file
		int 21h

		section .data
ldlinux_sys_str: db 'LDLINUX.SYS', 0

		section .text

		; 2. Create LDLINUX.SYS and write data to it

		mov dx,ldlinux_sys_str
		xor cx,cx			; Normal file
		mov ah,3Ch			; Create file
		int 21h
		jc file_write_error
		mov [FileHandle],ax

		mov bx,ax
		mov cx,ldlinux_size
		mov dx,LDLinuxSYS
		mov ah,40h			; Write data
		int 21h
		jc file_write_error
		cmp ax,ldlinux_size
		jne file_write_error

		mov bx,[FileHandle]
		mov ah,3Eh			; Close file
		int 21h

		section .bss
FileHandle:	resw 1

		section .text

		; 3. Set the readonly flag on LDLINUX.SYS

		mov dx,ldlinux_sys_str
		mov cx,1			; Read only
		mov ax,4301h			; Set attributes
		int 21h
;
; Writing boot sector
;
		mov bx,BootSector
		mov cx,1			; One sector
		xor dx,dx			; Absolute sector 0
		int 26h				; DOS absolute disk write
		add sp,byte 2			; Remove flags
		jc disk_write_error

all_done:	mov ax,4C00h			; Exit good status
		int 21h

disk_read_error:
disk_write_error:
file_write_error:
		mov dx,msg_bummer
		mov ah,09h			; Write string
		int 21h

		mov ax,4C01h			; Exit error status
		int 21h

		section .data
msg_bummer:	db 'Disk write error', 0Dh, 0Ah, '$'

		section .data
		align 4, db 0
BootSector:	incbin "bootsect.bin"
LDLinuxSYS:	incbin "ldlinux.sys"
ldlinux_size:	equ $-LDLinuxSYS

		section .bss
		alignb 4
SectorBuffer:	resb 512
