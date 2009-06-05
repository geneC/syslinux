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
trackbufsize	equ 8192
trackbuf	resb trackbufsize	; Track buffer goes here
		; ends at 2800h

		section .bss
SuperBlock	resb 1024		; ext2 superblock
ClustSize	resd 1			; Bytes/cluster ("block")
ClustMask	resd 1			; Sectors/cluster - 1
PtrsPerBlock1	resd 1			; Pointers/cluster
PtrsPerBlock2	resd 1			; (Pointers/cluster)^2
DriveNumber	resb 1			; BIOS drive number
ClustShift	resb 1			; Shift count for sectors/cluster
ClustByteShift	resb 1			; Shift count for bytes/cluster

		alignb open_file_t_size
Files		resb MAX_OPEN*open_file_t_size

;
; Common bootstrap code for disk-based derivatives
;
%include "diskstart.inc"

;
; Load the real (ext2) superblock; 1024 bytes long at offset 1024
;
		mov bx,SuperBlock
		mov eax,1024 >> SECTOR_SHIFT
		mov bp,ax
		call getlinsecsr

;
; Compute some values...
;
		xor edx,edx
		inc edx

		; s_log_block_size = log2(blocksize) - 10
		mov cl,[SuperBlock+s_log_block_size]
		add cl,10
		mov [ClustByteShift],cl
		mov eax,edx
		shl eax,cl
		mov [ClustSize],eax

		sub cl,SECTOR_SHIFT
		mov [ClustShift],cl
		shr eax,SECTOR_SHIFT
		mov [SecPerClust],eax
		dec eax
		mov [ClustMask],eax

		add cl,SECTOR_SHIFT-2		; 4 bytes/pointer
		shl edx,cl
		mov [PtrsPerBlock1],edx
		shl edx,cl
		mov [PtrsPerBlock2],edx

;
; Common initialization code
;
%include "init.inc"
%include "cpuinit.inc"

;
; Initialize the metadata cache
;
		call initcache

;
; Now, everything is "up and running"... patch kaboom for more
; verbosity and using the full screen system
;
		; E9 = JMP NEAR
		mov dword [kaboom.patch],0e9h+((kaboom2-(kaboom.patch+3)) << 8)

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
load_config:
		mov si,config_name	; Save config file name
		mov di,ConfigName
		call strcpy
		mov dword [CurrentDirName],CUR_DIR_DWORD	; Write './',0,0 to the CurrentDirName
		call build_curdir_str

		mov di,ConfigName
		call open
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
		jae getlinsecsr			; Nothing fancy

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

;
; allocate_file: Allocate a file structure
;
;		If successful:
;		  ZF set
;		  BX = file pointer
;		In unsuccessful:
;		  ZF clear
;
allocate_file:
		TRACER 'a'
		push cx
		mov bx,Files
		mov cx,MAX_OPEN
.check:		cmp dword [bx], byte 0
		je .found
		add bx,open_file_t_size		; ZF = 0
		loop .check
		; ZF = 0 if we fell out of the loop
.found:		pop cx
		ret
;
; open_inode:
;	     Open a file indicated by an inode number in EAX
;
;	     NOTE: This file considers finding a zero-length file an
;	     error.  This is so we don't have to deal with that special
;	     case elsewhere in the program (most loops have the test
;	     at the end).
;
;	     If successful:
;		ZF clear
;		SI	    = file pointer
;		EAX         = file length in bytes
;		ThisInode   = the first 128 bytes of the inode
;	     If unsuccessful
;		ZF set
;
;	     Assumes CS == DS == ES.
;
open_inode.allocate_failure:
		xor eax,eax
		pop bx
		pop di
		ret

