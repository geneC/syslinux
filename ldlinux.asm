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

;
; Some semi-configurable constants... change on your own risk.  Most are imposed
; by the kernel.
;
max_cmd_len	equ 255			; Must be odd; 255 is the kernel limit
retry_count	equ 6			; How patient are we with the disk?
HIGHMEM_MAX	equ 037FFFFFFh		; DEFAULT highest address for an initrd
DEFAULT_BAUD	equ 9600		; Default baud rate for serial port
BAUD_DIVISOR	equ 115200		; Serial port parameter
;
; Should be updated with every release to avoid bootsector/SYS file mismatch
;
%define	version_str	VERSION		; Must be 4 characters long!
%define date		DATE_STR	; Defined from the Makefile
%define	year		'2002'
;
; Debgging stuff
;
; %define debug 1			; Uncomment to enable debugging
;
; ID for SYSLINUX (reported to kernel)
;
syslinux_id	equ 031h		; SYSLINUX (3) version 1.x (1)
;
; Segments used by Linux
;
; Note: the real_mode_seg is supposed to be 9000h, but some device drivers
; hog some of high memory.  Therefore, we load it at 7000:0000h and copy
; it before starting the Linux kernel.
;
real_mode_seg	equ 7000h
fake_setup_seg	equ real_mode_seg+020h

		struc real_mode_seg_t
		resb 20h-($-$$)		; org 20h
kern_cmd_magic	resw 1			; 0020 Magic # for command line
kern_cmd_offset resw 1			; 0022 Offset for kernel command line
		resb 497-($-$$)		; org 497d
bs_setupsecs	resb 1			; 01F1 Sectors for setup code (0 -> 4)
bs_rootflags	resw 1			; 01F2 Root readonly flag
bs_syssize	resw 1			; 01F4
bs_swapdev	resw 1			; 01F6 Swap device (obsolete)
bs_ramsize	resw 1			; 01F8 Ramdisk flags, formerly ramdisk size
bs_vidmode	resw 1			; 01FA Video mode
bs_rootdev	resw 1			; 01FC Root device
bs_bootsign	resw 1			; 01FE Boot sector signature (0AA55h)
su_jump		resb 1			; 0200 0EBh
su_jump2	resb 1			; 0201 Size of following header
su_header	resd 1			; 0202 New setup code: header
su_version	resw 1			; 0206 See linux/arch/i386/boot/setup.S
su_switch	resw 1			; 0208
su_setupseg	resw 1			; 020A
su_startsys	resw 1			; 020C
su_kver		resw 1			; 020E Kernel version pointer
su_loader	resb 1			; 0210 Loader ID
su_loadflags	resb 1			; 0211 Load high flag
su_movesize	resw 1			; 0212
su_code32start	resd 1			; 0214 Start of code loaded high
su_ramdiskat	resd 1			; 0218 Start of initial ramdisk
su_ramdisklen	equ $			; Length of initial ramdisk
su_ramdisklen1	resw 1			; 021C
su_ramdisklen2	resw 1			; 021E
su_bsklugeoffs	resw 1			; 0220
su_bsklugeseg	resw 1			; 0222
su_heapend	resw 1			; 0224
su_pad1		resw 1			; 0226
su_cmd_line_ptr	resd 1			; 0228
su_ramdisk_max	resd 1			; 022C
		resb (9000h-12)-($-$$)	; The setup is up to 32K long
linux_stack	equ $			; 8FF4
linux_fdctab	equ $
		resb 9000h-($-$$)
cmd_line_here	equ $			; 9000 Should be out of the way
		endstruc

;
; Kernel command line signature
;
CMD_MAGIC	equ 0A33Fh		; Command line magic

;
; Magic number of su_header field
;
HEADER_ID       equ 'HdrS'		; HdrS (in littleendian hex)

;
; Flags for the su_loadflags field
;
LOAD_HIGH	equ 01h			; Large kernel, load high
CAN_USE_HEAP    equ 80h                 ; Boot loader reports heap size

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
vk_vname:	resb 11			; Virtual name **MUST BE FIRST!**
vk_rname:	resb 11			; Real name
vk_appendlen:	resw 1
		alignb 4
vk_append:	resb max_cmd_len+1	; Command line
		alignb 4
vk_end:		equ $			; Should be <= vk_size
		endstruc

%if (vk_end > vk_size) || (vk_size*max_vk > 65536)
%error "Too many vkernels defined, reduce vk_power"
%endif

;
; Segment assignments in the bottom 640K
; Stick to the low 512K in case we're using something like M-systems flash
; which load a driver into low RAM (evil!!)
;
; 0000h - main code/data segment (and BIOS segment)
; 7000h - real_mode_seg
;
fat_seg		equ 5000h		; 128K area for FAT (2x64K)
vk_seg          equ 4000h		; Virtual kernels
xfer_buf_seg	equ 3000h		; Bounce buffer for I/O to high mem
comboot_seg	equ 2000h		; COMBOOT image loading zone

;
; For our convenience: define macros for jump-over-unconditinal jumps
;
%macro	jmpz	1
	jnz %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnz	1
	jz %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpe	1
	jne %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpne	1
	je %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpc	1
	jnc %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnc	1
	jc %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpb	1
	jnb %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnb	1
	jb %%skip
	jmp %1
%%skip:
%endmacro

;
; Macros similar to res[bwd], but which works in the code segment (after
; section .text)
;
%macro	zb	1
	times %1 db 0
%endmacro

%macro	zw	1
	times %1 dw 0
%endmacro

%macro	zd	1
	times %1 dd 0
%endmacro

; ---------------------------------------------------------------------------
;   BEGIN THE BIOS/CODE/DATA SEGMENT
; ---------------------------------------------------------------------------
		absolute 4*1Eh		; In the interrupt table
fdctab		equ $
fdctab1		resw 1
fdctab2		resw 1

		absolute 0400h
serial_base	resw 4			; Base addresses for 4 serial ports

                absolute 0484h
BIOS_vidrows    resb 1			; Number of screen rows

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
SavedSP		resw 1			; Our SP while running a COMBOOT image
A20Test		resw 1			; Counter for testing status of A20
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
%if bs_checkpt_off > 1EFh
%error "Boot sector overflow"
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
%if rl_checkpt_off > 400h
%error "Sector 1 overflow"
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
; Check that no moron is trying to boot Linux on a 286 or so.  According
; to Intel, the way to check is to see if the high 4 bits of the FLAGS
; register are either all stuck at 1 (8086/8088) or all stuck at 0
; (286 in real mode), if not it is a 386 or higher.  They didn't
; say how to check for a 186/188, so I *hope* it falls out as a 8086
; or 286 in this test.
;
; Also, provide an escape route in case it doesn't work.
;
check_escapes:
		mov ah,02h			; Check keyboard flags
		int 16h
		mov [KbdFlags],al		; Save for boot prompt check
		test al,04h			; Ctrl->skip 386 check
		jnz skip_checks
test_8086:
		pushf				; Get flags
		pop ax
		and ax,0FFFh			; Clear top 4 bits
		push ax				; Load into FLAGS
		popf
		pushf				; And load back
		pop ax
		and ax,0F000h			; Get top 4 bits
		cmp ax,0F000h			; If set -> 8086/8088
		je not_386
test_286:
		pushf				; Get flags
		pop ax
		or ax,0F000h			; Set top 4 bits
		push ax
		popf
		pushf
		pop ax
		and ax,0F000h			; Get top 4 bits
		jnz is_386			; If not clear -> 386
not_386:
		mov si,err_not386
		call writestr
		jmp kaboom
is_386:
		; Now we know it's a 386 or higher
;
; Now check that there is at least 512K of low (DOS) memory
;
		int 12h
		cmp ax,512
		jae enough_ram
		mov si,err_noram
		call writestr
		jmp kaboom
enough_ram:
skip_checks:
;
; Check if we're 386 (as opposed to 486+); if so we need to blank out
; the WBINVD instruction
;
; We check for 486 by setting EFLAGS.AC
;
		pushfd				; Save the good flags
		pushfd
		pop eax
		mov ebx,eax
		xor eax,(1 << 18)		; AC bit
		push eax
		popfd
		pushfd
		pop eax
		popfd				; Restore the original flags
		xor eax,ebx
		jnz is_486
;
; 386 - Looks like we better blot out the WBINVD instruction
;
		mov byte [try_wbinvd],0c3h		; Near RET		
is_486:

;
; Initialization that does not need to go into the any of the pre-load
; areas
;
		; Now set up screen parameters
		call adjust_screen
;
; Now, everything is "up and running"... patch kaboom for more
; verbosity and using the full screen system
;
		mov byte [kaboom.patch],0e9h		; JMP NEAR
		mov word [kaboom.patch+1],kaboom2-(kaboom.patch+3)

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
parse_config:
		call getkeyword
                jc near end_config_file		; Config file loaded
		cmp ax,'de'			; DEfault
		je pc_default
		cmp ax,'ap'			; APpend
		je pc_append
		cmp ax,'ti'			; TImeout
		je near pc_timeout
		cmp ax,'pr'			; PRompt
		je near pc_prompt
		cmp ax,'fo'			; FOnt
		je near pc_font
		cmp ax,'kb'			; KBd
		je near pc_kbd
		cmp ax,'di'			; DIsplay
		je near pc_display
		cmp ax,'la'			; LAbel
		je near pc_label
		cmp ax,'ke'			; KErnel
		je pc_kernel
                cmp ax,'im'                     ; IMplicit
                je near pc_implicit
		cmp ax,'se'			; SErial
		je near pc_serial
		cmp al,'f'			; F-key
		jne parse_config
		jmp pc_fkey

pc_default:	mov di,default_cmd		; "default" command
		call getline
		xor al,al
		stosb				; null-terminate
		jmp short parse_config

pc_append:      cmp word [VKernelCtr],byte 0	; "append" command
		ja pc_append_vk
                mov di,AppendBuf
		call getline
                sub di,AppendBuf
pc_app1:        mov [AppendLen],di
                jmp short parse_config
