; -*- fundamental -*- (asm-mode sucks)
; $Id$
; ****************************************************************************
;
;  isolinux.asm
;
;  A program to boot Linux kernels off a CD-ROM using the El Torito
;  boot standard in "no emulation" mode, making the entire filesystem
;  available.  It is based on the SYSLINUX boot loader for MS-DOS
;  floppies.
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

; Note: The Makefile builds one version with DEBUG_MESSAGES automatically.
; %define DEBUG_TRACERS			; Uncomment to get debugging tracers
; %define DEBUG_MESSAGES		; Uncomment to get debugging messages

%ifdef DEBUG_TRACERS

%macro TRACER	1
	call debug_tracer
	db %1
%endmacro

%else	; DEBUG_TRACERS

%macro	TRACER	1
%endmacro

%endif	; DEBUG_TRACERS

;
; Some semi-configurable constants... change on your own risk.  Most are imposed
; by the kernel.
;
max_cmd_len	equ 255			; Must be odd; 255 is the kernel limit
FILENAME_MAX_LG2 equ 8			; log2(Max filename size Including final null)
FILENAME_MAX	equ (1 << FILENAME_MAX_LG2)
HIGHMEM_MAX	equ 037FFFFFFh		; DEFAULT highest address for an initrd
HIGHMEM_SLOP	equ 128*1024		; Avoid this much memory near the top
DEFAULT_BAUD	equ 9600		; Default baud rate for serial port
BAUD_DIVISOR	equ 115200		; Serial port parameter
MAX_OPEN_LG2	equ 6			; log2(Max number of open files)
MAX_OPEN	equ (1 << MAX_OPEN_LG2)
SECTORSIZE_LG2	equ 11			; 2048 bytes/sector (El Torito requirement)
SECTORSIZE	equ (1 << SECTORSIZE_LG2)

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
syslinux_id	equ 033h		; SYSLINUX (3) 3 = ISOLINUX

;
; Segments used by Linux
;
real_mode_seg	equ 5000h
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
		resb (9000h-12)-($-$$)	; Were bootsect.S puts it...
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
%define vk_power	6		; log2(max number of vkernels)
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

%if (vk_end > vk_size) || (vk_size*max_vk > 65536)
%error "Too many vkernels defined, reduce vk_power"
%endif

;
; Segment assignments in the bottom 640K
; 0000h - main code/data segment (and BIOS segment)
; 5000h - real_mode_seg
;
vk_seg          equ 4000h		; Virtual kernels
xfer_buf_seg	equ 3000h		; Bounce buffer for I/O to high mem
comboot_seg	equ 2000h		; COMBOOT image loading zone

;
; File structure.  This holds the information for each currently open file.
;
		struc open_file_t
file_sector	resd 1			; Sector pointer (0 = structure free)
file_left	resd 1			; Number of sectors left
		endstruc

%if (open_file_t_size & (open_file_t_size-1))
%error "open_file_t is not a power of 2"
%endif

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

		absolute 0400h
serial_base	resw 4			; Base addresses for 4 serial ports
		absolute 0413h
BIOS_fbm	resw 1			; Free Base Memory (kilobytes)
		absolute 046Ch
BIOS_timer	resw 1			; Timer ticks
		absolute 0472h
BIOS_magic	resw 1			; BIOS reset magic
                absolute 0484h
BIOS_vidrows    resb 1			; Number of screen rows

;
; Memory below this point is reserved for the BIOS and the MBR
;
 		absolute 1000h
trackbuf	resb 8192		; Track buffer goes here
trackbufsize	equ $-trackbuf
;		trackbuf ends at 3000h


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

		struc dir_t
dir_lba		resd 1			; Directory start (LBA)
dir_len		resd 1			; Length in bytes
dir_clust	resd 1			; Length in clusters
		endstruc

                absolute 5000h          ; Here we keep our BSS stuff
VKernelBuf:	resb vk_size		; "Current" vkernel
		alignb 4
AppendBuf       resb max_cmd_len+1	; append=
KbdMap		resb 256		; Keyboard map
FKeyName	resb 10*FILENAME_MAX	; File names for F-key help
NumBuf		resb 15			; Buffer to load number
NumBufEnd	resb 1			; Last byte in NumBuf
ISOFileName	resb 64			; ISO filename canonicalization buffer
ISOFileNameEnd	equ $
		alignb 32
KernelName      resb FILENAME_MAX       ; Mangled name for kernel
KernelCName     resb FILENAME_MAX	; Unmangled kernel name
InitRDCName     resb FILENAME_MAX       ; Unmangled initrd name
MNameBuf	resb FILENAME_MAX
InitRD		resb FILENAME_MAX
PartInfo	resb 16			; Partition table entry
E820Buf		resd 5			; INT 15:E820 data buffer
HiLoadAddr      resd 1			; Address pointer for high load loop
HighMemSize	resd 1			; End of memory pointer (bytes)
RamdiskMax	resd 1			; Highest address for a ramdisk
KernelSize	resd 1			; Size of kernel (bytes)
RootDir		resb dir_t_size		; Root directory
CurDir		resb dir_t_size		; Current directory
SavedSSSP	resd 1			; Our SS:SP while running a COMBOOT image
KernelClust	resd 1			; Kernel size in clusters
InitStack	resd 1			; Initial stack pointer (SS:SP)
FirstSecSum	resd 1			; Checksum of bytes 64-2048
ImageDwords	resd 1			; isolinux.bin size, dwords
FBytes		equ $			; Used by open/getc
FBytes1		resw 1
FBytes2		resw 1
FClust		resw 1			; Number of clusters in open/getc file
FNextClust	resw 1			; Pointer to next cluster in d:o
FPtr		resw 1			; Pointer to next char in buffer
CmdOptPtr       resw 1			; Pointer to first option on cmd line
KernelCNameLen  resw 1			; Length of unmangled kernel name
InitRDCNameLen  resw 1			; Length of unmangled initrd name
NextCharJump    resw 1			; Routine to interpret next print char
SetupSecs	resw 1			; Number of setup sectors
A20Test		resw 1			; Counter for testing status of A20
CmdLineLen	resw 1			; Length of command line including null
GraphXSize	resw 1			; Width of splash screen file
VGAPos		resw 1			; Pointer into VGA memory
VGACluster	resw 1			; Cluster pointer for VGA image file
VGAFilePtr	resw 1			; Pointer into VGAFileBuf
ConfigFile	resw 1			; Socket for config file
PktTimeout	resw 1			; Timeout for current packet
KernelExtPtr	resw 1			; During search, final null pointer
LocalBootType	resw 1			; Local boot return code
ImageSectors	resw 1			; isolinux.bin size, sectors
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
FuncFlag	resb 1			; == 1 if <Ctrl-F> pressed
DisplayMask	resb 1			; Display modes mask
ISOFlags	resb 1			; Flags for ISO directory search
DiskError	resb 1			; Error code for disk I/O
DriveNo		resb 1			; CD-ROM BIOS drive number
TextColorReg	resb 17			; VGA color registers for text mode
VGAFileBuf	resb FILENAME_MAX	; Unmangled VGA image name
VGAFileBufEnd	equ $
VGAFileMBuf	resb FILENAME_MAX	; Mangled VGA image name

		alignb open_file_t_size
