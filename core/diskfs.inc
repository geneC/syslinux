; -*- fundamental -*- (asm-mode sucks)
; -----------------------------------------------------------------------
;   
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009-2011 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;   Boston MA 02110-1301, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; diskfs.inc
;
; Common code for conventional disk-based filesystems
;

;
; Some semi-configurable constants... change on your own risk.
;
NULLFILE	equ 0			; Null character == empty filename
NULLOFFSET	equ 0			; Position in which to look
retry_count	equ 16			; How patient are we with the disk?
%assign HIGHMEM_SLOP 0			; Avoid this much memory near the top
LDLINUX_MAGIC	equ 0x3eb202fe		; A random number to identify ourselves with

; This indicates the general format of the last few bytes in the boot sector
BS_MAGIC_VER	equ 0x1b << 9

MIN_SECTOR_SHIFT	equ 9
MIN_SECTOR_SIZE		equ (1 << MIN_SECTOR_SHIFT)

;
; The following structure is used for "virtual kernels"; i.e. LILO-style
; option labels.  The options we permit here are `kernel' and `append
; Since there is no room in the bottom 64K for all of these, we
; stick them in high memory and copy them down before we need them.
;
		struc vkernel
vk_vname:	resb FILENAME_MAX	; Virtual name **MUST BE FIRST!**
vk_rname:	resb FILENAME_MAX	; Real name
vk_appendlen:	resw 1
vk_type:	resb 1			; Type of file
		alignb 4
vk_append:	resb max_cmd_len+1	; Command line
		alignb 4
vk_end:		equ $			; Should be <= vk_size
		endstruc



; ---------------------------------------------------------------------------
;   BEGIN CODE
; ---------------------------------------------------------------------------

;
; Memory below this point is reserved for the BIOS and the MBR
;
		section .earlybss
		global trackbuf
trackbufsize	equ 8192
trackbuf	resb trackbufsize	; Track buffer goes here
		; ends at 2800h

;
; Common bootstrap code for disk-based derivatives
;
%include "diskstart.inc"


;
; Now, everything is "up and running"... patch kaboom for more
; verbosity and using the full screen system
;
		; E9 = JMP NEAR
		mov di,kaboom.patch
		mov al,0e9h
		stosb
		mov ax,kaboom2-2
		sub ax,di
		stosw

;
; Now we're all set to start with our *real* business.	First load the
; configuration file (if any) and parse it.
;
; In previous versions I avoided using 32-bit registers because of a
; rumour some BIOSes clobbered the upper half of 32-bit registers at
; random.  I figure, though, that if there are any of those still left
; they probably won't be trying to install Linux on them...
;
; The code is still ripe with 16-bitisms, though.  Not worth the hassle
; to take'm out.  In fact, we may want to put them back if we're going
; to boot ELKS at some point.
;

;
; Load configuration file
;
		pm_call pm_load_config
		jz no_config_file

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"

;
;
; kaboom2: once everything is loaded, replace the part of kaboom
;	   starting with "kaboom.patch" with this part

kaboom2:
		mov si,err_bootfailed
		call writestr
		cmp byte [kaboom.again+1],18h	; INT 18h version?
		je .int18
		call getchar
		call vgaclearmode
		int 19h			; And try once more to boot...
.norge:		jmp short .norge	; If int 19h returned; this is the end
.int18:
		call vgaclearmode
		int 18h
.noreg:		jmp short .noreg	; Nynorsk

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "common.inc"		; Universal modules
%include "plaincon.inc"		; writechr
%include "writestr.inc"		; String output
%include "writehex.inc"		; Hexadecimal output
%include "localboot.inc"	; Disk-based local boot

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data16
copyright_str   db ' Copyright (C) 1994-'
		asciidec YEAR
		db ' H. Peter Anvin et al', CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: please change disks and press '
		db 'a key to continue.', CR, LF, 0

;
; Config file keyword table
;
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
;
		alignz 4
exten_table:	db '.cbt'		; COMBOOT (specific)
%if IS_SYSLINUX
		db '.bss'		; Boot sector (add superblock)
%endif
		db '.bs', 0		; Boot sector
		db '.com'		; COMBOOT (same as DOS)
		db '.c32'		; COM32
exten_table_end:
		dd 0, 0			; Need 8 null bytes here

;
; Misc initialized (data) variables
;
%ifdef debug				; This code for debugging only
debug_magic	dw 0D00Dh		; Debug code sentinel
%endif