pc_append_vk:	mov di,VKernelBuf+vk_append	; "append" command (vkernel)
		call getline
		sub di,VKernelBuf+vk_append
                cmp di,byte 2
                jne pc_app2
                cmp byte [VKernelBuf+vk_append],'-'
                jne pc_app2
                mov di,0                        ; If "append -" -> null string
pc_app2:        mov [VKernelBuf+vk_appendlen],di
		jmp short parse_config_2	

pc_kernel:	cmp word [VKernelCtr],byte 0	; "kernel" command
		je near parse_config		; ("label" section only)
		mov di,trackbuf
		push di
		call getline
		pop si
		mov di,VKernelBuf+vk_rname
		call mangle_name
		jmp short parse_config_2

pc_timeout:	call getint			; "timeout" command
		jc parse_config_2
		mov ax,0D215h			; There are approx 1.D215h
		mul bx				; clock ticks per 1/10 s
		add bx,dx
		mov [KbdTimeOut],bx
		jmp short parse_config_2

pc_display:	call pc_getfile			; "display" command
		jz parse_config_2		; File not found?
		call get_msg_file		; Load and display file
parse_config_2: jmp parse_config

pc_prompt:	call getint			; "prompt" command
		jc parse_config_2
		mov [ForcePrompt],bx
		jmp short parse_config_2

pc_implicit:    call getint                     ; "implicit" command
                jc parse_config_2
                mov [AllowImplicit],bx
                jmp short parse_config_2

pc_serial:	call getint			; "serial" command
		jc parse_config_2
		push bx				; Serial port #
		call skipspace
		jc parse_config_2
		call ungetc
		call getint
		mov [FlowControl], word 0	; Default to no flow control
		jc .nobaud
.valid_baud:	
		push ebx
		call skipspace
		jc .no_flow
		call ungetc
		call getint			; Hardware flow control?
		jnc .valid_flow
.no_flow:
		xor bx,bx			; Default -> no flow control
.valid_flow:
		and bh,0Fh			; FlowIgnore
		shl bh,4
		mov [FlowIgnore],bh
		mov bh,bl
		and bx,0F003h			; Valid bits
		mov [FlowControl],bx
		pop ebx				; Baud rate
		jmp short .parse_baud
.nobaud:
		mov ebx,DEFAULT_BAUD		; No baud rate given
.parse_baud:
		pop di				; Serial port #
		cmp ebx,byte 75
		jb parse_config_2		; < 75 baud == bogus
		mov eax,BAUD_DIVISOR
		cdq
		div ebx
		push ax				; Baud rate divisor
		cmp di,3
		ja .port_is_io			; If port > 3 then port is I/O addr
		shl di,1
		mov di,[di+serial_base]		; Get the I/O port from the BIOS
.port_is_io:
		mov [SerialPort],di
		lea dx,[di+3]			; DX -> LCR
		mov al,83h			; Enable DLAB
		call slow_out
		pop ax				; Divisor
		mov dx,di			; DX -> LS
		call slow_out
		inc dx				; DX -> MS
		mov al,ah
		call slow_out
		mov al,03h			; Disable DLAB
		add dx,byte 2			; DX -> LCR
		call slow_out
		in al,dx			; Read back LCR (detect missing hw)
		cmp al,03h			; If nothing here we'll read 00 or FF
		jne .serial_port_bad		; Assume serial port busted
		sub dx,byte 2			; DX -> IER
		xor al,al			; IRQ disable
		call slow_out

		add dx,byte 3			; DX -> MCR
		in al,dx
		or al,[FlowOutput]		; Assert bits
		call slow_out

		; Show some life
		mov si,syslinux_banner
		call write_serial_str
		mov si,copyright_str
		call write_serial_str

		jmp short parse_config_3

.serial_port_bad:
		mov [SerialPort], word 0
		jmp short parse_config_3

pc_fkey:	sub ah,'1'
		jnb pc_fkey1
		mov ah,9			; F10
pc_fkey1:	xor cx,cx
		mov cl,ah
		push cx
		mov ax,1
		shl ax,cl
		or [FKeyMap], ax		; Mark that we have this loaded
		mov di,trackbuf
		push di
		call getline			; Get filename to display
		pop si
		pop di
		shl di,4			; Multiply number by 16
		add di,FKeyName
		call mangle_name		; Mangle file name
		jmp short parse_config_3

pc_label:	call commit_vk			; Commit any current vkernel
		mov di,trackbuf			; Get virtual filename
		push di
		call getline
		pop si
		mov di,VKernelBuf+vk_vname
		call mangle_name		; Mangle virtual name
		inc word [VKernelCtr]		; One more vkernel
		mov si,VKernelBuf+vk_vname 	; By default, rname == vname
		mov di,VKernelBuf+vk_rname
		mov cx,11
		rep movsb
                mov si,AppendBuf         	; Default append==global append
                mov di,VKernelBuf+vk_append
                mov cx,[AppendLen]
                mov [VKernelBuf+vk_appendlen],cx
                rep movsb
		jmp near parse_config_3

pc_font:	call pc_getfile			; "font" command
		jz parse_config_3		; File not found?
		call loadfont			; Load and install font
		jmp short parse_config_3

pc_kbd:		call pc_getfile			; "kbd" command
		jz parse_config_3
		call loadkeys
parse_config_3:	jmp parse_config

;
; pc_getfile:	For command line options that take file argument, this
; 		routine decodes the file argument and runs it through searchdir
;
pc_getfile:	mov di,trackbuf
		push di
		call getline
		pop si
		mov di,MNameBuf
		push di
		call mangle_name
		pop di
		jmp searchdir			; Tailcall

;
; commit_vk: Store the current VKernelBuf into buffer segment
;
commit_vk:
		cmp word [VKernelCtr],byte 0
		je cvk_ret			; No VKernel = return
		cmp word [VKernelCtr],max_vk	; Above limit?
		ja cvk_overflow
		mov di,[VKernelCtr]
		dec di
		shl di,vk_shift
		mov si,VKernelBuf
		mov cx,(vk_size >> 2)
		push es
		push word vk_seg
		pop es
		rep movsd			; Copy to buffer segment
		pop es
cvk_ret:	ret
cvk_overflow:	mov word [VKernelCtr],max_vk	; No more than max_vk, please
		ret

;
; End of configuration file
;
end_config_file:
		call commit_vk			; Commit any current vkernel
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
		push di
		cmp ah,68			; F10
		ja get_char_2
		sub ah,59			; F1
		jb get_char_2
		shr ax,8
show_help:	; AX = func key # (0 = F1, 9 = F10)
		mov cl,al
		shl ax,4			; Convert to x16
		mov bx,1
		shl bx,cl
		and bx,[FKeyMap]
		jz get_char_2			; Undefined F-key
		mov di,ax
		add di,FKeyName
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
; .COM 	- COMBOOT image
; .CBT	- COMBOOT image
; .BS	- Boot sector
; .BSS	- Boot sector, but transfer over DOS superblock
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
		cmp ecx,'BSS'
		je near is_bss_sector
		; Otherwise Linux kernel
;
; A Linux kernel consists of three parts: boot sector, setup code, and
; kernel code.	The boot sector is never executed when using an external
; booting utility, but it contains some status bytes that are necessary.
; The boot sector and setup code together form exactly 5 sectors that
; should be loaded at 9000:0.  The subsequent code should be loaded
; at 1000:0.  For simplicity, we load the whole thing at 0F60:0, and
; copy the latter stuff afterwards.
;
; NOTE: In the previous code I have avoided making any assumptions regarding
; the size of a sector, in case this concept ever gets extended to other
; media like CD-ROM (not that a CD-ROM would be bloody likely to use a FAT
; filesystem, of course).  However, a "sector" when it comes to Linux booting
; stuff means 512 bytes *no matter what*, so here I am using that piece
; of knowledge.
;
; First check that our kernel is at least 1K and less than 8M (if it is
; more than 8M, we need to change the logic for loading it anyway...)
;
; We used to require the kernel to be 64K or larger, but it has gotten
; popular to use the Linux kernel format for other things, which may
; not be so large.
;
is_linux_kernel:
                cmp dx,80h			; 8 megs
		ja kernel_corrupt
		and dx,dx
		jnz kernel_sane
		cmp ax,1024			; Bootsect + 1 setup sect
		jb kernel_corrupt
kernel_sane:	push ax
		push dx
		push si
		mov si,loading_msg
                call cwritestr
;
; Now start transferring the kernel
;
		push word real_mode_seg
		pop es

		push ax
		push dx
		div word [ClustSize]		; # of clusters total
		and dx,dx			; Round up
		setnz dl
		movzx dx,dl
		add ax,dx
                mov [KernelClust],ax
		pop dx
		pop ax
		mov [KernelSize],ax
		mov [KernelSize+2],dx
;
; Now, if we transfer these straight, we'll hit 64K boundaries.	 Hence we
; have to see if we're loading more than 64K, and if so, load it step by
; step.
;
		mov dx,1			; 10000h
		xor ax,ax
		div word [ClustSize]
		mov [ClustPerMoby],ax		; Clusters/64K
;
; Start by loading the bootsector/setup code, to see if we need to
; do something funky.  It should fit in the first 32K (loading 64K won't
; work since we might have funny stuff up near the end of memory).
; If we have larger than 32K clusters, yes, we're hosed.
;
		call abort_check		; Check for abort key
		mov cx,[ClustPerMoby]
		shr cx,1			; Half a moby
		cmp cx,[KernelClust]
		jna .normalkernel
		mov cx,[KernelClust]
.normalkernel:
		sub [KernelClust],cx
		xor bx,bx
                pop si                          ; Cluster pointer on stack
		call getfssec
                cmp word [es:bs_bootsign],0AA55h
		jne near kernel_corrupt		; Boot sec signature missing
;
; Get the BIOS' idea of what the size of high memory is.
;
		push si				; Save our cluster pointer!
;
; First, try INT 15:E820 (get BIOS memory map)
;
get_e820:
		push es
		xor ebx,ebx			; Start with first record
		mov es,bx			; Need ES = DS = 0 for now
		jmp short .do_e820		; Skip "at end" check first time!
.int_loop:	and ebx,ebx			; If we're back at beginning...
		jz no_e820			; ... bail; nothing found
