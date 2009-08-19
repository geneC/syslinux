; -*- fundamental -*- (asm-mode sucks)
; ****************************************************************************
;
;  ldlinux.asm
;
;  A program to boot Linux kernels off an MS-DOS formatted floppy disk.	 This
;  functionality is good to have for installation floppies, where it may
;  be hard to find a functional Linux system to run LILO off.
;
;  This program allows manipulation of the disk to take place entirely
;  from MS-LOSS, and can be especially useful in conjunction with the
;  umsdos filesystem.
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

%ifndef IS_MDSLINUX
%define IS_SYSLINUX 1
%endif
%include "head.inc"

;
; Some semi-configurable constants... change on your own risk.
;
my_id		equ syslinux_id
FILENAME_MAX_LG2 equ 6			; log2(Max filename size Including final null)
FILENAME_MAX	equ (1<<FILENAME_MAX_LG2) ; Max mangled filename size
NULLFILE	equ 0			; First char space == null filename
NULLOFFSET	equ 0			; Position in which to look
retry_count	equ 16			; How patient are we with the disk?
%assign HIGHMEM_SLOP 0			; Avoid this much memory near the top
LDLINUX_MAGIC	equ 0x3eb202fe		; A random number to identify ourselves with

MAX_OPEN_LG2	equ 6			; log2(Max number of open files)
MAX_OPEN	equ (1 << MAX_OPEN_LG2)

SECTOR_SHIFT	equ 9
SECTOR_SIZE	equ (1 << SECTOR_SHIFT)

DIRENT_SHIFT	equ 5
DIRENT_SIZE	equ (1 << DIRENT_SHIFT)

ROOT_DIR_WORD	equ 0x002F

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
file_sector	resd 1			; Sector pointer (0 = structure free)
file_bytesleft	resd 1			; Number of bytes left
file_left	resd 1			; Number of sectors left
		resd 1			; Unused
		endstruc

;
; Structure for codepage files
;
		struc cp
.magic		resd 2			; 8-byte magic number
.reserved	resd 6			; Reserved for future use
.uppercase	resb 256		; Internal upper-case table
.unicode	resw 256		; Unicode matching table
.unicode_alt	resw 256		; Alternate Unicode matching table
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
		alignb 4
FAT		resd 1			; Location of (first) FAT
RootDirArea	resd 1			; Location of root directory area
RootDir		resd 1			; Location of root directory proper
DataArea	resd 1			; Location of data area
RootDirSize	resd 1			; Root dir size in sectors
TotalSectors	resd 1			; Total number of sectors
ClustSize	resd 1			; Bytes/cluster
ClustMask	resd 1			; Sectors/cluster - 1
CopySuper	resb 1			; Distinguish .bs versus .bss
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
; Compute some information about this filesystem.
;

; First, generate the map of regions
genfatinfo:
		mov edx,[bxSectors]
		and dx,dx
		jnz .have_secs
		mov edx,[bsHugeSectors]
.have_secs:
		mov [TotalSectors],edx

		mov eax,[bxResSectors]
		mov [FAT],eax			; Beginning of FAT
		mov edx,[bxFATsecs]
		and dx,dx
		jnz .have_fatsecs
		mov edx,[bootsec+36]		; FAT32 BPB_FATsz32
.have_fatsecs:
		imul edx,[bxFATs]
		add eax,edx
		mov [RootDirArea],eax		; Beginning of root directory
		mov [RootDir],eax		; For FAT12/16 == root dir location

		mov edx,[bxRootDirEnts]
		add dx,SECTOR_SIZE/32-1
		shr dx,SECTOR_SHIFT-5
		mov [RootDirSize],edx
		add eax,edx
		mov [DataArea],eax		; Beginning of data area

; Next, generate a cluster size shift count and mask
		mov eax,[bxSecPerClust]
		bsr cx,ax
		mov [ClustShift],cl
		push cx
		add cl,SECTOR_SHIFT
		mov [ClustByteShift],cl
		pop cx
		dec ax
		mov [ClustMask],eax
		inc ax
		shl eax,SECTOR_SHIFT
		mov [ClustSize],eax

