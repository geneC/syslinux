; -*- fundamental -*- (asm-mode sucks)
; $Id$
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
;  This file is loaded in stages; first the boot sector at offset 7C00h,
;  then the first sector (cluster, really, but we can only assume 1 sector)
;  of LDLINUX.SYS at 7E00h and finally the remainder of LDLINUX.SYS at 8000h.
;
;   Copyright (C) 1994-2002  H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;  USA; either version 2 of the License, or (at your option) any later
;  version; incorporated herein by reference.
; 
; ****************************************************************************

%define IS_SYSLINUX 1
%include "macros.inc"
%include "config.inc"
%include "kernel.inc"
%include "bios.inc"
%include "tracers.inc"

;
; Some semi-configurable constants... change on your own risk.
;
my_id		equ syslinux_id
FILENAME_MAX_LG2 equ 4			; log2(Max filename size Including final null)
FILENAME_MAX	equ 11			; Max mangled filename size
NULLFILE	equ ' '			; First char space == null filename
retry_count	equ 6			; How patient are we with the disk?
%assign HIGHMEM_SLOP 0			; Avoid this much memory near the top

;
; The following structure is used for "virtual kernels"; i.e. LILO-style
; option labels.  The options we permit here are `kernel' and `append
; Since there is no room in the bottom 64K for all of these, we
; stick them at vk_seg:0000 and copy them down before we need them.
;
; Note: this structure can be added to, but it must 
;
%define vk_power	7		; log2(max number of vkernels)
%define	max_vk		(1 << vk_power)	; Maximum number of vkernels
%define vk_shift	(16-vk_power)	; Number of bits to shift
%define vk_size		(1 << vk_shift)	; Size of a vkernel buffer

		struc vkernel
vk_vname:	resb FILENAME_MAX	; Virtual name **MUST BE FIRST!**
vk_rname:	resb FILENAME_MAX	; Real name
vk_appendlen:	resw 1
		alignb 4
vk_append:	resb max_cmd_len+1	; Command line
		alignb 4
vk_end:		equ $			; Should be <= vk_size
		endstruc

%ifndef DEPEND
%if (vk_end > vk_size) || (vk_size*max_vk > 65536)
%error "Too many vkernels defined, reduce vk_power"
%endif
%endif

;
; Segment assignments in the bottom 640K
; Stick to the low 512K in case we're using something like M-systems flash
; which load a driver into low RAM (evil!!)
;
; 0000h - main code/data segment (and BIOS segment)
;
real_mode_seg	equ 7000h
fat_seg		equ 5000h		; 128K area for FAT (2x64K)
vk_seg          equ 4000h		; Virtual kernels
xfer_buf_seg	equ 3000h		; Bounce buffer for I/O to high mem
comboot_seg	equ 2000h		; COMBOOT image loading zone

; ---------------------------------------------------------------------------
;   BEGIN CODE
; ---------------------------------------------------------------------------

;
; Memory below this point is reserved for the BIOS and the MBR
;
 		absolute 1000h
trackbuf	equ $			; Track buffer goes here
trackbufsize	equ 16384		; Safe size of track buffer
;		trackbuf ends at 5000h


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


                absolute 5000h          ; Here we keep our BSS stuff
VKernelBuf:	resb vk_size		; "Current" vkernel
		alignb 4
AppendBuf       resb max_cmd_len+1	; append=
KbdMap		resb 256		; Keyboard map
FKeyName	resb 10*16		; File names for F-key help
NumBuf		resb 15			; Buffer to load number
NumBufEnd	resb 1			; Last byte in NumBuf
		alignb 4
PartInfo	resb 16			; Partition table entry
E820Buf		resd 5			; INT 15:E820 data buffer
HiLoadAddr      resd 1			; Address pointer for high load loop
HighMemSize	resd 1			; End of memory pointer (bytes)
RamdiskMax	resd 1			; Highest address for a ramdisk
KernelSize	resd 1			; Size of kernel (bytes)
SavedSSSP	resd 1			; Our SS:SP while running a COMBOOT image
KernelName      resb 12		        ; Mangled name for kernel
					; (note the spare byte after!)
RootDir		equ $			; Location of root directory
RootDir1	resw 1
RootDir2	resw 1
DataArea	equ $			; Location of data area
DataArea1	resw 1
DataArea2	resw 1
FBytes		equ $			; Used by open/getc
FBytes1		resw 1
FBytes2		resw 1
RootDirSize	resw 1			; Root dir size in sectors
DirScanCtr	resw 1			; Used while searching directory
DirBlocksLeft	resw 1			; Ditto
EndofDirSec	resw 1			; = trackbuf+bsBytesPerSec-31
RunLinClust	resw 1			; Cluster # for LDLINUX.SYS
ClustSize	resw 1			; Bytes/cluster
SecPerClust	resw 1			; Same as bsSecPerClust, but a word
NextCluster	resw 1			; Pointer to "nextcluster" routine
BufSafe		resw 1			; Clusters we can load into trackbuf
BufSafeSec	resw 1			; = how many sectors?
BufSafeBytes	resw 1			; = how many bytes?
EndOfGetCBuf	resw 1			; = getcbuf+BufSafeBytes
KernelClust	resw 1			; Kernel size in clusters
ClustPerMoby	resw 1			; Clusters per 64K
FClust		resw 1			; Number of clusters in open/getc file
FNextClust	resw 1			; Pointer to next cluster in d:o
FPtr		resw 1			; Pointer to next char in buffer
CmdOptPtr       resw 1			; Pointer to first option on cmd line
KernelCNameLen  resw 1			; Length of unmangled kernel name
InitRDCNameLen  resw 1			; Length of unmangled initrd name
NextCharJump    resw 1			; Routine to interpret next print char
SetupSecs	resw 1			; Number of setup sectors
A20Test		resw 1			; Counter for testing status of A20
A20Type		resw 1			; A20 type
CmdLineLen	resw 1			; Length of command line including null
GraphXSize	resw 1			; Width of splash screen file
VGAPos		resw 1			; Pointer into VGA memory
VGACluster	resw 1			; Cluster pointer for VGA image file
VGAFilePtr	resw 1			; Pointer into VGAFileBuf
TextAttrBX      equ $
TextAttribute   resb 1			; Text attribute for message file
TextPage        resb 1			; Active display page
CursorDX        equ $
CursorCol       resb 1			; Cursor column for message file
CursorRow       resb 1			; Cursor row for message file
ScreenSize      equ $
VidCols         resb 1			; Columns on screen-1
VidRows         resb 1			; Rows on screen-1
FlowControl	equ $
FlowOutput	resb 1			; Outputs to assert for serial flow
FlowInput	resb 1			; Input bits for serial flow
FlowIgnore	resb 1			; Ignore input unless these bits set
RetryCount      resb 1			; Used for disk access retries
KbdFlags	resb 1			; Check for keyboard escapes
LoadFlags	resb 1			; Loadflags from kernel
A20Tries	resb 1			; Times until giving up on A20
FuncFlag	resb 1			; Escape sequences received from keyboard
DisplayMask	resb 1			; Display modes mask
MNameBuf        resb 11            	; Generic mangled file name buffer
InitRD          resb 11                 ; initrd= mangled name
KernelCName     resb 13                 ; Unmangled kernel name
InitRDCName     resb 13            	; Unmangled initrd name
TextColorReg	resb 17			; VGA color registers for text mode
VGAFileBuf	resb 13			; Unmangled VGA image name
VGAFileBufEnd	equ $
VGAFileMBuf	resb 11			; Mangled VGA image name

		section .text
                org 7C00h