open_inode:
		push di
		push bx
		call allocate_file
		jnz .allocate_failure

		push cx
		push gs
		; First, get the appropriate inode group and index
		dec eax				; There is no inode 0
		xor edx,edx
		mov [bx+file_sector],edx
		div dword [SuperBlock+s_inodes_per_group]
		; EAX = inode group; EDX = inode within group
		push edx

		; Now, we need the block group descriptor.
		; To get that, we first need the relevant descriptor block.

		shl eax, ext2_group_desc_lg2size ; Get byte offset in desc table
		xor edx,edx
		div dword [ClustSize]
		; eax = block #, edx = offset in block
		add eax,dword [SuperBlock+s_first_data_block]
		inc eax				; s_first_data_block+1
		mov cl,[ClustShift]
		shl eax,cl
		push edx
		shr edx,SECTOR_SHIFT
		add eax,edx
		pop edx
		and dx,SECTOR_SIZE-1
		call getcachesector		; Get the group descriptor
		add si,dx
		mov esi,[gs:si+bg_inode_table]	; Get inode table block #
		pop eax				; Get inode within group
		movzx edx, word [SuperBlock+s_inode_size]
		mul edx
		; edx:eax = byte offset in inode table
		div dword [ClustSize]
		; eax = block # versus inode table, edx = offset in block
		add eax,esi
		shl eax,cl			; Turn into sector
		push dx
		shr edx,SECTOR_SHIFT
		add eax,edx
		mov [bx+file_in_sec],eax
		pop dx
		and dx,SECTOR_SIZE-1
		mov [bx+file_in_off],dx

		call getcachesector
		add si,dx
		mov cx,EXT2_GOOD_OLD_INODE_SIZE >> 2
		mov di,ThisInode
		gs rep movsd

		mov ax,[ThisInode+i_mode]
		mov [bx+file_mode],ax
		mov eax,[ThisInode+i_size]
		mov [bx+file_bytesleft],eax
		mov si,bx
		and eax,eax			; ZF clear unless zero-length file
		pop gs
		pop cx
		pop bx
		pop di
		ret

		section .bss
		alignb 4
ThisInode	resb EXT2_GOOD_OLD_INODE_SIZE	; The most recently opened inode

		section .text
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

;
; searchdir:
;	     Search the root directory for a pre-mangled filename in DS:DI.
;
;	     NOTE: This file considers finding a zero-length file an
;	     error.  This is so we don't have to deal with that special
;	     case elsewhere in the program (most loops have the test
;	     at the end).
;
;	     If successful:
;		ZF clear
;		SI	    = file pointer
;		DX:AX = EAX = file length in bytes
;	     If unsuccessful
;		ZF set
;
;	     Assumes CS == DS == ES; *** IS THIS CORRECT ***?
;
searchdir:
		push bx
		push cx
		push bp
		mov byte [SymlinkCtr],MAX_SYMLINKS

		mov eax,[CurrentDir]
.begin_path:
.leadingslash:
		cmp byte [di],'/'	; Absolute filename?
		jne .gotdir
		mov eax,EXT2_ROOT_INO
		inc di			; Skip slash
		jmp .leadingslash
.gotdir:

		; At this point, EAX contains the directory inode,
		; and DS:DI contains a pathname tail.
.open:
		push eax		; Save directory inode

		call open_inode
		jz .missing		; If error, done

		mov cx,[si+file_mode]
		shr cx,S_IFSHIFT	; Get file type

		cmp cx,T_IFDIR
		je .directory

		add sp,4		; Drop directory inode

		cmp cx,T_IFREG
		je .file
		cmp cx,T_IFLNK
		je .symlink

		; Otherwise, something bad...
.err:
		call close_file
.err_noclose:
		xor eax,eax
		xor si,si
		cwd			; DX <- 0

.done:
		and eax,eax		; Set/clear ZF
		pop bp
		pop cx
		pop bx
		ret

.missing:
		add sp,4		; Drop directory inode
		jmp .done

		;
		; It's a file.
		;
.file:
		cmp byte [di],0		; End of path?
		je .done		; If so, done
		jmp .err		; Otherwise, error

		;
		; It's a directory.
		;