.do_e820:	mov eax,0000E820h
		mov edx,534D4150h		; "SMAP" backwards
		mov ecx,20
		mov di,E820Buf
		int 15h
		jc no_e820
		cmp eax,534D4150h
		jne no_e820
;
; Look for a memory block starting at <= 1 MB and continuing upward
;
		cmp dword [E820Buf+4], byte 0
		ja .int_loop			; Start >= 4 GB?
		mov edx, (1 << 20)
		sub edx, [E820Buf]
		jb .int_loop			; Start >= 1 MB?
		mov eax, 0FFFFFFFFh
		cmp dword [E820Buf+12], byte 0
		ja .huge			; Size >= 4 GB
		mov eax, [E820Buf+8]
.huge:		sub eax, edx			; Adjust size to start at 1 MB
		jbe .int_loop			; Completely below 1 MB?

		; Now EAX contains the size of memory 1 MB...up
		cmp dword [E820Buf+16], byte 1
		jne near err_nohighmem		; High memory isn't usable memory!!!!

		; We're good!
		pop es
		jmp short got_highmem_add1mb	; Still need to add low 1 MB

;
; INT 15:E820 failed.  Try INT 15:E801.
;
no_e820:	pop es

		mov ax,0e801h			; Query high memory (semi-recent)
		int 15h
		jc no_e801
		cmp ax,3c00h
		ja no_e801			; > 3C00h something's wrong with this call
		jb e801_hole			; If memory hole we can only use low part

		mov ax,bx
		shl eax,16			; 64K chunks
		add eax,(16 << 20)		; Add first 16M
		jmp short got_highmem				

;
; INT 15:E801 failed.  Try INT 15:88.
;
no_e801:
		mov ah,88h			; Query high memory (oldest)
		int 15h
		cmp ax,14*1024			; Don't trust memory >15M
		jna e801_hole
		mov ax,14*1024
e801_hole:
		and eax,0ffffh
		shl eax,10			; Convert from kilobytes
got_highmem_add1mb:
		add eax,(1 << 20)		; First megabyte
got_highmem:
		mov [HighMemSize],eax

;
; Construct the command line (append options have already been copied)
;
		mov di,[CmdLinePtr]
                mov si,boot_image        	; BOOT_IMAGE=
                mov cx,boot_image_len
                rep movsb
                mov si,KernelCName       	; Unmangled kernel name
                mov cx,[KernelCNameLen]
                rep movsb
                mov al,' '                      ; Space
                stosb
                mov si,[CmdOptPtr]              ; Options from user input
		mov cx,(kern_cmd_len+3) >> 2
		rep movsd
;
%ifdef debug
                push ds                         ; DEBUG DEBUG DEBUG
                push es
                pop ds
                mov si,cmd_line_here
                call cwritestr
                pop ds
		call crlf
%endif
;
; Scan through the command line for anything that looks like we might be
; interested in.  The original version of this code automatically assumed
; the first option was BOOT_IMAGE=, but that is no longer certain.
;
		mov si,cmd_line_here
                mov byte [initrd_flag],0
                push es				; Set DS <- real_mode_seg
                pop ds
get_next_opt:   lodsb
		and al,al
		jz near cmdline_end
		cmp al,' '
		jbe get_next_opt
		dec si
                mov eax,[si]
                cmp eax,'vga='
		je is_vga_cmd
                cmp eax,'mem='
		je is_mem_cmd
                push es                         ; Save ES -> real_mode_seg
                push ss
                pop es                          ; Set ES <- normal DS
                mov di,initrd_cmd
		mov cx,initrd_cmd_len
		repe cmpsb
                jne not_initrd
		mov di,InitRD
                push si                         ; mangle_dir mangles si
                call mangle_name                ; Mangle ramdisk name
                pop si
		cmp byte [es:InitRD],' '	; Null filename?
                seta byte [es:initrd_flag]	; Set flag if not
not_initrd:	pop es                          ; Restore ES -> real_mode_seg
skip_this_opt:  lodsb                           ; Load from command line
                cmp al,' '
                ja skip_this_opt
                dec si
                jmp short get_next_opt
is_vga_cmd:
                add si,byte 4
                mov eax,[si]
                mov bx,-1
                cmp eax, 'norm'                 ; vga=normal
                je vc0
                and eax,0ffffffh		; 3 bytes
                mov bx,-2
                cmp eax, 'ext'                  ; vga=ext
                je vc0
                mov bx,-3
                cmp eax, 'ask'                  ; vga=ask
                je vc0
                call parseint                   ; vga=<number>
		jc skip_this_opt		; Not an integer
vc0:		mov [bs_vidmode],bx		; Set video mode
		jmp short skip_this_opt
is_mem_cmd:
                add si,byte 4
                call parseint
		jc skip_this_opt		; Not an integer
		mov [cs:HighMemSize],ebx
		jmp short skip_this_opt
cmdline_end:
                push cs                         ; Restore standard DS
                pop ds
		sub si,cmd_line_here
		mov [CmdLineLen],si		; Length including final null
;
; Now check if we have a large kernel, which needs to be loaded high
;
		mov dword [RamdiskMax], HIGHMEM_MAX	; Default initrd limit
		cmp dword [es:su_header],HEADER_ID	; New setup code ID
		jne near old_kernel		; Old kernel, load low
		cmp word [es:su_version],0200h	; Setup code version 2.0
		jb near old_kernel		; Old kernel, load low
                cmp word [es:su_version],0201h	; Version 2.01+?
                jb new_kernel                   ; If 2.00, skip this step
                mov word [es:su_heapend],linux_stack	; Set up the heap
                or byte [es:su_loadflags],80h	; Let the kernel know we care
		cmp word [es:su_version],0203h	; Version 2.03+?
		jb new_kernel			; Not 2.03+
		mov eax,[es:su_ramdisk_max]
		mov [RamdiskMax],eax		; Set the ramdisk limit

;
; We definitely have a new-style kernel.  Let the kernel know who we are,
; and that we are clueful
;
new_kernel:
		mov byte [es:su_loader],syslinux_id	; Show some ID
		movzx ax,byte [es:bs_setupsecs]	; Variable # of setup sectors
		mov [SetupSecs],ax
;
; Now see if we have an initial RAMdisk; if so, do requisite computation
;
                test byte [initrd_flag],1
                jz nk_noinitrd
                push es                         ; ES->real_mode_seg
                push ds
                pop es                          ; We need ES==DS
                mov si,InitRD
                mov di,InitRDCName
                call unmangle_name              ; Create human-readable name
                sub di,InitRDCName
                mov [InitRDCNameLen],di
                mov di,InitRD
                call searchdir                  ; Look for it in directory
                pop es
		jz initrd_notthere
		mov [es:su_ramdisklen1],ax	; Ram disk length
		mov [es:su_ramdisklen2],dx
		mov edx,[HighMemSize]		; End of memory
		dec edx
		mov eax,[RamdiskMax]		; Highest address allowed by kernel
		cmp edx,eax
		jna memsize_ok
		mov edx,eax			; Adjust to fit inside limit
memsize_ok:
		inc edx
                xor dx,dx			; Round down to 64K boundary
		sub edx,[es:su_ramdisklen]	; Subtract size of ramdisk
                xor dx,dx			; Round down to 64K boundary
                mov [es:su_ramdiskat],edx	; Load address
		call loadinitrd			; Load initial ramdisk
		jmp short initrd_end

initrd_notthere:
                mov si,err_noinitrd
                call cwritestr
                mov si,InitRDCName
                call cwritestr
                mov si,crlf_msg
                jmp abort_load

no_high_mem:    mov si,err_nohighmem		; Error routine
                jmp abort_load
;
; About to load the kernel.  This is a modern kernel, so use the boot flags
; we were provided.
;
nk_noinitrd:
initrd_end:
                mov al,[es:su_loadflags]
		mov [LoadFlags],al
;
; Load the kernel.  We always load it at 100000h even if we're supposed to
; load it "low"; for a "low" load we copy it down to low memory right before
; jumping to it.
;
read_kernel:
                mov si,KernelCName		; Print kernel name part of
                call cwritestr                  ; "Loading" message
                mov si,dotdot_msg		; Print dots
                call cwritestr

                mov eax,[HighMemSize]
		sub eax,100000h			; Load address
		cmp eax,[KernelSize]
		jb no_high_mem			; Not enough high memory
;
; Move the stuff beyond the setup code to high memory at 100000h
;
		movzx esi,word [SetupSecs]	; Setup sectors
		inc esi				; plus 1 boot sector
                shl esi,9			; Convert to bytes
                mov ecx,8000h			; 32K
                sub ecx,esi			; Number of bytes to copy
		push ecx
		shr ecx,2			; Convert to dwords
		add esi,(real_mode_seg << 4)	; Pointer to source
                mov edi,100000h                 ; Copy to address 100000h
                call bcopy			; Transfer to high memory

		; On exit EDI -> where to load the rest

                mov si,dot_msg			; Progress report
                call cwritestr
                call abort_check

		pop ecx				; Number of bytes in the initial portion
		pop si				; Restore file handle/cluster pointer
		mov eax,[KernelSize]
		sub eax,ecx			; Amount of kernel left over
		jbe high_load_done		; Zero left (tiny kernel)

		call load_high			; Copy the file

high_load_done:
                mov ax,real_mode_seg		; Set to real mode seg
                mov fs,ax			; FS -> real_mode_seg

                mov si,dot_msg
                call cwritestr
;
; Abandon hope, ye that enter here!  We do no longer permit aborts.
;
                call abort_check        	; Last chance!!

		mov si,ready_msg
		call cwritestr

		call vgaclearmode		; We can't trust ourselves after this
;
; Now, if we were supposed to load "low", copy the kernel down to 10000h
; and the real mode stuff to 90000h.  We assume that all bzImage kernels are
; capable of starting their setup from a different address.
;

;
; Copy command line.  Unfortunately, the kernel boot protocol requires
; the command line to exist in the 9xxxxh range even if the rest of the
; setup doesn't.
;
		cli				; In case of hooked interrupts
		test byte [LoadFlags],LOAD_HIGH
		jz need_high_cmdline
		cmp word [fs:su_version],0202h	; Support new cmdline protocol?
		jb need_high_cmdline
		; New cmdline protocol
		; Store 32-bit (flat) pointer to command line
		mov dword [fs:su_cmd_line_ptr],(real_mode_seg << 4) + cmd_line_here
		jmp short in_proper_place