;
; FAT12, FAT16 or FAT28^H^H32?  This computation is fscking ridiculous.
;
getfattype:
		mov eax,[TotalSectors]
		sub eax,[DataArea]
		shr eax,cl			; cl == ClustShift
		mov cl,nextcluster_fat12-(nextcluster+2)
		cmp eax,4085			; FAT12 limit
		jb .setsize
		mov cl,nextcluster_fat16-(nextcluster+2)
		cmp eax,65525			; FAT16 limit
		jb .setsize
		;
		; FAT32, root directory is a cluster chain
		;
		mov cl,[ClustShift]
		mov eax,[bootsec+44]		; Root directory cluster
		sub eax,2
		shl eax,cl
		add eax,[DataArea]
		mov [RootDir],eax
		mov cl,nextcluster_fat28-(nextcluster+2)
		mov byte [SuperSize],superblock_len_fat32
.setsize:
		mov byte [nextcluster+1],cl

;
; Common initialization code
;
%include "cpuinit.inc"
%include "init.inc"

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
		mov si,config_name	; Save configuration file name
		mov di,ConfigName
		call strcpy
		mov word [CurrentDirName],ROOT_DIR_WORD	; Write '/',0 to the CurrentDirName

		mov eax,[RootDir]	; Make the root directory ...
		mov [CurrentDir],eax	; ... the current directory
		mov di,syslinux_cfg1
		push di
		call open
		pop di
		jnz .config_open
		mov di,syslinux_cfg2
		push di
		call open
		pop di
		jnz .config_open
		mov di,syslinux_cfg3
		push di
		call open
		pop di
		jz no_config_file
.config_open:
		push si
		mov si,di
		push si
		mov di,CurrentDirName
			; This is inefficient as it will copy more than needed
			;   but not by too much
		call strcpy
		mov ax,config_name	;Cut it down
		pop si
		sub ax,si
		mov di,CurrentDirName
		add di,ax
		mov byte [di],0
		pop si
		mov eax,[PrevDir]	; Make the directory with syslinux.cfg ...
		mov [CurrentDir],eax	; ... the current directory

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"

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
; alloc_fill_dir:
;	Allocate then fill a file structure for a directory starting in
;	sector EAX.
;
;	Assumes DS == ES == CS.
;
;	     If successful:
;		ZF clear
;		SI	= file pointer
;	     If unsuccessful
;		ZF set
;		EAX clobbered
;
alloc_fill_dir:
		push bx
		call allocate_file
		jnz .alloc_failure
.found:
		mov si,bx
		mov [si+file_sector],eax	; Current sector
		mov dword [si+file_bytesleft],0	; Current offset
		mov [si+file_left],eax		; Beginning sector
		pop bx
		ret

.alloc_failure:
		pop bx
		xor eax,eax			; ZF <- 1
		ret

;
; search_dos_dir:
;	     Search a specific directory for a pre-mangled filename in
;            MangledBuf, in the directory starting in sector EAX.
;
;	     NOTE: This file considers finding a zero-length file an
;	     error.  This is so we don't have to deal with that special
;	     case elsewhere in the program (most loops have the test
;	     at the end).
;
;	     Assumes DS == ES == CS.
;
;	     If successful:
;		ZF clear
;		SI	= file pointer
;		EAX	= file length (MAY BE ZERO!)
;		DL	= file attribute
;               DH	= clobbered
;	     If unsuccessful
;		ZF set
;		EAX, SI, DX clobbered
;

search_dos_dir:
		push bx
		call allocate_file
		jnz .alloc_failure

		push cx
		push gs
		push es
		push ds
		pop es				; ES = DS

		; Compute the value of a possible VFAT longname
		; "last" entry (which, of course, comes first...)
		push ax
		push dx
		mov ax,[NameLen]
		add ax,12
		xor dx,dx
		mov cx,13
		div cx
		or al,40h
		mov [VFATInit],al
		mov [VFATNext],al
		pop dx
		pop ax