.directory:
		pop dword [ThisDir]	; Remember what directory we're searching

		cmp byte [di],0		; More path?
		je .err			; If not, bad

.skipslash:				; Skip redundant slashes
		cmp byte [di],'/'
		jne .readdir
		inc di
		jmp .skipslash

.readdir:
		mov cx,[SecPerClust]
		push cx
		shl cx,SECTOR_SHIFT
		mov bx,trackbuf
		add cx,bx
		mov [EndBlock],cx
		pop cx
		push bx
		call getfssec
		pop bx
		pushf			; Save EOF flag
		push si			; Save filesystem pointer
.getent:
		cmp bx,[EndBlock]
		jae .endblock

		push di
		cmp dword [bx+d_inode],0	; Zero inode = void entry
		je .nope

		movzx cx,byte [bx+d_name_len]
		lea si,[bx+d_name]
		repe cmpsb
		je .maybe
.nope:
		pop di
		add bx,[bx+d_rec_len]
		jmp .getent

.endblock:
		pop si
		popf
		jnc .readdir		; There is more
		jmp .err		; Otherwise badness...

.maybe:
		mov eax,[bx+d_inode]

		; Does this match the end of the requested filename?
		cmp byte [di],0
		je .finish
		cmp byte [di],'/'
		jne .nope

		; We found something; now we need to open the file
.finish:
		pop bx			; Adjust stack (di)
		pop si
		call close_file		; Close directory
		pop bx			; Adjust stack (flags)
		jmp .open

		;
		; It's a symlink.  We have to determine if it's a fast symlink
		; (data stored in the inode) or not (data stored as a regular
		; file.)  Either which way, we start from the directory
		; which we just visited if relative, or from the root directory
		; if absolute, and append any remaining part of the path.
		;
.symlink:
		dec byte [SymlinkCtr]
		jz .err			; Too many symlink references

		cmp eax,SYMLINK_SECTORS*SECTOR_SIZE
		jae .err		; Symlink too long

		; Computation for fast symlink, as defined by ext2/3 spec
		xor ecx,ecx
		cmp [ThisInode+i_file_acl],ecx
		setne cl		; ECX <- i_file_acl ? 1 : 0
		cmp [ThisInode+i_blocks],ecx
		jne .slow_symlink

		; It's a fast symlink
.fast_symlink:
		call close_file		; We've got all we need
		mov si,ThisInode+i_block

		push di
		mov di,SymlinkTmpBuf
		mov ecx,eax
		rep movsb
		pop si

.symlink_finish:
		cmp byte [si],0
		je .no_slash
		mov al,'/'
		stosb
.no_slash:
		mov bp,SymlinkTmpBufEnd
		call strecpy
		jc .err_noclose		; Buffer overflow

		; Now copy it to the "real" buffer; we need to have
		; two buffers so we avoid overwriting the tail on the
		; next copy
		mov si,SymlinkTmpBuf
		mov di,SymlinkBuf
		push di
		call strcpy
		pop di
		mov eax,[ThisDir]	; Resume searching previous directory
		jmp .begin_path

.slow_symlink:
		mov bx,SymlinkTmpBuf
		mov cx,SYMLINK_SECTORS
		call getfssec
		; The EOF closed the file

		mov si,di		; SI = filename tail
		mov di,SymlinkTmpBuf
		add di,ax		; AX = file length
		jmp .symlink_finish


		section .bss
		alignb	4
