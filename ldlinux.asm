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
;   Copyright (C) 1994-2007   H. Peter Anvin
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
file_sector	resd 1			; Sector pointer (0 = structure free)
file_left	resd 1			; Number of sectors left
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
OrigFDCTabPtr	equ StackBuf-4		; The high dword on the stack

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
		mov es,ax
;
; DS:SI may contain a partition table entry.  Preserve it for us.
;
		mov cx,8		; Save partition info
		mov di,sp
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
		add cl,9
		mov [ClustByteShift],cl
		pop cx
		dec ax
		mov [ClustMask],eax
		inc ax
		shl eax,9
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
; Clear Files structures
;
		mov di,Files
		mov cx,(MAX_OPEN*open_file_t_size)/4
		xor eax,eax
		rep stosd

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

		mov di,syslinux_cfg1
		call open
		jnz .config_open
		mov di,syslinux_cfg2
		call open
		jnz .config_open
		mov di,syslinux_cfg3
		call open
		jz no_config_file
.config_open:
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
;		DL	= file attributes
;	     If unsuccessful
;		ZF set
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

.scansector:
		; EAX <- directory sector to scan
		call getcachesector
		; GS:SI now points to this sector

		mov cx,SECTOR_SIZE/32		; 32 == directory entry size
.scanentry:
		cmp byte [gs:si],0
		jz .failure			; Hit directory high water mark
		test byte [gs:si+11],8		; Ignore volume labels and
						; VFAT long filename entries
		jnz .nomatch
		push cx
		push si
		push di
		mov di,MangledBuf
		mov cx,11
		gs repe cmpsb
		pop di
		pop si
		pop cx
		jz .found
.nomatch:
		add si,32
		loop .scanentry

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
;
;	Open a file
;
;	     On entry:
;		DS:DI	= filename
;	     If successful:
;		ZF clear
;		SI		= file pointer
;		DX:AX or EAX	= file length in bytes
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
		xchg si,di
		pop eax			; <A> Current directory sector

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
		mov edx,eax
		shr edx,16		; Old 16-bit remnant...
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

.badfile:
		xor eax,eax
		mov [si],eax		; Free file structure

.notfound:
		xor eax,eax
		xor dx,dx
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
;		into [MangledBuf]; ends on encountering any whitespace or slash.
;		Assumes CS == DS == ES.
;

mangle_dos_name:
		pusha
		mov di,MangledBuf

		mov cx,11			; # of bytes to write
.loop:
		lodsb
		cmp al,' '			; If control or space, end
		jna .end
		cmp al,'/'			; Slash, too
		je .end
		cmp al,'.'			; Period -> space-fill
		je .is_period
		cmp al,'a'
		jb .not_lower
		cmp al,'z'
		ja .not_uslower
		sub al,020h
		jmp short .not_lower
.is_period:	mov al,' '			; We need to space-fill
.period_loop:	cmp cx,3			; If <= 3 characters left
		jbe .loop			; Just ignore it
		stosb				; Otherwise, write a period
		loop .period_loop		; Dec CX and (always) jump
.not_uslower:	cmp al,ucase_low
		jb .not_lower
		cmp al,ucase_high
		ja .not_lower
		mov bx,ucase_tab-ucase_low
                xlatb
.not_lower:	stosb
		loop .loop			; Don't continue if too long
.end:
		mov al,' '			; Space-fill name
		rep stosb			; Doesn't do anything if CX=0
		popa
		ret				; Done

		section .bss
MangledBuf	resb 11

		section .text
;
; Case tables for extended characters; this is technically code page 865,
; but code page 437 users will probably not miss not being able to use the
; cent sign in kernel images too much :-)
;
; The table only covers the range 129 to 164; the rest we can deal with.
;
		section .data

ucase_low	equ 129
ucase_high	equ 164
ucase_tab	db 154, 144, 'A', 142, 'A', 143, 128, 'EEEIII'
		db 142, 143, 144, 146, 146, 'O', 153, 'OUUY', 153, 154
		db 157, 156, 157, 158, 159, 'AIOU', 165

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
;	All arguments are advanced to reflect data read.
;
getfssec:
		push edx
		movzx edx,cx
		cmp edx,[si+4]
		jbe .sizeok
		mov edx,[si+4]
		mov cx,dx
.sizeok:
		sub [si+4],edx
		mov edx,[si]
		call getfssec_edx
		mov [si],edx
		pop edx
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
BufSafeSec	dw trackbufsize/SECTOR_SIZE	; = how many sectors?
BufSafeBytes	dw trackbufsize		; = how many bytes?
EndOfGetCBuf	dw getcbuf+trackbufsize	; = getcbuf+BufSafeBytes
%ifndef DEPEND
%if ( trackbufsize % SECTOR_SIZE ) != 0
%error trackbufsize must be a multiple of SECTOR_SIZE
%endif
%endif