.scansector:
		; EAX <- directory sector to scan
		call getcachesector
		; GS:SI now points to this sector

		mov cx,SECTOR_SIZE/32		; 32 == directory entry size
.scanentry:
		cmp byte [gs:si],0
		jz .failure			; Hit directory high water mark
		cmp word [gs:si+11],0Fh		; Long filename
		jne .short_entry

		; Process a VFAT long entry
		pusha
		mov al,[gs:si]
		cmp al,[VFATNext]
		jne .not_us
		mov bl,[gs:si+13]
		test al,40h
		jz .match_csum
		; Get the initial checksum value
		mov [VFATCsum],bl
		jmp .done_csum
.match_csum:
		cmp bl,[VFATCsum]
		jne .not_us			; Checksum mismatch
.done_csum:
		and ax,03fh
		jz .not_us			; Can't be zero...
		dec ax
		mov [VFATNext],al		; Optimistically...
		mov bx,ax
		shl bx,2			; *4
		add ax,bx			; *5
		add bx,bx			; *8
		add bx,ax			; *13
		cmp bx,[NameLen]
		jae .not_us
		mov di,[NameStart]
		inc si
		mov cx,13
.vfat_cmp:
		gs lodsw
		push bx
		cmp bx,[NameLen]
		jae .vfat_tail
		movzx bx,byte [bx+di]
		add bx,bx
		cmp ax,[cp_unicode+bx]		; Primary case
		je .ucs_ok
		cmp ax,[cp_unicode_alt+bx]	; Alternate case
		je .ucs_ok
		; Mismatch...
		jmp .not_us_pop
.vfat_tail:
		; *AT* the end we should have 0x0000, *AFTER* the end
		; we should have 0xFFFF...
		je .vfat_end
		inc ax			; 0xFFFF -> 0x0000
.vfat_end:
		and ax,ax
		jnz .not_us_pop
.ucs_ok:
		pop bx
		inc bx
		cmp cx,3
		je .vfat_adj_add2
		cmp cx,9
		jne .vfat_adj_add0
.vfat_adj_add3:	inc si
.vfat_adj_add2:	inc si
.vfat_adj_add1:	inc si
.vfat_adj_add0:
		loop .vfat_cmp
		; Okay, if we got here we had a match on this particular
		; entry... live to see another one.
		popa
		jmp .next_entry

.not_us_pop:
		pop bx
.not_us:
		popa
		jmp .nomatch

.short_entry:
		test byte [gs:si+11],8		; Ignore volume labels
		jnz .nomatch

		cmp byte [VFATNext],0		; Do we have a longname match?
		jne .no_long_match

		; We already have a VFAT longname match, however,
		; the match is only valid if the checksum matches
		push cx
		push si
		push ax
		xor ax,ax
		mov cx,11
.csum_loop:
		gs lodsb
		ror ah,1
		add ah,al
		loop .csum_loop
		cmp ah,[VFATCsum]
		pop ax
		pop si
		pop cx
		je .found			; Got a match on longname

.no_long_match:					; Look for a shortname match
		push cx
		push si
		push di
		mov di,MangledBuf
		mov cx,11
		gs repe cmpsb
		pop di
		pop si
		pop cx
		je .found
.nomatch:
		; Reset the VFAT matching state machine
		mov dh,[VFATInit]
		mov [VFATNext],dh
.next_entry:
		add si,32
		dec cx
		jnz .scanentry

		call nextsector
		jnc .scansector			; CF is set if we're at end

		; If we get here, we failed
.failure:
		pop es
		pop gs
		pop cx
.alloc_failure:
		pop bx
		xor eax,eax			; ZF <- 1
		ret
