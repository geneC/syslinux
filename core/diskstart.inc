; -----------------------------------------------------------------------
;
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;   Boston MA 02110-1301, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; diskstart.inc
;
; Common early-bootstrap code for harddisk-based Syslinux derivatives.
;

		section .init
;
; Some of the things that have to be saved very early are saved
; "close" to the initial stack pointer offset, in order to
; reduce the code size...
;

StackBuf	equ STACK_TOP-44-92	; Start the stack here (grow down - 4K)
PartInfo	equ StackBuf
.mbr		equ PartInfo
.gptlen		equ PartInfo+16
.gpt		equ PartInfo+20
FloppyTable	equ PartInfo+76
; Total size of PartInfo + FloppyTable == 76+16 = 92 bytes
Hidden		equ StackBuf-24		; Partition offset (qword)
OrigFDCTabPtr	equ StackBuf-16		; Original FDC table
OrigDSSI	equ StackBuf-12		; DS:SI -> partinfo
OrigESDI	equ StackBuf-8		; ES:DI -> $PnP structure
DriveNumber	equ StackBuf-4		; Drive number
StackHome	equ Hidden		; The start of the canonical stack

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
bsOemName	db MY_NAME		; The SYS command sets this, so...
		zb 8-($-bsOemName)

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
		; FAT12/16 need 26 more bytes,
		; FAT32 need 54 more bytes
		;
superblock_len_fat16	equ $-superblock+26
superblock_len_fat32	equ $-superblock+54
		zb 54			; Maximum needed size
superblock_max	equ $-superblock

		global SecPerClust
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
		xor cx,cx
		mov ss,cx
		mov sp,StackBuf-2	; Just below BSS (-2 for alignment)
		push dx			; Save drive number (in DL)
		push es			; Save initial ES:DI -> $PnP pointer
		push di
		push ds			; Save original DS:SI -> partinfo
		push si
		mov es,cx

;
; DS:SI may contain a partition table entry and possibly a GPT entry.
; Preserve it for us.  This saves 56 bytes of the GPT entry, which is
; currently the maximum we care about.  Total is 76 bytes.
;
		mov cl,(16+4+56)/2	; Save partition info
		mov di,PartInfo
		rep movsw		; This puts CX back to zero

		mov ds,cx		; Now we can initialize DS...

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
		and dl,dl		; If floppy disk (00-7F), assume no
					; partition table
		js harddisk

floppy:
		xor ax,ax
		mov cl,6		; 12 bytes (CX == 0)
		; es:di -> FloppyTable already
		; This should be safe to do now, interrupts are off...
		mov [bx],di		; FloppyTable
		mov [bx+2],ax		; Segment 0
		fs rep movsw		; Faster to move words
		mov cl,[bsSecPerTrack]  ; Patch the sector count
		mov [di-76+8],cl

		push ax			; Partition offset == 0
		push ax
		push ax
		push ax

		int 13h			; Some BIOSes need this
		jmp short not_harddisk
;
; The drive number and possibly partition information was passed to us
; by the BIOS or previous boot loader (MBR).  Current "best practice" is to
; trust that rather than what the superblock contains.
;
; Note: di points to beyond the end of PartInfo
;
harddisk:
		mov dx,[di-76-10]	; Original DS
		mov si,[di-76-12]	; Original SI
		shr si,4
		add dx,si
		cmp dx,PartInfo >> 4
		jae .no_partition
		test byte [di-76],7Fh	; Sanity check: "active flag" should
		jnz .no_partition	; be 00 or 80
		cmp [di-76+4],cl	; Sanity check: partition type != 0
		je .no_partition
		cmp eax,'!GPT'		; !GPT signature?
		jne .mbr
		cmp byte [di-76+4],0EDh	; Synthetic GPT partition entry?
		jne .mbr
.gpt:					; GPT-style partition info
		push dword [di-76+20+36]
		push dword [di-76+20+32]
		jmp .gotoffs
.mbr:					; MBR-style partition info
		push cx			; Upper half partition offset == 0
		push cx
		push dword [di-76+8]	; Partition offset (dword)
		jmp .gotoffs
.no_partition:
;
; No partition table given... assume that the Hidden field in the boot sector
; tells the truth (in particular, is zero if this is an unpartitioned disk.)
;
		push cx
		push cx
		push dword [bsHidden]
