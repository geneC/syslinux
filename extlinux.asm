; -*- fundamental -*- (asm-mode sucks)
; ****************************************************************************
;
;  extlinux.asm
;
;  A program to boot Linux kernels off an ext2/ext3 filesystem.
;
;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
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

;
; This is what we need to do when idle
;
%macro	RESET_IDLE 0
	; Nothing
%endmacro
%macro	DO_IDLE 0
	; Nothing
%endmacro

;
; The following structure is used for "virtual kernels"; i.e. LILO-style
; option labels.  The options we permit here are `kernel' and `append
; Since there is no room in the bottom 64K for all of these, we
; stick them at vk_seg:0000 and copy them down before we need them.
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
; Segment assignments in the bottom 640K
; Stick to the low 512K in case we're using something like M-systems flash
; which load a driver into low RAM (evil!!)
;
; 0000h - main code/data segment (and BIOS segment)
;
real_mode_seg	equ 4000h
cache_seg	equ 3000h		; 64K area for metadata cache
vk_seg          equ 2000h		; Virtual kernels
xfer_buf_seg	equ 1000h		; Bounce buffer for I/O to high mem
comboot_seg	equ real_mode_seg	; COMBOOT image loading zone

;
; File structure.  This holds the information for each currently open file.
;
		struc open_file_t
file_left	resd 1			; Number of sectors left (0 = free)
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
getcbuf		resb trackbufsize
		; ends at 4800h

		section .bss1
SuperBlock	resb 1024		; ext2 superblock
SuperInfo	resq 16			; DOS superblock expanded
ClustSize	resd 1			; Bytes/cluster ("block")
SecPerClust	resd 1			; Sectors/cluster
ClustMask	resd 1			; Sectors/cluster - 1
PtrsPerBlock1	resd 1			; Pointers/cluster
PtrsPerBlock2	resd 1			; (Pointers/cluster)^2
DriveNumber	resb 1			; BIOS drive number
ClustShift	resb 1			; Shift count for sectors/cluster
ClustByteShift	resb 1			; Shift count for bytes/cluster

		alignb open_file_t_size
Files		resb MAX_OPEN*open_file_t_size

;
; Constants for the xfer_buf_seg
;
; The xfer_buf_seg is also used to store message file buffers.  We
; need two trackbuffers (text and graphics), plus a work buffer
; for the graphics decompressor.
;
xbs_textbuf	equ 0			; Also hard-coded, do not change
xbs_vgabuf	equ trackbufsize
xbs_vgatmpbuf	equ 2*trackbufsize


		section .text
;
; Some of the things that have to be saved very early are saved
; "close" to the initial stack pointer offset, in order to
; reduce the code size...
;
StackBuf	equ $-44-32		; Start the stack here (grow down - 4K)
PartInfo	equ StackBuf		; Saved partition table entry
FloppyTable	equ PartInfo+16		; Floppy info table (must follow PartInfo)
OrigFDCTabPtr	equ StackBuf-8		; The 2nd high dword on the stack
OrigESDI	equ StackBuf-4		; The high dword on the stack

;
; Primary entry point.  Tempting as though it may be, we can't put the
; initial "cli" here; the jmp opcode in the first byte is part of the
; "magic number" (using the term very loosely) for the DOS superblock.
;
bootsec		equ $
		jmp short start		; 2 bytes
		nop			; 1 byte
;
; "Superblock" follows -- it's in the boot sector, so it's already
; loaded and ready for us
;
bsOemName	db 'EXTLINUX'		; The SYS command sets this, so...
;
; These are the fields we actually care about.  We end up expanding them
; all to dword size early in the code, so generate labels for both
; the expanded and unexpanded versions.
;
%macro		superb 1
bx %+ %1	equ SuperInfo+($-superblock)*8+4
bs %+ %1	equ $
		zb 1
%endmacro
%macro		superw 1
bx %+ %1	equ SuperInfo+($-superblock)*8
bs %+ %1	equ $
		zw 1
%endmacro
%macro		superd 1
bx %+ %1	equ $			; no expansion for dwords
bs %+ %1	equ $
		zd 1
%endmacro
superblock	equ $
		superw BytesPerSec
		superb SecPerClust
		superw ResSectors
		superb FATs
		superw RootDirEnts
		superw Sectors
		superb Media
		superw FATsecs
		superw SecPerTrack
		superw Heads
superinfo_size	equ ($-superblock)-1	; How much to expand
		superd Hidden
		superd HugeSectors
		;
		; This is as far as FAT12/16 and FAT32 are consistent
		;
		zb 54			; FAT12/16 need 26 more bytes,
					; FAT32 need 54 more bytes