.found:
		mov eax,[gs:si+28]		; File size
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT
		mov [bx+4],eax			; Sector count

		mov cl,[ClustShift]
		mov dx,[gs:si+20]		; High cluster word
		shl edx,16
		mov dx,[gs:si+26]		; Low cluster word
		sub edx,2
		shl edx,cl
		add edx,[DataArea]
		mov [bx],edx			; Starting sector

		mov eax,[gs:si+28]		; File length again
		mov dl,[gs:si+11]		; File attribute
		mov si,bx			; File pointer...
		and si,si			; ZF <- 0

		pop es
		pop gs
		pop cx
		pop bx
		ret

		section .data
		alignz 4
		; Note: we have no use of the first 32 bytes (header),
		; nor of the folloing 32 bytes (case mapping of control
		; characters), as long as we adjust the offsets appropriately.
codepage	equ $-(32+32)
codepage_data:	incbin "codepage.cp",32+32
cp_uppercase	equ	codepage+cp.uppercase
cp_unicode	equ	codepage+cp.unicode
cp_unicode_alt	equ	codepage+cp.unicode_alt
codepage_end	equ $

		section .text
;
; Input:  UCS-2 character in AX
; Output: Single byte character in AL, ZF = 1
;         On failure, returns ZF = 0
;
ucs2_to_cp:
		push es
		push di
		push cx
		push cs
		pop es
		mov di,cp_unicode
		mov cx,512
		repne scasw
		xchg ax,cx
		pop cx
		pop di
		pop es
		not ax		; Doesn't change the flags!
		ret

		section .bss
VFATInit	resb 1
VFATNext	resb 1
VFATCsum	resb 1

		section .text
;
; close_file:
;	     Deallocates a file structure (pointer in SI)
;	     Assumes CS == DS.
;
close_file:
		and si,si
		jz .closed
		mov dword [si],0		; First dword == file_sector
		xor si,si
.closed:	ret

;
; close_dir:
;	     Deallocates a directory structure (pointer in SI)
;	     Assumes CS == DS.
;
close_dir:
		and si,si
		jz .closed
		mov dword [si],0		; First dword == file_sector
		xor si,si
.closed:	ret

;
; searchdir:
;
;	Open a file
;
;	     On entry:
;		DS:DI	= filename
;	     If successful:
;		ZF clear
;		SI		= file pointer
;		EAX		= file length in bytes
;	     If unsuccessful
;		ZF set
;
; Assumes CS == DS == ES, and trashes BX and CX.
;
searchdir:
		mov eax,[CurrentDir]
		cmp byte [di],'/'	; Root directory?
		jne .notroot
		mov eax,[RootDir]
		inc di
.notroot:

.pathwalk:
		push eax		; <A> Current directory sector
		mov si,di
.findend:
		lodsb
		cmp al,' '
		jbe .endpath
		cmp al,'/'
		jne .findend
.endpath:
		xchg si,di		; GRC: si begin; di end[ /]+1
		pop eax			; <A> Current directory sector

			; GRC Here I need to check if di-1 = si which signifies
			;	we have the desired directory in EAX
			; What about where the file name = "."; later
		mov dx,di
		dec dx
		cmp dx,si
		jz .founddir

		mov [PrevDir],eax	; Remember last directory searched

		push di
		call mangle_dos_name	; MangledBuf <- component
		call search_dos_dir
		pop di
		jz .notfound		; Pathname component missing

		cmp byte [di-1],'/'	; Do we expect a directory
		je .isdir

		; Otherwise, it should be a file
.isfile:
		test dl,18h		; Subdirectory|Volume Label
		jnz .badfile		; If not a file, it's a bad thing

		; SI and EAX are already set
		mov [si+file_bytesleft],eax
		push eax
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT
		mov [si+file_left],eax	; Sectors left
		pop eax
		and eax,eax		; EAX != 0
		jz .badfile
		ret			; Done!

		; If we expected a directory, it better be one...
.isdir:
		test dl,10h		; Subdirectory
		jz .badfile

		xor eax,eax
		xchg eax,[si+file_sector] ; Get sector number and free file structure
		jmp .pathwalk		; Walk the next bit of the path

		; Found the desired directory; ZF set but EAX not 0