.gotoffs:
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
		mov byte [getonesec.jmp+1],(getonesec_ebios-(getonesec.jmp+2))
.noedd:

;
; Load the first sector of LDLINUX.SYS; this used to be all proper
; with parsing the superblock and root directory; it doesn't fit
; together with EBIOS support, unfortunately.
;
		mov eax,strict dword 0xdeadbeef
Sect1Ptr0	equ $-4
		mov edx,strict dword 0xfeedface
Sect1Ptr1	equ $-4
		mov bx,ldlinux_sys	; Where to load it
		call getonesec

		; Some modicum of integrity checking
		cmp dword [ldlinux_magic+4],LDLINUX_MAGIC^HEXDATE
		jne kaboom

		; Go for it...
		jmp 0:ldlinux_ent


;
; getonesec: load a single disk linear sector EDX:EAX into the buffer
;	     at ES:BX.
;
;            This routine assumes CS == DS == SS, and trashes most registers.
;
; Stylistic note: use "xchg" instead of "mov" when the source is a register
; that is dead from that point; this saves space.  However, please keep
; the order to dst,src to keep things sane.
;
getonesec:
		add eax,[Hidden]		; Add partition offset
		adc edx,[Hidden+4]
		mov cx,retry_count
.jmp:		jmp strict short getonesec_cbios

;
; getonesec_ebios:
;
; getonesec implementation for EBIOS (EDD)
;
getonesec_ebios:
.retry:
		; Form DAPA on stack
		push edx
		push eax
		push es
		push bx
		push word 1
		push word 16
		mov si,sp
		pushad
                mov ah,42h                      ; Extended Read
		call xint13
		popad
		lea sp,[si+16]			; Remove DAPA
		jc .error
                ret

.error:
		; Some systems seem to get "stuck" in an error state when
		; using EBIOS.  Doesn't happen when using CBIOS, which is
		; good, since some other systems get timeout failures
		; waiting for the floppy disk to spin up.

		pushad				; Try resetting the device
		xor ax,ax
		call xint13
		popad
		loop .retry			; CX-- and jump if not zero

		; Total failure.  Try falling back to CBIOS.
		mov byte [getonesec.jmp+1],(getonesec_cbios-(getonesec.jmp+2))

;
; getonesec_cbios:
;
; getlinsec implementation for legacy CBIOS
;
getonesec_cbios:
.retry:
		pushad

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

		cmp eax,1023		; Outside the CHS range?
		ja kaboom

		;
		; Now we have AX = cyl, DX = head, CX = sector (0-based),
		; SI = bsSecPerTrack, ES:BX = data target
		;
		shl ah,6		; Because IBM was STOOPID
					; and thought 8 bits were enough
					; then thought 10 bits were enough...
		inc cx			; Sector numbers are 1-based, sigh
		or cl,ah
		mov ch,al
		mov dh,dl
		mov ax,0201h		; Read one sector
		call xint13
		popad
		jc .error
		ret

.error:
		loop .retry
		; Fall through to disk_error

;
; kaboom: write a message and bail out.
;
		global kaboom
disk_error:
kaboom:
		xor si,si
		mov ss,si
		mov sp,OrigFDCTabPtr	; Reset stack
		mov ds,si		; Reset data segment
		pop dword [fdctab]	; Restore FDC table
.patch:					; When we have full code, intercept here
		mov si,bailmsg
		call writestr_early

		xor ax,ax
.again:		int 16h			; Wait for keypress
					; NB: replaced by int 18h if
					; chosen at install time..
		int 19h			; And try once more to boot...
.norge:		hlt			; If int 19h returned; this is the end
		jmp short .norge

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

;
; INT 13h wrapper function
;
xint13:
                mov dl,[DriveNumber]
		int 13h
		ret

;
; Error message on failure
;
bailmsg:	db 'Boot error', 0Dh, 0Ah, 0

		; This fails if the boot sector overflowsg
		zb 1FEh-($-$$)

bootsignature	dw 0xAA55

;
; ===========================================================================
;  End of boot sector
; ===========================================================================
;  Start of LDLINUX.SYS
; ===========================================================================

LDLINUX_SYS	equ ($-$$)+TEXT_START
ldlinux_sys:

syslinux_banner	db CR, LF, MY_NAME, ' ', VERSION_STR, ' ', DATE_STR, ' ', 0
		db CR, LF, 1Ah	; EOF if we "type" this in DOS

		alignz 8
ldlinux_magic	dd LDLINUX_MAGIC
		dd LDLINUX_MAGIC^HEXDATE

;
; This area is patched by the installer.  It is found by looking for
; LDLINUX_MAGIC, plus 8 bytes.
;
SUBVOL_MAX	equ 256
CURRENTDIR_MAX	equ FILENAME_MAX

patch_area:
DataSectors	dw 0		; Number of sectors (not including bootsec)
ADVSectors	dw 0		; Additional sectors for ADVs
LDLDwords	dd 0		; Total dwords starting at ldlinux_sys,
CheckSum	dd 0		; Checksum starting at ldlinux_sys
				; value = LDLINUX_MAGIC - [sum of dwords]
MaxTransfer	dw 127		; Max sectors to transfer
EPAPtr		dw EPA - LDLINUX_SYS	; Pointer to the extended patch area

;
; Extended patch area -- this is in .data16 so it doesn't occupy space in
; the first sector.  Use this structure for anything that isn't used by
; the first sector itself.
;
		section .data16
		alignz 2
EPA:
ADVSecPtr	dw ADVSec0 - LDLINUX_SYS
CurrentDirPtr	dw CurrentDirName-LDLINUX_SYS	; Current directory name string
CurrentDirLen	dw CURRENTDIR_MAX
SubvolPtr	dw SubvolName-LDLINUX_SYS
SubvolLen	dw SUBVOL_MAX
SecPtrOffset	dw SectorPtrs-LDLINUX_SYS
SecPtrCnt	dw (SectorPtrsEnd - SectorPtrs)/10

;
; Boot sector patch pointers
;
Sect1Ptr0Ptr	dw Sect1Ptr0 - bootsec		; Pointers to Sector 1 location
Sect1Ptr1Ptr	dw Sect1Ptr1 - bootsec
RAIDPatchPtr	dw kaboom.again - bootsec	; Patch to INT 18h in RAID mode

;
; Base directory name and subvolume, if applicable.
;
%define HAVE_CURRENTDIRNAME
		global CurrentDirName, SubvolName
CurrentDirName	times CURRENTDIR_MAX db 0
SubvolName	times SUBVOL_MAX db 0

		section .init
ldlinux_ent:
;
; Note that some BIOSes are buggy and run the boot sector at 07C0:0000
; instead of 0000:7C00 and the like.  We don't want to add anything
; more to the boot sector, so it is written to not assume a fixed
; value in CS, but we don't want to deal with that anymore from now
; on.
;
		sti		; In case of broken INT 13h BIOSes

;
; Tell the user we got this far
;
		mov si,syslinux_banner
		call writestr_early

;
; Checksum data thus far
;
		mov si,ldlinux_sys
		mov cx,SECTOR_SIZE >> 2
		mov edx,-LDLINUX_MAGIC
.checksum:
		lodsd
		add edx,eax
		loop .checksum
		mov [CheckSum],edx		; Save intermediate result

;
; Tell the user if we're using EBIOS or CBIOS
;
print_bios:
		mov si,cbios_name
		cmp byte [getonesec.jmp+1],(getonesec_ebios-(getonesec.jmp+2))
		jne .cbios
		mov si,ebios_name
		mov byte [getlinsec.jmp+1],(getlinsec_ebios-(getlinsec.jmp+2))
.cbios:
		mov [BIOSName],si
		call writestr_early

		section .earlybss
%define	HAVE_BIOSNAME 1
BIOSName	resw 1

		section .init
;
; Now we read the rest of LDLINUX.SYS.
;
load_rest:
		lea esi,[SectorPtrs]
		mov ebx,TEXT_START+2*SECTOR_SIZE ; Where we start loading
		mov cx,[DataSectors]
		dec cx				; Minus this sector

.get_chunk:
		jcxz .done
		mov eax,[si]
		mov edx,[si+4]
		movzx ebp,word [si+8]
		sub cx,bp
		push ebx
		shr ebx,4			; Convert to a segment
		mov es,bx
		xor bx,bx
		call getlinsec
		pop ebx
		shl ebp,SECTOR_SHIFT
		add ebx,ebp
		add si,10
		jmp .get_chunk