Files		resb MAX_OPEN*open_file_t_size

		section .text
                org 7C00h
;;
;; Primary entry point.  Because BIOSes are buggy, we only load the first
;; CD-ROM sector (2K) of the file, so the number one priority is actually
;; loading the rest.
;;
bootsec		equ $
_start:		; Far jump makes sure we canonicalize the address
		cli
		jmp 0:_start1
		times 8-($-$$) nop		; Pad to file offset 8

		; This table gets filled in by mkisofs using the
		; -boot-info-table option
bi_pvd:		dd 0xdeadbeef			; LBA of primary volume descriptor
bi_file:	dd 0xdeadbeef			; LBA of boot file
bi_length:	dd 0xdeadbeef			; Length of boot file
bi_csum:	dd 0xdeadbeef			; Checksum of boot file
bi_reserved:	times 10 dd 0xdeadbeef		; Reserved

_start1:	mov [cs:InitStack],sp		; Save initial stack pointer
		mov ax,ss
		mov [cs:InitStack+2],ax
		xor ax,ax
		mov ss,ax
		mov sp,_start			; Set up stack
		mov ds,ax
		mov es,ax
		mov fs,ax
		mov gs,ax
		sti

		cld
		; Show signs of life
		mov si,isolinux_banner
		call writestr
%ifdef DEBUG_MESSAGES
		mov si,copyright_str
		call writestr
%endif

		;
		; Before modifying any memory, get the checksum of bytes
		; 64-2048
		;
initial_csum:	xor edi,edi
		mov si,_start1
		mov cx,(SECTORSIZE-64) >> 2
.loop:		lodsd
		add edi,eax
		loop .loop
		mov [FirstSecSum],edi

		; Set up boot file sizes
		mov eax,[bi_length]
		sub eax,SECTORSIZE-3
		shr eax,2			; bytes->dwords
		mov [ImageDwords],eax		; boot file dwords
		add eax,(2047 >> 2)
		shr eax,9			; dwords->sectors
		mov [ImageSectors],ax		; boot file sectors

		mov [DriveNo],dl
%ifdef DEBUG_MESSAGES
		mov si,startup_msg
		call writemsg
		mov al,dl
		call writehex2
		call crlf
%endif

		; Now figure out what we're actually doing
		; Note: use passed-in DL value rather than 7Fh because
		; at least some BIOSes will get the wrong value otherwise
		mov ax,4B01h			; Get disk emulation status
		mov dl,[DriveNo]
		mov si,spec_packet
		int 13h
		jc near spec_query_failed	; Shouldn't happen (BIOS bug)
		mov dl,[DriveNo]
		cmp [sp_drive],dl		; Should contain the drive number
		jne near spec_query_failed

%ifdef DEBUG_MESSAGES
		mov si,spec_ok_msg
		call writemsg
		mov al,byte [sp_drive]
		call writehex2
		call crlf
%endif

found_drive:
		; Get drive information
		mov ah,48h
		mov dl,[DriveNo]
		mov si,drive_params
		int 13h
		jnc params_ok

		mov si,nosecsize_msg
		call writemsg

