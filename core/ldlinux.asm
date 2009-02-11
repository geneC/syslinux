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
;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
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
; Segment assignments in the bottom 640K
; Stick to the low 512K in case we're using something like M-systems flash
; which load a driver into low RAM (evil!!)
;
; 0000h - main code/data segment (and BIOS segment)
;
real_mode_seg	equ 3000h
cache_seg	equ 2000h		; 64K area for metadata cache
xfer_buf_seg	equ 1000h		; Bounce buffer for I/O to high mem
comboot_seg	equ real_mode_seg	; COMBOOT image loading zone

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
		alignb 8

		; Expanded superblock
SuperInfo	equ $
		resq 16			; The first 16 bytes expanded 8 times
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
_start:		jmp short start		; 2 bytes
		nop			; 1 byte
;
; "Superblock" follows -- it's in the boot sector, so it's already
; loaded and ready for us
;
bsOemName	db 'SYSLINUX'		; The SYS command sets this, so...
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

SecPerClust	equ bxSecPerClust
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
%if IS_MDSLINUX
		db 'MDSLINUX '
%else
		db 'SYSLINUX '
%endif
		db VERSION_STR, ' ', DATE_STR, ' ', 0
		db 0Dh, 0Ah, 1Ah	; EOF if we "type" this in DOS

		align 8, db 0
ldlinux_magic	dd LDLINUX_MAGIC
		dd LDLINUX_MAGIC^HEXDATE

;
; This area is patched by the installer.  It is found by looking for
; LDLINUX_MAGIC, plus 8 bytes.
;
patch_area:
LDLDwords	dw 0		; Total dwords starting at ldlinux_sys
LDLSectors	dw 0		; Number of sectors - (bootsec+this sec)
CheckSum	dd 0		; Checksum starting at ldlinux_sys
				; value = LDLINUX_MAGIC - [sum of dwords]

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
		call writestr_early

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
		call writestr_early

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
		call writestr_early
		jmp kaboom

;
; -----------------------------------------------------------------------------
; Subroutines that have to be in the first sector
; -----------------------------------------------------------------------------

;
;
; writestr_early: write a null-terminated string to the console
;	    This assumes we're on page 0.  This is only used for early
;           messages, so it should be OK.
;
writestr_early:
		pushad
.loop:		lodsb
		and al,al
                jz .return
		mov ah,0Eh		; Write to screen as TTY
		mov bx,0007h		; Attribute
		int 10h
		jmp short .loop
.return:	popad
		ret


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
		call writestr_early


;
; Insane hack to expand the superblock to dwords
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
		align 4, db 0
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
%include "adv.inc"		; Auxillary Data Vector
%include "localboot.inc"	; Disk-based local boot

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data
copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
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

		alignb 4, db 0
BufSafe		dw trackbufsize/SECTOR_SIZE	; Clusters we can load into trackbuf
BufSafeBytes	dw trackbufsize		; = how many bytes?
%ifndef DEPEND
%if ( trackbufsize % SECTOR_SIZE ) != 0
%error trackbufsize must be a multiple of SECTOR_SIZE
%endif
%endif
