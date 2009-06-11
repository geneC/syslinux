; -*- fundamental -*- (asm-mode sucks)
; ****************************************************************************
;
;  extlinux.asm
;
;  A program to boot Linux kernels off an ext2/ext3 filesystem.
;
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;  Boston MA 02111-1307, USA; either version 2 of the License, or
;  (at your option) any later version; incorporated herein by reference.
;
; ****************************************************************************

%define IS_EXTLINUX 1
%include "head.inc"
%include "ext2_fs.inc"

;
; Some semi-configurable constants... change on your own risk.
;
my_id		equ extlinux_id
; NASM 0.98.38 croaks if these are equ's rather than macros...
FILENAME_MAX_LG2 equ 8			; log2(Max filename size Including final null)
FILENAME_MAX	equ (1 << FILENAME_MAX_LG2)	; Max mangled filename size
NULLFILE	equ 0			; Null character == empty filename
NULLOFFSET	equ 0			; Position in which to look
retry_count	equ 16			; How patient are we with the disk?
%assign HIGHMEM_SLOP 0			; Avoid this much memory near the top
LDLINUX_MAGIC	equ 0x3eb202fe		; A random number to identify ourselves with

MAX_OPEN_LG2	equ 6			; log2(Max number of open files)
MAX_OPEN	equ (1 << MAX_OPEN_LG2)

SECTOR_SHIFT	equ 9
SECTOR_SIZE	equ (1 << SECTOR_SHIFT)

MAX_SYMLINKS	equ 64			; Maximum number of symlinks per lookup
SYMLINK_SECTORS	equ 2			; Max number of sectors in a symlink
					; (should be >= FILENAME_MAX)

ROOT_DIR_WORD	equ 0x002F
CUR_DIR_DWORD	equ 0x00002F2E

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

;
; File structure.  This holds the information for each currently open file.
;
		struc open_file_t
file_bytesleft	resd 1			; Number of bytes left (0 = free)
file_sector	resd 1			; Next linear sector to read
file_in_sec	resd 1			; Sector where inode lives
file_in_off	resw 1
file_mode	resw 1
		endstruc

%ifndef DEPEND
%if (open_file_t_size & (open_file_t_size-1))
%error "open_file_t is not a power of 2"
%endif
%endif

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

		section .bss16
		global SuperBlock, ClustSize, ClustMask, PtrsPerBlock1
		global PtrsPerBlock2, ClustShift, ClustByteShift
SuperBlock	resb 1024		; ext2 superblock
ClustSize	resd 1			; Bytes/cluster ("block")
ClustMask	resd 1			; Sectors/cluster - 1
PtrsPerBlock1	resd 1			; Pointers/cluster
PtrsPerBlock2	resd 1			; (Pointers/cluster)^2
ClustShift	resb 1			; Shift count for sectors/cluster
ClustByteShift	resb 1			; Shift count for bytes/cluster

		alignb open_file_t_size
		global Files
Files		resb MAX_OPEN*open_file_t_size

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
		pm_call load_config
		jz no_config_file

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"


		section .bss16
		alignb 4
		global ThisInode
ThisInode	resb EXT2_GOOD_OLD_INODE_SIZE	; The most recently opened inode


		section .bss16
		alignb	4
		global SymlinkBuf
SymlinkBuf	resb	SYMLINK_SECTORS*SECTOR_SIZE+64
ThisDir		resd	1


		section .text16
;
; unmangle_name: Does the opposite of mangle_name; converts a DOS-mangled
;                filename to the conventional representation.  This is needed
;                for the BOOT_IMAGE= parameter for the kernel.
;
;                DS:SI -> input mangled file name
;                ES:DI -> output buffer
;
;                On return, DI points to the first byte after the output name,
;                which is set to a null byte.
;
unmangle_name:	call strcpy
		dec di				; Point to final null byte
		ret

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



build_curdir_str:
		ret

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "common.inc"		; Universal modules
%include "plaincon.inc"		; writechr
%include "writestr.inc"		; String output
%include "writehex.inc"		; Hexadecimal output
%include "strecpy.inc"          ; strcpy with end pointer check
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
config_name	db 'extlinux.conf',0		; Unmangled form

;
; Config file keyword table
;
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
;
		alignz 4
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.img'		; Disk image
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

		alignz 4
BufSafe		dw trackbufsize/SECTOR_SIZE	; Clusters we can load into trackbuf
BufSafeBytes	dw trackbufsize		; = how many bytes?
%ifndef DEPEND
%if ( trackbufsize % SECTOR_SIZE ) != 0
%error trackbufsize must be a multiple of SECTOR_SIZE
%endif
%endif
