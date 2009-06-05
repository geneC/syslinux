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
; Common initialization code
;
%include "init.inc"
%include "cpuinit.inc"

;
; Initialize the ext2 fs meta data
;
		pm_call init_fs         ; will return the block size shift in eax(for now, is the sector shift)

;
; Initialize the metadata cache
;
		pm_call cache_init

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

;
; getlinsec_ext: same as getlinsec, except load any sector from the zero
;		 block as all zeros; use to load any data derived
;		 from an ext2 block pointer, i.e. anything *except the
;		 superblock.*
;
getonesec_ext:
		mov bp,1

getlinsec_ext:
		cmp eax,[SecPerClust]
		jae getlinsec			; Nothing fancy

		; If we get here, at least part of what we want is in the
		; zero block.  Zero one sector at a time and loop.
		push eax
		push cx
		xchg di,bx
		xor eax,eax
		mov cx,SECTOR_SIZE >> 2
		rep stosd
		xchg di,bx
		pop cx
		pop eax
		inc eax
		dec bp
		jnz getlinsec_ext
		ret



		section .bss16
		alignb 4
		global ThisInode
ThisInode	resb EXT2_GOOD_OLD_INODE_SIZE	; The most recently opened inode

		section .text16
;
; close_file:
;	     Deallocates a file structure (pointer in SI)
;	     Assumes CS == DS.
;
close_file:
		and si,si
		jz .closed
		mov dword [si],0		; First dword == file_bytesleft
		xor si,si
.closed:	ret

		section .bss16
		alignb	4
		global SymlinkBuf
SymlinkBuf	resb	SYMLINK_SECTORS*SECTOR_SIZE+64
ThisDir		resd	1


		section .text16
;
; mangle_name: Mangle a filename pointed to by DS:SI into a buffer pointed
;	       to by ES:DI; ends on encountering any whitespace.
;	       DI is preserved.
;
;	       This verifies that a filename is < FILENAME_MAX characters,
;	       doesn't contain whitespace, zero-pads the output buffer,
;	       and removes redundant slashes,
;	       so "repe cmpsb" can do a compare, and the
;	       path-searching routine gets a bit of an easier job.
;
;	       FIX: we may want to support \-escapes here (and this would
;	       be the place.)
;
mangle_name:
		push di
		push bx
		xor ax,ax
		mov cx,FILENAME_MAX-1
		mov bx,di

.mn_loop:
		lodsb
		cmp al,' '			; If control or space, end
		jna .mn_end
		cmp al,ah			; Repeated slash?
		je .mn_skip
		xor ah,ah
		cmp al,'/'
		jne .mn_ok
		mov ah,al
.mn_ok		stosb
.mn_skip:	loop .mn_loop
.mn_end:
		cmp bx,di			; At the beginning of the buffer?
		jbe .mn_zero
		cmp byte [di-1],'/'		; Terminal slash?
		jne .mn_zero
.mn_kill:	dec di				; If so, remove it
		inc cx
		jmp short .mn_end
.mn_zero:
		inc cx				; At least one null byte
		xor ax,ax			; Zero-fill name
		rep stosb
		pop bx
		pop di
		ret				; Done

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



;
; getfssec: Get multiple sectors from a file
;
;	Same as above, except SI is a pointer to a open_file_t
;
;	ES:BX	-> Buffer
;	DS:SI	-> Pointer to open_file_t
;	CX	-> Sector count (0FFFFh = until end of file)
;                  Must not exceed the ES segment
;	Returns CF=1 on EOF (not necessarily error)
;	On return ECX = number of bytes read
;	All arguments are advanced to reflect data read.
;
		global getfssec
getfssec:
		push ebp
		push eax
		push edx
		push edi

		movzx ecx,cx
		push ecx			; Sectors requested read
		mov eax,[si+file_bytesleft]
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT
		cmp ecx,eax			; Number of sectors left
		jbe .lenok
		mov cx,ax
.lenok:
.getfragment:
		mov eax,[si+file_sector]	; Current start index
		mov edi,eax
		pm_call linsector
		push eax			; Fragment start sector
		mov edx,eax
		xor ebp,ebp			; Fragment sector count
.getseccnt:
		inc bp
		dec cx
		jz .do_read
		xor eax,eax
		mov ax,es
		shl ax,4
		add ax,bx			; Now DI = how far into 64K block we are
		not ax				; Bytes left in 64K block
		inc eax
		shr eax,SECTOR_SHIFT		; Sectors left in 64K block
		cmp bp,ax
		jnb .do_read			; Unless there is at least 1 more sector room...
		inc edi				; Sector index
		inc edx				; Linearly next sector
		mov eax,edi
		pm_call linsector
		; jc .do_read
		cmp edx,eax
		je .getseccnt
.do_read:
		pop eax				; Linear start sector
		pushad
		call getlinsec_ext
		popad
		push bp
		shl bp,9
		add bx,bp			; Adjust buffer pointer
		pop bp
		add [si+file_sector],ebp	; Next sector index
		jcxz .done
		jnz .getfragment
		; Fall through
.done:
		pop ecx				; Sectors requested read
		shl ecx,SECTOR_SHIFT
		sub [si+file_bytesleft],ecx
		jnbe .noteof			; CF=0 in this case
		add ecx,[si+file_bytesleft]	; Actual number of bytes read
		call close_file
		stc				; We hit EOF
.noteof:
		pop edi
		pop edx
		pop eax
		pop ebp
		ret

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