.founddir:
		ret

.badfile:
		xor eax,eax
		mov [si],eax		; Free file structure

.notfound:
		xor eax,eax		; Zero out EAX
		ret

;
; readdir: Read one file from a directory
;
;	ES:DI	-> String buffer (filename)
;	DS:SI	-> Pointer to open_file_t
;	DS	Must be the SYSLINUX Data Segment
;
;	Returns the file's name in the filename string buffer
;	EAX returns the file size
;	EBX returns the beginning sector (currently without offsetting)
;	DL returns the file type
;	The directory handle's data is incremented to reflect a name read.
;
readdir:
		push ecx
		push bp		; Using bp to transfer between segment registers
		push si
		push es
		push fs		; Using fs to store the current es (from COMBOOT)
		push gs
		mov bp,es
		mov fs,bp
		cmp si,0
		jz .fail
.load_handle:
		mov eax,[ds:si+file_sector]	; Current sector
		mov ebx,[ds:si+file_bytesleft]	; Current offset
		cmp eax,0
		jz .fail
.fetch_cache:
		call getcachesector
.move_current:
		add si,bx	; Resume last position in sector
		mov ecx,SECTOR_SIZE	; 0 out high part
		sub cx,bx
		shr cx,5	; Number of entries left
.scanentry:
		cmp byte [gs:si],0
		jz .fail
		cmp word [gs:si+11],0Fh		; Long filename
		jne .short_entry

.vfat_entry:
		push eax
		push ecx
		push si
		push di
.vfat_ln_info:		; Get info about the line that we're on
		mov al,[gs:si]
		test al,40h
		jz .vfat_tail_ln
		and al,03Fh
		mov ah,1	; On beginning line
		jmp .vfat_ck_ln

.vfat_tail_ln:	; VFAT tail line processing (later in VFAT, head in name)
		test al,80h	; Invalid data?
		jnz .vfat_abort
		mov ah,0	; Not on beginning line
		cmp dl,al
		jne .vfat_abort	; Is this the entry we need?
		mov bl,[gs:si+13]
		cmp bl,[VFATCsum]
		je .vfat_cp_ln
		jmp .vfat_abort

.vfat_ck_ln:		; Load this line's VFAT CheckSum
		mov bl,[gs:si+13]
		mov [VFATCsum],bl
.vfat_cp_ln:		; Copy VFAT line
		dec al		; Store the next line we need
		mov dx,ax	; Use DX to store the progress
		mov cx,13	; 13 characters per VFAT DIRENT
		cbw		; AH <- 0
		mul cl		; Offset for DI
		add di,ax	; Increment DI
		inc si		; Align to the real characters
.vfat_cp_chr:
		gs lodsw	; Unicode here!!
		call ucs2_to_cp	; Convert to local codepage
		jnz .vfat_abort	; Use short name if character not on codepage
		stosb		; CAN NOT OVERRIDE es
		cmp al,0
		jz .vfat_find_next ; Null-terminated string; don't process more
		cmp cx,3
		je .vfat_adj_add2
		cmp cx,9
		jne .vfat_adj_add0
.vfat_adj_add3:	inc si
.vfat_adj_add2:	inc si
.vfat_adj_add1:	inc si
.vfat_adj_add0:
		loop .vfat_cp_chr
		cmp dh,1	; Is this the first round?
		jnz .vfat_find_next
.vfat_null_term:	; Need to null-terminate if first line as we rolled over the end
		mov al,0
		stosb

.vfat_find_next:	;Find the next part of the name
		pop di
		pop si
		pop ecx
		pop eax
		cmp dl,0
		jz .vfat_find_info	; We're done with the name
		add si,DIRENT_SIZE
		dec cx
		jnz .vfat_entry
		call nextsector
		jnc .vfat_entry			; CF is set if we're at end
		jmp .fail
.vfat_find_info:	; Fetch next entry for the size/"INode"
		add si,DIRENT_SIZE
		dec cx
		jnz .get_info
		call nextsector
		jnc .get_info			; CF is set if we're at end
		jmp .fail
