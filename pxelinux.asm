; -*- fundamental -*- (asm-mode sucks)
; $Id$
; ****************************************************************************
;
;  pxelinux.asm
;
;  A program to boot Linux kernels off a TFTP server using the Intel PXE
;  network booting API.  It is based on the SYSLINUX boot loader for
;  MS-DOS floppies.
;
;   Copyright (C) 1994-2000  H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;  USA; either version 2 of the License, or (at your option) any later
;  version; incorporated herein by reference.
; 
; ****************************************************************************


%include "pxe.inc"

;
; Macros for byte order
;
%define htons(x)  ( ( ((x) & 0FFh) << 8 ) + ( ((x) & 0FF00h) >> 8 ) )
%define ntohs(x) htons(x)
%define htonl(x)  ( ( ((x) & 0FFh) << 24) + ( ((x) & 0FF00h) << 8 ) + ( ((x) & 0FF0000h) >> 8 ) + ( ((x) & 0FF000000h) >> 24) )
%define ntohl(x) htonl(x)

;
; Some semi-configurable constants... change on your own risk.  Most are imposed
; by the kernel.
;
max_cmd_len	equ 255			; Must be odd; 255 is the kernel limit
FILENAME_MAX	equ 32			; Including final null; should be a power of 2
LOG_FILENAME_MAX equ 5			; log2(FILENAME_MAX)
REBOOT_TIME	equ 5*60		; If failure, time until full reset
HIGHMEM_MAX	equ 038000000h		; Highest address for an initrd
HIGHMEM_SLOP	equ 128*1024		; Avoid this much memory near the top
DEFAULT_BAUD	equ 9600		; Default baud rate for serial port
BAUD_DIVISOR	equ 115200		; Serial port parameter
MAX_SOCKETS	equ 64			; Max number of open sockets
TFTP_PORT	equ htons(69)		; Default TFTP port 
PKT_RETRY	equ 6			; Packet transmit retry count
PKT_TIMEOUT	equ 8			; Initial timeout, timer ticks @ 55 ms
TFTP_BLOCKSIZE	equ 512			; Bytes/block
LOG_TFTP_BLOCKSIZE equ 9		; log2(TFTP_BLOCKSIZE)

;
; TFTP operation codes
;
TFTP_RRQ	equ htons(1)		; Read request
TFTP_WRQ	equ htons(2)		; Write request
TFTP_DATA	equ htons(3)		; Data packet
TFTP_ACK	equ htons(4)		; ACK packet
TFTP_ERROR	equ htons(5)		; ERROR packet
TFTP_OACK	equ htons(6)		; OACK packet

;
; Should be updated with every release to avoid bootsector/SYS file mismatch
;
%define	version_str	VERSION		; Must be 4 characters long!
%define date		DATE_STR	; Defined from the Makefile
%define	year		'1999'
;
; Debgging stuff
;
; %define debug 1			; Uncomment to enable debugging
;
; ID for SYSLINUX (reported to kernel)
;
syslinux_id	equ 032h		; SYSLINUX (3) 2 = PXELINUX
;
; Segments used by Linux
;
; Note: the real_mode_seg is supposed to be 9000h, but PXE uses that
; memory.  Therefore, we load it at 5000:0000h and copy it before starting
; the Linux kernel.
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

%if (vk_end > vk_size) || (vk_size*max_vk > 65536)
%error "Too many vkernels defined, reduce vk_power"
%endif

;
; Segment assignments in the bottom 640K
; 0000h - main code/data segment (and BIOS segment)
; 5000h - real_mode_seg
;
vk_seg          equ 4000h		; This is where we stick'em
xfer_buf_seg	equ 3000h		; Bounce buffer for I/O to high mem
comboot_seg	equ 2000h		; COMBOOT image loading zone

;
; BOOTP/DHCP packet pattern
;
		struc bootp_t		
bootp:
.opcode		resb 1			; BOOTP/DHCP "opcode"
.hardware	resb 1			; ARP hardware type
.hardlen	resb 1			; Hardware address length
.gatehops	resb 1			; Used by forwarders
.ident		resd 1			; Transaction ID
.seconds	resw 1			; Seconds elapsed
.flags		resw 1			; Broadcast flags
.cip		resd 1			; Client IP
.yip		resd 1			; "Your" IP
.sip		resd 1			; Next server IP
.gip		resd 1			; Relay agent IP
.macaddr	resb 16			; Client MAC address
.sname		resb 64			; Server name (optional)
.bootfile	resb 128		; Boot file name
.v_magic	resd 1			; DHCP magic cookie
.v_flags	resd 1			; DHCP flags
.v_pad		resb 56			; Vendor options padding
		endstruc		

;
; TFTP connection data structure.  Each one of these corresponds to a local
; UDP port.  The size of this structure must be a power of 2.
;
		struc tftp_port_t
tftp_localport	resw 1			; Local port number	(0 = not in use)
tftp_remoteport	resw 1			; Remote port number
tftp_remoteip	resd 1			; Remote IP address
tftp_filepos	resd 1			; Position within file
tftp_filesize	resd 1			; Total file size
		endstruc

%if (tftp_port_t_size & (tftp_port_t_size-1))
%error "tftp_port_t is not a power of 2"
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
trackbuf	resb 16384		; Track buffer goes here
trackbufsize	equ $-trackbuf

;		trackbuf ends at 5000h

                absolute 5000h          ; Here we keep our BSS stuff
StackBuf	equ $			; Start the stack here (grow down - 4K)
VKernelBuf:	resb vk_size		; "Current" vkernel
		alignb 4
AppendBuf       resb max_cmd_len+1	; append=
KbdMap		resb 256		; Keyboard map
PathPrefix	resb 128		; 128 bytes (comes from BOOTP size)
FKeyName	resb 10*FILENAME_MAX	; File names for F-key help
NumBuf		resb 16			; Buffer to load number
NumBufEnd	equ NumBuf+15		; Pointer to last byte in NumBuf
		alignb 32
BootFile	resb 128		; Boot file name from DHCP query
KernelName      resb FILENAME_MAX       ; Mangled name for kernel
KernelCName     resb FILENAME_MAX	; Unmangled kernel name
InitRDCName     resb FILENAME_MAX       ; Unmangled initrd name
MNameBuf	resb FILENAME_MAX
InitRD		resb FILENAME_MAX
PartInfo	resb 16			; Partition table entry
E820Buf		resd 5			; INT 15:E820 data buffer
InitRDat	resd 1			; Load address (linear) for initrd
HiLoadAddr      resd 1			; Address pointer for high load loop
HighMemSize	resd 1			; End of memory pointer (bytes)
KernelSize	resd 1			; Size of kernel (bytes)
Stack		resd 1			; Pointer to reset stack
PXEEntry	resd 1			; !PXE API entry point
MyIP		resd 1			; My IP address
ServerIP	resd 1			; IP address of boot server
SavedSSSP	resw 1			; Our SS:SP while running a COMBOOT image
FBytes		equ $			; Used by open/getc
FBytes1		resw 1
FBytes2		resw 1
KernelClust	resw 1			; Kernel size in clusters
InitRDClust	resw 1			; Ramdisk size in clusters
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
ServerPort	resw 1			; TFTP server port
ConfigFile	resw 1			; Socket for config file
PktTimeout	resw 1			; Timeout for current packet
KernelExtPtr	resw 1			; During search, final null pointer
TextAttrBX      equ $
TextAttribute   resb 1			; Text attribute for message file
TextPage        resb 1			; Active display page
CursorDX        equ $
CursorCol       resb 1			; Cursor column for message file
CursorRow       resb 1			; Cursor row for message file
ScreenSize      equ $
VidCols         resb 1			; Columns on screen-1
VidRows         resb 1			; Rows on screen-1
RetryCount      resb 1			; Used for disk access retries
KbdFlags	resb 1			; Check for keyboard escapes
LoadFlags	resb 1			; Loadflags from kernel
A20Tries	resb 1			; Times until giving up on A20
FuncFlag	resb 1			; == 1 if <Ctrl-F> pressed

		alignb tftp_port_t_size