need_high_cmdline:
;
; Copy command line up to 90000h
;
		mov ax,9000h
		mov es,ax
		mov si,cmd_line_here
		mov di,si
		mov [fs:kern_cmd_magic],word CMD_MAGIC ; Store magic
		mov [fs:kern_cmd_offset],di	; Store pointer

		mov cx,[CmdLineLen]
		add cx,byte 3
		shr cx,2			; Convert to dwords
		fs rep movsd

		push fs
		pop es

		test byte [LoadFlags],LOAD_HIGH
		jnz in_proper_place		; If high load, we're done

;
; Loading low; we can't assume it's safe to run in place.
;
; Copy real_mode stuff up to 90000h
;
		mov ax,9000h
		mov es,ax
		mov cx,[SetupSecs]
		inc cx				; Setup + boot sector
		shl cx,7			; Sectors -> dwords
		xor si,si
		xor di,di
		fs rep movsd			; Copy setup + boot sector
;
; Some kernels in the 1.2 ballpark but pre-bzImage have more than 4
; setup sectors, but the boot protocol had not yet been defined.  They
; rely on a signature to figure out if they need to copy stuff from
; the "protected mode" kernel area.  Unfortunately, we used that area
; as a transfer buffer, so it's going to find the signature there.
; Hence, zero the low 32K beyond the setup area.
;
		mov di,[SetupSecs]
		inc di				; Setup + boot sector
		mov cx,32768/512		; Sectors/32K
		sub cx,di			; Remaining sectors
		shl di,9			; Sectors -> bytes
		shl cx,7			; Sectors -> dwords
		xor eax,eax
		rep stosd			; Clear region
;
; Copy the kernel down to the "low" location
;
		mov ecx,[KernelSize]
		add ecx,3			; Round upwards
		shr ecx,2			; Bytes -> dwords
		mov esi,100000h
		mov edi,10000h
		call bcopy

;
; Now everything is where it needs to be...
;
; When we get here, es points to the final segment, either
; 9000h or real_mode_seg
;
in_proper_place:

;
; If the default root device is set to FLOPPY (0000h), change to
; /dev/fd0 (0200h)
;
		cmp word [es:bs_rootdev],byte 0
		jne root_not_floppy
		mov word [es:bs_rootdev],0200h
root_not_floppy:
;
; Copy the disk table to high memory, then re-initialize the floppy
; controller
;
; This needs to be moved before the copy
;
%if 0
		push ds
		push bx
		lds si,[fdctab]
		mov di,linux_fdctab
		mov cx,3			; 12 bytes
		push di
		rep movsd
		pop di
		mov [fdctab1],di		; Save new floppy tab pos
		mov [fdctab2],es
		xor ax,ax
		xor dx,dx
		int 13h
		pop bx
		pop ds
%endif
;
; Linux wants the floppy motor shut off before starting the kernel,
; at least bootsect.S seems to imply so
;
kill_motor:
		mov dx,03F2h
		xor al,al
		call slow_out
;
; If we're debugging, wait for a keypress so we can read any debug messages
;
%ifdef debug
                xor ax,ax
                int 16h
%endif
;
; Set up segment registers and the Linux real-mode stack
; Note: es == the real mode segment
;
		cli
		mov bx,es
		mov ds,bx
		mov fs,bx
		mov gs,bx
		mov ss,bx
		mov sp,linux_stack
;
; We're done... now RUN THAT KERNEL!!!!
; Setup segment == real mode segment + 020h; we need to jump to offset
; zero in the real mode segment.
;
		add bx,020h
		push bx
		push word 0h
		retf

;
; Load an older kernel.  Older kernels always have 4 setup sectors, can't have
; initrd, and are always loaded low.
;
old_kernel:
                test byte [initrd_flag],1	; Old kernel can't have initrd
                jz load_old_kernel
                mov si,err_oldkernel
                jmp abort_load
load_old_kernel:
		mov word [SetupSecs],4		; Always 4 setup sectors
		mov byte [LoadFlags],0		; Always low
		jmp read_kernel

;
; Load a COMBOOT image.  A COMBOOT image is basically a DOS .COM file,
; except that it may, of course, not contain any DOS system calls.  We
; do, however, allow the execution of INT 20h to return to SYSLINUX.
;
is_comboot_image:
		and dx,dx
		jnz comboot_too_large
		cmp ax,0ff00h		; Max size in bytes
		jae comboot_too_large

		;
		; Set up the DOS vectors in the IVT (INT 20h-3fh)
		;
		mov dword [4*0x20],comboot_return	; INT 20h vector
		mov eax,comboot_bogus
		mov di,4*0x21
		mov cx,31		; All remaining DOS vectors
		rep stosd
	
		mov cx,comboot_seg
		mov es,cx

		mov bx,100h		; Load at <seg>:0100h

		mov cx,[ClustPerMoby]	; Absolute maximum # of clusters
		call getfssec

		xor di,di
		mov cx,64		; 256 bytes (size of PSP)
		xor eax,eax		; Clear PSP
		rep stosd

		mov word [es:0], 020CDh	; INT 20h instruction
		; First non-free paragraph
		mov word [es:02h], comboot_seg+1000h

		; Copy the command line from high memory
		mov cx,125		; Max cmdline len (minus space and CR)
		mov si,[CmdOptPtr]
		mov di,081h		; Offset in PSP for command line
		mov al,' '		; DOS command lines begin with a space
		stosb

comboot_cmd_cp:	lodsb
		and al,al
		jz comboot_end_cmd
		stosb
		loop comboot_cmd_cp
comboot_end_cmd: mov al,0Dh		; CR after last character
		stosb
		mov al,126		; Include space but not CR
		sub al,cl
		mov [es:80h], al	; Store command line length

		call vgaclearmode	; Reset video

		mov ax,es
		mov ds,ax
		mov ss,ax
		xor sp,sp
		push word 0		; Return to address 0 -> exit

		jmp comboot_seg:100h	; Run it

; Looks like a COMBOOT image but too large
comboot_too_large:
		mov si,err_comlarge
		call cwritestr
cb_enter:	jmp enter_command

; Proper return vector
comboot_return:	cli			; Don't trust anyone
		xor ax,ax
		mov ss,ax
		mov sp,[ss:SavedSP]
		mov ds,ax
		mov es,ax
		sti
		cld
		jmp short cb_enter

; Attempted to execute DOS system call
comboot_bogus:	cli			; Don't trust anyone
		xor ax,ax
		mov ss,ax
		mov sp,[ss:SavedSP]
		mov ds,ax
		mov es,ax
		sti
		cld
		mov si,KernelCName
		call cwritestr
		mov si,err_notdos
		call cwritestr
		jmp short cb_enter

;
; Load a boot sector
;
is_bootsector:
		; Transfer zero bytes
		push word 0
		jmp short load_bootsec
is_bss_sector:
		; Transfer the superblock
		push word superblock_len
load_bootsec:
		and dx,dx
		jnz bad_bootsec
		mov bx,[bsBytesPerSec]
		cmp ax,bx
		jne bad_bootsec

		; Make sure we don't test this uninitialized
		mov [bx+trackbuf-2],dx	; Note DX == 0

		mov bx,trackbuf
		mov cx,1		; 1 cluster >= 1 sector
		call getfssec

		mov bx,[bsBytesPerSec]
		mov ax,[bx+trackbuf-2]
		cmp ax,0AA55h		; Boot sector signature
		jne bad_bootsec

		mov si,superblock
		mov di,trackbuf+(superblock-bootsec)
		pop cx			; Transfer count
		rep movsb
;
; Okay, here we go... copy over our own boot sector and run the new one
;
		call vgaclearmode	; Reset video

		cli			; Point of no return
	
		mov dl,[bsDriveNumber]	; May not be in new bootsector!

		mov si,trackbuf
		mov di,bootsec
		mov cx,[bsBytesPerSec]
		rep movsb		; Copy the boot sector!
		
		mov si,PartInfo
		mov di,800h-18		; Put partition info here
		push di
		mov cx,8		; 16 bytes
		rep movsw
		pop si			; DS:SI points to partition info

		jmp bootsec

bad_bootsec:
		mov si,err_bootsec
		call cwritestr
		jmp enter_command

;
; 32-bit bcopy routine for real mode
;
; We enter protected mode, set up a flat 32-bit environment, run rep movsd
; and then exit.  IMPORTANT: This code assumes cs == ss == 0.
;
; This code is probably excessively anal-retentive in its handling of
; segments, but this stuff is painful enough as it is without having to rely
; on everything happening "as it ought to."
;
		align 4
bcopy_gdt:	dw bcopy_gdt_size-1	; Null descriptor - contains GDT
		dd bcopy_gdt		; pointer for LGDT instruction
		dw 0
		dd 0000ffffh		; Code segment, use16, readable,
		dd 00009b00h		; present, dpl 0, cover 64K
		dd 0000ffffh		; Data segment, use16, read/write,
		dd 008f9300h		; present, dpl 0, cover all 4G
		dd 0000ffffh		; Data segment, use16, read/write,
		dd 00009300h		; present, dpl 0, cover 64K
bcopy_gdt_size:	equ $-bcopy_gdt

bcopy:		push eax
		pushf			; Saves, among others, the IF flag
		push gs
		push fs
		push ds
		push es

		cli
		call enable_a20

		o32 lgdt [cs:bcopy_gdt]
		mov eax,cr0
		or al,1
		mov cr0,eax		; Enter protected mode
		jmp 08h:.in_pm

.in_pm:		mov ax,10h		; Data segment selector
		mov es,ax
		mov ds,ax

		mov al,18h		; "Real-mode-like" data segment
		mov ss,ax
		mov fs,ax
		mov gs,ax	
	
		a32 rep movsd		; Do our business
		
		mov es,ax		; Set to "real-mode-like"
		mov ds,ax
	
		mov eax,cr0
		and al,~1
		mov cr0,eax		; Disable protected mode
		jmp 0:.in_rm