StackBuf	equ $			; Start the stack here (grow down - 4K)

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
superblock	equ $
bsBytesPerSec	zw 1
bsSecPerClust	zb 1
bsResSectors	zw 1
bsFATs		zb 1
bsRootDirEnts	zw 1
bsSectors	zw 1
bsMedia		zb 1
bsFATsecs	zw 1
bsSecPerTrack	zw 1
bsHeads		zw 1
bsHiddenSecs	equ $
bsHidden1	zw 1
bsHidden2	zw 1
bsHugeSectors	equ $
bsHugeSec1	zw 1
bsHugeSec2	zw 1
bsDriveNumber	zb 1
bsReserved1	zb 1
bsBootSignature zb 1			; 29h if the following fields exist
bsVolumeID	zd 1
bsVolumeLabel	zb 11
bsFileSysType	zb 8			; Must be FAT12 for this version
superblock_len	equ $-superblock
;
; Note we don't check the constraints above now; we did that at install
; time (we hope!)
;

;floppy_table	equ $			; No sense in wasting memory, overwrite start

start:
		cli			; No interrupts yet, please
		cld			; Copy upwards
;
; Set up the stack
;
		xor cx,cx
		mov ss,cx
		mov sp,StackBuf		; Just below BSS
		mov es,cx
;
; DS:SI may contain a partition table entry.  Preserve it for us.
;
		mov cl,8		; Save partition info (CH == 0)
		mov di,PartInfo
		rep movsw
;
; Now sautee the BIOS floppy info block to that it will support decent-
; size transfers; the floppy block is 11 bytes and is stored in the
; INT 1Eh vector (brilliant waste of resources, eh?)
;
; Of course, if BIOSes had been properly programmed, we wouldn't have
; had to waste precious boot sector space with this code.
;
; This code no longer fits.  Hope that noone really needs it anymore.
; (If so, it needs serious updating.)  In fact, some indications is that
; this code does more harm than good with all the new kinds of drives and
; media.
;
%ifdef SUPPORT_REALLY_BROKEN_BIOSES
		lds si,[ss:fdctab]	; DS:SI -> original
		push ds			; Save on stack in case
		push si			; we have to bail
		push bx
		mov cx,6		; 12 bytes
		mov di,floppy_table
		push di
		cld
		rep movsw		; Faster to move words
		pop di
		mov ds,ax		; Now we can point DS to here, too
		mov cl,[bsSecPerTrack]  ; Patch the sector count
		mov [di+4],cl
		mov [fdctab+2],ax	; Segment 0
		mov [fdctab],di		; offset floppy_block
%else
		mov ds,cx		; CX == 0
%endif
;
; Ready to enable interrupts, captain
;
		sti
;
; The drive number and possibly partition information was passed to us
; by the BIOS or previous boot loader (MBR).  Current "best practice" is to
; trust that rather than what the superblock contains.
;
; Would it be better to zero out bsHidden if we don't have a partition table?
;
; Note: di points to beyond the end of PartInfo
;
		mov [bsDriveNumber],dl
		test dl,80h		; If floppy disk (00-7F), assume no
		jz not_harddisk		; partition table
		test byte [di-16],7Fh	; Sanity check: "active flag" should
		jnz no_partition	; be 00 or 80
		lea si,[di-8]		; Partition offset (dword)
		mov di,bsHidden1
		mov cl,2		; CH == 0
		rep movsw
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
		inc dh			; Contains # of heads - 1
		mov [bsHeads],dh
		and cx,3fh
		mov [bsSecPerTrack],cx
no_driveparm:
not_harddisk:
;
; Now we have to do some arithmetric to figure out where things are located.
; If Micro$oft had had brains they would already have done this for us,
; and stored it in the superblock at format time, but here we go,
; wasting precious boot sector space again...
;
debugentrypt:
		xor ax,ax		; INT 13:08 destroys ES
		mov es,ax
		mov al,[bsFATs]		; Number of FATs (AH == 0)
		mul word [bsFATsecs]	; Get the size of the FAT area
		add ax,[bsHidden1]	; Add hidden sectors
		adc dx,[bsHidden2]
		add ax,[bsResSectors]	; And reserved sectors
		adc dx,byte 0

		mov [RootDir1],ax	; Location of root directory
		mov [RootDir2],dx
		mov [DataArea1],ax
		mov [DataArea2],dx
		push ax
		push dx

		mov ax,32		; Size of a directory entry
		mul word [bsRootDirEnts]
		mov bx,[bsBytesPerSec]
		add ax,bx		; Round up, not down
		dec ax
		div bx			; Now we have the size of the root dir
		mov [RootDirSize],ax
		mov [DirScanCtr],ax
		add bx,trackbuf-31
		mov [EndofDirSec],bx	; End of a single directory sector

		add [DataArea1],ax
		adc word [DataArea2],byte 0

		pop dx			; Reload root directory starting point
		pop ax
;
; Now the fun begins.  We have to search the root directory for
; LDLINUX.SYS and load the first sector, so we have a little more
; space to have fun with.  Then we can go chasing through the FAT.
; Joy!!
;
sd_nextsec:	push ax
		push dx
		mov bx,trackbuf
		push bx
		call getonesec
		pop si
sd_nextentry:	cmp byte [si],0		; Directory high water mark
		je kaboom
		test byte [si+11],18h	; Must be a file
		jnz sd_not_file
		mov di,ldlinux_name
		mov cx,11
		push si
		repe cmpsb
		pop si
		je found_it