.vfat_abort:		; Something went wrong, skip
		pop di
		pop si
		pop ecx
		pop eax
		jmp .skip_entry

.short_entry:
		test byte [gs:si+11],8		; Ignore volume labels //HERE
		jnz .skip_entry
		mov edx,eax		;Save current sector
		push cx
		push si
		push di
		mov cx,8
.short_file:
		gs lodsb
		cmp al,'.'
		jz .short_dot
.short_file_loop:
		cmp al,' '
		jz .short_skip_bs
		stosb
		loop .short_file_loop
		jmp .short_period
.short_skip_bs:		; skip blank spaces in FILENAME (before EXT)
		add si,cx
		dec si
.short_period:
		mov al,'.'
		stosb
		mov cx,3
.short_ext:
		gs lodsb
		cmp al,' '
		jz .short_done
		stosb
		loop .short_ext
		jmp .short_done
.short_dot:
		stosb
		gs lodsb
		cmp al,' '
		jz .short_done
		stosb
.short_done:
		mov al,0	; Null-terminate the short strings
		stosb
		pop di
		pop si
		pop cx
		mov eax,edx
.get_info:
		mov ebx,[gs:si+28]	; length
		mov dl,[gs:si+11]	; type
.next_entry:
		add si,DIRENT_SIZE
		dec cx
		jnz .store_offset
		call nextsector
		jnc .store_sect			; CF is set if we're at end
		jmp .fail

.skip_entry:
		add si,DIRENT_SIZE
		dec cx
		jnz .scanentry
		call nextsector
		jnc .scanentry			; CF is set if we're at end
		jmp .fail

.store_sect:
		pop gs
		pop fs
		pop es
		pop si
		mov [ds:si+file_sector],eax
		mov eax,0	; Now at beginning of new sector
		jmp .success

.store_offset:
		pop gs
		pop fs
		pop es
		pop si		; cx=num remain; SECTOR_SIZE-(cx*32)=cur pos
		shl ecx,DIRENT_SHIFT
		mov eax,SECTOR_SIZE
		sub eax,ecx
		and eax,0ffffh

.success:
		mov [ds:si+file_bytesleft],eax
		; "INode" number = ((CurSector-RootSector)*SECTOR_SIZE + Offset)/DIRENT_SIZE)
		mov ecx,eax
		mov eax,[ds:si+file_sector]
		sub eax,[RootDir]
		shl eax,SECTOR_SHIFT
		add eax,ecx
		shr eax,DIRENT_SHIFT
		dec eax
		xchg eax,ebx	; -> EBX=INode, EAX=FileSize
		jmp .done

.fail:
		pop gs
		pop fs
		pop es
		pop si
		call close_dir
		xor eax,eax
		stc
.done:
		pop bp
		pop ecx
.end:
		ret

		section .bss
		alignb 4
CurrentDir	resd 1			; Current directory
PrevDir		resd 1			; Last scanned directory

		section .text

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
; mangle_name: Mangle a filename pointed to by DS:SI into a buffer pointed
;	       to by ES:DI; ends on encountering any whitespace.
;	       DI is preserved.
;
;	       This verifies that a filename is < FILENAME_MAX characters,
;	       doesn't contain whitespace, zero-pads the output buffer,
;	       and removes trailing dots and redundant slashes, plus changes
;              backslashes to forward slashes,
;	       so "repe cmpsb" can do a compare, and the path-searching routine
;              gets a bit of an easier job.
;
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
		cmp al,'\'			; Backslash?
		jne .mn_not_bs
		mov al,'/'			; Change to forward slash
.mn_not_bs:
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
		cmp byte [es:di-1],'.'		; Terminal dot?
		je .mn_kill
		cmp byte [es:di-1],'/'		; Terminal slash?
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
;                NOTE: A 13-byte buffer is mandatory, even if the string is
;                known to be shorter.
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
; mangle_dos_name:
;		Mangle a DOS filename component pointed to by DS:SI
;		into [MangledBuf]; ends on encountering any whitespace or
;		slash.
;
;		WARNING: saves pointers into the buffer for longname
;		matches!
;
;		Assumes CS == DS == ES.
;