.in_rm:		xor ax,ax		; Back in real mode
		mov ss,ax
		pop es
		pop ds
		pop fs
		pop gs
		call disable_a20

		popf			; Re-enables interrupts
		pop eax
		ret

;
; Routines to enable and disable (yuck) A20.  These routines are gathered
; from tips from a couple of sources, including the Linux kernel and
; http://www.x86.org/.  The need for the delay to be as large as given here
; is indicated by Donnie Barnes of RedHat, the problematic system being an
; IBM ThinkPad 760EL.
;
; We typically toggle A20 twice for every 64K transferred.
; 
%define	io_delay	call _io_delay
%define IO_DELAY_PORT	80h		; Invalid port (we hope!)
%define disable_wait 	32		; How long to wait for a disable

%define A20_DUNNO	0		; A20 type unknown
%define A20_NONE	1		; A20 always on?
%define A20_BIOS	2		; A20 BIOS enable
%define A20_KBC		3		; A20 through KBC
%define A20_FAST	4		; A20 through port 92h

slow_out:	out dx, al		; Fall through

_io_delay:	out IO_DELAY_PORT,al
		out IO_DELAY_PORT,al
		ret

enable_a20:
		pushad
		mov byte [cs:A20Tries],255 ; Times to try to make this work

try_enable_a20:
;
; Flush the caches
;
;		call try_wbinvd

;
; If the A20 type is known, jump straight to type
;
		mov bp,[cs:A20Type]
		add bp,bp			; Convert to word offset
		jmp word [cs:bp+A20List]

;
; First, see if we are on a system with no A20 gate
;
a20_dunno:
a20_none:
		mov byte [cs:A20Type], A20_NONE
		call a20_test
		jnz a20_done

;
; Next, try the BIOS (INT 15h AX=2401h)
;
a20_bios:
		mov byte [cs:A20Type], A20_BIOS
		mov ax,2401h
		pushf				; Some BIOSes muck with IF
		int 15h
		popf

		call a20_test
		jnz a20_done

;
; Enable the keyboard controller A20 gate
;
a20_kbc:
		mov dl, 1			; Allow early exit
		call empty_8042
		jnz a20_done			; A20 live, no need to use KBC

		mov byte [cs:A20Type], A20_KBC	; Starting KBC command sequence

		mov al,0D1h			; Command write
		out 064h, al
		call empty_8042_uncond

		mov al,0DFh			; A20 on
		out 060h, al
		call empty_8042_uncond

		; Verify that A20 actually is enabled.  Do that by
		; observing a word in low memory and the same word in
		; the HMA until they are no longer coherent.  Note that
		; we don't do the same check in the disable case, because
		; we don't want to *require* A20 masking (SYSLINUX should
		; work fine without it, if the BIOS does.)
.kbc_wait:	push cx
		xor cx,cx
.kbc_wait_loop:
		call a20_test
		jnz a20_done_pop
		loop .kbc_wait_loop

		pop cx
;
; Running out of options here.  Final attempt: enable the "fast A20 gate"
;
a20_fast:
		mov byte [cs:A20Type], A20_FAST	; Haven't used the KBC yet
		in al, 092h
		or al,02h
		and al,~01h			; Don't accidentally reset the machine!
		out 092h, al

.fast_wait:	push cx
		xor cx,cx
.fast_wait_loop:
		call a20_test
		jnz a20_done_pop
		loop .fast_wait_loop

		pop cx

;
; Oh bugger.  A20 is not responding.  Try frobbing it again; eventually give up
; and report failure to the user.
;


		dec byte [cs:A20Tries]
		jnz try_enable_a20

		mov si, err_a20
		jmp abort_load
;
; A20 unmasked, proceed...
;
a20_done_pop:	pop cx
a20_done:	popad
		ret

;
; This routine tests if A20 is enabled (ZF = 0).  This routine
; must not destroy any register contents.
;
a20_test:
		push es
		push cx
		push ax
		mov cx,0FFFFh		; HMA = segment 0FFFFh
		mov es,cx
		mov cx,32		; Loop count
		mov ax,[cs:A20Test]
.a20_wait:	inc ax
		mov [cs:A20Test],ax
		io_delay		; Serialize, and fix delay
		cmp ax,[es:A20Test+10h]
		loopz .a20_wait
.a20_done:	pop ax
		pop cx
		pop es
		ret

disable_a20:
		pushad
;
; Flush the caches
;
;		call try_wbinvd

		mov bp,[cs:A20Type]
		add bp,bp			; Convert to word offset
		jmp word [cs:bp+A20DList]

a20d_bios:
		mov ax,2400h
		pushf				; Some BIOSes muck with IF
		int 15h
		popf
		jmp short a20d_snooze

;
; Disable the "fast A20 gate"
;
a20d_fast:
		in al, 092h
		and al,~03h
		out 092h, al
		jmp short a20d_snooze

;
; Disable the keyboard controller A20 gate
;
a20d_kbc:
		call empty_8042_uncond
		mov al,0D1h
		out 064h, al		; Command write
		call empty_8042_uncond
		mov al,0DDh		; A20 off
		out 060h, al
		call empty_8042_uncond
		; Wait a bit for it to take effect
a20d_snooze:
		push cx
		mov cx, disable_wait
.delayloop:	call a20_test
		jz .disabled
		loop .delayloop
.disabled:	pop cx
a20d_dunno:
a20d_none:
		popad
		ret

;
; Routine to empty the 8042 KBC controller.  If dl != 0
; then we will test A20 in the loop and exit if A20 is
; suddenly enabled.
;
empty_8042_uncond:
		xor dl,dl
empty_8042:
		call a20_test
		jz .a20_on
		and dl,dl
		jnz .done
.a20_on:	io_delay
		in al, 064h		; Status port
		test al,1
		jz .no_output
		io_delay
		in al, 060h		; Read input
		jmp short empty_8042
.no_output:
		test al,2
		jnz empty_8042
		io_delay
.done:		ret	

;
; WBINVD instruction; gets auto-eliminated on 386 CPUs
;
try_wbinvd:
		wbinvd
		ret

;
; Load RAM disk into high memory
;
; Need to be set:
;	su_ramdiskat	- Where in memory to load
;	su_ramdisklen	- Size of file
;	SI		- initrd filehandle/cluster pointer
;
loadinitrd:
                push es                         ; Save ES on entry
		mov ax,real_mode_seg
                mov es,ax
                mov edi,[es:su_ramdiskat]	; initrd load address
		push si
                mov si,InitRDCName		; Write ramdisk name
                call cwritestr
                mov si,dotdot_msg		; Write dots
                call cwritestr
		pop si

		mov eax,[es:su_ramdisklen]
		call load_high			; Load the file

		call crlf
                mov si,loading_msg		; Write new "Loading " for
                call cwritestr                  ; the benefit of the kernel
                pop es                          ; Restore original ES
                ret

;
; load_high:	loads (the remainder of) a file into high memory.
;		This routine prints dots for each 64K transferred, and
;		calls abort_check periodically.
; 
;		The xfer_buf_seg is used as a bounce buffer.
;
;		The input address (EDI) should be dword aligned, and the final
;		dword written is padded with zeroes if necessary.
;
; Inputs:	SI  = file handle/cluster pointer
;		EDI = target address in high memory
;		EAX = size of remaining file in bytes
;
; Outputs:	SI  = file handle/cluster pointer
;		EDI = first untouched address (not including padding)
;
load_high:
		push es

		mov bx,xfer_buf_seg
		mov es,bx

.read_loop:
		push si
		mov si,dot_msg
		call cwritestr
		pop si
		call abort_check

		push eax			; Total chunk to transfer
		cmp eax,(1 << 16)		; Max 64K in one transfer
		jna .size_ok
		mov eax,(1 << 16)
.size_ok:
		cdq				; EDX <- 0
		push eax			; Bytes transferred this chunk
		div dword [ClustSize]		; Convert to clusters
		; Round up...
		add edx,byte -1			; Sets CF if EDX >= 1
		adc eax,byte 0			; Add 1 to EAX if CF set

		; Now (e)ax contains the number of clusters to get
		push edi
		mov cx,ax
		xor bx,bx			; ES:0
		call getfssec			; Load the data into xfer_buf_seg
		pop edi
		pop ecx				; Byte count this round
		push ecx
		push edi
.fix_slop:
		test cl,3
		jz .noslop
		; The last dword fractional - pad with zeroes
		; Zero-padding is critical for multi-file initramfs.
		mov bx,cx
		mov byte [es:bx],0
		inc ecx
		jmp short .fix_slop
.noslop:
		shr ecx,2			; Convert to dwords
		push esi
		mov esi,(xfer_buf_seg << 4)	; Source address
		call bcopy			; Copy to high memory
		pop edi
		pop esi
		pop ecx
		pop eax
		add edi,ecx
		sub eax,ecx
		jnz .read_loop			; More to read...
		
		pop es
		ret

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
lf_ret:		ret