sd_not_file:	add si,byte 32		; Distance to next
		cmp si,[EndofDirSec]
		jb sd_nextentry
		pop dx
		pop ax
		add ax,byte 1
		adc dx,byte 0
		dec word [DirScanCtr]
		jnz sd_nextsec
;
; kaboom: write a message and bail out.
;
kaboom:
		xor si,si
		mov ss,si		
		mov sp,StackBuf 	; Reset stack
		mov ds,si		; Reset data segment
.patch:		mov si,bailmsg
		call writestr		; Returns with AL = 0
		cbw			; AH <- 0
		int 16h			; Wait for keypress
		int 19h			; And try once more to boot...
.norge:		jmp short .norge	; If int 19h returned; this is the end

;
; found_it: now we compute the location of the first sector, then
;	    load it and JUMP (since we're almost out of space)
;
found_it:	; Note: we actually leave two words on the stack here
		; (who cares?)
		xor ax,ax
		mov al,[bsSecPerClust]
		mov bp,ax		; Load an entire cluster
		mov bx,[si+26]		; First cluster
		mov [RunLinClust],bx	; Save for later use
		dec bx			; First cluster is "cluster 2"
		dec bx
		mul bx
		add ax,[DataArea1]
		adc dx,[DataArea2]
		mov bx,ldlinux_sys
		call getlinsec
		mov si,bs_magic
		mov di,ldlinux_magic
		mov cx,magic_len
		repe cmpsb		; Make sure that the bootsector
		jne kaboom		; matches LDLINUX.SYS
;
; Done! Jump to the entry point!
; 
; Note that some BIOSes are buggy and run the boot sector at 07C0:0000
; instead of 0000:7C00 and the like.  We don't want to add anything
; more to the boot sector, so it is written to not assume a fixed
; value in CS, but we don't want to deal with that anymore from now
; on.
;
		jmp 0:ldlinux_ent

;
;
; writestr: write a null-terminated string to the console
;
writestr:
wstr_1:         lodsb
		and al,al
                jz return
		mov ah,0Eh		; Write to screen as TTY
		mov bx,0007h		; White on black, current page
		int 10h
		jmp short wstr_1
;
; disk_error: decrement the retry count and bail if zero
;
disk_error:	dec si			; SI holds the disk retry counter
		jz kaboom
		pop bx			; <I>
		pop cx			; <H>
		pop dx			; <G>
		pop ax			; <F> (AH = 0)
		mov al,1		; Once we fail, only transfer 1 sector
		jmp short disk_try_again

return:		ret

;
; getonesec: like getlinsec, but pre-sets the count to 1
;
getonesec:
		mov bp,1
		; Fall through to getlinsec

;
; getlinsec: load a sequence of BP floppy sector given by the linear sector
;	     number in DX:AX into the buffer at ES:BX.	We try to optimize
;	     by loading up to a whole track at a time, but the user
;	     is responsible for not crossing a 64K boundary.
;	     (Yes, BP is weird for a count, but it was available...)
;
;	     On return, BX points to the first byte after the transferred
;	     block.
;
;	     The "stupid patch area" gets replaced by the code
;	     mov bp,1 ; nop ... (BD 01 00 90 90...) when installing with
;	     the -s option.
;
; Stylistic note: use "xchg" instead of "mov" when the source is a register
; that is dead from that point; this saves space.  However, please keep
; the order to dst,src to keep things sane.
;
getlinsec:
		mov si,[bsSecPerTrack]
		;
		; Dividing by sectors to get (track,sector): we may have
		; up to 2^18 tracks, so we need to do this in two steps
		; to produce a 32-bit quotient.
		;
		xchg cx,ax		; CX <- LSW of LBA
		xchg ax,dx
		xor dx,dx		; DX:AX now == MSW of LBA
		div si			; Obtain MSW of track #
		xchg ax,cx		; Remainder -> MSW of new dividend
					; LSW of LBA -> LSW of new dividend
					; Quotient -> MSW of track # 
		div si			; Obtain LSW of track #, remainder
		xchg cx,dx		; CX <- Sector index (0-based)
					; DX <- MSW of track #
		div word [bsHeads]	; Convert track to head/cyl
		;
		; Now we have AX = cyl, DX = head, CX = sector (0-based),
		; BP = sectors to transfer, SI = bsSecPerTrack,
		; ES:BX = data target
		;
gls_nextchunk:	push si			; <A> bsSecPerTrack
		push bp			; <B> Sectors to transfer

__BEGIN_STUPID_PATCH_AREA:
		sub si,cx		; Sectors left on track
		cmp bp,si
		jna gls_lastchunk
		mov bp,si		; No more than a trackful, please!
__END_STUPID_PATCH_AREA:
gls_lastchunk:	
		push ax			; <C> Cylinder #
		push dx			; <D> Head #

		push cx			; <E> Sector #
		mov cl,6		; Because IBM was STOOPID
		shl ah,cl		; and thought 8 bits were enough
					; then thought 10 bits were enough...
		pop cx			; <E> Sector #
		push cx			; <E> Sector #
		inc cx			; Sector numbers are 1-based
		or cl,ah
		mov ch,al
		mov dh,dl
		mov dl,[bsDriveNumber]
		xchg ax,bp		; Sector to transfer count
					; (xchg shorter than mov)
		mov si,retry_count	; # of times to retry a disk access
;
; Do the disk transfer... save the registers in case we fail :(
;
disk_try_again: 
		push ax			; <F> Number of sectors we're transferring
		mov ah,02h		; READ DISK
		push dx			; <G>
		push cx			; <H>
		push bx			; <I>
		push si			; <J>
		int 13h
		pop si			; <J>
		jc disk_error
;
; Disk access successful
;
		pop bx			; <I> Buffer location
		pop ax			; <H> No longer needed
		pop ax			; <G> No longer needed
		pop di			; <F> Sector transferred count
		pop cx			; <E> Sector #
		mov ax,di		; Reduce sector left count
		mul word [bsBytesPerSec] ; Figure out how much to advance ptr
		add bx,ax		; Update buffer location
		pop dx			; <D> Head #
		pop ax			; <C> Cyl #
		pop bp			; <B> Sectors left to transfer
		pop si			; <A> Number of sectors/track
		sub bp,di		; Reduce with # of sectors just read
		jz return		; Done!
		add cx,di
		cmp cx,si
		jb gls_nextchunk
		inc dx			; Next track on cyl
		cmp dx,[bsHeads]	; Was this the last one?
		jb gls_nonewcyl
		inc ax			; If so, new cylinder
		xor dx,dx		; First head on new cylinder
gls_nonewcyl:	sub cx,si		; First sector on new track
		jmp short gls_nextchunk

bailmsg:	db 'Boot failed', 0Dh, 0Ah, 0

bs_checkpt	equ $			; Must be <= 7DEFh

bs_checkpt_off	equ ($-$$)
%ifndef DEPEND
%if bs_checkpt_off > 1EFh
%error "Boot sector overflow"
%endif
%endif

		zb 1EFh-($-$$)
bs_magic	equ $			; From here to the magic_len equ
					; must match ldlinux_magic
ldlinux_name:	db 'LDLINUX SYS'	; Looks like this in the root dir
		dd HEXDATE		; Hopefully unique between compiles

bootsignature	dw 0AA55h
magic_len	equ $-bs_magic

;
; ===========================================================================
;  End of boot sector
; ===========================================================================
;  Start of LDLINUX.SYS
; ===========================================================================

ldlinux_sys:

syslinux_banner	db 0Dh, 0Ah, 'SYSLINUX ', version_str, ' ', date, ' ', 0
		db 0Dh, 0Ah, 1Ah	; EOF if we "type" this in DOS

ldlinux_magic	db 'LDLINUX SYS'
		dd HEXDATE
		dw 0AA55h

		align 4

ldlinux_ent:
;
; Tell the user we got this far
;
		mov si,syslinux_banner
		call writestr
;
; Remember, the boot sector loaded only the first cluster of LDLINUX.SYS.
; We can really only rely on a single sector having been loaded.  Hence
; we should load the FAT into RAM and start chasing pointers...
;
		mov dx,1			; 64K
		xor ax,ax
		div word [bsBytesPerSec]	; sectors/64K
		mov si,ax

		push es
		mov bx,fat_seg			; Load into fat_seg:0000
		mov es,bx
		
		mov ax,[bsHidden1]		; Hidden sectors
		mov dx,[bsHidden2]
		add ax,[bsResSectors]		; plus reserved sectors = FAT
		adc dx,byte 0
		mov cx,[bsFATsecs]		; Sectors/FAT
fat_load_loop:	
		mov bp,cx
		cmp bp,si
		jna fat_load
		mov bp,si			; A full 64K moby
fat_load:	
		xor bx,bx			; Offset 0 in the current ES
		call getlinsecsr
		sub cx,bp
		jz fat_load_done		; Last moby?
		add ax,bp			; Advance sector count
		adc dx,byte 0
		mov bx,es			; Next 64K moby
		add bx,1000h
		mov es,bx
		jmp short fat_load_loop
fat_load_done:
		pop es
;
; Fine, now we have the FAT in memory.	How big is a cluster, really?
; Also figure out how many clusters will fit in an 8K buffer, and how
; many sectors and bytes that is
;
		mov di,[bsBytesPerSec]		; Used a lot below

		mov al,[bsSecPerClust]		; We do this in the boot
		xor ah,ah			; sector, too, but there
		mov [SecPerClust],ax		; wasn't space to save it
		mov si,ax			; Also used a lot...
		mul di
		mov [ClustSize],ax		; Bytes/cluster
		mov bx,ax
		mov ax,trackbufsize
		xor dx,dx
		div bx
		mov [BufSafe],ax		; # of cluster in trackbuf
		mul word [SecPerClust]
		mov [BufSafeSec],ax
		mul di
		mov [BufSafeBytes],ax
		add ax,getcbuf			; Size of getcbuf is the same
		mov [EndOfGetCBuf],ax		; as for trackbuf
;
; FAT12 or FAT16?  This computation is fscking ridiculous...
;
		xor dx,dx
		xor cx,cx
		mov ax,[bsSectors]
		and ax,ax
		jnz have_secs
		mov ax,[bsHugeSectors]
		mov dx,[bsHugeSectors+2]
have_secs:	sub ax,[bsResSectors]
		sbb dx,byte 0
		mov cl,[bsFATs]
sec_fat_loop:	sub ax,[bsFATsecs]
		sbb dx,byte 0
		loop sec_fat_loop
		push ax
		push dx
		mov ax,[bsRootDirEnts]
		mov bx,32			; Smaller than shift since we
		mul bx				; need the doubleword product
		add ax,di
		adc dx,byte 0
		sub ax,byte 1
		sbb dx,byte 0
		div di
		mov bx,ax
		pop dx
		pop ax
		sub ax,bx
		sbb dx,byte 0
		div si
		cmp ax,4086			; Right value?
		mov ax,nextcluster_fat16
		ja have_fat_type
have_fat12:	mov ax,nextcluster_fat12
have_fat_type:	mov word [NextCluster],ax

;
; Now we read the rest of LDLINUX.SYS.	Don't bother loading the first
; cluster again, though.
;
load_rest:
		mov cx,[ClustSize]
		mov bx,ldlinux_sys
		add bx,cx
		mov si,[RunLinClust]
		call [NextCluster]
		xor dx,dx
		mov ax,ldlinux_len-1		; To be on the safe side
		add ax,cx
		div cx				; the number of clusters
		dec ax				; We've already read one
		jz all_read_jmp
		mov cx,ax
		call getfssec
;
; All loaded up
;
all_read_jmp:
		jmp all_read
;
; -----------------------------------------------------------------------------
; Subroutines that have to be in the first sector
; -----------------------------------------------------------------------------
;
; getfssec: Get multiple clusters from a file, given the starting cluster.
;
;	This routine makes sure the subtransfers do not cross a 64K boundary,
;	and will correct the situation if it does, UNLESS *sectors* cross
;	64K boundaries.
;
;	ES:BX	-> Buffer
;	SI	-> Starting cluster number (2-based)
;	CX	-> Cluster count (0FFFFh = until end of file)
;
						; 386 check
getfssec:
getfragment:	xor bp,bp			; Fragment sector count
		mov ax,si			; Get sector address
		dec ax				; Convert to 0-based
		dec ax
		mul word [SecPerClust]
		add ax,[DataArea1]
		adc dx,[DataArea2]
getseccnt:					; See if we can read > 1 clust
		add bp,[SecPerClust]
		dec cx				; Reduce clusters left to find
		mov di,si			; Predict next cluster
		inc di
		call [NextCluster]
		jc gfs_eof			; At EOF?
		jcxz endfragment		; Or was it the last we wanted?
		cmp si,di			; Is file continuous?
		jz getseccnt			; Yes, we can get
endfragment:	clc				; Not at EOF
gfs_eof:	pushf				; Remember EOF or not
		push si
		push cx
gfs_getchunk:
		push ax
		push dx
		mov ax,es			; Check for 64K boundaries.
		mov cl,4
		shl ax,cl
		add ax,bx
		xor dx,dx
		neg ax
		jnz gfs_partseg
		inc dx				; Full 64K segment
gfs_partseg:
		div word [bsBytesPerSec]	; How many sectors fit?
		mov si,bp
		sub si,ax			; Compute remaining sectors
		jbe gfs_lastchunk
		mov bp,ax
		pop dx
		pop ax
		call getlinsecsr
		add ax,bp
		adc dx,byte 0
		mov bp,si			; Remaining sector count
		jmp short gfs_getchunk
gfs_lastchunk:	pop dx
		pop ax		
		call getlinsec
		pop cx
		pop si
		popf
		jcxz gfs_return			; If we hit the count limit
		jnc getfragment			; If we didn't hit EOF
gfs_return:	ret

;
; getlinsecsr: save registers, call getlinsec, restore registers
;
getlinsecsr:	push ax
		push dx
		push cx
		push bp
		push si
		push di
		call getlinsec
		pop di
		pop si
		pop bp
		pop cx
		pop dx
		pop ax
		ret

;
; nextcluster: Advance a cluster pointer in SI to the next cluster
;	       pointed at in the FAT tables (note: FAT12 assumed)
;	       Sets CF on return if end of file.
;
;	       The variable NextCluster gets set to the appropriate
;	       value here.
;
nextcluster_fat12:
		push ax
		push ds
		mov ax,fat_seg
		mov ds,ax
		mov ax,si			; Multiply by 3/2
		shr ax,1
		pushf				; CF now set if odd
		add si,ax
		mov si,[si]
		popf
		jnc nc_even
		shr si,1			; Needed for odd only
		shr si,1
		shr si,1
		shr si,1
nc_even:
		and si,0FFFh
		cmp si,0FF0h			; Clears CF if at end of file
		cmc				; But we want it SET...
		pop ds
		pop ax
nc_return:	ret

;
; FAT16 decoding routine.  Note that a 16-bit FAT can be up to 128K,
; so we have to decide if we're in the "low" or the "high" 64K-segment...
;
nextcluster_fat16:
		push ax
		push ds
		mov ax,fat_seg
		shl si,1
		jnc .seg0
		mov ax,fat_seg+1000h
.seg0:		mov ds,ax
		mov si,[si]
		cmp si,0FFF0h
		cmc
		pop ds
		pop ax
		ret
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
; Common initialization code
;
%include "cpuinit.inc"

;
; Initialization that does not need to go into the any of the pre-load
; areas
;
		; Now set up screen parameters
		call adjust_screen

		; Wipe the F-key area
		mov al,NULLFILE
		mov di,FKeyName
		mov cx,10*(1 << FILENAME_MAX_LG2)
		rep stosb

;
; Now, everything is "up and running"... patch kaboom for more
; verbosity and using the full screen system
;
		mov byte [kaboom.patch],0e9h		; JMP NEAR
		mov word [kaboom.patch+1],kaboom2-(kaboom.patch+3)

;
; Compute some parameters that depend on cluster size
;
		mov dx,1
		xor ax,ax
		div word [ClustSize]
		mov [ClustPerMoby],ax		; Clusters/64K

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
		mov si,linuxauto_cmd		; Default command: "linux auto"
		mov di,default_cmd
                mov cx,linuxauto_len
		rep movsb

		mov di,KbdMap			; Default keymap 1:1
		xor al,al
		mov cx,256
mkkeymap:	stosb
		inc al
		loop mkkeymap

;
; Load configuration file
;
		mov di,syslinux_cfg
		call open
		jz near no_config_file
;
; Now we have the config file open
;
		call parse_config		; Parse configuration file
no_config_file:
;
; Check whether or not we are supposed to display the boot prompt.
;
check_for_key:
		cmp word [ForcePrompt],byte 0	; Force prompt?
		jnz enter_command
		test byte [KbdFlags],5Bh	; Caps, Scroll, Shift, Alt
		jz near auto_boot		; If neither, default boot

enter_command:
		mov si,boot_prompt
		call cwritestr

		mov byte [FuncFlag],0		; <Ctrl-F> not pressed
		mov di,command_line
;
; get the very first character -- we can either time
; out, or receive a character press at this time.  Some dorky BIOSes stuff
; a return in the buffer on bootup, so wipe the keyboard buffer first.
;
clear_buffer:	mov ah,1			; Check for pending char
		int 16h
		jz get_char_time
		xor ax,ax			; Get char
		int 16h
		jmp short clear_buffer
get_char_time:	
		call vgashowcursor
		mov cx,[KbdTimeOut]
		and cx,cx
		jz get_char			; Timeout == 0 -> no timeout
		inc cx				; The first loop will happen
						; immediately as we don't
						; know the appropriate DX value
time_loop:	push cx
tick_loop:	push dx
		call pollchar
		jnz get_char_pop
		xor ax,ax
		int 1Ah				; Get time "of day"
		pop ax
		cmp dx,ax			; Has the timer advanced?
		je tick_loop
		pop cx
		loop time_loop			; If so, decrement counter
		call vgahidecursor
		jmp command_done		; Timeout!

get_char_pop:	pop eax				; Clear stack
get_char:
		call vgashowcursor
		call getchar
		call vgahidecursor
		and al,al
		jz func_key

got_ascii:	cmp al,7Fh			; <DEL> == <BS>
		je backspace
		cmp al,' '			; ASCII?
		jb not_ascii
		ja enter_char
		cmp di,command_line		; Space must not be first
		je get_char
enter_char:	test byte [FuncFlag],1
		jz .not_ctrl_f
		mov byte [FuncFlag],0
		cmp al,'0'
		jb .not_ctrl_f
		je ctrl_f_0
		cmp al,'9'
		jbe ctrl_f
.not_ctrl_f:	cmp di,max_cmd_len+command_line ; Check there's space
		jnb get_char
		stosb				; Save it
		call writechr			; Echo to screen
get_char_2:	jmp short get_char
not_ascii:	mov byte [FuncFlag],0
		cmp al,0Dh			; Enter
		je command_done
		cmp al,06h			; <Ctrl-F>
		je set_func_flag
		cmp al,08h			; Backspace
		jne get_char
backspace:	cmp di,command_line		; Make sure there is anything
		je get_char			; to erase
		dec di				; Unstore one character
		mov si,wipe_char		; and erase it from the screen
		call cwritestr
		jmp short get_char_2

set_func_flag:
		mov byte [FuncFlag],1
		jmp short get_char_2

ctrl_f_0:	add al,10			; <Ctrl-F>0 == F10
ctrl_f:		push di
		sub al,'1'
		xor ah,ah
		jmp short show_help

func_key:
		; AL = 0 if we get here
		push di
		cmp ah,68			; F10
		ja get_char_2
		sub ah,59			; F1
		jb get_char_2
		xchg al,ah
show_help:	; AX = func key # (0 = F1, 9 = F10)
		shl ax,FILENAME_MAX_LG2		; Convert to pointer
		xchg di,ax
		add di,FKeyName
		cmp byte [di],NULLFILE
		je get_char_2			; Undefined F-key
		call searchdir
		jz fk_nofile
		push si
		call crlf
		pop si
		call get_msg_file
		jmp short fk_wrcmd
fk_nofile:
		call crlf
fk_wrcmd:
		mov si,boot_prompt
		call cwritestr
		pop di				; Command line write pointer
		push di
		mov byte [di],0			; Null-terminate command line
		mov si,command_line
		call cwritestr			; Write command line so far
		pop di
		jmp short get_char_2
auto_boot:
		mov si,default_cmd
		mov di,command_line
		mov cx,(max_cmd_len+4) >> 2
		rep movsd
		jmp short load_kernel
command_done:
		call crlf
		cmp di,command_line		; Did we just hit return?
		je auto_boot
		xor al,al			; Store a final null
		stosb

load_kernel:					; Load the kernel now
;
; First we need to mangle the kernel name the way DOS would...
;
		mov si,command_line
                mov di,KernelName
                push si
                push di
		call mangle_name
		pop di
                pop si
;
; Fast-forward to first option (we start over from the beginning, since
; mangle_name doesn't necessarily return a consistent ending state.)
;
clin_non_wsp:   lodsb
                cmp al,' '
                ja clin_non_wsp
clin_is_wsp:    and al,al
                jz clin_opt_ptr
                lodsb
                cmp al,' '
                jbe clin_is_wsp
clin_opt_ptr:   dec si                          ; Point to first nonblank
                mov [CmdOptPtr],si		; Save ptr to first option
;
; Now check if it is a "virtual kernel"
;
		mov cx,[VKernelCtr]
		push ds
		push word vk_seg
		pop ds
		cmp cx,byte 0
		je not_vk
		xor si,si			; Point to first vkernel
vk_check:	pusha
		mov cx,11
		repe cmpsb			; Is this it?
		je near vk_found
		popa
		add si,vk_size
		loop vk_check
not_vk:		pop ds
;
; Not a "virtual kernel" - check that's OK and construct the command line
;
                cmp word [AllowImplicit],byte 0
                je bad_implicit
                push es
                push si
                push di
                mov di,real_mode_seg
                mov es,di
                mov si,AppendBuf
                mov di,cmd_line_here
                mov cx,[AppendLen]
                rep movsb
                mov [CmdLinePtr],di
                pop di
                pop si
                pop es
		mov bx,exten_count << 2		; Alternates to try
;
; Find the kernel on disk
;
get_kernel:     mov byte [KernelName+11],0	; Zero-terminate filename/extension
		mov eax,[KernelName+8]		; Save initial extension
		mov [OrigKernelExt],eax
.search_loop:	push bx
                mov di,KernelName	      	; Search on disk
                call searchdir
		pop bx
                jnz kernel_good
		mov eax,[exten_table+bx]	; Try a different extension
		mov [KernelName+8],eax
		sub bx,byte 4
		jnb .search_loop
bad_kernel:     
		mov si,KernelName
                mov di,KernelCName
		push di
                call unmangle_name              ; Get human form
		mov si,err_notfound		; Complain about missing kernel
		call cwritestr
		pop si				; KernelCName
                call cwritestr
                mov si,crlf_msg
                jmp abort_load                  ; Ask user for clue
;
; bad_implicit: The user entered a nonvirtual kernel name, with "implicit 0"
;
bad_implicit:   mov si,KernelName		; For the error message
                mov di,KernelCName
                call unmangle_name
                jmp short bad_kernel
;
; vk_found: We *are* using a "virtual kernel"
;
vk_found:	popa
		push di
		mov di,VKernelBuf
		mov cx,vk_size >> 2
		rep movsd
		push es				; Restore old DS
		pop ds
		push es
		push word real_mode_seg
		pop es
		mov di,cmd_line_here
		mov si,VKernelBuf+vk_append
		mov cx,[VKernelBuf+vk_appendlen]
		rep movsb
		mov [CmdLinePtr],di		; Where to add rest of cmd
		pop es
                pop di                          ; DI -> KernelName
		push di	
		mov si,VKernelBuf+vk_rname
		mov cx,11			; We need ECX == CX later
		rep movsb
		pop di
		xor bx,bx			; Try only one version
		jmp get_kernel
;
; kernel_corrupt: Called if the kernel file does not seem healthy
;
kernel_corrupt: mov si,err_notkernel
                jmp abort_load
;
; This is it!  We have a name (and location on the disk)... let's load
; that sucker!!  First we have to decide what kind of file this is; base
; that decision on the file extension.  The following extensions are
; recognized:
;
; .com 	- COMBOOT image
; .cbt	- COMBOOT image
; .bs	- Boot sector
; .0	- PXE bootstrap program (PXELINUX only)
; .bin  - Boot sector
; .bss	- Boot sector, but transfer over DOS superblock (SYSLINUX only)
; .img  - Floppy image (ISOLINUX only)
;
; Anything else is assumed to be a Linux kernel.
;
kernel_good:
		pusha
		mov si,KernelName
                mov di,KernelCName
                call unmangle_name              ; Get human form
                sub di,KernelCName
                mov [KernelCNameLen],di
		popa

		mov ecx,[KernelName+8]		; Get (mangled) extension
		cmp ecx,'COM'
		je near is_comboot_image
		cmp ecx,'CBT'
		je near is_comboot_image
		cmp ecx,'BS '
		je near is_bootsector
		cmp ecx,'BIN'
		je near is_bootsector
		cmp ecx,'BSS'
		je near is_bss_sector
		; Otherwise Linux kernel

;
; Linux kernel loading code is common.
;
%include "runkernel.inc"

;
; COMBOOT-loading code
;
%include "comboot.inc"

;
; Boot sector loading code
;
%include "bootsect.inc"

;
; abort_check: let the user abort with <ESC> or <Ctrl-C>
;
abort_check:
		call pollchar
		jz ac_ret1
		pusha
		call getchar
		cmp al,27			; <ESC>
		je ac_kill
		cmp al,3			; <Ctrl-C>
		jne ac_ret2
ac_kill:	mov si,aborted_msg

;
; abort_load: Called by various routines which wants to print a fatal
;             error message and return to the command prompt.  Since this
;             may happen at just about any stage of the boot process, assume
;             our state is messed up, and just reset the segment registers
;             and the stack forcibly.
;
;             SI    = offset (in _text) of error message to print
;
abort_load:
                mov ax,cs                       ; Restore CS = DS = ES
                mov ds,ax
                mov es,ax
                cli
                mov sp,StackBuf-2*3    		; Reset stack
                mov ss,ax                       ; Just in case...
                sti
                call cwritestr                  ; Expects SI -> error msg
al_ok:          jmp enter_command               ; Return to command prompt
;
; End of abort_check
;
ac_ret2:	popa
ac_ret1:	ret

;
; searchdir: Search the root directory for a pre-mangled filename in
;	     DS:DI.  This routine is similar to the one in the boot
;	     sector, but is a little less Draconian when it comes to
;	     error handling, plus it reads the root directory in
;	     larger chunks than a sector at a time (which is probably
;	     a waste of coding effort, but I like to do things right).
;
;	     FIXME: usually we can load the entire root dir in memory,
;	     and files are usually at the beginning anyway.  It probably
;	     would be worthwhile to remember if we have the first chunk
;	     in memory and skip the load if that (it would speed up online
;	     help, mainly.)
;
;	     NOTE: This file considers finding a zero-length file an
;	     error.  This is so we don't have to deal with that special
;	     case elsewhere in the program (most loops have the test
;	     at the end).
;
;	     If successful:
;		ZF clear
;		SI	= cluster # for the first cluster
;		DX:AX	= file length in bytes
;	     If unsuccessful
;		ZF set
;

searchdir:
		mov ax,[bsRootDirEnts]
		mov [DirScanCtr],ax
		mov ax,[RootDirSize]
		mov [DirBlocksLeft],ax
		mov ax,[RootDir1]
		mov dx,[RootDir2]
scan_group:
		mov bp,[DirBlocksLeft]
		and bp,bp
		jz dir_return
		cmp bp,[BufSafeSec]
		jna load_last
		mov bp,[BufSafeSec]
load_last:
		sub [DirBlocksLeft],bp
		push ax
		push dx
		mov ax,[bsBytesPerSec]
		mul bp
		add ax,trackbuf-31
		mov [EndofDirSec],ax	; End of loaded
		pop dx
		pop ax
		push bp			; Save number of sectors
		push ax			; Save present location
		push dx
		push di			; Save name
		mov bx,trackbuf
		call getlinsec
		pop di
		pop dx
		pop ax
		pop bp
		mov si,trackbuf
dir_test_name:	cmp byte [si],0		; Directory high water mark
		je dir_return		; Failed
                test byte [si+11],18h	; Check it really is a file
                jnz dir_not_this
		push di
		push si
		mov cx,11		; Filename = 11 bytes
		repe cmpsb
		pop si
		pop di
		je dir_success
dir_not_this:   add si,byte 32
		dec word [DirScanCtr]
		jz dir_return		; Out of it...
		cmp si,[EndofDirSec]
		jb dir_test_name
		add ax,bp		; Increment linear sector number
		adc dx,byte 0
		jmp short scan_group
dir_success:
		mov ax,[si+28]		; Length of file
		mov dx,[si+30]
		mov si,[si+26]		; Cluster pointer
		mov bx,ax
		or bx,dx		; Sets ZF iff DX:AX is zero
dir_return:
		ret

;
; writechr:	Write a single character in AL to the console without
;		mangling any registers
;
writechr:
		call write_serial	; write to serial port if needed
		pushfd
		pushad
		mov ah,0Eh
		mov bx,0007h		; white text on this page
		int 10h
		popad
		popfd
		ret

;
;
; kaboom2: once everything is loaded, replace the part of kaboom
;	   starting with "kaboom.patch" with this part

kaboom2:
		mov si,err_bootfailed
		call cwritestr
		call getchar
		call vgaclearmode
		int 19h			; And try once more to boot...
.norge:		jmp short .norge	; If int 19h returned; this is the end

;
; mangle_name: Mangle a DOS filename pointed to by DS:SI into a buffer pointed
;	       to by ES:DI; ends on encountering any whitespace
;

mangle_name:
		mov cx,11			; # of bytes to write
mn_loop:
		lodsb
		cmp al,' '			; If control or space, end
		jna mn_end
		cmp al,'.'			; Period -> space-fill
		je mn_is_period
		cmp al,'a'
		jb mn_not_lower
		cmp al,'z'
		ja mn_not_uslower
		sub al,020h
		jmp short mn_not_lower
mn_is_period:	mov al,' '			; We need to space-fill
mn_period_loop: cmp cx,3			; If <= 3 characters left
		jbe mn_loop			; Just ignore it
		stosb				; Otherwise, write a period
		loop mn_period_loop		; Dec CX and (always) jump
mn_not_uslower: cmp al,ucase_low
		jb mn_not_lower
		cmp al,ucase_high
		ja mn_not_lower
		mov bx,ucase_tab-ucase_low
                cs xlatb
mn_not_lower:	stosb
		loop mn_loop			; Don't continue if too long
mn_end:
		mov al,' '			; Space-fill name
		rep stosb			; Doesn't do anything if CX=0
		ret				; Done

;
; Upper-case table for extended characters; this is technically code page 865,
; but code page 437 users will probably not miss not being able to use the
; cent sign in kernel images too much :-)
;
; The table only covers the range 129 to 164; the rest we can deal with.
;
ucase_low	equ 129
ucase_high	equ 164
ucase_tab	db 154, 144, 'A', 142, 'A', 143, 128, 'EEEIII'
		db 142, 143, 144, 146, 146, 'O', 153, 'OUUY', 153, 154
		db 157, 156, 157, 158, 159, 'AIOU', 165

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
unmangle_name:
                push si                 ; Save pointer to original name
                mov cx,8
                mov bp,di
un_copy_body:   lodsb
                call lower_case
                stosb
                cmp al,' '
                jbe un_cb_space
                mov bp,di               ; Position of last nonblank+1
un_cb_space:    loop un_copy_body
                mov di,bp
                mov al,'.'              ; Don't save
                stosb
                mov cx,3
un_copy_ext:    lodsb
                call lower_case
                stosb
                cmp al,' '
                jbe un_ce_space
                mov bp,di
un_ce_space:    loop un_copy_ext
                mov di,bp
                mov byte [es:di], 0
                pop si
                ret

;
; lower_case: Lower case a character in AL
;
lower_case:
                cmp al,'A'
                jb lc_ret
                cmp al,'Z'
                ja lc_1
                or al,20h
                ret
lc_1:           cmp al,lcase_low
                jb lc_ret
                cmp al,lcase_high
                ja lc_ret
                push bx
                mov bx,lcase_tab-lcase_low
               	cs xlatb
                pop bx
lc_ret:         ret

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "writestr.inc"		; String output
%include "parseconfig.inc"	; High-level config file handling
%include "parsecmd.inc"		; Low-level config file handling
%include "bcopy32.inc"		; 32-bit bcopy
%include "loadhigh.inc"		; Load a file into high memory
%include "font.inc"		; VGA font stuff
%include "graphics.inc"		; VGA graphics

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

CR		equ 13		; Carriage Return
LF		equ 10		; Line Feed
FF		equ 12		; Form Feed
BS		equ  8		; Backspace

;
; Lower-case table for codepage 865
;
lcase_low       equ 128
lcase_high      equ 165
lcase_tab       db 135, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138
                db 139, 140, 141, 132, 134, 130, 145, 145, 147, 148, 149
                db 150, 151, 152, 148, 129, 155, 156, 155, 158, 159, 160
                db 161, 162, 163, 164, 164

copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
boot_prompt	db 'boot: ', 0
wipe_char	db BS, ' ', BS, 0
err_notfound	db 'Could not find kernel image: ',0
err_notkernel	db CR, LF, 'Invalid or corrupt kernel image.', CR, LF, 0
err_not386	db 'It appears your computer uses a 286 or lower CPU.'
		db CR, LF
		db 'You cannot run Linux unless you have a 386 or higher CPU'
		db CR, LF
		db 'in your machine.  If you get this message in error, hold'
		db CR, LF
		db 'down the Ctrl key while booting, and I will take your'
		db CR, LF
		db 'word for it.', CR, LF, 0
err_noram	db 'It appears your computer has less than 488K of low ("DOS")'
		db CR, LF
		db 'RAM.  Linux needs at least this amount to boot.  If you get'
		db CR, LF
		db 'this message in error, hold down the Ctrl key while'
		db CR, LF
		db 'booting, and I will take your word for it.', CR, LF, 0
err_badcfg      db 'Unknown keyword in syslinux.cfg.', CR, LF, 0
err_noparm      db 'Missing parameter in syslinux.cfg.', CR, LF, 0
err_noinitrd    db CR, LF, 'Could not find ramdisk image: ', 0
err_nohighmem   db 'Not enough memory to load specified kernel.', CR, LF, 0
err_highload    db CR, LF, 'Kernel transfer failure.', CR, LF, 0
err_oldkernel   db 'Cannot load a ramdisk with an old kernel image.'
                db CR, LF, 0
err_notdos	db ': attempted DOS system call', CR, LF, 0
err_comlarge	db 'COMBOOT image too large.', CR, LF, 0
err_a20		db CR, LF, 'A20 gate not responding!', CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: please change disks and press '
		db 'a key to continue.', CR, LF, 0
ready_msg	db 'Ready.', CR, LF, 0
crlfloading_msg	db CR, LF
loading_msg     db 'Loading ', 0
dotdot_msg      db '.'
dot_msg         db '.', 0
aborted_msg	db ' aborted.'			; Fall through to crlf_msg!
crlf_msg	db CR, LF, 0
crff_msg	db CR, FF, 0
syslinux_cfg	db 'SYSLINUXCFG'
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
; Extensions to search for (in *reverse* order).  Note that the last
; (lexically first) entry in the table is a placeholder for the original
; extension, needed for error messages.  The exten_table is shifted so
; the table is 1-based; this is because a "loop" cx is used as index.
;
exten_table:
OrigKernelExt:	dd 0			; Original extension
		db 'COM',0		; COMBOOT (same as DOS)
		db 'BS ',0		; Boot Sector 
		db 'BSS',0		; Boot Sector (add superblock)
		db 'CBT',0		; COMBOOT (specific)

exten_count	equ (($-exten_table) >> 2) - 1	; Number of alternates
;
; Misc initialized (data) variables
;
%ifdef debug				; This code for debugging only
debug_magic	dw 0D00Dh		; Debug code sentinel
%endif
AppendLen       dw 0                    ; Bytes in append= command
KbdTimeOut      dw 0                    ; Keyboard timeout (if any)
CmdLinePtr	dw cmd_line_here	; Command line advancing pointer
initrd_flag	equ $
initrd_ptr	dw 0			; Initial ramdisk pointer/flag
VKernelCtr	dw 0			; Number of registered vkernels
ForcePrompt	dw 0			; Force prompt
AllowImplicit   dw 1                    ; Allow implicit kernels
SerialPort	dw 0			; Serial port base (or 0 for no serial port)
VGAFontSize	dw 16			; Defaults to 16 byte font
UserFont	db 0			; Using a user-specified font
ScrollAttribute	db 07h			; White on black (for text mode)
;
; Stuff for the command line; we do some trickery here with equ to avoid
; tons of zeros appended to our file and wasting space
;
linuxauto_cmd	db 'linux auto',0
linuxauto_len   equ $-linuxauto_cmd
boot_image      db 'BOOT_IMAGE='
boot_image_len  equ $-boot_image
                align 4, db 0		; For the good of REP MOVSD
command_line	equ $
default_cmd	equ $+(max_cmd_len+2)
ldlinux_end	equ default_cmd+(max_cmd_len+1)
kern_cmd_len    equ ldlinux_end-command_line
ldlinux_len	equ ldlinux_end-ldlinux_magic
;
; Put the getcbuf right after the code, aligned on a sector boundary
;
end_of_code	equ (ldlinux_end-bootsec)+7C00h
getcbuf		equ (end_of_code + 511) & 0FE00h

; VGA font buffer at the end of memory (so loading a font works even
; in graphics mode.)
vgafontbuf	equ 0E000h

; This is a compile-time assert that we didn't run out of space
%ifndef DEPEND
%if (getcbuf+trackbufsize) > vgafontbuf
%error "Out of memory, better reorganize something..."
%endif
%endif