Sockets		resb MAX_SOCKETS*tftp_port_t_size

		alignb 16
		; BOOTP/DHCP packet buffer

		alignb 16
packet_buf	resb 2048		; Transfer packet
packet_buf_size	equ $-packet_buf

		section .text
                org 7C00h
;
; Primary entry point.
;
bootsec		equ $
_start:
		jmp 0:_start1		; Canonicalize address
_start1:	
		mov bp,sp
		les bx,[bp+4]		; Initial !PXE structure pointer

		mov ax,cs
		mov ds,ax
		sti			; Stack already set up by PXE
		cld			; Copy upwards

		push ds
		mov [Stack],sp
		mov ax,ss
		mov [Stack+2],ax
;
; Initialize screen (if we're using one)
;
		call adjust_screen
;
; Tell the user we got this far
;
		mov si,pxelinux_banner
		call writestr

		mov si,copyright_str
		call writestr

;
; Now we need to find the !PXE structure.  It's *supposed* to be pointed
; to by SS:[SP+4], but support INT 1Ah, AX=5650h method as well.
;
		cmp dword [es:bx], '!PXE'
		je near have_pxe

		; Uh-oh, not there... try plan B
		mov ax, 5650h
		int 1Ah
		jc no_pxe
		cmp ax,564Eh
		jne no_pxe

		; Okay, that gave us the PXENV+ structure, find !PXE
		; structure from that
		cmp dword [es:bx], 'PXEN'
		jne no_pxe
		cmp word [es:bx+4], 'V+'
		je have_pxenv

		; Nothing there either.  Last-ditch: scan memory
		call memory_scan_for_pxe_struct		; !PXE scan
		jnc have_pxe
		call memory_scan_for_pxenv_struct	; PXENV+ scan
		jnc have_pxenv

no_pxe:		mov si,err_nopxe
		call writestr
		jmp kaboom

have_pxenv:
		mov si,found_pxenv
		call writestr

		mov si,apiver_str
		call writestr
		mov ax,[es:bx+6]
		call writehex4
		call crlf

		cmp word [es:bx+6], 0201h	; API version 2.1 or higher
		jb old_api
		mov si,bx
		mov ax,es
		les bx,[es:bx+26h]		; !PXE structure pointer
		cmp dword [es:bx],'!PXE'
		je have_pxe

		; Nope, !PXE structure missing despite API 2.1+, or at least
		; the pointer is missing.  Do a last-ditch attempt to find it.
		call memory_scan_for_pxe_struct
		jnc have_pxe

		; Otherwise, no dice, use PXENV+ structure
		mov bx,si
		mov es,ax

old_api:	; Need to use a PXENV+ structure
		mov si,using_pxenv_msg
		call writestr

		mov eax,[es:bx+0Ah]		; PXE RM API
		mov [PXENVEntry],eax

		mov si,pxenventry_msg
		call writestr
		mov ax,[PXENVEntry+2]
		call writehex4
		mov al,':'
		call writechr
		mov ax,[PXENVEntry]
		call writehex4
		call crlf
		jmp short have_entrypoint

have_pxe:
		mov eax,[es:bx+10h]
		mov [PXEEntry],eax

		mov si,pxeentry_msg
		call writestr
		mov ax,[PXEEntry+2]
		call writehex4
		mov al,':'
		call writechr
		mov ax,[PXEEntry]
		call writehex4
		call crlf

have_entrypoint:

;
; Clear Sockets structures
;
		mov di,Sockets
		mov cx,(MAX_SOCKETS*tftp_port_t_size)/4
		xor eax,eax
		rep stosd

;
; Now attempt to get the BOOTP/DHCP packet that brought us life (and an IP
; address).  This lives in the DHCPACK packet (query info 2).
;
query_bootp:
		mov ax,ds
		mov es,ax
		mov di,pxe_bootp_query_pkt
		mov bx,PXENV_GET_CACHED_INFO

		call far [PXENVEntry]
		jc .pxe_err1
		cmp ax,byte 0
		je .pxe_ok
.pxe_err1:	
		mov di,pxe_bootp_size_query_pkt
		mov bx,PXENV_GET_CACHED_INFO

		call far [PXENVEntry]
		jc .pxe_err
.pxe_size:
		mov ax,[pxe_bootp_size_query_pkt.buffersize]
		call writehex4
		call crlf

.pxe_err:
		mov si,err_pxefailed
		call writestr
		call writehex4
		mov al, ' '
		call writechr
		mov ax,[pxe_bootp_query_pkt.status]
		call writehex4
		call crlf

.pxe_ok:
		mov eax,[trackbuf+bootp.yip]	; "Your" IP address
		mov [MyIP],eax

;
; Now, get the boot file and other info.  This lives in the CACHED_REPLY
; packet (query info 3).
;
		mov [pxe_bootp_query_pkt.packettype], byte 3
		mov [pxe_bootp_size_query_pkt.packettype], byte 3

		mov di,pxe_bootp_query_pkt
		mov bx,PXENV_GET_CACHED_INFO

		call far [PXENVEntry]
		jc .pxe_err1
		cmp ax,byte 0
		jne .pxe_err1

		mov si,trackbuf+bootp.bootfile
		mov di,BootFile
		mov cx,128 >> 2
		rep movsd			; Copy bootfile name

;
; If packet 2 didn't contain a valid IP address, guess that it's in this
; packet instead
;
		mov si,myipaddr_msg
		call writestr
		mov eax,[MyIP]
		cmp eax, byte 0			; 0.0.0.0 bad
		je .badip
		cmp al,224			; 224..255.x.x.x
		jb .goodip
.badip:
		mov eax,[trackbuf+bootp.yip]	; Hope this is better...
		mov [MyIP],eax
.goodip:
		xchg ah,al			; Host byte order
		ror eax,16
		xchg ah,al
		call writehex8
		call crlf
;
; Normalize ES = DS
;
		mov ax,ds
		mov es,ax

;
; Save away the server IP and port number
;
		mov eax,[trackbuf+bootp.sip]
		mov [ServerIP],eax
		mov [ServerPort], word TFTP_PORT

;
; Initialize UDP stack
;
udp_init:
		mov eax,[MyIP]
		mov [pxe_udp_open_pkt.sip],eax
		mov di,pxe_udp_open_pkt
		mov bx,PXENV_UDP_OPEN
		call far [PXENVEntry]
		jc .failed
		cmp word [pxe_udp_open_pkt.status], byte 0
		je .success
.failed:	mov si,err_udpinit
		call writestr
		jmp kaboom
.success:

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
; Store standard filename prefix
;
prefix:		mov si,BootFile
		mov di,PathPrefix
		cld
		call strcpy
		lea cx,[di-PathPrefix-1]
		std
		lea si,[di-2]			; Skip final null!
.find_alnum:	lodsb
		or al,20h
		cmp al,'.'			; Count . or - as alphanum
		je .alnum
		cmp al,'-'
		je .alnum
		cmp al,'0'
		jb .notalnum
		cmp al,'9'
		jbe .alnum
		cmp al,'a'
		jb .notalnum
		cmp al,'z'
		ja .notalnum
.alnum:		loop .find_alnum
		dec si
.notalnum:	mov byte [si+2],0		; Zero-terminate after delimiter
		cld
		mov si,tftpprefix_msg
		call writestr
		mov si,PathPrefix
		call writestr
		call crlf

;
; Load configuration file
;
find_config:	mov di,trackbuf
		mov si,cfgprefix
		mov cx,cfgprefix_len
		rep movsb
		mov cx,8
		mov eax,[MyIP]
		xchg ah,al			; Host byte order
		ror eax,16
		xchg ah,al
.hexify_loop:	rol eax,4
		push eax
		and al,0Fh
		cmp al,10
		jae .high
.low:		add al,'0'
		jmp short .char
.high:		add al,'A'-10
.char:		stosb
		pop eax
		loop .hexify_loop

;
; Begin looking for configuration file
;
config_scan:
		mov cx,9			; Up to 9 attempts

.tryagain:	mov byte [di],0
		cmp cx,byte 1
		jne .not_default
		pusha
		mov si,default_str
		mov cx,default_len
		rep movsb			; Copy "default" string
		popa
.not_default:	pusha
		mov si,trying_msg
		call writestr
		mov di,trackbuf
		mov si,di
		call writestr
		call crlf
		call open
		jnz .success

.badness:	popa
		dec di
		loop .tryagain

		jmp no_config_file

;
; Now we have the config file open
;
.success:	add sp,byte 16			; Adjust stack
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
		cmp ax,'sa'			; SAy
		je near pc_say
		cmp al,'f'			; F-key
		jne parse_config
		jmp pc_fkey

pc_default:	mov di,default_cmd		; "default" command
		call getline
		mov si,auto_cmd			; add "auto"+null
                mov cx,auto_len
		rep movsb
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
		jnc .valid_baud
		mov ebx,DEFAULT_BAUD		; No baud rate given
.valid_baud:	pop di				; Serial port #
		cmp ebx,byte 75
		jb parse_config_2		; < 75 baud == bogus
		mov eax,BAUD_DIVISOR
		cdq
		div ebx
		push ax				; Baud rate divisor
		mov dx,di
		shl di,1
		mov ax,[di+serial_base]
		mov [SerialPort],ax
		push ax				; Serial port base
		mov ax,00e3h			; INT 14h init parameters
		int 14h				; Init serial port
		pop bx				; Serial port base
		lea dx,[bx+3]
		mov al,83h			; Enable DLAB
		call slow_out
		pop ax				; Divisor
		mov dx,bx
		call slow_out
		inc dx
		mov al,ah
		call slow_out
		mov al,03h			; Disable DLAB
		add dx,byte 2
		call slow_out
		sub dx,byte 2
		xor al,al			; IRQ disable
		call slow_out

		; Show some life
		mov si,pxelinux_banner
		call write_serial_str
		mov si,copyright_str
		call write_serial_str

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
		shl di,LOG_FILENAME_MAX		; Convert to offset
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
get_char_time:	mov cx,[KbdTimeOut]
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
		jmp command_done		; Timeout!

get_char_pop:	pop eax				; Clear stack
get_char:	call getchar
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
		shl ax,LOG_FILENAME_MAX		; Convert to offset
		mov bx,1
		shl bx,cl
		and bx,[FKeyMap]
		jz get_char_2			; Undefined F-key
		mov di,ax
		add di,FKeyName
		call searchdir
		jz fk_nofile
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
                jnz kernel_good
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
; recognized; case insensitive:
;
; .com 	- COMBOOT image
; .cbt	- COMBOOT image
; .bs	- Boot sector
; .bss	- Boot sector, but transfer over DOS superblock
;
; Boot sectors are currently not supported by PXELINUX.
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

		or ecx,20202000h		; Force lower case

		cmp ecx,'.com'
		je near is_comboot_image
		cmp ecx,'.cbt'
		je near is_comboot_image
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
; First check that our kernel is at least 64K and less than 8M (if it is
; more than 8M, we need to change the logic for loading it anyway...)
;
is_linux_kernel:
                cmp dx,80h			; 8 megs
		ja kernel_corrupt
		and dx,dx
		jz kernel_corrupt
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
		sub [KernelClust],cx
		xor bx,bx
                pop si                          ; Cluster pointer on stack
		call getfssec
		jc near kernel_corrupt		; Failure in first 32K
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
		push ds
		pop es
		xor ebx,ebx
.int_loop:	mov eax,0000e820h
		mov edx,'SMAP'
		mov ecx,20
		mov di,E820Buf
		int 15h
		jc no_e820
		cmp eax,'SMAP'
		jne no_e820
		and ebx,ebx			; Did we not find anything?
		jz no_e820
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
		jmp short got_highmem

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
		add eax,(1 << 20)		; First megabyte
got_highmem:
		sub eax,HIGHMEM_SLOP
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
		cmp dword [es:su_header],HEADER_ID	; New setup code ID
		jne near old_kernel		; Old kernel, load low
		cmp word [es:su_version],0200h	; Setup code version 2.0
		jb near old_kernel		; Old kernel, load low
                cmp word [es:su_version],0201h	; Version 2.01+?
                jb new_kernel                   ; If 2.00, skip this step
                mov word [es:su_heapend],linux_stack	; Set up the heap
                or byte [es:su_loadflags],80h	; Let the kernel know we care
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
                mov ecx,108000h			; 108000h = 1M + 32K
                sub ecx,esi			; Adjust pointer to 2nd block
                mov [HiLoadAddr],ecx
		sub ecx,100000h			; Turn into a counter
		shr ecx,2			; Convert to dwords
		add esi,(real_mode_seg << 4)	; Pointer to source
                mov edi,100000h                 ; Copy to address 100000h
                call bcopy			; Transfer to high memory

                push word xfer_buf_seg		; Transfer buffer segment
                pop es
high_load_loop: 
                mov si,dot_msg			; Progress report
                call cwritestr
                call abort_check
                mov cx,[KernelClust]
		cmp cx,[ClustPerMoby]
		jna high_last_moby
		mov cx,[ClustPerMoby]
high_last_moby:
		sub [KernelClust],cx
		xor bx,bx			; Load at offset 0
                pop si                          ; Restore cluster pointer
		call getfssec
                push si                         ; Save cluster pointer
                pushf                           ; Save EOF
                xor bx,bx
		mov esi,(xfer_buf_seg << 4)
                mov edi,[HiLoadAddr]		; Destination address
                mov ecx,4000h			; Cheating - transfer 64K
                call bcopy			; Transfer to high memory
		mov [HiLoadAddr],edi		; Point to next target area
                popf                            ; Restore EOF
                jc high_load_done               ; If EOF we are done
                cmp word [KernelClust],byte 0	; Are we done?
		jne high_load_loop		; Apparently not
high_load_done:
		pop si				; No longer needed
                mov ax,real_mode_seg		; Set to real mode seg
                mov es,ax

                mov si,dot_msg
                call cwritestr

		call crlf
;
; Now see if we have an initial RAMdisk; if so, do requisite computation
; We know we have a new kernel; the old_kernel code already will have objected
; if we tried to load initrd using an old kernel
;
load_initrd:
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
		mov [initrd_ptr],si		; Save cluster pointer
		mov [es:su_ramdisklen1],ax	; Ram disk length
		mov [es:su_ramdisklen2],dx
		div word [ClustSize]
		and dx,dx			; Round up
		setnz dl
		movzx dx,dl
		add ax,dx
		mov [InitRDClust],ax		; Ramdisk clusters
		mov edx,[HighMemSize]		; End of memory
		mov eax,HIGHMEM_MAX		; Limit imposed by kernel
		cmp edx,eax
		jna memsize_ok
		mov edx,eax			; Adjust to fit inside limit
memsize_ok:
		sub edx,[es:su_ramdisklen]	; Subtract size of ramdisk
                xor dx,dx			; Round down to 64K boundary
                mov [InitRDat],edx		; Load address
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
;
; Unload PXE stack
;
		call unload_pxe
		cli
		xor ax,ax
		mov ss,ax
		mov sp,7C00h			; Set up a more normal stack
		
;
; Now, if we were supposed to load "low", copy the kernel down to 10000h
; and the real mode stuff to 90000h.  We assume that all bzImage kernels are
; capable of starting their setup from a different address.
;
		mov bx,real_mode_seg		; Real mode segment
		mov fs,bx			; FS -> real_mode_seg
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

		test byte [LoadFlags],LOAD_HIGH
		; Note bx -> real_mode_seg still
		jnz in_proper_place		; If high load, we're done

;
; Loading low; we can't assume it's safe to run in place.
;
; Copy real_mode stuff up to 90000h
;
		mov ax,real_mode_seg
		mov fs,ax
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
		mov ecx,[KernelSize]
		add ecx,3			; Round upwards
		shr ecx,2			; Bytes -> dwords
		mov esi,100000h
		mov edi,10000h
		call bcopy

		mov bx,9000h			; Real mode segment

;
; Now everything is where it needs to be...
;
in_proper_place:
		mov es,bx			; Real mode segment
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
; Note: bx == the real mode segment
;
		cli
		; es is already == real mode segment
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
		jnz short comboot_too_large
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
		mov [cs:SavedSSSP],sp
		mov ax,ss
		mov [cs:SavedSSSP+2],ax

		cli
		call enable_a20

		o32 lgdt [bcopy_gdt]
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
%define disable_wait 	256		; How long to wait for a disable

%define A20_DUNNO	0		; A20 type unknown
%define A20_NONE	1		; A20 always on?
%define A20_BIOS	2		; A20 BIOS enable
%define A20_KBC		3		; A20 through KBC
%define A20_FAST	4		; A20 through port 92h

slow_out:	out dx, al		; Fall through

_io_delay:	out IO_DELAY_PORT,al
		out IO_DELAY_PORT,al
		out IO_DELAY_PORT,al
		out IO_DELAY_PORT,al
		ret

enable_a20:
		pushad
		mov byte [ss:A20Tries],255 ; Times to try to make this work

try_enable_a20:
;
; Flush the caches
;
;		call try_wbinvd

;
; If the A20 type is known, jump straight to type
;
		mov bp,[ss:A20Type]
		add bp,bp			; Convert to word offset
		jmp word [bp+A20List]		; Implicit ss: because of bp

;
; First, see if we are on a system with no A20 gate
;
a20_dunno:
a20_none:
		mov byte [ss:A20Type], A20_NONE
		call a20_test
		jnz a20_done

;
; Next, try the BIOS (INT 15h AX=2401h)
;
a20_bios:
		mov byte [ss:A20Type], A20_BIOS
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

		mov byte [ss:A20Type], A20_KBC	; Starting KBC command sequence

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
		mov byte [ss:A20Type], A20_FAST	; Haven't used the KBC yet
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


		dec byte [ss:A20Tries]
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
		mov cx,0100h		; Loop count
		mov ax,[ss:A20Test]
.a20_wait:	inc ax
		mov [ss:A20Test],ax
;		call try_wbinvd
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

		mov bp,[ss:A20Type]
		add bp,bp			; Convert to word offset
		jmp word [bp+A20DList]		; Implicit ss: because of bp

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
loadinitrd:
                push es                         ; Save ES on entry
                mov ax,real_mode_seg
                mov es,ax
                mov si,[initrd_ptr]
                mov edi,[InitRDat]		; initrd load address
		mov [es:su_ramdiskat],edi	; Offset for ram disk
		push si
                mov si,loading_msg
                call cwritestr
                mov si,InitRDCName		; Write ramdisk name
                call cwritestr
                mov si,dotdot_msg		; Write dots
                call cwritestr
rd_load_loop:	
		mov si,dot_msg			; Progress report
                call cwritestr
		pop si				; Restore cluster pointer
                call abort_check
                mov cx,[InitRDClust]
		cmp cx,[ClustPerMoby]
		jna rd_last_moby
		mov cx,[ClustPerMoby]
rd_last_moby:
		sub [InitRDClust],cx
		xor bx,bx			; Load at offset 0
                push word xfer_buf_seg		; Bounce buffer segment
		pop es
		push cx
		call getfssec
		pop cx
                push si				; Save cluster pointer
		mov esi,(xfer_buf_seg << 4)
		mov edi,[InitRDat]
		mov ecx,4000h			; Copy 64K
		call bcopy			; Does not change flags!!
                jc rd_load_done                 ; EOF?
                add dword [InitRDat],10000h	; Point to next 64K
		cmp word [InitRDClust],byte 0	; Are we done?
		jne rd_load_loop		; Apparently not
rd_load_done:
                pop si                          ; Clean up the stack
		call crlf
                pop es                          ; Restore original ES
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
; kaboom: write a message and bail out.  Wait for quite a while, or a user keypress,
;	  then do a hard reboot.
;
kaboom:
		lss sp,[cs:Stack]
		pop ds
		sti
.patch:		mov si,bailmsg
		call writestr		; Returns with AL = 0
.drain:		call pollchar
		jz .drained
		call getchar
		jmp short .drain
.drained:
		mov cx,18
.wait1:		push cx
		mov cx,REBOOT_TIME
.wait2:		mov dx,[BIOS_timer]
.wait3:		call pollchar
		jnz .keypress
		cmp dx,[BIOS_timer]
		je .wait3
		loop .wait2
		mov al,'.'
		call writechr
		pop cx
		loop .wait1
.keypress:
		call crlf
		mov word [BIOS_magic],0	; Cold reboot
		jmp 0F000h:0FFF0h	; Reset vector address

;
; memory_scan_for_pxe_struct:
;
;	If none of the standard methods find the !PXE structure, look for it
;	by scanning memory.
;
;	On exit, if found:
;		CF = 0, ES:BX -> !PXE structure
;	Otherwise CF = 1, all registers saved
;	
memory_scan_for_pxe_struct:
		push ds
		pusha
		xor ax,ax
		mov ds,ax
		mov si,trymempxe_msg
		call writestr
		mov ax,[BIOS_fbm]	; Starting segment
		shl ax,(10-4)		; Kilobytes -> paragraphs
;		mov ax,01000h		; Start to look here
		dec ax			; To skip inc ax
.mismatch:
		inc ax
		cmp ax,0A000h		; End of memory
		jae .not_found
		call writehex4
		mov si,fourbs_msg
		call writestr
		mov es,ax
		mov edx,[es:0]
		cmp edx,'!PXE'
		jne .mismatch
		movzx cx,byte [es:4]	; Length of structure
		cmp cl,08h		; Minimum length
		jb .mismatch
		push ax
		xor ax,ax
		xor si,si
.checksum:	es lodsb
		add ah,al
		loop .checksum
		pop ax
		jnz .mismatch		; Checksum must == 0
.found:		mov bp,sp
		xor bx,bx
		mov [bp+8],bx		; Save BX into stack frame (will be == 0)
		mov ax,es
		call writehex4
		call crlf
		popa
		pop ds
		clc
		ret
.not_found:	mov si,notfound_msg
		call writestr
		popa
		pop ds
		stc
		ret

;
; memory_scan_for_pxenv_struct:
;
;	If none of the standard methods find the PXENV+ structure, look for it
;	by scanning memory.
;
;	On exit, if found:
;		CF = 0, ES:BX -> PXENV+ structure
;	Otherwise CF = 1, all registers saved
;	
memory_scan_for_pxenv_struct:
		pusha
		mov si,trymempxenv_msg
		call writestr
;		mov ax,[BIOS_fbm]	; Starting segment
;		shl ax,(10-4)		; Kilobytes -> paragraphs
		mov ax,01000h		; Start to look here
		dec ax			; To skip inc ax
.mismatch:
		inc ax
		cmp ax,0A000h		; End of memory
		jae .not_found
		mov es,ax
		mov edx,[es:0]
		cmp edx,'PXEN'
		jne .mismatch
		mov dx,[es:4]
		cmp dx,'V+'
		jne .mismatch
		movzx cx,byte [es:8]	; Length of structure
		cmp cl,26h		; Minimum length
		jb .mismatch
		xor ax,ax
		xor si,si
.checksum:	es lodsb
		add ah,al
		loop .checksum
		and ah,ah
		jnz .mismatch		; Checksum must == 0
.found:		mov bp,sp
		mov [bp+8],bx		; Save BX into stack frame
		mov ax,bx
		call writehex4
		call crlf
		clc
		ret
.not_found:	mov si,notfound_msg
		call writestr
		popad
		stc
		ret

;
; searchdir:
;
;	Open a TFTP connection to the server 
;
;	     On entry:
;		DS:DI	= filename
;	     If successful:
;		ZF clear
;		SI	= socket pointer
;		DX:AX	= file length in bytes
;	     If unsuccessful
;		ZF set
;

searchdir:
		mov si,di
		push bp
		mov bp,sp

		call allocate_socket
		jz near .error

		mov ax,PKT_RETRY	; Retry counter
		mov word [PktTimeout],PKT_TIMEOUT	; Initial timeout
	
.sendreq:	push ax			; [bp-2]  - Retry counter
		push si			; [bp-4]  - File name 
		push bx			; [bp-6]  - TFTP block
		mov bx,[bx]
		push bx			; [bp-8]  - TID (socket port no)

		mov eax,[ServerIP]	; Server IP
		mov [pxe_udp_write_pkt.sip],eax
		mov [pxe_udp_write_pkt.lport],bx
		mov ax,[ServerPort]
		mov [pxe_udp_write_pkt.rport],ax
		mov di,packet_buf
		mov [pxe_udp_write_pkt.buffer],di
		mov ax,TFTP_RRQ		; TFTP opcode
		stosw
		push si			; Add common prefix
		mov si,PathPrefix
		call strcpy
		dec di
		pop si
		call strcpy		; Filename
		mov si,tftp_tail
		mov cx,tftp_tail_len
		rep movsb
		sub di,packet_buf	; Get packet size
		mov [pxe_udp_write_pkt.buffersize],di

		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call far [PXENVEntry]
		jc near .failure
		cmp word [pxe_udp_write_pkt.status],byte 0
		jne near .failure

		;
		; Danger, Will Robinson!  We need to support timeout
		; and retry lest we just lost a packet...
		;

		; Packet transmitted OK, now we need to receive
.getpacket:	push word [PktTimeout]	; [bp-10]
		push word [BIOS_timer]	; [bp-12]

.pkt_loop:	mov bx,[bp-8]		; TID
		mov di,packet_buf
		mov [pxe_udp_read_pkt.buffer],di
		mov di,packet_buf_size
		mov [pxe_udp_read_pkt.buffersize],di
		mov eax,[MyIP]
		mov [pxe_udp_read_pkt.dip],eax
		mov [pxe_udp_read_pkt.lport],bx
		mov di,pxe_udp_read_pkt
		mov bx,PXENV_UDP_READ
		call far [PXENVEntry]
		and ax,ax
		jz .got_packet			; Wait for packet
.no_packet:
		mov dx,[BIOS_timer]
		cmp dx,[bp-12]
		je .pkt_loop
		mov [bp-12],dx
		dec word [bp-10]		; Timeout
		jnz .pkt_loop
		pop ax	; Adjust stack
		pop ax
		shl word [PktTimeout],1		; Exponential backoff
		jmp .failure
		
.got_packet:
		mov si,[bp-6]			; TFTP pointer
		mov bx,[bp-8]			; TID

		mov eax,[ServerIP]
		cmp [pxe_udp_read_pkt.sip],eax
		jne .no_packet
		mov [si+tftp_remoteip],eax

		; Got packet - reset timeout
		mov word [PktTimeout],PKT_TIMEOUT

		pop ax	; Adjust stack
		pop ax

		mov ax,[pxe_udp_read_pkt.rport]
		mov [si+tftp_remoteport],ax

		movzx eax,word [pxe_udp_read_pkt.buffersize]
		sub eax, byte 2
		jb near .failure		; Garbled reply

		cmp word [packet_buf], TFTP_ERROR
		je near .bailnow		; ERROR reply: don't try again

		cmp word [packet_buf], TFTP_OACK
		jne .err_reply

		; Now we need to parse the OACK packet to get the transfer
		; size.
.parse_oack:	mov cx,[pxe_udp_read_pkt.buffersize]
		mov si,packet_buf+2
		sub cx,byte 2
		jz .no_tsize			; No options acked
.get_opt_name:	mov di,si
		mov bx,si
.opt_name_loop:	lodsb
		and al,al
		jz .got_opt_name
		or al,20h			; Convert to lowercase
		stosb
		loop .opt_name_loop
		; We ran out, and no final null
		jmp short .err_reply
.got_opt_name:	dec cx
		jz .err_reply			; Option w/o value
		push cx
		mov si,bx
		mov di,tsize_str
		mov cx,tsize_len
		repe cmpsb
		pop cx
		jne .err_reply			; Bad option -> error
.get_value:	xor eax,eax
		xor edx,edx
.value_loop:	lodsb
		and al,al
		jz .got_value
		sub al,'0'
		cmp al, 9
		ja .err_reply
		imul edx,10
		add edx,eax
		loop .value_loop
		; Ran out before final null
		jmp short .err_reply
.got_value:	dec cx
		jnz .err_reply			; Not end of packet
		; Move size into DX:AX (old calling convention)
		; but let EAX == DX:AX
		mov eax,edx
		shr edx,16

		xor edi,edi		; ZF <- 1

		; Success, done!

		pop si			; Junk	
		pop si			; We want the packet ptr in SI

		mov [si+tftp_filesize],eax
		mov [si+tftp_filepos],edi

		inc di			; ZF <- 0
		pop bp			; Junk
		pop bp			; Junk (retry counter)
		pop bp
		ret

.err_reply:	; Option negotiation error.  Send ERROR reply.
		mov ax,[pxe_udp_read_pkt.rport]
		mov word [pxe_udp_write_pkt.rport],ax
		mov word [pxe_udp_write_pkt.buffer],tftp_opt_err
		mov word [pxe_udp_write_pkt.buffersize],tftp_opt_err_len
		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call far [PXENVEntry]

.no_tsize:	mov si,err_oldtftp
		call writestr
		jmp kaboom

.bailnow:	add sp,byte 8		; Immediate error - no retry
		jmp short .error

.failure:	pop bx			; Junk
		pop bx
		pop si
		pop ax
		dec ax			; Retry counter
		jnz near .sendreq	; Try again

.error:		xor si,si		; ZF <- 1
		pop bp
		ret

;
; allocate_socket: Allocate a local UDP port structure
;
;		If successful:
;		  ZF set
;		  BX     = socket pointer
;               If unsuccessful:
;                 ZF clear
;
allocate_socket:
		push cx
		mov bx,Sockets
		mov cx,MAX_SOCKETS
.check:		cmp word [bx], byte 0
		je .found
		add bx,tftp_port_t_size
		loop .check
		xor cx,cx			; ZF = 1
		pop cx
		ret
.found:		mov cx,[MySocket]
		inc cx
		and cx,0BFFFh			; Wrap 32768->49151 (ZF = 0)
		mov [MySocket],cx
		xchg ch,cl			; Convert to network byte order
		mov [bx],cx			; Socket in use
		pop cx
		ret

;
; strcpy: Copy DS:SI -> ES:DI up to and including a null byte
;
strcpy:		push ax
.loop:		lodsb
		stosb
		and al,al
		jnz .loop
		pop ax
		ret

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
bf_ret:		ret

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
		jne bf_ret

		mov al,[trackbuf+2]		; File mode
		cmp al,3			; Font modes 0-3 supported
		ja bf_ret

		mov bh,byte [trackbuf+3]	; Height of font
		cmp bh,2			; VGA minimum
		jb bf_ret
		cmp bh,32			; VGA maximum
		ja bf_ret

		mov bp,trackbuf+4		; Address of font data
		xor bl,bl
		mov cx,256
		xor dx,dx
		mov ax,1110h
		int 10h				; Load into VGA RAM

		xor bl,bl
		mov ax,1103h			; Select page 0
		int 10h

		jmp short adjust_screen

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
                mov word [NextCharJump],msg_putchar ; State machine for color
                mov byte [TextAttribute],07h	; Default grey on white
                pusha
                mov bh,[TextPage]
                mov ah,03h                      ; Read cursor position
                int 10h
                mov [CursorDX],dx
                popa
get_msg_chunk:  push ax                         ; DX:AX = length of file
                push dx
		mov bx,trackbuf
		mov cx,[BufSafe]
		call getfssec
                pop dx
                pop ax
		push si				; Save current cluster
		mov si,trackbuf
		mov cx,[BufSafeBytes]		; No more than many bytes
print_msg_file: push cx
                push ax
		push dx
		lodsb
                cmp al,1Ah                      ; ASCII EOF?
		je msg_done_pop
                call [NextCharJump]		; Do what shall be done
		pop dx
		pop ax
                pop cx
		sub ax,byte 1
		sbb dx,byte 0
		mov bx,ax
		or bx,dx
		jz msg_done
		loop print_msg_file
		pop si
		jmp short get_msg_chunk
msg_done_pop:
                add sp,byte 6			; Lose 3 words on the stack
msg_done:
		pop si
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

msg_normal:	call write_serial		; Write to serial port
                mov bx,[TextAttrBX]
                mov ah,09h                      ; Write character/attribute
                mov cx,1                        ; One character only
                int 10h                         ; Write to screen
                mov al,[CursorCol]
                inc ax
                cmp al,[VidCols]
                ja msg_newline
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
		push si
		mov si,crlf_msg
		call write_serial_str
		pop si
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
                mov bh,[TextAttribute]
                mov ax,0601h                    ; Scroll up one line
                int 10h
                jmp short msg_gotoxy
msg_formfeed:                                   ; Form feed character
		push si
		mov si,crff_msg
		call write_serial_str
		pop si
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
                mov [TextAttribute],al
                mov word [NextCharJump],msg_setfg
                ret
msg_setfg:                                      ; Color foreground character
                call unhexchar
                jc msg_color_bad
                or [TextAttribute],al		; setbg set foreground to 0
                mov word [NextCharJump],msg_putchar
                ret
msg_color_bad:
                mov byte [TextAttribute],07h	; Default attribute
                mov word [NextCharJump],msg_putchar
                ret

;
; write_serial:	If serial output is enabled, write character on serial port
;
write_serial:
		pusha
		mov bx,[SerialPort]
		and bx,bx
		je .noserial
		push ax
.waitspace:	lea dx,[bx+5]			; Wait for space in transmit reg
		in al,dx
		test al,20h
		jz .waitspace
		xchg dx,bx
		pop ax
		call slow_out			; Send data
.noserial:	popa
		ret

;
; write_serial_str: write_serial for strings
;
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
writechr:
		call write_serial	; write to serial port if needed
		pushad
		mov bh,[TextPage]
                mov ah,03h              ; Read cursor position
                int 10h
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
		popad
		ret
.scroll:	dec dh
		mov bh,[TextPage]
		mov ah,02h
		int 10h
		mov ax,0601h		; Scroll up one line
		mov bh,07h		; White on black
		xor cx,cx
		mov dx,[ScreenSize]	; The whole screen
		int 10h
		popad
		ret
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
; crlf: Print a newline
;
crlf:		mov si,crlf_msg
		; Fall through

;
; cwritestr: write a null-terminated string to the console, saving
;            registers on entry.
;
; Note: writestr and cwritestr are distinct in SYSLINUX, not in PXELINUX
;
cwritestr:
                pushad
.top:		lodsb
		and al,al
                jz .end
		call writechr
                jmp short .top
.end:		popad
                ret

writestr	equ cwritestr

;
; writehex[248]: Write a hex number in (AL, AX, EAX) to the console
;
writehex2:
		pushad
		rol eax,24
		mov cx,2
		jmp short writehex_common
writehex4:
		pushad
		rol eax,16
		mov cx,4
		jmp short writehex_common
writehex8:
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
		add dx,byte 5		; Serial status register
		in al,dx
		test al,1		; ZF = 0 if traffic
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
		lea dx,[bx+5]		; Serial status register
		in al,dx
		test al,1
		jz .again
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
;	       This verifies that a filename is < FILENAME_MAX characters
;	       and doesn't contain whitespace, and zero-pads the output buffer,
;	       so "repe cmpsb" can do a compare.
;
mangle_name:
		mov cx,FILENAME_MAX-1
.mn_loop:
		lodsb
		cmp al,' '			; If control or space, end
		jna .mn_end
		stosb
		loop .mn_loop
.mn_end:
		inc cx				; At least one null byte
		xor ax,ax			; Zero-fill name
		rep stosb			; Doesn't do anything if CX=0
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
; pxe_thunk
;
; Convert from the PXENV+ calling convention (BX, ES, DI) to the !PXE
; calling convention (using the stack.)
;
; This is called as a far routine so that we can just stick it into
; the PXENVEntry variable.
;
pxe_thunk:	push es
		push di
		push bx
		call far [cs:PXEEntry]
		add sp,byte 6
		cmp ax,byte 1
		cmc				; Set CF unless ax == 0
		retf

;
; getfssec: Get multiple clusters from a file, given the starting cluster.
;
;	In this case, get multiple blocks from a specific TCP connection.
;
;  On entry:
;	ES:BX	-> Buffer
;	SI	-> TFTP block pointer
;	CX	-> 512-byte block pointer; 0FFFFh = until end of file
;  On exit:
;	SI	-> TFTP block pointer (or 0 on EOF)
;	CF = 1	-> Hit EOF
;
getfssec:

.packet_loop:	push cx				; <A> Save count
		push es				; <B> Save buffer pointer
		push bx				; <C> Block pointer
	
		mov ax,ds
		mov es,ax

		; Start by ACKing the previous packet; this should cause the
		; next packet to be sent.
		mov cx,PKT_RETRY
		mov word [PktTimeout],PKT_TIMEOUT

.send_ack:	push cx				; <D> Retry count

		mov eax,[si+tftp_filepos]
		shr eax,LOG_TFTP_BLOCKSIZE
		xchg ah,al			; Network byte order
		call ack_packet			; Send ACK

		; We used to test the error code here, but sometimes
		; PXE would return negative status even though we really
		; did send the ACK.  Now, just treat a failed send as
		; a normally lost packet, and let it time out in due
		; course of events.

.send_ok:	; Now wait for packet.
		mov dx,[BIOS_timer]		; Get current time

		mov cx,[PktTimeout]
.wait_data:	push cx				; <E> Timeout
		push dx				; <F> Old time

		mov bx,packet_buf
		mov [pxe_udp_read_pkt.buffer],bx
		mov [pxe_udp_read_pkt.buffersize],word packet_buf_size
		mov eax,[bx+tftp_remoteip]
		mov [pxe_udp_read_pkt.sip],eax
		mov eax,[MyIP]
		mov [pxe_udp_read_pkt.dip],eax
		mov ax,[si+tftp_remoteport]
		mov [pxe_udp_read_pkt.rport],ax
		mov ax,[si+tftp_localport]
		mov [pxe_udp_read_pkt.lport],ax
		mov di,pxe_udp_read_pkt
		mov bx,PXENV_UDP_READ
		push si				; <G>
		call far [PXENVEntry]
		pop si				; <G>
		cmp ax,byte 0
		je .recv_ok

		; No packet, or receive failure
		mov dx,[BIOS_timer]
		pop ax				; <F> Old time
		pop cx				; <E> Timeout
		cmp ax,dx			; Same time -> don't advance timeout
		je .wait_data			; Same clock tick
		loop .wait_data			; Decrease timeout
		
		pop cx				; <D> Didn't get any, send another ACK
		shl word [PktTimeout],1		; Exponential backoff
		loop .send_ack
		jmp kaboom			; Forget it...

.recv_ok:	pop dx				; <F>
		pop cx				; <E>

		cmp word [pxe_udp_read_pkt.buffersize],byte 4
		jb .wait_data			; Bad size for a DATA packet

		cmp word [packet_buf],TFTP_DATA	; Not a data packet?
		jne .wait_data			; Then wait for something else

		mov eax,[si+tftp_filepos]
		shr eax,LOG_TFTP_BLOCKSIZE
		inc ax				; Which packet are we waiting for?
	
		xchg ah,al			; Network byte order
		cmp word [packet_buf+2],ax
		je .right_packet

		; Wrong packet, ACK the packet and then try again
		; This is presumably because the ACK got lost,
		; so the server just resent the previous packet
		mov ax,[packet_buf+2]
		call ack_packet
		jmp .send_ok			; Reset timeout

.right_packet:	; It's the packet we want.  We're also EOF if the size < 512.
		pop cx				; <D> Don't need the retry count anymore
		movzx ecx,word [pxe_udp_read_pkt.buffersize]
		sub cx,byte 4
		add [si+tftp_filepos],ecx

		cmp cx,TFTP_BLOCKSIZE		; Is it a full block
		jb .last_block

		pop di				; <C> Get target buffer
		pop es				; <B>

		cld
		push si
		mov si,packet_buf+4
		mov cx,TFTP_BLOCKSIZE >> 2
		rep movsd
		mov bx,di
		pop si

		pop cx				; <A>
		loop .packet_loop_jmp

		; If we had the exact right number of bytes, always get
		; one more packet to get the (zero-byte) EOF packet and
		; close the socket.
		mov eax,[si+tftp_filepos]
		cmp [si+tftp_filesize],eax
		je .packet_loop_jmp

		clc				; Not EOF
		ret				; Mission accomplished

.packet_loop_jmp: jmp .packet_loop

.last_block:	; Last block - ACK packet immediately and free socket
		mov ax,[packet_buf+2]
		call ack_packet
		mov word [si],0			; Socket closed
	
		; Copy data
		pop di				; <C>
		pop es				; <B>

		cld
		mov si,packet_buf+4
		rep movsb
		mov bx,di

		xor si,si
		
		pop cx				; <A> Not used
		stc				; EOF
		ret

;
; ack_packet:
;
; Send ACK packet.  This is a common operation and so is worth canning.
;
; Entry:
;	SI 	= TFTP block
;	AX 	= Packet # to ack (network byte order)
; Exit:
;	ZF = 0 -> Error
;	All registers preserved
;
; This function uses the pxe_udp_write_pkt but not the packet_buf.
;
ack_packet:
		pushad
		mov [ack_packet_buf+2],ax	; Packet number to ack
		mov ax,[si]
		mov [pxe_udp_write_pkt.lport],ax
		mov ax,[si+tftp_remoteport]
		mov [pxe_udp_write_pkt.rport],ax
		mov eax,[si+tftp_remoteip]
		mov [pxe_udp_write_pkt.sip],eax
		mov [pxe_udp_write_pkt.buffer],word ack_packet_buf
		mov [pxe_udp_write_pkt.buffersize], word 4
		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call far [PXENVEntry]
		cmp ax,byte 0			; ZF = 1 if write OK
		popad
		ret

;
; unload_pxe:
;
; This function unloads the PXE and UNDI stacks.
;
unload_pxe:
		mov di,pxe_udp_close_pkt
		mov bx,PXENV_UDP_CLOSE
		call far [PXENVEntry]
		mov di,pxe_undi_shutdown_pkt
		mov bx,PXENV_UNDI_SHUTDOWN
		call far [PXENVEntry]
		mov di,pxe_unload_stack_pkt
		mov bx,PXENV_UNLOAD_STACK
		call far [PXENVEntry]
		ret

; ----------------------------------------------------------------------------------
;  Begin data section
; ----------------------------------------------------------------------------------

CR		equ 13		; Carriage Return
LF		equ 10		; Line Feed
FF		equ 12		; Form Feed
BS		equ  8		; Backspace

copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
boot_prompt	db 'boot: ', 0
wipe_char	db 08h, ' ', 08h, 0
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
err_bootfailed	db CR, LF, 'Boot failed: press a key to retry, or wait for reset...', CR, LF, 0
bailmsg		equ err_bootfailed
err_nopxe	db "No !PXE or PXENV+ API found; we're dead...", CR, LF, 0
err_pxefailed	db 'PXE API call failed, error ', 0
err_udpinit	db 'Failed to initialize UDP stack', CR, LF, 0
err_oldtftp	db 'TFTP server does not support the tsize option', CR, LF, 0
found_pxenv	db 'Found PXENV+ structure', CR, LF, 0
using_pxenv_msg db 'Old PXE API detected, using PXENV+ structure', CR, LF, 0
apiver_str	db 'PXE API version is ',0
pxeentry_msg	db 'PXE entry point found (we hope) at ', 0
pxenventry_msg	db 'PXENV entry point found (we hope) at ', 0
trymempxe_msg	db 'Scanning memory for !PXE structure... ', 0
trymempxenv_msg	db 'Scanning memory for PXENV+ structure... ', 0
notfound_msg	db 'not found', CR, LF, 0
myipaddr_msg	db 'My IP address seems to be ',0
tftpprefix_msg	db 'TFTP prefix: ', 0
cmdline_msg	db 'Command line: ', CR, LF, 0
ready_msg	db ' ready.', CR, LF, 0
trying_msg	db 'Trying to load: ', 0
loading_msg     db 'Loading ', 0
dotdot_msg      db '.'
dot_msg         db '.', 0
fourbs_msg	db BS, BS, BS, BS, 0
aborted_msg	db ' aborted.'			; Fall through to crlf_msg!
crlf_msg	db CR, LF, 0
crff_msg	db CR, FF, 0
default_str	db 'default', 0
default_len	equ ($-default_str)
pxelinux_banner	db CR, LF, 'PXELINUX ', version_str, ' ', date, ' ', 0
cfgprefix	db 'pxelinux.cfg/'		; No final null!
cfgprefix_len	equ ($-cfgprefix)

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
		dw 0
;
; Extensions to search for (in *forward* order).
;
		align 4, db 0
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.bss'		; Boot Sector (add superblock)
		db '.bs', 0		; Boot Sector 
		db '.com'		; COMBOOT (same as DOS)
exten_table_end:
		dd 0, 0			; Need 8 null bytes here

;
; PXENV entry point.  If we use the !PXE API, this will point to a thunk
; function that converts to the !PXE calling convention.
;
PXENVEntry	dw pxe_thunk,0

;
; PXE query packets partially filled in
;
pxe_bootp_query_pkt:
.status:	dw 0			; Status
.packettype:	dw 2			; DHCPACK packet
.buffersize:	dw trackbufsize		; Packet size
.buffer:	dw trackbuf, 0		; seg:off of buffer
.bufferlimit:	dw trackbufsize		; Unused

pxe_bootp_size_query_pkt:
.status:	dw 0			; Status
.packettype:	dw 2			; DHCPACK packet
.buffersize:	dw 0			; Packet size
.buffer:	dw 0, 0			; seg:off of buffer
.bufferlimit:	dw 0			; Unused

pxe_udp_open_pkt:
.status:	dw 0			; Status
.sip:		dd 0			; Source (our) IP

pxe_udp_close_pkt:
.status:	dw 0			; Status

pxe_udp_write_pkt:
.status:	dw 0			; Status
.sip:		dd 0			; Server IP
.gip:		dd 0			; Gateway IP
.lport:		dw 0			; Local port
.rport:		dw 0			; Remote port
.buffersize:	dw 0			; Size of packet
.buffer:	dw 0, 0			; seg:off of buffer

pxe_udp_read_pkt:
.status:	dw 0			; Status
.sip:		dd 0			; Source IP
.dip:		dd 0			; Destination (our) IP
.rport:		dw 0			; Remote port
.lport:		dw 0			; Local port
.buffersize:	dw 0			; Max packet size
.buffer:	dw 0, 0			; seg:off of buffer

pxe_unload_stack_pkt:
.status:	dw 0			; Status
.reserved:	times 10 db 0		; Reserved

pxe_undi_shutdown_pkt:
.status:	dw 0			; Status

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
MySocket	dw 32768		; Local UDP socket counter
A20List		dw a20_dunno, a20_none, a20_bios, a20_kbc, a20_fast
A20DList	dw a20d_dunno, a20d_none, a20d_bios, a20d_kbc, a20d_fast
A20Type		dw A20_DUNNO		; A20 type unknown

;
; TFTP commands
;
tftp_tail	db 'octet', 0, 'tsize' ,0, '0', 0	; Octet mode, request size
tftp_tail_len	equ ($-tftp_tail)
tsize_str	db 'tsize', 0
tsize_len	equ ($-tsize_str)
tftp_opt_err	dw TFTP_ERROR				; ERROR packet
		dw htons(8)				; ERROR 8: bad options
		db 'tsize option required', 0		; Error message
tftp_opt_err_len equ ($-tftp_opt_err)

ack_packet_buf:	dw TFTP_ACK, 0				; TFTP ACK packet

;
; Variables that are uninitialized in SYSLINUX but initialized here
;
ClustSize	dw TFTP_BLOCKSIZE	; Bytes/cluster
SecPerClust	dw TFTP_BLOCKSIZE/512	; Same as bsSecPerClust, but a word
BufSafe		dw trackbufsize/TFTP_BLOCKSIZE	; Clusters we can load into trackbuf
BufSafeSec	dw trackbufsize/512	; = how many sectors?
BufSafeBytes	dw trackbufsize		; = how many bytes?
EndOfGetCBuf	dw getcbuf+trackbufsize	; = getcbuf+BufSafeBytes
ClustPerMoby	dw 65536/TFTP_BLOCKSIZE	; Clusters per 64K
%if ( trackbufsize % TFTP_BLOCKSIZE ) != 0
%error trackbufsize must be a multiple of TFTP_BLOCKSIZE
%endif

;
; Stuff for the command line; we do some trickery here with equ to avoid
; tons of zeros appended to our file and wasting space
;
linuxauto_cmd	db 'linux '
auto_cmd	db 'auto',0
linuxauto_len   equ $-linuxauto_cmd
auto_len        equ $-auto_cmd
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