;
; loadfont:	Load a .psf font file and install it onto the VGA console
;		(if we're not on a VGA screen then ignore.)  It is called with
;		SI and DX:AX set by routine searchdir
;
loadfont:
		mov bx,trackbuf			; The trackbuf is >= 16K; the part
		mov cx,[BufSafe]		; of a PSF file we care about is no
		call getfssec			; more than 8K+4 bytes

		mov ax,[trackbuf]		; Magic number
		cmp ax,0436h
		jne lf_ret

		mov al,[trackbuf+2]		; File mode
		cmp al,5			; Font modes 0-5 supported
		ja lf_ret

		mov bh,byte [trackbuf+3]	; Height of font
		cmp bh,2			; VGA minimum
		jb lf_ret
		cmp bh,32			; VGA maximum
		ja lf_ret

		; Copy to font buffer
		mov si,trackbuf+4		; Start of font data
		mov [VGAFontSize],bh
		mov di,vgafontbuf
		mov cx,(32*256) >> 2		; Maximum size
		rep movsd

		mov [UserFont], byte 1		; Set font flag

		; Fall through to use_font

;
; use_font:
; 	This routine activates whatever font happens to be in the
;	vgafontbuf, and updates the adjust_screen data.
;       Must be called with CS = DS = ES
;
use_font:
		test [UserFont], byte 1		; Are we using a user-specified font?
		jz adjust_screen		; If not, just do the normal stuff

		mov bp,vgafontbuf
		mov bh,[VGAFontSize]

		xor bl,bl			; Needed by both INT 10h calls
		cmp [UsingVGA], byte 1		; Are we in graphics mode?
		jne .text

.graphics:
		xor cx,cx
		mov cl,bh			; CX = bytes/character
		mov ax,480
		div cl				; Compute char rows per screen
		mov dl,al
		dec al
		mov [VidRows],al
		mov ax,1121h			; Set user character table
		int 10h
		mov [VidCols], byte 79		; Always 80 bytes/line
		mov [TextPage], byte 0		; Always page 0
		ret	; No need to call adjust_screen

.text:
		mov cx,256
		xor dx,dx
		mov ax,1110h
		int 10h				; Load into VGA RAM

		xor bl,bl
		mov ax,1103h			; Select page 0
		int 10h

		; Fall through to adjust_screen

;
; adjust_screen: Set the internal variables associated with the screen size.
;		This is a subroutine in case we're loading a custom font.
;
adjust_screen:
                mov al,[BIOS_vidrows]
                and al,al
                jnz vidrows_is_ok
                mov al,24                       ; No vidrows in BIOS, assume 25
						; (Remember: vidrows == rows-1)
vidrows_is_ok:  mov [VidRows],al
                mov ah,0fh
                int 10h                         ; Read video state
                mov [TextPage],bh
                dec ah                          ; Store count-1 (same as rows)
                mov [VidCols],ah
		ret

;
; loadkeys:	Load a LILO-style keymap; SI and DX:AX set by searchdir
;
loadkeys:
		and dx,dx			; Should be 256 bytes exactly
		jne loadkeys_ret
		cmp ax,256
		jne loadkeys_ret

		mov bx,trackbuf
		mov cx,1			; 1 cluster should be >= 256 bytes
		call getfssec

		mov si,trackbuf
		mov di,KbdMap
		mov cx,256 >> 2
		rep movsd

loadkeys_ret:	ret
		
;
; get_msg_file: Load a text file and write its contents to the screen,
;               interpreting color codes.  Is called with SI and DX:AX
;               set by routine searchdir
;
get_msg_file:
		push es
		shl edx,16			; EDX <- DX:AX (length of file)
		mov dx,ax
		mov ax,xfer_buf_seg		; Use for temporary storage
		mov es,ax

                mov byte [TextAttribute],07h	; Default grey on white
		mov byte [DisplayMask],07h	; Display text in all modes
		call msg_initvars

get_msg_chunk:  push edx			; EDX = length of file
		xor bx,bx			; == xbs_textbuf
		mov cx,[BufSafe]
		call getfssec
		pop edx
		push si				; Save current cluster
		xor si,si			; == xbs_textbuf
		mov cx,[BufSafeBytes]		; Number of bytes left in chunk
print_msg_file:
		push cx
		push edx
		es lodsb
                cmp al,1Ah                      ; DOS EOF?
		je msg_done_pop
		push si
		mov cl,[UsingVGA]
		inc cl				; 01h = text mode, 02h = graphics
                call [NextCharJump]		; Do what shall be done
		pop si
		pop edx
                pop cx
		dec edx
		jz msg_done
		loop print_msg_file
		pop si
		jmp short get_msg_chunk
msg_done_pop:
                add sp,byte 6			; Drop pushed EDX, CX
msg_done:
		pop si
		pop es
		ret
msg_putchar:                                    ; Normal character
                cmp al,0Fh                      ; ^O = color code follows
                je msg_ctrl_o
                cmp al,0Dh                      ; Ignore <CR>
                je msg_ignore
                cmp al,0Ah                      ; <LF> = newline
                je msg_newline
                cmp al,0Ch                      ; <FF> = clear screen
                je msg_formfeed
		cmp al,19h			; <EM> = return to text mode
		je near msg_novga
		cmp al,18h			; <CAN> = VGA filename follows
		je near msg_vga
		jnb .not_modectl
		cmp al,10h			; 10h to 17h are mode controls
		jae near msg_modectl
.not_modectl:

msg_normal:	call write_serial_displaymask	; Write to serial port
		test [DisplayMask],cl
		jz msg_ignore			; Not screen
                mov bx,[TextAttrBX]
                mov ah,09h                      ; Write character/attribute
                mov cx,1                        ; One character only
                int 10h                         ; Write to screen
                mov al,[CursorCol]
                inc ax
                cmp al,[VidCols]
                ja msg_line_wrap		; Screen wraparound
                mov [CursorCol],al

msg_gotoxy:     mov bh,[TextPage]
                mov dx,[CursorDX]
                mov ah,02h                      ; Set cursor position
                int 10h
msg_ignore:     ret
msg_ctrl_o:                                     ; ^O = color code follows
                mov word [NextCharJump],msg_setbg
                ret
msg_newline:                                    ; Newline char or end of line
		mov si,crlf_msg
		call write_serial_str_displaymask
msg_line_wrap:					; Screen wraparound
		test [DisplayMask],cl
		jz msg_ignore
                mov byte [CursorCol],0
                mov al,[CursorRow]
                inc ax
                cmp al,[VidRows]
                ja msg_scroll
                mov [CursorRow],al
                jmp short msg_gotoxy
msg_scroll:     xor cx,cx                       ; Upper left hand corner
                mov dx,[ScreenSize]
                mov [CursorRow],dh		; New cursor at the bottom
                mov bh,[ScrollAttribute]
                mov ax,0601h                    ; Scroll up one line
                int 10h
                jmp short msg_gotoxy
msg_formfeed:                                   ; Form feed character
		mov si,crff_msg
		call write_serial_str_displaymask
		test [DisplayMask],cl
		jz msg_ignore
                xor cx,cx
                mov [CursorDX],cx		; Upper lefthand corner
                mov dx,[ScreenSize]
                mov bh,[TextAttribute]
                mov ax,0600h                    ; Clear screen region
                int 10h
                jmp short msg_gotoxy
msg_setbg:                                      ; Color background character
                call unhexchar
                jc msg_color_bad
                shl al,4
		test [DisplayMask],cl
		jz .dontset
                mov [TextAttribute],al
.dontset:
                mov word [NextCharJump],msg_setfg
                ret
msg_setfg:                                      ; Color foreground character
                call unhexchar
                jc msg_color_bad
		test [DisplayMask],cl
		jz .dontset
                or [TextAttribute],al		; setbg set foreground to 0
.dontset:
		jmp short msg_putcharnext
msg_vga:
		mov word [NextCharJump],msg_filename
		mov di, VGAFileBuf
		jmp short msg_setvgafileptr

msg_color_bad:
                mov byte [TextAttribute],07h	; Default attribute
msg_putcharnext:
                mov word [NextCharJump],msg_putchar
		ret

msg_filename:					; Getting VGA filename
		cmp al,0Ah			; <LF> = end of filename
		je msg_viewimage
		cmp al,' '
		jbe msg_ret			; Ignore space/control char
		mov di,[VGAFilePtr]
		cmp di,VGAFileBufEnd
		jnb msg_ret
		mov [di],al			; Can't use stosb (DS:)
		inc di
msg_setvgafileptr:
		mov [VGAFilePtr],di
msg_ret:	ret

msg_novga:
		call vgaclearmode
		jmp short msg_initvars

msg_viewimage:
		push es
		push ds
		pop es				; ES <- DS
		mov si,VGAFileBuf
		mov di,VGAFileMBuf
		push di
		call mangle_name
		pop di
		call searchdir
		pop es
		jz msg_putcharnext		; Not there
		call vgadisplayfile
		; Fall through

		; Subroutine to initialize variables, also needed
		; after loading a graphics file
msg_initvars:
                pusha
                mov bh,[TextPage]
                mov ah,03h                      ; Read cursor position
                int 10h
                mov [CursorDX],dx
                popa
		jmp short msg_putcharnext	; Initialize state machine

msg_modectl:
		and al,07h
		mov [DisplayMask],al
		jmp short msg_putcharnext

;
; write_serial:	If serial output is enabled, write character on serial port
; write_serial_displaymask: d:o, but ignore if DisplayMask & 04h == 0
;
write_serial_displaymask:
		test byte [DisplayMask], 04h
		jz write_serial.end
write_serial:
		pushfd
		pushad
		mov bx,[SerialPort]
		and bx,bx
		je .noserial
		push ax
		mov ah,[FlowInput]
.waitspace:
		; Wait for space in transmit register
		lea dx,[bx+5]			; DX -> LSR
		in al,dx
		test al,20h
		jz .waitspace

		; Wait for input flow control
		inc dx				; DX -> MSR
		in al,dx
		and al,ah
		cmp al,ah
		jne .waitspace	
.no_flow:		

		xchg dx,bx			; DX -> THR
		pop ax
		call slow_out			; Send data
.noserial:	popad
		popfd
.end:		ret

;
; write_serial_str: write_serial for strings
; write_serial_str_displaymask: d:o, but ignore if DisplayMask & 04h == 0
;
write_serial_str_displaymask:
		test byte [DisplayMask], 04h
		jz write_serial_str.end

write_serial_str:
.loop		lodsb
		and al,al
		jz .end
		call write_serial
		jmp short .loop
.end:		ret

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
; crlf: Print a newline
;
crlf:		mov si,crlf_msg
		; Fall through

;
; cwritestr: write a null-terminated string to the console, saving
;            registers on entry.
;
cwritestr:
		pushfd
                pushad
.top:		lodsb
		and al,al
                jz .end
		call writechr
                jmp short .top
.end:		popad
		popfd
                ret

%ifdef debug

;
; writehex[248]: Write a hex number in (AL, AX, EAX) to the console
;
writehex2:
		pushfd
		pushad
		rol eax,24
		mov cx,2
		jmp short writehex_common
writehex4:
		pushfd
		pushad
		rol eax,16
		mov cx,4
		jmp short writehex_common
writehex8:
		pushfd
		pushad
		mov cx,8
writehex_common:
.loop:		rol eax,4
		push eax
		and al,0Fh
		cmp al,10
		jae .high
.low:		add al,'0'
		jmp short .ischar
.high:		add al,'A'-10
.ischar:	call writechr
		pop eax
		loop .loop
		popad
		popfd
		ret

;
; crlf: write CR LF
;
crlf:		push ax
		mov al, 13
		call writechr
		mov al, 10
		call writechr
		pop ax
		ret

%endif

;
; pollchar: check if we have an input character pending (ZF = 0)
;
pollchar:
		pushad
		mov ah,1		; Poll keyboard
		int 16h
		jnz .done		; Keyboard response
		mov dx,[SerialPort]
		and dx,dx
		jz .done		; No serial port -> no input
		add dx,byte 5		; DX -> LSR
		in al,dx
		test al,1		; ZF = 0 if data pending
		jz .done
		inc dx			; DX -> MSR
		mov ah,[FlowIgnore]	; Required status bits
		in al,dx
		and al,ah
		cmp al,ah
		setne al
		dec al			; Set ZF = 0 if equal
.done:		popad
		ret

;
; getchar: Read a character from keyboard or serial port
;
getchar:
.again:		mov ah,1		; Poll keyboard
		int 16h
		jnz .kbd		; Keyboard input?
		mov bx,[SerialPort]
		and bx,bx
		jz .again
		lea dx,[bx+5]		; DX -> LSR
		in al,dx
		test al,1
		jz .again
		inc dx			; DX -> MSR
		mov ah,[FlowIgnore]
		in al,dx
		and al,ah
		cmp al,ah
		jne .again
.serial:	xor ah,ah		; Avoid confusion
		xchg dx,bx		; Data port
		in al,dx
		ret
.kbd:		xor ax,ax		; Get keyboard input
		int 16h
		and al,al
		jz .func_key
		mov bx,KbdMap		; Convert character sets
		xlatb
.func_key:	ret

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
; open,getc:	Load a file a character at a time for parsing in a manner
;		similar to the C library getc routine.	Only one simultaneous
;		use is supported.  Note: "open" trashes the trackbuf.
;
;		open:	Input:	mangled filename in DS:DI
;			Output: ZF set on file not found or zero length
;
;		getc:	Output: CF set on end of file
;				Character loaded in AL
;
open:
		call searchdir
		jz open_return
		pushf
		mov [FBytes1],ax
		mov [FBytes2],dx
		add ax,[ClustSize]
		adc dx,byte 0
		sub ax,byte 1
		sbb dx,byte 0
		div word [ClustSize]
		mov [FClust],ax		; Number of clusters
		mov [FNextClust],si	; Cluster pointer
		mov ax,[EndOfGetCBuf]	; Pointer at end of buffer ->
		mov [FPtr],ax		;  nothing loaded yet
		popf			; Restore no ZF
open_return:	ret

;
getc:
		stc			; If we exit here -> EOF
		mov ecx,[FBytes]
		jecxz getc_ret
		mov si,[FPtr]
		cmp si,[EndOfGetCBuf]
		jb getc_loaded
		; Buffer empty -- load another set
		mov cx,[FClust]
		cmp cx,[BufSafe]
		jna getc_oksize
		mov cx,[BufSafe]
getc_oksize:	sub [FClust],cx		; Reduce remaining clusters
		mov si,[FNextClust]
		mov bx,getcbuf
		push bx
		push es			; ES may be != DS, save old ES
		push ds			; Trackbuf is in DS, not ES
		pop es
		call getfssec		; Load a trackbuf full of data
		mov [FNextClust],si	; Store new next pointer
		pop es			; Restore ES
		pop si			; SI -> newly loaded data
getc_loaded:	lodsb			; Load a byte
		mov [FPtr],si		; Update next byte pointer
		dec dword [FBytes]	; Update bytes left counter (CF = 1)
		clc			; Not EOF
getc_ret:	ret

;
; ungetc:	Push a character (in AL) back into the getc buffer
;		Note: if more than one byte is pushed back, this may cause
;		bytes to be written below the getc buffer boundary.  If there
;		is a risk for this to occur, the getcbuf base address should
;		be moved up.
;
ungetc:
		mov si,[FPtr]
		dec si
		mov [si],al
		mov [FPtr],si
		inc dword [FBytes]
		ret

;
; skipspace:	Skip leading whitespace using "getc".  If we hit end-of-line
;		or end-of-file, return with carry set; ZF = true of EOF
;		ZF = false for EOLN; otherwise CF = ZF = 0.
;
;		Otherwise AL = first character after whitespace
;
skipspace:
skipspace_loop: call getc
		jc skipspace_eof
		cmp al,1Ah			; DOS EOF
		je skipspace_eof
		cmp al,0Ah
		je skipspace_eoln
		cmp al,' '
		jbe skipspace_loop
		ret				; CF = ZF = 0
skipspace_eof:	cmp al,al			; Set ZF
		stc				; Set CF
		ret
skipspace_eoln: add al,0FFh			; Set CF, clear ZF
		ret

;
; getkeyword:	Get a keyword from the current "getc" file; only the two
;		first characters are considered significant.
;
;		Lines beginning with ASCII characters 33-47 are treated
;		as comments and ignored; other lines are checked for
;		validity by scanning through the keywd_table.
;
;		The keyword and subsequent whitespace is skipped.
;
;		On EOF, CF = 1; otherwise, CF = 0, AL:AH = lowercase char pair
;
getkeyword:
gkw_find:	call skipspace
		jz gkw_eof		; end of file
		jc gkw_find		; end of line: try again
		cmp al,'0'
		jb gkw_skipline		; skip comment line
		push ax
		call getc
		pop bx
		jc gkw_eof
		mov bh,al		; Move character pair into BL:BH
		or bx,2020h		; Lower-case it
		mov si,keywd_table
gkw_check:	lodsw
		and ax,ax
		jz gkw_badline		; Bad keyword, write message
		cmp ax,bx
		jne gkw_check
		push ax
gkw_skiprest:
		call getc
		jc gkw_eof_pop
		cmp al,'0'
		ja gkw_skiprest
		call ungetc
		call skipspace
		jz gkw_eof_pop
                jc gkw_missingpar       ; Missing parameter after keyword
		call ungetc		; Return character to buffer
		clc			; Successful return
gkw_eof_pop:	pop ax
gkw_eof:	ret			; CF = 1 on all EOF conditions
gkw_missingpar: pop ax
                mov si,err_noparm
                call cwritestr
                jmp gkw_find
gkw_badline_pop: pop ax
gkw_badline:	mov si,err_badcfg
		call cwritestr
		jmp short gkw_find
gkw_skipline:	cmp al,10		; Scan for LF
		je gkw_find
		call getc
		jc gkw_eof
		jmp short gkw_skipline

;
; getint:	Load an integer from the getc file.
;		Return CF if error; otherwise return integer in EBX
;
getint:
		mov di,NumBuf
gi_getnum:	cmp di,NumBufEnd	; Last byte in NumBuf
		jae gi_loaded
		push di
		call getc
		pop di
		jc gi_loaded
		stosb
		cmp al,'-'
		jnb gi_getnum
		call ungetc		; Unget non-numeric
gi_loaded:	mov byte [di],0
		mov si,NumBuf
		; Fall through to parseint

;
; parseint:	Convert an integer to a number in EBX
;		Get characters from string in DS:SI
;		Return CF on error
;		DS:SI points to first character after number
;
;               Syntaxes accepted: [-]dec, [-]0+oct, [-]0x+hex, val+K, val+M
;
parseint:
                push eax
                push ecx
		push bp
		xor eax,eax		; Current digit (keep eax == al)
		mov ebx,eax		; Accumulator
		mov ecx,ebx		; Base
                xor bp,bp               ; Used for negative flag
pi_begin:	lodsb
		cmp al,'-'
		jne pi_not_minus
		xor bp,1		; Set unary minus flag
		jmp short pi_begin
pi_not_minus:
		cmp al,'0'
		jb pi_err
		je pi_octhex
		cmp al,'9'
		ja pi_err
		mov cl,10		; Base = decimal
		jmp short pi_foundbase
pi_octhex:
		lodsb
		cmp al,'0'
		jb pi_km		; Value is zero
		or al,20h		; Downcase
		cmp al,'x'
		je pi_ishex
		cmp al,'7'
		ja pi_err
		mov cl,8		; Base = octal
		jmp short pi_foundbase
pi_ishex:
		mov al,'0'		; No numeric value accrued yet
		mov cl,16		; Base = hex
pi_foundbase:
                call unhexchar
                jc pi_km                ; Not a (hex) digit
                cmp al,cl
		jae pi_km		; Invalid for base
		imul ebx,ecx		; Multiply accumulated by base
                add ebx,eax             ; Add current digit
		lodsb
		jmp short pi_foundbase
pi_km:
		dec si			; Back up to last non-numeric
		lodsb
		or al,20h
		cmp al,'k'
		je pi_isk
		cmp al,'m'
		je pi_ism
		dec si			; Back up
pi_fini:	and bp,bp
		jz pi_ret		; CF=0!
		neg ebx			; Value was negative
pi_done:	clc
pi_ret:		pop bp
                pop ecx
                pop eax
		ret
pi_err:		stc
		jmp short pi_ret
pi_isk:		shl ebx,10		; x 2^10
		jmp short pi_done
pi_ism:		shl ebx,20		; x 2^20
		jmp short pi_done

;
; unhexchar:    Convert a hexadecimal digit in AL to the equivalent number;
;               return CF=1 if not a hex digit
;
unhexchar:
                cmp al,'0'
		jb uxc_ret		; If failure, CF == 1 already
                cmp al,'9'
                ja uxc_1
		sub al,'0'		; CF <- 0
		ret
uxc_1:          or al,20h		; upper case -> lower case
		cmp al,'a'
                jb uxc_ret		; If failure, CF == 1 already
                cmp al,'f'
                ja uxc_err
                sub al,'a'-10           ; CF <- 0
                ret
uxc_err:        stc
uxc_ret:	ret

;
;
; getline:	Get a command line, converting control characters to spaces
;               and collapsing streches to one; a space is appended to the
;               end of the string, unless the line is empty.
;		The line is terminated by ^J, ^Z or EOF and is written
;		to ES:DI.  On return, DI points to first char after string.
;		CF is set if we hit EOF.
;
getline:
		call skipspace
                mov dl,1                ; Empty line -> empty string.
                jz gl_eof               ; eof
                jc gl_eoln              ; eoln
		call ungetc
gl_fillloop:	push dx
		push di
		call getc
		pop di
		pop dx
		jc gl_ret		; CF set!
		cmp al,' '
		jna gl_ctrl
		xor dx,dx
gl_store:	stosb
		jmp short gl_fillloop
gl_ctrl:	cmp al,10
		je gl_ret		; CF clear!
		cmp al,26
		je gl_eof
		and dl,dl
		jnz gl_fillloop		; Ignore multiple spaces
		mov al,' '		; Ctrl -> space
		inc dx
		jmp short gl_store
gl_eoln:        clc                     ; End of line is not end of file
                jmp short gl_ret
gl_eof:         stc
gl_ret:		pushf			; We want the last char to be space!
		and dl,dl
		jnz gl_xret
		mov al,' '
		stosb
gl_xret:	popf
		ret


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

; ----------------------------------------------------------------------------------
;  VGA splash screen code
; ----------------------------------------------------------------------------------

;
; vgadisplayfile:
;	Display a graphical splash screen.
;
; Input:
;
; SI	= cluster/socket pointer
;
vgadisplayfile:
		mov [VGACluster],si
		push es

		; This is a cheap and easy way to make sure the screen is
		; cleared in case we were in graphics mode already
		call vgaclearmode
		call vgasetmode
		jnz .error_nz

.graphalready:
		mov ax,xfer_buf_seg		; Use as temporary storage
		mov es,ax
		mov fs,ax

		call vgagetchunk		; Get the first chunk

		; The header WILL be in the first chunk.
		cmp dword [es:xbs_vgabuf],0x1413f33d	; Magic number
.error_nz:	jne near .error
		mov ax,[es:xbs_vgabuf+4]
		mov [GraphXSize],ax

		mov dx,xbs_vgabuf+8		; Color map offset
		mov ax,1012h			; Set RGB registers
		xor bx,bx			; First register number
		mov cx,16			; 16 registers
		int 10h
	
.movecursor:
		mov ax,[es:xbs_vgabuf+6]	; Number of pixel rows
		mov dx,[VGAFontSize]
		add ax,dx
		dec ax
		div dl
		xor dx,dx			; Set column to 0
		cmp al,[VidRows]
		jb .rowsok
		mov al,[VidRows]
		dec al
.rowsok:
		mov dh,al
		mov ah,2
		xor bx,bx
		int 10h				; Set cursor below image

		mov cx,[es:xbs_vgabuf+6]	; Number of graphics rows

		mov si,xbs_vgabuf+8+3*16	; Beginning of pixel data
		mov word [VGAPos],0

.drawpixelrow:
		push cx
		mov cx,[GraphXSize]
		mov di,xbs_vgatmpbuf		; Row buffer
		call rledecode			; Decode one row
		push si
		mov si,xbs_vgatmpbuf
		mov di,si
		add di,[GraphXSize]
		mov cx,640/4
		xor eax,eax
		rep stosd			; Clear rest of row
		mov di,0A000h			; VGA segment
		mov es,di
		mov di,[VGAPos]
		mov bp,640
		call packedpixel2vga
		add word [VGAPos],byte 80	; Advance to next pixel row
		push fs
		pop es
		pop si
		pop cx
		loop .drawpixelrow

.error:
		pop es
		ret

;
; rledecode:
;	Decode a pixel row in RLE16 format.
;
; FS:SI	-> input
; CX -> pixel count
; ES:DI -> output (packed pixel)
;
rledecode:
		shl esi,1		; Nybble pointer
		xor dl,dl		; Last pixel
.loop:
		call .getnybble
		cmp al,dl
		je .run			; Start of run sequence
		stosb
		mov dl,al
		dec cx
		jnz .loop
.done:
		shr esi,1
		adc si,byte 0
		ret
.run:
		xor bx,bx
		call .getnybble
		and al,al
		jz .longrun
		mov bl,al
.dorun:
		push cx
		mov cx,bx
		mov al,dl
		rep stosb
		pop cx
		sub cx,bx
		ja .loop
		jmp short .done
.longrun:
		call .getnybble
		mov ah,al
		call .getnybble
		shl al,4
		or al,ah
		mov bl,al
		add bx,16
		jmp short .dorun
.getnybble:
		shr esi,1
		fs lodsb
		jc .high
		dec si
		and al,0Fh
		stc
		rcl esi,1
		ret
.high:
		shr al,4
		cmp si,xbs_vgabuf+trackbufsize	; Chunk overrun
		jb .nonewchunk
		call vgagetchunk
		mov si,xbs_vgabuf		; Start at beginning of buffer
.nonewchunk:
		shl esi,1
		ret

;
; vgagetchunk:
;	Get a new trackbufsize chunk of VGA image data
;
; On input, ES is assumed to point to the buffer segment.
;
vgagetchunk:
		pushad
		mov si,[VGACluster]
		and si,si
		jz .eof				; EOF overrun, not much to do...

		mov cx,[BufSafe]		; One trackbuf worth of data
		mov bx,xbs_vgabuf
		call getfssec

		jnc .noteof
		xor si,si
.noteof:	mov [VGACluster],si

.eof:		popad
		ret

;
; packedpixel2vga:
;	Convert packed-pixel to VGA bitplanes
;
; FS:SI -> packed pixel string
; BP    -> pixel count (multiple of 8)
; ES:DI -> output
;
packedpixel2vga:
		mov dx,3C4h	; VGA Sequencer Register select port
		mov al,2	; Sequencer mask
		out dx,al	; Select the sequencer mask
		inc dx		; VGA Sequencer Register data port
		mov al,1
		mov bl,al
.planeloop:
		pusha
		out dx,al
.loop1:
		mov cx,8
.loop2:
		xchg cx,bx
		fs lodsb
		shr al,cl
		rcl ch,1	; VGA is bigendian.  Sigh.
		xchg cx,bx
		loop .loop2
		mov al,bh
		stosb
		sub bp,byte 8
		ja .loop1
		popa
		inc bl
		shl al,1
		cmp bl,4
		jbe .planeloop
		ret

;
; vgasetmode:
;	Enable VGA graphics, if possible; return ZF=1 on success
;	DS must be set to the base segment; ES is set to DS.
;
vgasetmode:
		push ds
		pop es
		mov ax,1A00h		; Get video card and monitor
		xor bx,bx
		int 10h
		cmp bl, 8		; If not VGA card/VGA monitor, give up
		jne .error		; ZF=0
;		mov bx,TextColorReg
;		mov dx,1009h		; Read color registers
;		int 10h
		mov ax,0012h		; Set mode = 640x480 VGA 16 colors
		int 10h
		mov dx,linear_color
		mov ax,1002h		; Write color registers
		int 10h
		mov [UsingVGA], byte 1

		call use_font		; Set graphics font/data
		mov byte [ScrollAttribute], 00h

		xor ax,ax		; Set ZF
.error:
		ret

;
; vgaclearmode:
;	Disable VGA graphics.  It is not safe to assume any value
;	for DS or ES.
;
vgaclearmode:
		push ds
		push es
		pushad
		mov ax,cs
		mov ds,ax
		mov es,ax
		cmp [UsingVGA], byte 1
		jne .done
		mov ax,0003h		; Return to normal video mode
		int 10h
;		mov dx,TextColorReg	; Restore color registers
;		mov ax,1002h
;		int 10h
		mov [UsingVGA], byte 0

		call use_font		; Restore text font/data
		mov byte [ScrollAttribute], 07h
.done:
		popad
		pop es
		pop ds
		ret

;
; vgashowcursor/vgahidecursor:
;	If VGA graphics is enabled, draw a cursor/clear a cursor
;
vgashowcursor:
		pushad
		mov al,'_'
		jmp short vgacursorcommon
vgahidecursor:
		pushad
		mov al,' '
vgacursorcommon:
		cmp [UsingVGA], byte 1
		jne .done
		mov ah,09h
		mov bx,0007h
		mov cx,1
		int 10h
.done:
		popad
		ret


		; Map colors to consecutive DAC registers
linear_color	db 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0
UsingVGA	db 0

; ----------------------------------------------------------------------------------
;  Begin data section
; ----------------------------------------------------------------------------------

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
err_noram	db 'It appears your computer has less than 512K of low ("DOS")'
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
err_bootsec	db 'Invalid or corrupt boot sector image.', CR, LF, 0
err_a20		db CR, LF, 'A20 gate not responding!', CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: please change disks and press '
		db 'a key to continue.', CR, LF, 0
ready_msg	db ' ready.', CR, LF, 0
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
		align 2
keywd_table	db 'ap' ; append
		db 'de' ; default
		db 'ti' ; timeout
		db 'fo'	; font
		db 'kb' ; kbd
		db 'di' ; display
		db 'pr' ; prompt
		db 'la' ; label
                db 'im' ; implicit
		db 'ke' ; kernel
		db 'se' ; serial
		db 'f1' ; F1
		db 'f2' ; F2
		db 'f3' ; F3
		db 'f4' ; F4
		db 'f5' ; F5
		db 'f6' ; F6
		db 'f7' ; F7
		db 'f8' ; F8
		db 'f9' ; F9
		db 'f0' ; F10
		dw 0
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
FKeyMap		dw 0			; Bitmap for F-keys loaded
CmdLinePtr	dw cmd_line_here	; Command line advancing pointer
initrd_flag	equ $
initrd_ptr	dw 0			; Initial ramdisk pointer/flag
VKernelCtr	dw 0			; Number of registered vkernels
ForcePrompt	dw 0			; Force prompt
AllowImplicit   dw 1                    ; Allow implicit kernels
SerialPort	dw 0			; Serial port base (or 0 for no serial port)
A20List		dw a20_dunno, a20_none, a20_bios, a20_kbc, a20_fast
A20DList	dw a20d_dunno, a20d_none, a20d_bios, a20d_kbc, a20d_fast
A20Type		dw A20_DUNNO		; A20 type unknown
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
%if (getcbuf+trackbufsize) > vgafontbuf
%error "Out of memory, better reorganize something..."
%endif