mangle_dos_name:
		pusha
		mov di,MangledBuf
		mov [NameStart],si

		mov cx,11			; # of bytes to write
		mov bx,cp_uppercase		; Case-conversion table
.loop:
		lodsb
		cmp al,' '			; If control or space, end
		jna .end
		cmp al,'/'			; Slash, too
		je .end
		cmp al,'.'			; Period -> space-fill
		je .is_period
		xlatb				; Convert to upper case
		mov ah,cl			; If the first byte (only!)...
		cmp ax,0BE5h			; ... equals E5 hex ...
		jne .charok
		mov al,05h			; ... change it to 05 hex
.charok:	stosb
		loop .loop			; Don't continue if too long
		; Find the end for the benefit of longname search
.find_end:
		lodsb
		cmp al,' '
		jna .end
		cmp al,'/'
		jne .find_end
.end:
		dec si
		sub si,[NameStart]
		mov [NameLen],si
		mov al,' '			; Space-fill name
		rep stosb			; Doesn't do anything if CX=0
		popa
		ret				; Done

.is_period:
		mov al,' '			; We need to space-fill
.period_loop:	cmp cx,3			; If <= 3 characters left
		jbe .loop			; Just ignore it
		stosb				; Otherwise, write a space
		loop .period_loop		; Dec CX and *always* jump

		section .bss
		alignb 2
NameStart	resw 1
NameLen		resw 1
MangledBuf	resb 11

		section .text
;
; getfssec_edx: Get multiple sectors from a file
;
;	This routine makes sure the subtransfers do not cross a 64K boundary,
;	and will correct the situation if it does, UNLESS *sectors* cross
;	64K boundaries.
;
;	ES:BX	-> Buffer
;	EDX	-> Current sector number
;	CX	-> Sector count (0FFFFh = until end of file)
;                  Must not exceed the ES segment
;	Returns EDX=0, CF=1 on EOF (not necessarily error)
;	All arguments are advanced to reflect data read.
;
getfssec_edx:
		push ebp
		push eax
.getfragment:
		xor ebp,ebp			; Fragment sector count
		push edx			; Starting sector pointer
.getseccnt:
		inc bp
		dec cx
		jz .do_read
		xor eax,eax
		mov ax,es
		shl ax,4
		add ax,bx			; Now AX = how far into 64K block we are
		not ax				; Bytes left in 64K block
		inc eax
		shr eax,SECTOR_SHIFT		; Sectors left in 64K block
		cmp bp,ax
		jnb .do_read			; Unless there is at least 1 more sector room...
		mov eax,edx			; Current sector
		inc edx				; Predict it's the linearly next sector
		call nextsector
		jc .do_read
		cmp edx,eax			; Did it match?
		jz .getseccnt
.do_read:
		pop eax				; Starting sector pointer
		call getlinsecsr
		lea eax,[eax+ebp-1]		; This is the last sector actually read
		shl bp,9
		add bx,bp			; Adjust buffer pointer
		call nextsector
		jc .eof
		mov edx,eax
		and cx,cx
		jnz .getfragment
.done:
		pop eax
		pop ebp
		ret
.eof:
		xor edx,edx
		stc
		jmp .done

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
;	ECX returns number of bytes read.
;	All arguments are advanced to reflect data read.
;
getfssec:
		push edx
		movzx edx,cx
		push edx		; Zero-extended CX
		cmp edx,[si+file_left]
		jbe .sizeok
		mov edx,[si+file_left]
		mov cx,dx
.sizeok:
		sub [si+file_left],edx
		mov edx,[si+file_sector]
		call getfssec_edx
		mov [si+file_sector],edx
		pop ecx			; Sectors requested read
		shl ecx,SECTOR_SHIFT
		cmp ecx,[si+file_bytesleft]
		ja .eof