params_ok:
		; Check for the sector size (should be 2048, but
		; some BIOSes apparently think we're 512-byte media)
		;
		; FIX: We need to check what the proper behaviour
		; is for getlinsec when the BIOS thinks the sector
		; size is 512!!!  For that, we need such a BIOS, though...
%ifdef DEBUG_MESSAGES
		mov si,secsize_msg
		call writemsg
		mov ax,[dp_secsize]
		call writehex4
		call crlf
%endif

load_image:
		; Some BIOSes apparently have limitations on the size 
		; that may be loaded (despite the El Torito spec being very
		; clear on the fact that it must all be loaded.)  Therefore,
		; we load it ourselves, and *bleep* the BIOS.

		mov eax,[bi_file]		; Address of code to load
		inc eax				; Don't reload bootstrap code
%ifdef DEBUG_MESSAGES
		mov si,offset_msg
		call writemsg
		call writehex8
		call crlf
%endif

		; Just in case some BIOSes have problems with
		; segment wraparound, use the normalized address
		mov bx,((7C00h+2048) >> 4)
		mov es,bx
		xor bx,bx
		mov bp,[ImageSectors]
%ifdef DEBUG_MESSAGES
		push ax
		mov si,size_msg
		call writemsg
		mov ax,bp
		call writehex4
		call crlf
		pop ax
%endif
		call getlinsec

		push ds
		pop es

%ifdef DEBUG_MESSAGES
		mov si,loaded_msg
		call writemsg
%endif

		; Verify the checksum on the loaded image.
verify_image:
		mov si,7C00h+2048
		mov bx,es
		mov ecx,[ImageDwords]
		mov edi,[FirstSecSum]		; First sector checksum
.loop		es lodsd
		add edi,eax
		dec ecx
		jz .done
		and si,si
		jnz .loop
		; SI wrapped around, advance ES
		add bx,1000h
		mov es,bx
		jmp short .loop
.done:		mov ax,ds
		mov es,ax
		cmp [bi_csum],edi
		je integrity_ok

		mov si,checkerr_msg
		call writemsg
		jmp kaboom

integrity_ok:
%ifdef DEBUG_MESSAGES
		mov si,allread_msg
		call writemsg
%endif
		jmp all_read			; Jump to main code

		; INT 13h, AX=4B01h, DL=<passed in value> failed.
		; Try to scan the entire 80h-FFh from the end.
spec_query_failed:
		mov si,spec_err_msg
		call writemsg

		mov dl,0FFh
.test_loop:	pusha
		mov ax,4B01h
		mov si,spec_packet
		mov byte [si],13		; Size of buffer
		int 13h
		popa
		jc .still_broken

		mov si,maybe_msg
		call writemsg
		mov al,dl
		call writehex2
		call crlf

		cmp byte [sp_drive],dl
		jne .maybe_broken

		; Okay, good enough...
		mov si,alright_msg
		call writemsg
		mov [DriveNo],dl
.found_drive:	jmp found_drive

		; Award BIOS 4.51 apparently passes garbage in sp_drive,
		; but if this was the drive number originally passed in
		; DL then consider it "good enough"
.maybe_broken:
		cmp byte [DriveNo],dl
		je .found_drive

.still_broken:	dec dx
		cmp dl, 80h
		jnb .test_loop

fatal_error:
		mov si,nothing_msg
		call writemsg

.norge:		jmp short .norge

		; Information message (DS:SI) output
		; Prefix with "isolinux: "
		;
writemsg:	push ax
		push si
		mov si,isolinux_str
		call writestr
		pop si
		call writestr
		pop ax				
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
; Note: writestr and cwritestr are distinct in SYSLINUX, not in ISOLINUX
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

writestr	equ cwritestr

;
; writehex[248]: Write a hex number in (AL, AX, EAX) to the console
;
writehex2:
		pushfd
		pushad
		shl eax,24
		mov cx,2
		jmp short writehex_common
writehex4:
		pushfd
		pushad
		shl eax,16
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
; Write a character to the screen.  There is a more "sophisticated"
; version of this in the subsequent code, so we patch the pointer
; when appropriate.
;

writechr:
		jmp near writechr_simple	; NOT "short"!!!!

writechr_simple:
		pushfd
		pushad
		mov ah,0Eh
		xor bx,bx
		int 10h
		popad
		popfd
		ret

;
; Get one sector.  Convenience entry point.
;
getonesec:
		mov bp,1
		; Fall through to getlinsec

;
; Get linear sectors - EBIOS LBA addressing, 2048-byte sectors.
;
; Note that we can't always do this as a single request, because at least
; Phoenix BIOSes has a 127-sector limit.  To be on the safe side, stick
; to 32 sectors (64K) per request.
;
; Input:
;	EAX	- Linear sector number
;	ES:BX	- Target buffer
;	BP	- Sector count
;
getlinsec:
		mov si,dapa			; Load up the DAPA
		mov [si+4],bx
		mov bx,es
		mov [si+6],bx
		mov [si+8],eax
.loop:
		push bp				; Sectors left
		cmp bp,byte 32
		jbe .bp_ok
		mov bp,32
.bp_ok:
		mov [si+2],bp
		push si
		mov dl,[DriveNo]
		mov ah,42h			; Extended Read
		call xint13
		pop si
		pop bp
		movzx eax,word [si+2]		; Sectors we read
		add [si+8],eax			; Advance sector pointer
		sub bp,ax			; Sectors left
		shl ax,SECTORSIZE_LG2-4		; 2048-byte sectors -> segment
		add [si+6],ax			; Advance buffer pointer
		and bp,bp
		jnz .loop
		mov eax,[si+8]			; Next sector
		ret

		; INT 13h with retry
xint13:		mov byte [RetryCount], 6
.try:		pushad
		int 13h
		jc .error
		add sp,byte 8*4			; Clean up stack
		ret
.error:		mov [DiskError],ah		; Save error code
		popad
		dec byte [RetryCount]
		jnz .try

.real_error:	mov si,diskerr_msg
		call writemsg
		mov al,[DiskError]
		call writehex2
		mov si,ondrive_str
		call writestr
		mov al,dl
		call writehex2
		call crlf
		; Fall through to kaboom

;
; kaboom: write a message and bail out.  Wait for a user keypress,
;	  then do a hard reboot.
;
kaboom:
		lss sp,[cs:Stack]
		mov ax,cs
		mov ds,ax
		mov es,ax
		mov fs,ax
		mov gs,ax
		sti
		mov si,err_bootfailed
		call cwritestr
		call getchar
		cli
		mov word [BIOS_magic],0	; Cold reboot
		jmp 0F000h:0FFF0h	; Reset vector address

;
; Data that needs to be in the first sector
;
isolinux_banner	db CR, LF, 'ISOLINUX ', version_str, ' ', date, ' ', 0
copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
isolinux_str	db 'isolinux: ', 0
%ifdef DEBUG_MESSAGES
startup_msg:	db 'Starting up, DL = ', 0
spec_ok_msg:	db 'Loaded spec packet OK, drive = ', 0
secsize_msg:	db 'Sector size appears to be ', 0
offset_msg:	db 'Loading main image from LBA = ', 0
size_msg:	db 'Sectors to load = ', 0
loaded_msg:	db 'Loaded boot image, verifying...', CR, LF, 0
verify_msg:	db 'Image checksum verified.', CR, LF, 0
allread_msg	db 'Main image read, jumping to main code...', CR, LF, 0
%endif
spec_err_msg:	db 'Loading spec packet failed, trying to wing it...', CR, LF, 0
maybe_msg:	db 'Found something at drive = ', 0
alright_msg:	db 'Looks like it might be right, continuing...', CR, LF, 0
nosecsize_msg:	db 'Failed to get sector size, assuming 0800', CR, LF, 0
diskerr_msg:	db 'Disk error ', 0
ondrive_str:	db ', drive ', 0
nothing_msg:	db 'Failed to locate CD-ROM device; boot failed.', CR, LF, 0
checkerr_msg:	db 'Image checksum error, sorry...', CR, LF, 0

err_bootfailed	db CR, LF, 'Boot failed: press a key to retry...'
bailmsg		equ err_bootfailed
crlf_msg	db CR, LF, 0

;
; El Torito spec packet
;
		align 8, db 0
spec_packet:	db 13h				; Size of packet
sp_media:	db 0				; Media type
sp_drive:	db 0				; Drive number
sp_controller:	db 0				; Controller index
sp_lba:		dd 0				; LBA for emulated disk image
sp_devspec:	dw 0				; IDE/SCSI information
sp_buffer:	dw 0				; User-provided buffer
sp_loadseg:	dw 0				; Load segment
sp_sectors:	dw 0				; Sector count
sp_chs:		db 0,0,0			; Simulated CHS geometry
sp_dummy:	db 0				; Scratch, safe to overwrite

;
; Spec packet for disk image emulation
;
		align 8, db 0
dspec_packet:	db 13h				; Size of packet
dsp_media:	db 0				; Media type
dsp_drive:	db 0				; Drive number
dsp_controller:	db 0				; Controller index
dsp_lba:	dd 0				; LBA for emulated disk image
dsp_devspec:	dw 0				; IDE/SCSI information
dsp_buffer:	dw 0				; User-provided buffer
dsp_loadseg:	dw 0				; Load segment
dsp_sectors:	dw 1				; Sector count
dsp_chs:	db 0,0,0			; Simulated CHS geometry
dsp_dummy:	db 0				; Scratch, safe to overwrite

;
; EBIOS drive parameter packet
;
		align 8, db 0
drive_params:	dw 30				; Buffer size
dp_flags:	dw 0				; Information flags
dp_cyl:		dd 0				; Physical cylinders
dp_head:	dd 0				; Physical heads
dp_sec:		dd 0				; Physical sectors/track
dp_totalsec:	dd 0,0				; Total sectors
dp_secsize:	dw 0				; Bytes per sector
dp_dpte:	dd 0				; Device Parameter Table
dp_dpi_key:	dw 0				; 0BEDDh if rest valid
dp_dpi_len:	db 0				; DPI len
		db 0
		dw 0
dp_bus:		times 4 db 0			; Host bus type
dp_interface:	times 8 db 0			; Interface type
db_i_path:	dd 0,0				; Interface path
db_d_path:	dd 0,0				; Device path
		db 0
db_dpi_csum:	db 0				; Checksum for DPI info

;
; EBIOS disk address packet
;
		align 8, db 0
dapa:		dw 16				; Packet size
.count:		dw 0				; Block count
.off:		dw 0				; Offset of buffer
.seg:		dw 0				; Segment of buffer
.lba:		dd 0				; LBA (LSW)
		dd 0				; LBA (MSW)

		alignb 4, db 0
Stack		dw _start, 0		; SS:SP for stack reset

rl_checkpt	equ $				; Must be <= 800h

rl_checkpt_off	equ ($-$$)
%if rl_checkpt_off > 0x800
%error "Sector 0 overflow"
%endif

; ----------------------------------------------------------------------------
;  End of code and data that have to be in the first sector
; ----------------------------------------------------------------------------

all_read:
;
; Initialize screen (if we're using one)
;
		; Now set up screen parameters
		call adjust_screen

		; Patch the writechr routine to point to the full code
		mov word [writechr+1], writechr_full-(writechr+3)

; Tell the user we got this far...
%ifndef DEBUG_MESSAGES			; Gets messy with debugging on
		mov si,copyright_str
		call writestr
%endif

; Test tracers
		TRACER 'T'
		TRACER '>'

;
; Clear Files structures
;
		mov di,Files
		mov cx,(MAX_OPEN*open_file_t_size)/4
		xor eax,eax
		rep stosd

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
; Now check that there is at least 384K of low (DOS) memory
;
		int 12h
		cmp ax,384
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
; Now, we need to sniff out the actual filesystem data structures.
; mkisofs gave us a pointer to the primary volume descriptor
; (which will be at 16 only for a single-session disk!); from the PVD
; we should be able to find the rest of what we need to know.
; 
get_fs_structures:
		mov eax,[bi_pvd]
		mov bx,trackbuf
		call getonesec

		mov eax,[trackbuf+156+2]
		mov [RootDir+dir_lba],eax
		mov [CurDir+dir_lba],eax
%ifdef DEBUG_MESSAGES
		mov si,dbg_rootdir_msg
		call writemsg
		call writehex8
		call crlf
%endif
		mov eax,[trackbuf+156+10]
		mov [RootDir+dir_len],eax		
		mov [CurDir+dir_len],eax
		add eax,SECTORSIZE-1
		shr eax,SECTORSIZE_LG2
		mov [RootDir+dir_clust],eax
		mov [CurDir+dir_clust],eax

		; Look for an "isolinux" directory, and if found,
		; make it the current directory instead of the root
		; directory.
		mov di,isolinux_dir
		mov al,02h			; Search for a directory
		call searchdir_iso
		jz .no_isolinux_dir
		mov [CurDir+dir_len],eax
		mov eax,[si+file_left]
		mov [CurDir+dir_clust],eax
		xor eax,eax			; Free this file pointer entry
		xchg eax,[si+file_sector]
		mov [CurDir+dir_lba],eax
%ifdef DEBUG_MESSAGES
		push si
		mov si,dbg_isodir_msg
		call writemsg
		pop si
		call writehex8
		call crlf
%endif
.no_isolinux_dir:

;
; Locate the configuration file
;
load_config:
%ifdef DEBUG_MESSAGES
		mov si,dbg_config_msg
		call writemsg
%endif

		mov di,isolinux_cfg
		call open
		jz near no_config_file		; Not found or empty

%ifdef DEBUG_MESSAGES
		mov si,dbg_configok_msg
		call writemsg
%endif

;
; Now we have the config file open
;
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
		je near pc_kernel
                cmp ax,'im'                     ; IMplicit
                je near pc_implicit
		cmp ax,'se'			; SErial
		je near pc_serial
		cmp ax,'sa'			; SAy
		je near pc_say
		cmp ax,'lo'			; LOcalboot
		je pc_localboot
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
                jmp short parse_config_2
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

pc_localboot:	call getint			; "localboot" command
		cmp word [VKernelCtr],byte 0	; ("label" section only)
		je parse_config_2
		mov [VKernelBuf+vk_rname], byte 0	; Null kernel name
		mov [VKernelBuf+vk_rname+1], bx	; Return type
		jmp short parse_config_2

pc_kernel:	cmp word [VKernelCtr],byte 0	; "kernel" command
		je parse_config_2		; ("label" section only)
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
parse_config_2:	jmp parse_config

pc_display:	call pc_getfile			; "display" command
		jz parse_config_2		; File not found?
		call get_msg_file		; Load and display file
		jmp short parse_config_2

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
		mov si,isolinux_banner
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
		shl di,FILENAME_MAX_LG2		; Convert to offset
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
		mov cx,FILENAME_MAX
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

pc_say:		mov di,trackbuf			; "say" command
		push di
		call getline
		xor al,al
		stosb				; Null-terminate
		pop si
		call writestr
		call crlf
		jmp short parse_config_3

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
		mov dx,[BIOS_timer]		; Get time "of day"
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
		je near command_done
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
		shl ax,FILENAME_MAX_LG2		; Convert to offset
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
		mov cx,FILENAME_MAX
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
;
; Find the kernel on disk
;
get_kernel:     mov byte [KernelName+FILENAME_MAX],0	; Zero-terminate filename/extension
		mov di,KernelName
		xor al,al
		mov cx,FILENAME_MAX-5		; Need 4 chars + null
		repne scasb			; Scan for final null
		jne .no_skip
		dec di				; Point to final null 
.no_skip:	mov [KernelExtPtr],di
		mov bx,exten_table
.search_loop:	push bx
                mov di,KernelName	      	; Search on disk
                call searchdir
		pop bx
                jnz near kernel_good
		mov eax,[bx]			; Try a different extension
		mov si,[KernelExtPtr]
		mov [si],eax
		mov byte [si+4],0
		add bx,byte 4
		cmp bx,exten_table_end
		jna .search_loop		; allow == case (final case)
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
		mov cx,FILENAME_MAX		; We need ECX == CX later
		rep movsb
		pop di
		xor bx,bx			; Try only one version

		; Is this a "localboot" pseudo-kernel?
		cmp byte [VKernelBuf+vk_rname], 0
		jne near get_kernel		; No, it's real, go get it

		mov ax, [VKernelBuf+vk_rname+1]
		jmp local_boot
;
; kernel_corrupt: Called if the kernel file does not seem healthy
;
kernel_corrupt: mov si,err_notkernel
                jmp abort_load
;
; This is it!  We have a name (and location on the disk)... let's load
; that sucker!!  First we have to decide what kind of file this is; base
; that decision on the file extension.  The following extensions are
; recognized; case insensitive:
;
; .com 	- COMBOOT image
; .cbt	- COMBOOT image
; .bs	- Boot sector	  (SYSLINUX only)
; .bss	- DOS boot sector (SYSLINUX only)
; .img	- Disk image	  (ISOLINUX only)
;
; Anything else is assumed to be a Linux kernel.
;
kernel_good:
		pusha
		mov si,KernelName
		mov di,KernelCName
		call unmangle_name
		sub di,KernelCName
		mov [KernelCNameLen],di
		popa
		
		push di
		push ax
		mov di,KernelName
		xor al,al
		mov cx,FILENAME_MAX
		repne scasb
		jne .one_step
		dec di
.one_step:	mov ecx,[di-4]			; 4 bytes before end
		pop ax
		pop di

;
; At this point, DX:AX contains the size of the kernel, and SI contains
; the file handle/cluster pointer.
;
		or ecx,20202000h		; Force lower case

		cmp ecx,'.com'
		je near is_comboot_image
		cmp ecx,'.cbt'
		je near is_comboot_image
		cmp ecx,'.img'
		je near is_disk_image
		cmp ecx,'.bss'
		je near is_bss_sector
		and ecx, 00ffffffh
		cmp ecx,'.bs'
		je near is_bootsector
		; Otherwise Linux kernel
;
; A Linux kernel consists of three parts: boot sector, setup code, and
; kernel code.	The boot sector is never executed when using an external
; booting utility, but it contains some status bytes that are necessary.
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
		jb near kernel_corrupt
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

		movzx eax,ax			; Fix this by using a 32-bit
		shl edx,16			; register for the kernel size
		or eax,edx
		mov [KernelSize],eax
		xor edx,edx
		div dword [ClustSize]		; # of clusters total
		; Round up...
		add edx,byte -1			; Sets CF if EDX >= 1
		adc eax,byte 0			; Add 1 to EAX if CF set
                mov [KernelClust],eax

;
; Now, if we transfer these straight, we'll hit 64K boundaries.	 Hence we
; have to see if we're loading more than 64K, and if so, load it step by
; step.
;

;
; Start by loading the bootsector/setup code, to see if we need to
; do something funky.  It should fit in the first 32K (loading 64K won't
; work since we might have funny stuff up near the end of memory).
; If we have larger than 32K clusters, yes, we're hosed.
;
		call abort_check		; Check for abort key
		mov ecx,[ClustPerMoby]
		shr ecx,1			; Half a moby
		cmp ecx,[KernelClust]
		jna .normalkernel
		mov ecx,[KernelClust]
.normalkernel:
		sub [KernelClust],ecx
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
		sub eax,HIGHMEM_SLOP
		mov [HighMemSize],eax

;
; Construct the command line (append options have already been copied)
;
construct_cmdline:
		mov di,[CmdLinePtr]
                mov si,boot_image        	; BOOT_IMAGE=
                mov cx,boot_image_len
                rep movsb
                mov si,KernelCName       	; Unmangled kernel name
                mov cx,[KernelCNameLen]
                rep movsb
                mov al,' '                      ; Space
                stosb

.noipappend:
                mov si,[CmdOptPtr]              ; Options from user input
		mov cx,(kern_cmd_len+3) >> 2
		rep movsd
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
                push cs
                pop es                          ; Set ES <- normal DS
                mov di,initrd_cmd
		mov cx,initrd_cmd_len
		repe cmpsb
                jne not_initrd
		mov di,InitRD
                push si                         ; mangle_dir mangles si
                call mangle_name                ; Mangle ramdisk name
                pop si
		cmp byte [es:InitRD],0		; Null filename?
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
		sub ebx,HIGHMEM_SLOP
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
; About to load the kernel.  This is a modern kernel, so use the boot flags
; we were provided.
;
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
		jb near no_high_mem		; Not enough high memory
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
                mov es,ax

                mov si,dot_msg
                call cwritestr

;
; Now see if we have an initial RAMdisk; if so, do requisite computation
; We know we have a new kernel; the old_kernel code already will have objected
; if we tried to load initrd using an old kernel
;
load_initrd:
                test byte [initrd_flag],1
                jz near nk_noinitrd
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

initrd_end:
nk_noinitrd:
;
; Abandon hope, ye that enter here!  We do no longer permit aborts.
;
                call abort_check        	; Last chance!!

		mov si,ready_msg
		call cwritestr

		call vgaclearmode		; We can't trust ourselves after this

		cli
		xor ax,ax
		mov ss,ax
		mov sp,7C00h			; Set up a more normal stack
		
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
		jnz near comboot_too_large
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

		mov [SavedSSSP],sp
		mov ax,ss		; Save away SS:SP
		mov [SavedSSSP+2],ax

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
		mov ds,ax
		mov es,ax
		lss sp,[SavedSSSP]
		sti
		cld
		jmp short cb_enter

; Attempted to execute DOS system call
comboot_bogus:	cli			; Don't trust anyone
		xor ax,ax
		mov ds,ax
		mov es,ax
		lss sp,[SavedSSSP]
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
is_bss_sector:
		; Can't load these from the network, dang it!
.badness:	jmp short .badness

;
; Enable disk emulation.  The kind of disk we emulate is dependent on the size of
; the file: 1200K, 1440K or 2880K floppy, otherwise harddisk.
;
is_disk_image:
		TRACER CR
		TRACER LF
		TRACER 'D'
		TRACER ':'

		shl edx,16
		mov dx,ax			; Set EDX <- file size
		mov di,img_table
		mov cx,img_table_count
		mov eax,[si+file_sector]	; Starting LBA of file
		mov [dsp_lba],eax		; Location of file
		mov byte [dsp_drive], 0		; 00h floppy, 80h hard disk
.search_table:
		TRACER 't'
		mov eax,[di+4]
		cmp edx,[di]
		je near .type_found
		add di,8
		loop .search_table

		; Hard disk image.  Need to examine the partition table
		; in order to deduce the C/H/S geometry.  Sigh.
.hard_disk_image:
		TRACER 'h'
		cmp edx,512
		jb near .bad_image

		mov bx,trackbuf
		mov cx,1			; Load 1 sector
		call getfssec
		
		cmp word [trackbuf+510],0aa55h	; Boot signature
		jne near .bad_image		; Image not bootable

		mov cx,4			; 4 partition entries
		mov di,trackbuf+446		; Start of partition table

		xor ax,ax			; Highest sector(al) head(ah)

.part_scan:
		cmp byte [di+4], 0
		jz .part_loop
		lea si,[di+1]
		call .hs_check
		add si,byte 4
		call .hs_check
.part_loop:
		add di,byte 16
		loop .part_scan
		
		push eax			; H/S
		push edx			; File size
		mov bl,ah
		xor bh,bh
		inc bx				; # of heads in BX
		xor ah,ah			; # of sectors in AX
		cwde				; EAX[31:16] <- 0
		mul bx
		shl eax,9			; Convert to bytes
		; Now eax contains the number of bytes per cylinder
		pop ebx				; File size
		xor edx,edx
		div ebx
		and edx,edx
		jz .no_remainder
		inc eax				; Fractional cylinder...
		; Now (e)ax contains the number of cylinders
.no_remainder:	cmp eax,1024
		jna .ok_cyl
		mov ax,1024			; Max possible #
.ok_cyl:	dec ax				; Convert to max cylinder no
		pop ebx				; S(bl) H(bh)
		shl ah,6
		or bl,ah
		xchg ax,bx
		shl eax,16
		mov ah,bl
		mov al,4			; Hard disk boot
		mov byte [dsp_drive], 80h	; Drive 80h = hard disk

.type_found:
		TRACER 'T'
		mov bl,[sp_media]
		and bl,0F0h			; Copy controller info bits
		or al,bl
		mov [dsp_media],al		; Emulation type
		shr eax,8
		mov [dsp_chs],eax		; C/H/S geometry
		mov ax,[sp_devspec]		; Copy device spec
		mov [dsp_devspec],ax
		mov al,[sp_controller]		; Copy controller index
		mov [dsp_controller],al

		TRACER 'V'
		call vgaclearmode		; Reset video

		mov ax,4C00h			; Enable emulation and boot
		mov si,dspec_packet
		mov dl,[DriveNo]
		lss sp,[InitStack]
		TRACER 'X'

		int 13h

		; If this returns, we have problems
.bad_image:
		mov si,err_disk_image
		call cwritestr
		jmp enter_command

;
; Look for the highest seen H/S geometry
; We compute cylinders separately
;
.hs_check:
		mov bl,[si]			; Head #
		cmp bl,ah
		jna .done_track
		mov ah,bl			; New highest head #
.done_track:	mov bl,[si+1]
		and bl,3Fh			; Sector #
		cmp bl,al
		jna .done_sector
		mov al,bl
.done_sector:	ret

;
; Boot a specified local disk.  AX specifies the BIOS disk number; or
; 0xFFFF in case we should execute INT 18h ("next device.")
;
local_boot:
		call vgaclearmode
		lss sp,[cs:Stack]		; Restore stack pointer
		xor dx,dx
		mov ds,dx
		mov es,dx
		mov fs,dx
		mov gs,dx
		mov si,localboot_msg
		call writestr
		cmp ax,-1
		je .int18
		
		; Load boot sector from the specified BIOS device and jump to it.
		mov dl,al
		xor dh,dh
		push dx
		xor ax,ax			; Reset drive
		call xint13
		mov ax,0201h			; Read one sector
		mov cx,0001h			; C/H/S = 0/0/1 (first sector)
		mov bx,trackbuf
		call xint13
		pop dx
		cli				; Abandon hope, ye who enter here
		mov si,trackbuf
		mov di,07C00h
		mov cx,512			; Probably overkill, but should be safe
		rep movsd
		lss sp,[cs:InitStack]
		jmp 0:07C00h			; Jump to new boot sector

.int18:
		int 18h				; Hope this does the right thing...
		jmp kaboom			; If we returned, oh boy...

;
; 32-bit bcopy routine for real mode
;
; We enter protected mode, set up a flat 32-bit environment, run rep movsd
; and then exit.  IMPORTANT: This code assumes cs == 0.
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
		mov [cs:SavedSSSP],sp
		mov ax,ss
		mov [cs:SavedSSSP+2],ax

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

.in_rm:		; Back in real mode
		lss sp,[cs:SavedSSSP]
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
		mov si,crlfloading_msg		; Write "Loading "
		call cwritestr
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
		pop esi
		pop edi
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
		lss sp,[cs:Stack]		; Reset the stack
                sti
                call cwritestr                  ; Expects SI -> error msg
al_ok:          jmp enter_command               ; Return to command prompt
;
; End of abort_check
;
ac_ret2:	popa
ac_ret1:	ret


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

;
; searchdir_iso is a special entry point for ISOLINUX only.  In addition
; to the above, searchdir_iso passes a file flag mask in AL.  This is useful
; for searching for directories.
;
alloc_failure:
		xor ax,ax			; ZF <- 1
		ret

searchdir:
		xor al,al
searchdir_iso:
		mov [ISOFlags],al
		TRACER 'S'
		call allocate_file		; Temporary file structure for directory
		jnz alloc_failure
		push es
		push ds
		pop es				; ES = DS
		mov si,CurDir
		cmp byte [di],'/'		; If filename begins with slash
		jne .not_rooted
		inc di				; Skip leading slash
		mov si,RootDir			; Reference root directory instead
.not_rooted:
		mov eax,[si+dir_clust]
		mov [bx+file_left],eax
		mov eax,[si+dir_lba]
		mov [bx+file_sector],eax
		mov edx,[si+dir_len]

.look_for_slash:
		mov ax,di
.scan:
		mov cl,[di]
		inc di
		and cl,cl
		jz .isfile
		cmp cl,'/'
		jne .scan
		mov [di-1],byte 0		; Terminate at directory name
		mov cl,02h			; Search for directory
		xchg cl,[ISOFlags]
		push di
		push cx
		push word .resume		; Where to "return" to
		push es
.isfile:	xchg ax,di

.getsome:
		; Get a chunk of the directory
		mov si,trackbuf
		TRACER 'g'
		pushad
		xchg bx,si
		mov cx,[BufSafe]
		dec cx				; ... minus one sector
		call getfssec
		popad

.compare:
		movzx eax,byte [si]		; Length of directory entry
		cmp al,33
		jb .next_sector
		TRACER 'c'
		mov cl,[si+25]
		xor cl,[ISOFlags]
		test cl, byte 8Eh		; Unwanted file attributes!
		jnz .not_file
		pusha
		movzx cx,byte [si+32]		; File identifier length
		add si,byte 33			; File identifier offset
		TRACER 'i'
		call iso_compare_names
		popa
		je .success
.not_file:
		sub edx,eax			; Decrease bytes left
		jbe .failure
		add si,ax			; Advance pointer

.check_overrun:
		; Did we finish the buffer?
		cmp si,trackbuf+trackbufsize
		jb .compare			; No, keep going

		jmp short .getsome		; Get some more directory

.next_sector:
		; Advance to the beginning of next sector
		lea ax,[si+SECTORSIZE-1]
		and ax,~(SECTORSIZE-1)
		sub ax,si
		jmp short .not_file		; We still need to do length checks

.failure:	xor eax,eax			; ZF = 1
		mov [bx+file_sector],eax
		pop es
		ret

.success:
		mov eax,[si+2]			; Location of extent
		mov [bx+file_sector],eax
		mov eax,[si+10]			; Data length
		push eax
		add eax,SECTORSIZE-1
		shr eax,SECTORSIZE_LG2
		mov [bx+file_left],eax
		pop eax
		mov edx,eax
		shr edx,16
		and bx,bx			; ZF = 0
		mov si,bx
		pop es
		ret

.resume:	; We get here if we were only doing part of a lookup
		; This relies on the fact that .success returns bx == si
		xchg edx,eax			; Directory length in edx
		pop cx				; Old ISOFlags
		pop di				; Next filename pointer
		mov [ISOFlags],cl		; Restore the flags
		jz .failure			; Did we fail?  If so fail for real!
		jmp .look_for_slash		; Otherwise, next level

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
; iso_compare_names: 
;	Compare the names DS:SI and DS:DI and report if they are
;	equal from an ISO 9660 perspective.  SI is the name from
;	the filesystem; CX indicates its length, and ';' terminates.
;	DI is expected to end with a null.
;
;	Note: clobbers AX, CX, SI, DI; assumes DS == ES == base segment
;
iso_compare_names:
		; First, terminate and canonicalize input filename
		push di
		mov di,ISOFileName
.canon_loop:	jcxz .canon_end
		lodsb
		dec cx
		cmp al,';'
		je .canon_end
		and al,al
		je .canon_end
		stosb
		cmp di,ISOFileNameEnd-1		; Guard against buffer overrun
		jb .canon_loop
.canon_end:
		cmp di,ISOFileName
		jbe .canon_done
		cmp byte [di-1],'.'		; Remove terminal dots
		jne .canon_done
		dec di
		jmp short .canon_end
.canon_done:
		mov [di],byte 0			; Null-terminate string
		pop di
		mov si,ISOFileName
.compare:
		lodsb
		mov ah,[di]
		inc di
		and ax,ax
		jz .success			; End of string for both
		and al,al			; Is either one end of string?
		jz .failure			; If so, failure
		and ah,ah
		jz .failure
		or ax,2020h			; Convert to lower case
		cmp al,ah
		je .compare
.failure:	and ax,ax			; ZF = 0 (at least one will be nonzero)
.success:	ret

;
; strcpy: Copy DS:SI -> ES:DI up to and including a null byte
;
strcpy:		push ax
.loop:		lodsb
		stosb
		and al,al
		jnz .loop
		pop ax
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
;		mangling any registers.  This does raw console writes,
;		since some PXE BIOSes seem to interfere regular console I/O.
;
writechr_full:
		call write_serial	; write to serial port if needed
		pushfd
		pushad
		mov bh,[TextPage]
		push ax
                mov ah,03h              ; Read cursor position
                int 10h
		pop ax
		cmp al,8
		je .bs
		cmp al,13
		je .cr
		cmp al,10
		je .lf
		push dx
                mov bh,[TextPage]
		mov bl,07h		; White on black
		mov cx,1		; One only
		mov ah,09h		; Write char and attribute
		int 10h
		pop dx
		inc dl
		cmp dl,[VidCols]
		jna .curxyok
		xor dl,dl
.lf:		inc dh
		cmp dh,[VidRows]
		ja .scroll
.curxyok:	mov bh,[TextPage]
		mov ah,02h		; Set cursor position
		int 10h			
.ret:		popad
		popfd
		ret
.scroll:	dec dh
		mov bh,[TextPage]
		mov ah,02h
		int 10h
		mov ax,0601h		; Scroll up one line
		mov bh,[ScrollAttribute]
		xor cx,cx
		mov dx,[ScreenSize]	; The whole screen
		int 10h
		jmp short .ret
.cr:		xor dl,dl
		jmp short .curxyok
.bs:		sub dl,1
		jnc .curxyok
		mov dl,[VidCols]
		sub dh,1
		jnc .curxyok
		xor dh,dh
		jmp short .curxyok

;
; debug hack to print a character with minimal code impact
;
debug_tracer:	pushad
		pushfd
		mov bp,sp
		mov bx,[bp+9*4]		; Get return address
		mov al,[cs:bx]		; Get data byte
		inc word [bp+9*4]	; Return to after data byte
		call writechr
		popfd
		popad
		ret

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
; open,getc:	Load a file a character at a time for parsing in a manner
;		similar to the C library getc routine.	Only one simultaneous
;		use is supported.  Note: "open" trashes the trackbuf.
;
;		open:	Input:	filename in DS:DI
;			Output: ZF set on file not found or zero length
;
;		openfd:	Input:	file handle in SI
;			Output: none
;
;		getc:	Output: CF set on end of file
;				Character loaded in AL
;
open:
		call searchdir
		jz open_return
openfd:
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
		push es			; ES may be != DS, save old ES
		mov bx,ds
		mov es,bx
		mov bx,getcbuf
		push bx
		call getfssec		; Load a trackbuf full of data
		mov [FNextClust],si	; Store new next pointer
		pop si			; SI -> newly loaded data
		pop es			; Restore ES
getc_loaded:	lodsb			; Load a byte
		mov [FPtr],si		; Update next byte pointer
		dec dword [FBytes]	; Update bytes left counter
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
; mangle_name: Mangle a filename pointed to by DS:SI into a buffer pointed
;	       to by ES:DI; ends on encountering any whitespace.
;
;	       This verifies that a filename is < FILENAME_MAX characters,
;	       doesn't contain whitespace, zero-pads the output buffer,
;	       and removes trailing dots and redundant slashes,
;	       so "repe cmpsb" can do a compare, and the
;	       path-searching routine gets a bit of an easier job.
;	       
mangle_name:
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
		cmp byte [di-1],'.'		; Terminal dot?
		je .mn_kill
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
; getfssec: Get multiple clusters from a file, given the file pointer.
;
;  On entry:
;	ES:BX	-> Buffer
;	SI	-> File pointer
;	CX	-> Cluster count; 0FFFFh = until end of file
;  On exit:
;	SI	-> File pointer (or 0 on EOF)
;	CF = 1	-> Hit EOF
;
getfssec:
		TRACER 'F'

		push ds
		push cs
		pop ds				; DS <- CS

		cmp cx,[si+file_left]
		jna .ok_size
		mov cx,[si+file_left]
.ok_size:

		mov bp,cx
		push cx
		push si
		mov eax,[si+file_sector]
		TRACER 'l'
		call getlinsec
		xor ecx,ecx
		pop si
		pop cx

		add [si+file_sector],ecx
		sub [si+file_left],ecx
		ja .not_eof			; CF = 0

		xor ecx,ecx
		mov [si+file_sector],ecx	; Mark as unused
		xor si,si
		stc

.not_eof:
		pop ds
		TRACER 'f'
		ret


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
err_noram	db 'It appears your computer has less than 384K of low ("DOS")'
		db 0Dh, 0Ah
		db 'RAM.  Linux needs at least this amount to boot.  If you get'
		db 0Dh, 0Ah
		db 'this message in error, hold down the Ctrl key while'
		db 0Dh, 0Ah
		db 'booting, and I will take your word for it.', 0Dh, 0Ah, 0
err_badcfg      db 'Unknown keyword in config file.', CR, LF, 0
err_noparm      db 'Missing parameter in config file.', CR, LF, 0
err_noinitrd    db CR, LF, 'Could not find ramdisk image: ', 0
err_nohighmem   db 'Not enough memory to load specified kernel.', CR, LF, 0
err_highload    db CR, LF, 'Kernel transfer failure.', CR, LF, 0
err_oldkernel   db 'Cannot load a ramdisk with an old kernel image.'
                db CR, LF, 0
err_notdos	db ': attempted DOS system call', CR, LF, 0
err_comlarge	db 'COMBOOT image too large.', CR, LF, 0
err_bootsec	db 'Invalid or corrupt boot sector image.', CR, LF, 0
err_a20		db CR, LF, 'A20 gate not responding!', CR, LF, 0
notfound_msg	db 'not found', CR, LF, 0
localboot_msg	db 'Booting from local disk...', CR, LF, 0
cmdline_msg	db 'Command line: ', CR, LF, 0
ready_msg	db ' ready.', CR, LF, 0
trying_msg	db 'Trying to load: ', 0
crlfloading_msg	db CR, LF			; Fall through
loading_msg     db 'Loading ', 0
dotdot_msg      db '.'
dot_msg         db '.', 0
fourbs_msg	db BS, BS, BS, BS, 0
aborted_msg	db ' aborted.'			; Fall through to crlf_msg!
crff_msg	db CR, FF, 0
default_str	db 'default', 0
default_len	equ ($-default_str)
isolinux_dir	db '/isolinux', 0
isolinux_cfg	db 'isolinux.cfg', 0
err_disk_image	db 'Cannot load disk image (invalid file)?', CR, LF, 0

%ifdef DEBUG_MESSAGES
dbg_rootdir_msg	db 'Root directory at LBA = ', 0
dbg_isodir_msg	db 'isolinux directory at LBA = ', 0
dbg_config_msg	db 'About to load config file...', CR, LF, 0
dbg_configok_msg	db 'Configuration file opened...', CR, LF, 0
%endif
;
; Command line options we'd like to take a look at
;
; mem= and vga= are handled as normal 32-bit integer values
initrd_cmd	db 'initrd='
initrd_cmd_len	equ 7
;
; Config file keyword table
;
		align 2, db 0

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
		db 'sa' ; say
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
		; PXELINUX specific options...
		db 'ip' ; ipappend
		db 'lo' ; localboot
		dw 0
;
; Extensions to search for (in *forward* order).
;
		align 4, db 0
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.img'		; Disk image
		db '.com'		; COMBOOT (same as DOS)
exten_table_end:
		dd 0, 0			; Need 8 null bytes here

;
; Floppy image table
;
		align 4, db 0
img_table_count	equ 3
img_table:
		dd 1200*1024		; 1200K floppy
		db 1			; Emulation type
		db 80-1			; Max cylinder
		db 15			; Max sector
		db 2-1			; Max head

		dd 1440*1024		; 1440K floppy
		db 2			; Emulation type
		db 80-1			; Max cylinder
		db 18			; Max sector
		db 2-1			; Max head

		dd 2880*1024		; 2880K floppy
		db 3			; Emulation type
		db 80-1			; Max cylinder
		db 36			; Max sector
		db 2-1			; Max head

;
; Misc initialized (data) variables
;
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
NextSocket	dw 49152		; Counter for allocating socket numbers
A20List		dw a20_dunno, a20_none, a20_bios, a20_kbc, a20_fast
A20DList	dw a20d_dunno, a20d_none, a20d_bios, a20d_kbc, a20d_fast
A20Type		dw A20_DUNNO		; A20 type unknown
VGAFontSize	dw 16			; Defaults to 16 byte font
UserFont	db 0			; Using a user-specified font
ScrollAttribute	db 07h			; White on black (for text mode)

;
; Variables that are uninitialized in SYSLINUX but initialized here
;
; **** ISOLINUX:: We may have to make this flexible, based on what the
; **** BIOS expects our "sector size" to be.
;
		alignb 4, db 0
ClustSize	dd SECTORSIZE		; Bytes/cluster
ClustPerMoby	dd 65536/SECTORSIZE	; Clusters per 64K
SecPerClust	dw 1			; Same as bsSecPerClust, but a word
BufSafe		dw trackbufsize/SECTORSIZE	; Clusters we can load into trackbuf
BufSafeSec	dw trackbufsize/SECTORSIZE	; = how many sectors?
BufSafeBytes	dw trackbufsize		; = how many bytes?
EndOfGetCBuf	dw getcbuf+trackbufsize	; = getcbuf+BufSafeBytes
%if ( trackbufsize % SECTORSIZE ) != 0
%error trackbufsize must be a multiple of SECTORSIZE
%endif

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

