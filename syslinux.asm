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

		absolute 0
pspInt20:		resw 1
pspNextParagraph:	resw 1
			resb 1		; reserved
pspDispatcher:		resb 5
pspTerminateVector:	resd 1
pspControlCVector:	resd 1
pspCritErrorVector:	resd 1
			resw 11		; reserved
pspEnvironment:		resw 1
			resw 23		; reserved
pspFCB_1:		resb 16
pspFCB_2:		resb 16
			resd 1		; reserved
pspCommandLen:		resb 1
pspCommandArg:		resb 127

		section .text
		org 0100h
_start:		
		; BUG: check DOS version

;
; Scan command line for a drive letter followed by a colon
;
		xor cx,cx
		mov si,pspCommandArg
		mov cl,[pspCommandLen]
		
cmdscan1:	jcxz bad_usage			; End of command line?
		lodsb				; Load character
		dec cx
		cmp al,' '			; White space
		jbe cmdscan1
		cmp al,'-'
		je scan_option
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
		jmp die
;
; Scan for options after a - sign.  The only recognized option right now
; is -s.
;
scan_option:	jcxz bad_usage
		lodsb
		dec cx
		cmp al,' '
		jbe cmdscan1
		or al,20h
		cmp al,'s'
		jne bad_usage
		push si			; make_stupid doesn't save these
		push cx
		call make_stupid	; Enable stupid boot sector
		pop cx
		pop si
		jmp short scan_option

		section .data
msg_unfair:	db 'Usage: syslinux [-s] <drive>:', 0Dh, 0Ah, '$'

		section .text
;
; Parsed the command line OK.  Check that the drive parameters are acceptable
;
		struc DPB
dpbDrive:	resb 1
dpbUnit:	resb 1
dpbSectorSize:	resw 1
dpbClusterMask:	resb 1
dpbClusterShift: resb 1
dpbFirstFAT:	resw 1
dpbFATCount:	resb 1
dpbRootEntries:	resw 1
dpbFirstSector:	resw 1
dpbMaxCluster:	resw 1
dpbFATSize:	resw 1
dpbDirSector:	resw 1
dpbDriverAddr:	resd 1
dpbMedia:	resb 1
dpbFirstAccess:	resb 1
dpbNextDPB:	resd 1
dpbNextFree:	resw 1
dpbFreeCnt:	resw 1
		endstruc

got_cmdline:
		mov dl,[DriveNo]
		inc dl				; 1-based
		mov bx,DPB
		mov ah,32h
		int 21h				; Get Drive Parameter Block
		
		and al,al
		jnz filesystem_error

		cmp word [bx+dpbSectorSize],512	; Sector size = 512 required
		jne sectorsize_error

		cmp byte [bx+dpbClusterShift],5	; Max size = 16K = 2^5 sectors
		jna read_bootsect

hugeclust_error:
		mov dx,msg_hugeclust_err
		jmp die
filesystem_error:
		mov dx,msg_filesystem_err
		jmp die
sectorsize_error:
		mov dx,msg_sectorsize_err
		jmp die

;
; Good enough.  Now read the old boot sector and copy the superblock.
;
read_bootsect:
		push cs				; Set DS == ES
		pop ds

		mov bx,SectorBuffer
		mov al,[DriveNo]
		mov cx,1			; One sector
		xor dx,dx			; Absolute sector 0
		int 25h				; DOS absolute disk read
		add sp,byte 2			; Remove flags from stack
		jc disk_read_error

		mov si,SectorBuffer+11		; Offset of superblock
		mov di,BootSector+11
		mov cx,51			; Superblock = 51 bytes
		rep movsb			; Copy the superblock
;
; Writing LDLINUX.SYS
;
		; 0. Set the correct filename

		mov al,[DriveNo]
		add [ldlinux_sys_str],al

		; 1. If the file exists, strip its attributes and delete

		xor cx,cx			; Clear attributes
		mov dx,ldlinux_sys_str
		mov ax,4301h			; Set file attributes
		int 21h

		mov dx,ldlinux_sys_str
		mov ah,41h			; Delete file
		int 21h

		section .data
ldlinux_sys_str: db 'A:\LDLINUX.SYS', 0

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
		mov al,[DriveNo]
		mov bx,BootSector
		mov cx,1			; One sector
		xor dx,dx			; Absolute sector 0
		int 26h				; DOS absolute disk write
		add sp,byte 2			; Remove flags
		jc disk_write_error

all_done:	mov ax,4C00h			; Exit good status
		int 21h
;
; Error routine jump
;
disk_read_error:
		mov dx,msg_read_err
		jmp short die
disk_write_error:
file_write_error:
		mov dx,msg_write_err
die:
		push cs
		pop ds
		push dx
		mov dx,msg_error
		mov ah,09h
		int 21h
		pop dx

		mov ah,09h			; Write string
		int 21h

		mov ax,4C01h			; Exit error status
		int 21h

;
; This includes a small subroutine make_stupid to patch up the boot sector
; in case we give the -s (stupid) option
;
		%include "stupid.inc"

		section .data
msg_error:		db 'ERROR: $'
msg_filesystem_err:	db 'Filesystem not found on disk', 0Dh, 0Ah, '$'
msg_sectorsize_err:	db 'Sector sizes other than 512 bytes not supported', 0Dh, 0Ah, '$'
msg_hugeclust_err:	db 'Clusters larger than 16K not supported', 0Dh, 0Ah, '$'
msg_read_err:		db 'Disk read failed', 0Dh, 0Ah, '$'
msg_write_err:		db 'Disk write failed', 0Dh, 0Ah, '$'

		section .data
		align 4, db 0
BootSector:	incbin "bootsect.bin"
LDLinuxSYS:	incbin "ldlinux.sys"
ldlinux_size:	equ $-LDLinuxSYS

		section .bss
		alignb 4
SectorBuffer:	resb 512