SymlinkBuf	resb	SYMLINK_SECTORS*SECTOR_SIZE+64
SymlinkTmpBuf	 equ	trackbuf
SymlinkTmpBufEnd equ	trackbuf+SYMLINK_SECTORS*SECTOR_SIZE+64
ThisDir		resd	1
EndBlock	resw	1
SymlinkCtr	resb	1

		section .text
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
; linsector:	Convert a linear sector index in a file to a linear sector number
;	EAX	-> linear sector number
;	DS:SI	-> open_file_t
;
;		Returns next sector number in EAX; CF on EOF (not an error!)
;
linsector:
		push gs
		push ebx
		push esi
		push edi
		push ecx
		push edx
		push ebp

		push eax		; Save sector index
		mov cl,[ClustShift]
		shr eax,cl		; Convert to block number
		push eax
		mov eax,[si+file_in_sec]
		mov bx,si
		call getcachesector	; Get inode
		add si,[bx+file_in_off]	; Get *our* inode
		pop eax
		lea ebx,[i_block+4*eax]
		cmp eax,EXT2_NDIR_BLOCKS
		jb .direct
		mov ebx,i_block+4*EXT2_IND_BLOCK
		sub eax,EXT2_NDIR_BLOCKS
		mov ebp,[PtrsPerBlock1]
		cmp eax,ebp
		jb .ind1
		mov ebx,i_block+4*EXT2_DIND_BLOCK
		sub eax,ebp
		mov ebp,[PtrsPerBlock2]
		cmp eax,ebp
		jb .ind2
		mov ebx,i_block+4*EXT2_TIND_BLOCK
		sub eax,ebp

.ind3:
		; Triple indirect; eax contains the block no
		; with respect to the start of the tind area;
		; ebx contains the pointer to the tind block.
		xor edx,edx
		div dword [PtrsPerBlock2]
		; EAX = which dind block, EDX = pointer within dind block
		push ax
		shr eax,SECTOR_SHIFT-2
		mov ebp,[gs:si+bx]
		shl ebp,cl
		add eax,ebp
		call getcachesector
		pop bx
		and bx,(SECTOR_SIZE >> 2)-1
		shl bx,2
		mov eax,edx		; The ind2 code wants the remainder...

.ind2:
		; Double indirect; eax contains the block no
		; with respect to the start of the dind area;
		; ebx contains the pointer to the dind block.
		xor edx,edx
		div dword [PtrsPerBlock1]
		; EAX = which ind block, EDX = pointer within ind block
		push ax
		shr eax,SECTOR_SHIFT-2
		mov ebp,[gs:si+bx]
		shl ebp,cl
		add eax,ebp
		call getcachesector
		pop bx
		and bx,(SECTOR_SIZE >> 2)-1
		shl bx,2
		mov eax,edx		; The int1 code wants the remainder...

.ind1:
		; Single indirect; eax contains the block no
		; with respect to the start of the ind area;
		; ebx contains the pointer to the ind block.
		push ax
		shr eax,SECTOR_SHIFT-2
		mov ebp,[gs:si+bx]
		shl ebp,cl
		add eax,ebp
		call getcachesector
		pop bx
		and bx,(SECTOR_SIZE >> 2)-1
		shl bx,2

.direct:
		mov ebx,[gs:bx+si]	; Get the pointer

		pop eax			; Get the sector index again
		shl ebx,cl		; Convert block number to sector
		and eax,[ClustMask]	; Add offset within block
		add eax,ebx

		pop ebp
		pop edx
		pop ecx
		pop edi
		pop esi
		pop ebx
		pop gs
		ret

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
		call linsector
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
		call linsector
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

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "plaincon.inc"		; writechr
%include "writestr.inc"		; String output
%include "writehex.inc"		; Hexadecimal output
%include "configinit.inc"	; Initialize configuration
%include "parseconfig.inc"	; High-level config file handling
%include "parsecmd.inc"		; Low-level config file handling
%include "bcopy32.inc"		; 32-bit bcopy
%include "loadhigh.inc"		; Load a file into high memory
%include "font.inc"		; VGA font stuff
%include "graphics.inc"		; VGA graphics
%include "highmem.inc"		; High memory sizing
%include "strcpy.inc"           ; strcpy()
%include "strecpy.inc"          ; strcpy with end pointer check
%include "cache.inc"		; Metadata disk cache
%include "idle.inc"		; Idle handling
%include "adv.inc"		; Auxillary Data Vector
%include "localboot.inc"	; Disk-based local boot

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data
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