.done:

;
; All loaded up, verify that we got what we needed.
; Note: the checksum field is embedded in the checksum region, so
; by the time we get to the end it should all cancel out.
;
verify_checksum:
		mov si,ldlinux_sys + SECTOR_SIZE
		mov ecx,[LDLDwords]
		sub ecx,SECTOR_SIZE >> 2
		mov eax,[CheckSum]
.checksum:
		add eax,[si]
		add si,4
		jnz .nowrap
		; Handle segment wrap
		mov dx,ds
		add dx,1000h
		mov ds,dx
.nowrap:
		dec ecx
		jnz .checksum

		and eax,eax			; Should be zero
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
; getlinsec: load a sequence of BP floppy sector given by the linear sector
;	     number in EAX into the buffer at ES:BX.  We try to optimize
;	     by loading up to a whole track at a time, but the user
;	     is responsible for not crossing a 64K boundary.
;	     (Yes, BP is weird for a count, but it was available...)
;
;	     On return, BX points to the first byte after the transferred
;	     block.
;
;            This routine assumes CS == DS.
;
		global getlinsec
getlinsec:
		pushad
		add eax,[Hidden]		; Add partition offset
		adc edx,[Hidden+4]
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
                mov ah,42h                      ; Extended Read
		push ds
		push ss
		pop ds
		call xint13
		pop ds
		popad
		lea sp,[si+16]			; Remove DAPA
		jc .error
		pop bp
		add eax,edi			; Advance sector pointer
		adc edx,0
		sub bp,di			; Sectors left
                shl di,SECTOR_SHIFT		; 512-byte sectors
                add bx,di			; Advance buffer pointer
                and bp,bp
                jnz .loop

		popad
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

		cmp eax,1023		; Outside the CHS range?
		ja kaboom

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
		xchg ax,bp		; Sector to transfer count
		mov ah,02h		; Read sectors
		mov bp,retry_count
.retry:
		pushad
		call xint13
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
		popad
		ret

.error:
		dec bp
		jnz .retry

		xchg ax,bp		; Sectors transferred <- 0
		shr word [MaxTransfer],1
		jnz .resume
		jmp kaboom

maxtrans:
		cmp bp,[MaxTransfer]
		jna .ok
		mov bp,[MaxTransfer]
.ok:		ret

;
; Checksum error message
;
checksumerr_msg	db ' Load error - ', 0	; Boot failed appended

;
; BIOS type string
;
cbios_name	db 'CHS', 0			; CHS/CBIOS
ebios_name	db 'EDD', 0			; EDD/EBIOS

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
 %if rl_checkpt_off > 3F6h			; Need one extent
  %assign rl_checkpt_overflow rl_checkpt_off - 3F6h
  %error Sector 1 overflow by rl_checkpt_overflow bytes
 %endif
%endif

;
; Extent pointers... each extent contains an 8-byte LBA and an 2-byte
; sector count.  In most cases, we will only ever need a handful of
; extents, but we have to assume a maximally fragmented system where each
; extent contains only one sector.
;
		alignz 2
MaxInitDataSize	equ 96 << 10
MaxLMA		equ TEXT_START+SECTOR_SIZE+MaxInitDataSize
SectorPtrs	zb 10*(MaxInitDataSize >> SECTOR_SHIFT)
SectorPtrsEnd	equ $

; ----------------------------------------------------------------------------
;  End of code and data that have to be in the first sector
; ----------------------------------------------------------------------------

		section .text16
all_read:
		; We enter here with both DS and ES scrambled...
		xor ax,ax
		mov ds,ax
		mov es,ax
;
; Let the user (and programmer!) know we got this far.  This used to be
; in Sector 1, but makes a lot more sense here.
;
		mov si,copyright_str
		call writestr_early


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
; Common initialization code
;
%include "init.inc"
		
		pushad
		mov eax,ROOT_FS_OPS
		movzx dx,byte [DriveNumber]
		; DH = 0: we are boot from disk not CDROM
		mov ecx,[Hidden]
		mov ebx,[Hidden+4]
		mov si,[bsHeads]
		mov di,[bsSecPerTrack]
		movzx ebp,word [MaxTransfer]
		pm_call fs_init
		popad

		section .bss16
SuperInfo	resq 16			; The first 16 bytes expanded 8 times

		section .text16