.noteof:
		sub [si+file_bytesleft],ecx	; CF <- 0
		pop edx
		ret
.eof:
		mov ecx,[si+file_bytesleft]
		call close_file
		pop edx
		stc
		ret

;
; nextcluster: Advance a cluster pointer in EDI to the next cluster
;	       pointed at in the FAT tables.  CF=0 on return if end of file.
;
nextcluster:
		jmp strict short nextcluster_fat28	; This gets patched

nextcluster_fat12:
		push eax
		push edx
		push bx
		push cx
		push si
		mov edx,edi
		shr edi,1
		pushf			; Save the shifted-out LSB (=CF)
		add edx,edi
		mov eax,edx
		shr eax,9
		call getfatsector
		mov bx,dx
		and bx,1FFh
		mov cl,[gs:si+bx]
		inc edx
		mov eax,edx
		shr eax,9
		call getfatsector
		mov bx,dx
		and bx,1FFh
		mov ch,[gs:si+bx]
		popf
		jnc .even
		shr cx,4
.even:		and cx,0FFFh
		movzx edi,cx
		cmp di,0FF0h
		pop si
		pop cx
		pop bx
		pop edx
		pop eax
		ret

;
; FAT16 decoding routine.
;
nextcluster_fat16:
		push eax
		push si
		push bx
		mov eax,edi
		shr eax,SECTOR_SHIFT-1
		call getfatsector
		mov bx,di
		add bx,bx
		and bx,1FEh
		movzx edi,word [gs:si+bx]
		cmp di,0FFF0h
		pop bx
		pop si
		pop eax
		ret
;
; FAT28 ("FAT32") decoding routine.
;
nextcluster_fat28:
		push eax
		push si
		push bx
		mov eax,edi
		shr eax,SECTOR_SHIFT-2
		call getfatsector
		mov bx,di
		add bx,bx
		add bx,bx
		and bx,1FCh
		mov edi,dword [gs:si+bx]
		and edi,0FFFFFFFh	; 28 bits only
		cmp edi,0FFFFFF0h
		pop bx
		pop si
		pop eax
		ret

;
; nextsector:	Given a sector in EAX on input, return the next sector
;		of the same filesystem object, which may be the root
;		directory or a cluster chain.  Returns  EOF.
;
;		Assumes CS == DS.
;
nextsector:
		push edi
		push edx
		mov edx,[DataArea]
		mov edi,eax
		sub edi,edx
		jae .isdata

		; Root directory
		inc eax
		cmp eax,edx
		cmc
		jmp .done

.isdata:
		not edi
		test edi,[ClustMask]
		jz .endcluster

		; It's not the final sector in a cluster
		inc eax
		jmp .done

.endcluster:
		push gs			; nextcluster trashes gs
		push cx
		not edi
		mov cl,[ClustShift]
		shr edi,cl
		add edi,2

		; Now EDI contains the cluster number
		call nextcluster
		cmc
		jc .exit		; There isn't anything else...

		; New cluster number now in EDI
		sub edi,2
		shl edi,cl		; CF <- 0, unless something is very wrong
		lea eax,[edi+edx]
.exit:
		pop cx
		pop gs
.done:
		pop edx
		pop edi
		ret

;
; getfatsector: Check for a particular sector (in EAX) in the FAT cache,
;		and return a pointer in GS:SI, loading it if needed.
;
;		Assumes CS == DS.
;
getfatsector:
		add eax,[FAT]		; FAT starting address
		jmp getcachesector

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
syslinux_cfg1	db '/boot'			; /boot/syslinux/syslinux.cfg
syslinux_cfg2	db '/syslinux'			; /syslinux/syslinux.cfg
syslinux_cfg3	db '/'				; /syslinux.cfg
config_name	db 'syslinux.cfg', 0		; syslinux.cfg

;
; Config file keyword table
;
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
;
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.bss'		; Boot Sector (add superblock)
		db '.bs', 0		; Boot Sector
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