superblock_len	equ $-superblock

;
; Note we don't check the constraints above now; we did that at install
; time (we hope!)
;
start:
		cli			; No interrupts yet, please
		cld			; Copy upwards
;
; Set up the stack
;
		xor ax,ax
		mov ss,ax
		mov sp,StackBuf		; Just below BSS
		push es			; Save initial ES:DI -> $PnP pointer
		push di
		mov es,ax
;
; DS:SI may contain a partition table entry.  Preserve it for us.
;
		mov cx,8		; Save partition info
		mov di,PartInfo
		rep movsw

		mov ds,ax		; Now we can initialize DS...

;
; Now sautee the BIOS floppy info block to that it will support decent-
; size transfers; the floppy block is 11 bytes and is stored in the
; INT 1Eh vector (brilliant waste of resources, eh?)
;
; Of course, if BIOSes had been properly programmed, we wouldn't have
; had to waste precious space with this code.
;
		mov bx,fdctab
		lfs si,[bx]		; FS:SI -> original fdctab
		push fs			; Save on stack in case we need to bail
		push si

		; Save the old fdctab even if hard disk so the stack layout
		; is the same.  The instructions above do not change the flags
		mov [DriveNumber],dl	; Save drive number in DL
		and dl,dl		; If floppy disk (00-7F), assume no
					; partition table
		js harddisk

floppy:
		mov cl,6		; 12 bytes (CX == 0)
		; es:di -> FloppyTable already
		; This should be safe to do now, interrupts are off...
		mov [bx],di		; FloppyTable
		mov [bx+2],ax		; Segment 0
		fs rep movsw		; Faster to move words
		mov cl,[bsSecPerTrack]  ; Patch the sector count
		mov [di-8],cl
		; AX == 0 here
		int 13h			; Some BIOSes need this

		jmp short not_harddisk
;
; The drive number and possibly partition information was passed to us
; by the BIOS or previous boot loader (MBR).  Current "best practice" is to
; trust that rather than what the superblock contains.
;
; Would it be better to zero out bsHidden if we don't have a partition table?
;
; Note: di points to beyond the end of PartInfo
;
harddisk:
		test byte [di-16],7Fh	; Sanity check: "active flag" should
		jnz no_partition	; be 00 or 80
		mov eax,[di-8]		; Partition offset (dword)
		mov [bsHidden],eax
no_partition:
;
; Get disk drive parameters (don't trust the superblock.)  Don't do this for
; floppy drives -- INT 13:08 on floppy drives will (may?) return info about
; what the *drive* supports, not about the *media*.  Fortunately floppy disks
; tend to have a fixed, well-defined geometry which is stored in the superblock.
;
		; DL == drive # still
		mov ah,08h
		int 13h
		jc no_driveparm
		and ah,ah
		jnz no_driveparm
		shr dx,8
		inc dx			; Contains # of heads - 1
		mov [bsHeads],dx
		and cx,3fh
		mov [bsSecPerTrack],cx
no_driveparm:
not_harddisk:
;
; Ready to enable interrupts, captain
;
		sti

;
; Do we have EBIOS (EDD)?
;
eddcheck:
		mov bx,55AAh
		mov ah,41h		; EDD existence query
		mov dl,[DriveNumber]
		int 13h
		jc .noedd
		cmp bx,0AA55h
		jne .noedd
		test cl,1		; Extended disk access functionality set
		jz .noedd
		;
		; We have EDD support...
		;
		mov byte [getlinsec.jmp+1],(getlinsec_ebios-(getlinsec.jmp+2))
.noedd:

;
; Load the first sector of LDLINUX.SYS; this used to be all proper
; with parsing the superblock and root directory; it doesn't fit
; together with EBIOS support, unfortunately.
;
		mov eax,[FirstSector]	; Sector start
		mov bx,ldlinux_sys	; Where to load it
		call getonesec

		; Some modicum of integrity checking
		cmp dword [ldlinux_magic+4],LDLINUX_MAGIC^HEXDATE
		jne kaboom

		; Go for it...
		jmp ldlinux_ent

;
; getonesec: get one disk sector
;
getonesec:
		mov bp,1		; One sector
		; Fall through

;
; getlinsec: load a sequence of BP floppy sector given by the linear sector
;	     number in EAX into the buffer at ES:BX.  We try to optimize
;	     by loading up to a whole track at a time, but the user
;	     is responsible for not crossing a 64K boundary.
;	     (Yes, BP is weird for a count, but it was available...)
;
;	     On return, BX points to the first byte after the transferred
;	     block.
;
;            This routine assumes CS == DS, and trashes most registers.
;
; Stylistic note: use "xchg" instead of "mov" when the source is a register
; that is dead from that point; this saves space.  However, please keep
; the order to dst,src to keep things sane.
;
getlinsec:
		add eax,[bsHidden]		; Add partition offset
		xor edx,edx			; Zero-extend LBA (eventually allow 64 bits)

.jmp:		jmp strict short getlinsec_cbios

;
; getlinsec_ebios:
;
; getlinsec implementation for EBIOS (EDD)
;
getlinsec_ebios:
.loop:
                push bp                         ; Sectors left
.retry2:
		call maxtrans			; Enforce maximum transfer size
		movzx edi,bp			; Sectors we are about to read
		mov cx,retry_count
.retry:

		; Form DAPA on stack
		push edx
		push eax
		push es
		push bx
		push di
		push word 16
		mov si,sp
		pushad
                mov dl,[DriveNumber]
		push ds
		push ss
		pop ds				; DS <- SS
                mov ah,42h                      ; Extended Read
		int 13h
		pop ds
		popad
		lea sp,[si+16]			; Remove DAPA
		jc .error
		pop bp
		add eax,edi			; Advance sector pointer
		sub bp,di			; Sectors left
                shl di,SECTOR_SHIFT		; 512-byte sectors
                add bx,di			; Advance buffer pointer
                and bp,bp
                jnz .loop

                ret

.error:
		; Some systems seem to get "stuck" in an error state when
		; using EBIOS.  Doesn't happen when using CBIOS, which is
		; good, since some other systems get timeout failures
		; waiting for the floppy disk to spin up.

		pushad				; Try resetting the device
		xor ax,ax
		mov dl,[DriveNumber]
		int 13h
		popad
		loop .retry			; CX-- and jump if not zero

		;shr word [MaxTransfer],1	; Reduce the transfer size
		;jnz .retry2

		; Total failure.  Try falling back to CBIOS.
		mov byte [getlinsec.jmp+1],(getlinsec_cbios-(getlinsec.jmp+2))
		;mov byte [MaxTransfer],63	; Max possibe CBIOS transfer

		pop bp
		; ... fall through ...

;
; getlinsec_cbios:
;
; getlinsec implementation for legacy CBIOS
;
getlinsec_cbios:
.loop:
		push edx
		push eax
		push bp
		push bx

		movzx esi,word [bsSecPerTrack]
		movzx edi,word [bsHeads]
		;
		; Dividing by sectors to get (track,sector): we may have
		; up to 2^18 tracks, so we need to use 32-bit arithmetric.
		;
		div esi
		xor cx,cx
		xchg cx,dx		; CX <- sector index (0-based)
					; EDX <- 0
		; eax = track #
		div edi			; Convert track to head/cyl

		; We should test this, but it doesn't fit...
		; cmp eax,1023
		; ja .error

		;
		; Now we have AX = cyl, DX = head, CX = sector (0-based),
		; BP = sectors to transfer, SI = bsSecPerTrack,
		; ES:BX = data target
		;

		call maxtrans			; Enforce maximum transfer size

		; Must not cross track boundaries, so BP <= SI-CX
		sub si,cx
		cmp bp,si
		jna .bp_ok
		mov bp,si
.bp_ok:

		shl ah,6		; Because IBM was STOOPID
					; and thought 8 bits were enough
					; then thought 10 bits were enough...
		inc cx			; Sector numbers are 1-based, sigh
		or cl,ah
		mov ch,al
		mov dh,dl
		mov dl,[DriveNumber]
		xchg ax,bp		; Sector to transfer count
		mov ah,02h		; Read sectors
		mov bp,retry_count
.retry:
		pushad
		int 13h
		popad
		jc .error
.resume:
		movzx ecx,al		; ECX <- sectors transferred
		shl ax,SECTOR_SHIFT	; Convert sectors in AL to bytes in AX
		pop bx
		add bx,ax
		pop bp
		pop eax
		pop edx
		add eax,ecx
		sub bp,cx
		jnz .loop
		ret

.error:
		dec bp
		jnz .retry

		xchg ax,bp		; Sectors transferred <- 0
		shr word [MaxTransfer],1
		jnz .resume
		; Fall through to disk_error

;
; kaboom: write a message and bail out.
;
disk_error:
kaboom:
		xor si,si
		mov ss,si
		mov sp,StackBuf-4	; Reset stack
		mov ds,si		; Reset data segment
		pop dword [fdctab]	; Restore FDC table
.patch:					; When we have full code, intercept here
		mov si,bailmsg

		; Write error message, this assumes screen page 0
.loop:		lodsb
		and al,al
                jz .done
		mov ah,0Eh		; Write to screen as TTY
		mov bx,0007h		; Attribute
		int 10h
		jmp short .loop
.done:
		cbw			; AH <- 0
.again:		int 16h			; Wait for keypress
					; NB: replaced by int 18h if
					; chosen at install time..
		int 19h			; And try once more to boot...
.norge:		jmp short .norge	; If int 19h returned; this is the end

;
; Truncate BP to MaxTransfer
;
maxtrans:
		cmp bp,[MaxTransfer]
		jna .ok
		mov bp,[MaxTransfer]
.ok:		ret

;
; Error message on failure
;
bailmsg:	db 'Boot error', 0Dh, 0Ah, 0

		; This fails if the boot sector overflows
		zb 1F8h-($-$$)

FirstSector	dd 0xDEADBEEF			; Location of sector 1
MaxTransfer	dw 0x007F			; Max transfer size

; This field will be filled in 0xAA55 by the installer, but we abuse it
; to house a pointer to the INT 16h instruction at
; kaboom.again, which gets patched to INT 18h in RAID mode.
bootsignature	dw kaboom.again-bootsec

;
; ===========================================================================
;  End of boot sector
; ===========================================================================
;  Start of LDLINUX.SYS
; ===========================================================================

ldlinux_sys:

syslinux_banner	db 0Dh, 0Ah
		db 'EXTLINUX '
		db version_str, ' ', date, ' ', 0
		db 0Dh, 0Ah, 1Ah	; EOF if we "type" this in DOS

		align 8, db 0
ldlinux_magic	dd LDLINUX_MAGIC
		dd LDLINUX_MAGIC^HEXDATE

;
; This area is patched by the installer.  It is found by looking for
; LDLINUX_MAGIC, plus 8 bytes.
;
patch_area:
LDLDwords	dw 0		; Total dwords starting at ldlinux_sys,
				; not including ADVs
LDLSectors	dw 0		; Number of sectors, not including
				; bootsec & this sec, but including the two ADVs
CheckSum	dd 0		; Checksum starting at ldlinux_sys
				; value = LDLINUX_MAGIC - [sum of dwords]
CurrentDir	dd 2		; "Current" directory inode number

; Space for up to 64 sectors, the theoretical maximum
SectorPtrs	times 64 dd 0

ldlinux_ent:
;
; Note that some BIOSes are buggy and run the boot sector at 07C0:0000
; instead of 0000:7C00 and the like.  We don't want to add anything
; more to the boot sector, so it is written to not assume a fixed
; value in CS, but we don't want to deal with that anymore from now
; on.
;
		jmp 0:.next
.next:

;
; Tell the user we got this far
;
		mov si,syslinux_banner
		call writestr

;
; Tell the user if we're using EBIOS or CBIOS
;
print_bios:
		mov si,cbios_name
		cmp byte [getlinsec.jmp+1],(getlinsec_ebios-(getlinsec.jmp+2))
		jne .cbios
		mov si,ebios_name
.cbios:
		mov [BIOSName],si
		call writestr

		section .bss
%define	HAVE_BIOSNAME 1
BIOSName	resw 1

		section .text
;
; Now we read the rest of LDLINUX.SYS.	Don't bother loading the first
; sector again, though.
;
load_rest:
		mov si,SectorPtrs
		mov bx,7C00h+2*SECTOR_SIZE	; Where we start loading
		mov cx,[LDLSectors]

.get_chunk:
		jcxz .done
		xor bp,bp
		lodsd				; First sector of this chunk

		mov edx,eax

.make_chunk:
		inc bp
		dec cx
		jz .chunk_ready
		inc edx				; Next linear sector
		cmp [si],edx			; Does it match
		jnz .chunk_ready		; If not, this is it
		add si,4			; If so, add sector to chunk
		jmp short .make_chunk

.chunk_ready:
		call getlinsecsr
		shl bp,SECTOR_SHIFT
		add bx,bp
		jmp .get_chunk

.done:

;
; All loaded up, verify that we got what we needed.
; Note: the checksum field is embedded in the checksum region, so
; by the time we get to the end it should all cancel out.
;
verify_checksum:
		mov si,ldlinux_sys
		mov cx,[LDLDwords]
		mov edx,-LDLINUX_MAGIC
.checksum:
		lodsd
		add edx,eax
		loop .checksum

		and edx,edx			; Should be zero
		jz all_read			; We're cool, go for it!

;
; Uh-oh, something went bad...
;
		mov si,checksumerr_msg
		call writestr
		jmp kaboom

;
; -----------------------------------------------------------------------------
; Subroutines that have to be in the first sector
; -----------------------------------------------------------------------------

;
;
; writestr: write a null-terminated string to the console
;	    This assumes we're on page 0.  This is only used for early
;           messages, so it should be OK.
;
writestr:
.loop:		lodsb
		and al,al
                jz .return
		mov ah,0Eh		; Write to screen as TTY
		mov bx,0007h		; Attribute
		int 10h
		jmp short .loop
.return:	ret


; getlinsecsr: save registers, call getlinsec, restore registers
;
getlinsecsr:	pushad
		call getlinsec
		popad
		ret

;
; Checksum error message
;
checksumerr_msg	db ' Load error - ', 0	; Boot failed appended

;
; BIOS type string
;
cbios_name	db 'CBIOS', 0
ebios_name	db 'EBIOS', 0

;
; Debug routine
;
%ifdef debug
safedumpregs:
		cmp word [Debug_Magic],0D00Dh
		jnz nc_return
		jmp dumpregs
%endif

rl_checkpt	equ $				; Must be <= 8000h

rl_checkpt_off	equ ($-$$)
%ifndef DEPEND
%if rl_checkpt_off > 400h
%error "Sector 1 overflow"
%endif
%endif

; ----------------------------------------------------------------------------
;  End of code and data that have to be in the first sector
; ----------------------------------------------------------------------------

all_read:
;
; Let the user (and programmer!) know we got this far.  This used to be
; in Sector 1, but makes a lot more sense here.
;
		mov si,copyright_str
		call writestr

;
; Insane hack to expand the DOS superblock to dwords
;
expand_super:
		xor eax,eax
		mov si,superblock
		mov di,SuperInfo
		mov cx,superinfo_size
.loop:
		lodsw
		dec si
		stosd				; Store expanded word
		xor ah,ah
		stosd				; Store expanded byte
		loop .loop

;
; Load the real (ext2) superblock; 1024 bytes long at offset 1024
;
		mov bx,SuperBlock
		mov eax,1024 >> SECTOR_SHIFT
		mov bp,ax
		call getlinsec

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
;		DX:AX = EAX = file length in bytes
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
		push eax
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT
		mov [bx+file_left],eax
		pop eax
		mov si,bx
		mov edx,eax
		shr edx,16			; 16-bitism, sigh
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
		mov dword [si],0		; First dword == file_left
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
		mov bx,trackbuf
		push bx
		mov cx,[SecPerClust]
		call getfssec
		pop bx
		pushf			; Save EOF flag
		push si			; Save filesystem pointer
.getent:
		cmp dword [bx+d_inode],0
		je .endblock

		push di
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
;
; kaboom2: once everything is loaded, replace the part of kaboom
;	   starting with "kaboom.patch" with this part

kaboom2:
		mov si,err_bootfailed
		call cwritestr
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
;	All arguments are advanced to reflect data read.
;
getfssec:
		push ebp
		push eax
		push edx
		push edi

		movzx ecx,cx
		cmp ecx,[si]			; Number of sectors left
		jbe .lenok
		mov cx,[si]
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
		sub [si],ebp			; Sectors consumed
		jcxz .done
		jnz .getfragment
		; Fall through
.done:
		cmp dword [si],1		; Did we run out of file?
		; CF set if [SI] < 1, i.e. == 0
		pop edi
		pop edx
		pop eax
		pop ebp
		ret

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "plaincon.inc"		; writechr
%include "writestr.inc"		; String output
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
%include "adv.inc"		; Auxillary Data Vector

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data
copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: please change disks and press '
		db 'a key to continue.', CR, LF, 0
config_name	db 'extlinux.conf',0		; Unmangled form

;
; Command line options we'd like to take a look at
;
; mem= and vga= are handled as normal 32-bit integer values
initrd_cmd	db 'initrd='
initrd_cmd_len	equ 7

;
; Config file keyword table
;
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
;
		align 4, db 0
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

		alignb 4, db 0
BufSafe		dw trackbufsize/SECTOR_SIZE	; Clusters we can load into trackbuf
BufSafeBytes	dw trackbufsize		; = how many bytes?
%ifndef DEPEND
%if ( trackbufsize % SECTOR_SIZE ) != 0
%error trackbufsize must be a multiple of SECTOR_SIZE
%endif
%endif
