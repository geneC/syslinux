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
;   Copyright (C) 1994-2002  H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;  USA; either version 2 of the License, or (at your option) any later
;  version; incorporated herein by reference.
; 
; ****************************************************************************

%define IS_PXELINUX 1
%include "macros.inc"
%include "config.inc"
%include "kernel.inc"
%include "bios.inc"
%include "tracers.inc"
%include "pxe.inc"

;
; Some semi-configurable constants... change on your own risk.
;
my_id		equ pxelinux_id
FILENAME_MAX_LG2 equ 6			; log2(Max filename size Including final null)
FILENAME_MAX	equ (1 << FILENAME_MAX_LG2)
NULLFILE	equ 0			; Zero byte == null file name
REBOOT_TIME	equ 5*60		; If failure, time until full reset
%assign HIGHMEM_SLOP 128*1024		; Avoid this much memory near the top
MAX_SOCKETS_LG2	equ 6			; log2(Max number of open sockets)
MAX_SOCKETS	equ (1 << MAX_SOCKETS_LG2)
TFTP_PORT	equ htons(69)		; Default TFTP port 
PKT_RETRY	equ 6			; Packet transmit retry count
PKT_TIMEOUT	equ 12			; Initial timeout, timer ticks @ 55 ms
TFTP_BLOCKSIZE_LG2 equ 9		; log2(bytes/block)
TFTP_BLOCKSIZE	equ (1 << TFTP_BLOCKSIZE_LG2)

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
%define	year		'2002'

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
vk_ipappend:	resb 1			; "IPAPPEND" flag
		resb 1			; Pad
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
; 0000h - main code/data segment (and BIOS segment)
;
real_mode_seg	equ 5000h
vk_seg          equ 4000h		; Virtual kernels
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
.option_magic	resd 1			; Vendor option magic cookie
.options	resb 1260		; Vendor options
		endstruc	

BOOTP_OPTION_MAGIC	equ htonl(0x63825363)	; See RFC 2132

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

%ifndef DEPEND
%if (tftp_port_t_size & (tftp_port_t_size-1))
%error "tftp_port_t is not a power of 2"
%endif
%endif

; ---------------------------------------------------------------------------
;   BEGIN CODE
; ---------------------------------------------------------------------------

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

                absolute 5000h          ; Here we keep our BSS stuff
VKernelBuf:	resb vk_size		; "Current" vkernel
		alignb 4
AppendBuf       resb max_cmd_len+1	; append=
KbdMap		resb 256		; Keyboard map
BootFile	resb 256		; Boot file from DHCP packet
PathPrefix	resb 256		; Path prefix derived from the above
ConfigName	resb 256		; Configuration file from DHCP option
FKeyName	resb 10*FILENAME_MAX	; File names for F-key help
NumBuf		resb 15			; Buffer to load number
NumBufEnd	resb 1			; Last byte in NumBuf
DotQuadBuf	resb 16			; Buffer for dotted-quad IP address
IPOption	resb 80			; ip= option buffer
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
SavedSSSP	resd 1			; Our SS:SP while running a COMBOOT image
PMESP		resd 1			; Protected-mode ESP
InitStack	resd 1			; Pointer to reset stack
PXEEntry	resd 1			; !PXE API entry point
RebootTime	resd 1			; Reboot timeout, if set by option
KernelClust	resd 1			; Kernel size in clusters
StrucPtr	resd 1			; Pointer to PXENV+ or !PXE structure
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
A20Type		resw 1			; A20 type
CmdLineLen	resw 1			; Length of command line including null
GraphXSize	resw 1			; Width of splash screen file
VGAPos		resw 1			; Pointer into VGA memory
VGACluster	resw 1			; Cluster pointer for VGA image file
VGAFilePtr	resw 1			; Pointer into VGAFileBuf
ConfigFile	resw 1			; Socket for config file
PktTimeout	resw 1			; Timeout for current packet
KernelExtPtr	resw 1			; During search, final null pointer
IPOptionLen	resw 1			; Length of IPOption
LocalBootType	resw 1			; Local boot return code
RealBaseMem	resw 1			; Amount of DOS memory after freeing
APIVer		resw 1			; PXE API version found
TextAttrBX      equ $
TextAttribute   resb 1			; Text attribute for message file
TextPage        resb 1			; Active display page
CursorDX        equ $
CursorCol       resb 1			; Cursor column for message file
CursorRow       resb 1			; Cursor row for message file
ScreenSize      equ $
VidCols         resb 1			; Columns on screen-1
VidRows         resb 1			; Rows on screen-1
BaudDivisor	resw 1			; Baud rate divisor
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
OverLoad	resb 1			; Set if DHCP packet uses "overloading"
TextColorReg	resb 17			; VGA color registers for text mode
VGAFileBuf	resb FILENAME_MAX	; Unmangled VGA image name
VGAFileBufEnd	equ $
VGAFileMBuf	resb FILENAME_MAX	; Mangled VGA image name

;
; PXE packets which don't need static initialization
;
		alignb 4
pxe_unload_stack_pkt:
.status:	resw 1			; Status
.reserved:	resw 10			; Reserved
pxe_unload_stack_pkt_len	equ $-pxe_unload_stack_pkt

		alignb tftp_port_t_size
Sockets		resb MAX_SOCKETS*tftp_port_t_size

		alignb 16
		; BOOTP/DHCP packet buffer

		alignb 16
packet_buf	resb 2048		; Transfer packet
packet_buf_size	equ $-packet_buf

		section .text
                org 7C00h
StackBuf	equ $

;
; Primary entry point.
;
bootsec		equ $
_start:
		jmp 0:_start1		; Canonicalize address
_start1:
		pushad			; Paranoia... in case of return to PXE
		pushfd			; ... save as much state as possible
		push ds
		push es
		push fs
		push gs

		mov bp,sp
		les bx,[bp+48]		; ES:BX -> !PXE or PXENV+ structure

		mov ax,cs
		mov ds,ax

		; That is all pushed onto the PXE stack.  Save the pointer
		; to it and switch to an internal stack.
		mov [InitStack],sp
		mov [InitStack+2],ss

		cli			; Paranoia
		mov ss,ax
		mov sp,StackBuf

		sti			; Stack set up and ready
		cld			; Copy upwards

;
; Initialize screen (if we're using one)
;
		; Now set up screen parameters
		call adjust_screen

		; Wipe the F-key area
		mov al,NULLFILE
		mov di,FKeyName
		mov cx,10*(1 << FILENAME_MAX_LG2)
		push es			; Save ES -> PXE structure
		push ds			; ES <- DS
		pop es
		rep stosb
		pop es			; Restore ES

;
; Tell the user we got this far
;
		mov si,syslinux_banner
		call writestr

		mov si,copyright_str
		call writestr

;
; Assume API version 2.1, in case we find the !PXE structure without
; finding the PXENV+ structure.  This should really look at the Base
; Code ROM ID structure in have_pxe, but this is adequate for now --
; if we have !PXE, we have to be 2.1 or higher, and we don't care
; about higher versions than that.
;
		mov word [APIVer],0201h

;
; Now we need to find the !PXE structure.  It's *supposed* to be pointed
; to by SS:[SP+4], but support INT 1Ah, AX=5650h method as well.
;
		cmp dword [es:bx], '!PXE'
		je have_pxe

		; Uh-oh, not there... try plan B
		mov ax, 5650h
		int 1Ah
		jc no_pxe
		cmp ax,564Eh
		jne no_pxe

		; Okay, that gave us the PXENV+ structure, find !PXE
		; structure from that (if available)
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
		mov [StrucPtr],bx
		mov [StrucPtr+2],es

		mov si,found_pxenv
		call writestr

		mov si,apiver_str
		call writestr
		mov ax,[es:bx+6]
		mov [APIVer],ax
		call writehex4
		call crlf

		cmp ax,0201h			; API version 2.1 or higher
		jb old_api
		mov si,bx
		mov ax,es
		les bx,[es:bx+28h]		; !PXE structure pointer
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

		mov si,undi_data_msg		; ***
		call writestr
		mov ax,[es:bx+20h]
		call writehex4
		call crlf
		mov si,undi_data_len_msg
		call writestr
		mov ax,[es:bx+22h]
		call writehex4
		call crlf
		mov si,undi_code_msg
		call writestr
		mov ax,[es:bx+24h]
		call writehex4
		call crlf
		mov si,undi_code_len_msg
		call writestr
		mov ax,[es:bx+26h]
		call writehex4
		call crlf

		; Compute base memory size from PXENV+ structure
		xor esi,esi
		movzx eax,word [es:bx+20h]	; UNDI data seg
		cmp ax,[es:bx+24h]		; UNDI code seg
		ja .use_data
		mov ax,[es:bx+24h]
		mov si,[es:bx+26h]
		jmp short .combine
.use_data:
		mov si,[es:bx+22h]
.combine:
		shl eax,4
		add eax,esi
		shr eax,10			; Convert to kilobytes
		mov [RealBaseMem],ax

		mov si,pxenventry_msg
		call writestr
		mov ax,[PXENVEntry+2]
		call writehex4
		mov al,':'
		call writechr
		mov ax,[PXENVEntry]
		call writehex4
		call crlf
		jmp have_entrypoint

have_pxe:
		mov [StrucPtr],bx
		mov [StrucPtr+2],es

		mov eax,[es:bx+10h]
		mov [PXEEntry],eax

		mov si,undi_data_msg		; ***
		call writestr
		mov eax,[es:bx+2Ah]
		call writehex8
		call crlf
		mov si,undi_data_len_msg
		call writestr
		mov ax,[es:bx+2Eh]
		call writehex4
		call crlf
		mov si,undi_code_msg
		call writestr
		mov ax,[es:bx+32h]
		call writehex8
		call crlf
		mov si,undi_code_len_msg
		call writestr
		mov ax,[es:bx+36h]
		call writehex4
		call crlf

		; Compute base memory size from !PXE structure
		xor esi,esi
		mov eax,[es:bx+2Ah]
		cmp eax,[es:bx+32h]
		ja .use_data
		mov eax,[es:bx+32h]
		mov si,[es:bx+36h]
		jmp short .combine
.use_data:
		mov si,[es:bx+2Eh]
.combine:
		add eax,esi
		shr eax,10
		mov [RealBaseMem],ax

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
		push es		; Save ES -> PXE structure
		push ds		; ES <- DS
		pop es
		rep stosd
		pop es

;
; Now attempt to get the BOOTP/DHCP packet that brought us life (and an IP
; address).  This lives in the DHCPACK packet (query info 2).
;
query_bootp:
		mov ax,ds
		mov es,ax
		mov di,pxe_bootp_query_pkt_2
		mov bx,PXENV_GET_CACHED_INFO

		call far [PXENVEntry]
		push word [pxe_bootp_query_pkt_2.status]
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
		pop ax				; Status
		call writehex4
		call crlf
		jmp kaboom			; We're dead

.pxe_ok:
		pop cx				; Forget status
		mov cx,[pxe_bootp_query_pkt_2.buffersize]
		call parse_dhcp			; Parse DHCP packet

;
; Now, get the boot file and other info.  This lives in the CACHED_REPLY
; packet (query info 3).
;
		mov [pxe_bootp_size_query_pkt.packettype], byte 3

		mov di,pxe_bootp_query_pkt_3
		mov bx,PXENV_GET_CACHED_INFO

		call far [PXENVEntry]
		push word [pxe_bootp_query_pkt_3.status]
		jc .pxe_err1
		cmp ax,byte 0
		jne .pxe_err1

		; Packet loaded OK...
		pop cx				; Forget status
		mov cx,[pxe_bootp_query_pkt_3.buffersize]
		call parse_dhcp			; Parse DHCP packet
;
; Generate ip= option
;
		call genipopt

;
; Print IP address
;
		mov eax,[MyIP]
		mov di,DotQuadBuf
		push di
		call gendotquad			; This takes network byte order input

		xchg ah,al			; Convert to host byte order
		ror eax,16			; (BSWAP doesn't work on 386)
		xchg ah,al

		mov si,myipaddr_msg
		call writestr
		call writehex8
		mov al,' '
		call writechr
		pop si				; DotQuadBuf
		call writestr
		call crlf

		mov si,IPOption			; ***
		call writestr			; ***
		call crlf			; ***

;
; Check to see if we got any PXELINUX-specific DHCP options; in particular,
; if we didn't get the magic enable, do not recognize any other options.
;
check_dhcp_magic:
		test byte [DHCPMagic], 1	; If we didn't get the magic enable...
		jnz .got_magic
		mov byte [DHCPMagic], 0		; If not, kill all other options
.got_magic:
	

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
; Common initialization code
;
%include "cpuinit.inc"

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
prefix:		test byte [DHCPMagic], 04h	; Did we get a path prefix option
		jnz .got_prefix
		mov si,BootFile
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
.got_prefix:
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
		xchg ah,al			; Convert to host byte order
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
		test byte [DHCPMagic], 02h
		jz .no_option

		; We got a DHCP option, try it first
		push di
		mov si,trying_msg
		call writestr
		mov di,ConfigName
		mov si,di
		call writestr
		call crlf
		call open
		pop di
		jnz .success

.no_option:		; Have to guess config file name

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

.success:	add sp,byte 16			; Adjust stack

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"

;
; Linux kernel loading code is common.  However, we need to define
; a couple of helper macros...
;

; Handle "ipappend" option
%define HAVE_SPECIAL_APPEND
%macro	SPECIAL_APPEND 0
		mov al,[IPAppend]		; ip=
		and al,al
		jz .noipappend
		mov si,IPOption
		mov cx,[IPOptionLen]
		rep movsb
		mov al,' '
		stosb
.noipappend:
%endmacro

; Unload PXE stack
%define HAVE_UNLOAD_PREP
%macro	UNLOAD_PREP 0
		call unload_pxe
%endmacro

%include "runkernel.inc"

;
; COMBOOT-loading code
;
%include "comboot.inc"
%include "com32.inc"

;
; Boot sector loading code
;
%include "bootsect.inc"

;
; Boot to the local disk by returning the appropriate PXE magic.
; AX contains the appropriate return code.
;
local_boot:
		mov si,cs
		mov ds,si			; Restore DI
		mov ss,si
		mov esp,StackBuf		; Reset the stack
		mov [LocalBootType],ax
		call vgaclearmode
		mov si,localboot_msg
		call writestr
		; Restore the environment we were called with
		lss sp,[InitStack]
		pop gs
		pop fs
		pop es
		pop ds
		popfd
		popad
		mov ax,[cs:LocalBootType]
		retf				; Return to PXE

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
		mov ss,ax
		mov sp,StackBuf			; Reset the stack
		sti
                call cwritestr                  ; Expects SI -> error msg
al_ok:          jmp enter_command               ; Return to command prompt
;
; End of abort_check
;
ac_ret2:	popa
ac_ret1:	ret


;
; kaboom: write a message and bail out.  Wait for quite a while,
;	  or a user keypress, then do a hard reboot.
;
kaboom:
		mov ax,cs
		mov es,ax
		mov ds,ax
		mov ss,ax
		mov sp,StackBuf
		sti
.patch:		mov si,bailmsg
		call writestr		; Returns with AL = 0
.drain:		call pollchar
		jz .drained
		call getchar
		jmp short .drain
.drained:
		mov edi,[RebootTime]
		mov al,[DHCPMagic]
		and al,09h		; Magic+Timeout
		cmp al,09h
		je .time_set
		mov edi,REBOOT_TIME
.time_set:
		mov cx,18
.wait1:		push cx
		mov ecx,edi
.wait2:		mov dx,[BIOS_timer]
.wait3:		call pollchar
		jnz .keypress
		cmp dx,[BIOS_timer]
		je .wait3
		loop .wait2,ecx
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
		mov ax,cs
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
		jz .error

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
		jc .failure
		cmp word [pxe_udp_write_pkt.status],byte 0
		jne .failure

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
		jb .failure		; Garbled reply

		cmp word [packet_buf], TFTP_ERROR
		je .bailnow		; ERROR reply: don't try again

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
		jnz .sendreq	; Try again

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
		; Allocate a socket number.  Socket numbers are made
		; guaranteed unique by including the socket slot number
		; (inverted, because we use the loop counter cx); add a
		; counter value to keep the numbers from being likely to
		; get immediately reused.
		;
		; The NextSocket variable also contains the top two bits
		; set.  This generates a value in the range 49152 to
		; 57343.
.found:
		dec cx
		push ax
		mov ax,[NextSocket]
		inc ax
		and ax,((1 << (13-MAX_SOCKETS_LG2))-1) | 0xC000
		mov [NextSocket],ax
		shl cx,13-MAX_SOCKETS_LG2
		add cx,ax			; ZF = 0
		xchg ch,cl			; Convert to network byte order
		mov [bx],cx			; Socket in use
		pop ax
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
; writechr:	Write a single character in AL to the console without
;		mangling any registers.  This does raw console writes,
;		since some PXE BIOSes seem to interfere regular console I/O.
;
writechr:
		push ds
		push cs
		pop ds
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
		pop ds
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
		shr eax,TFTP_BLOCKSIZE_LG2
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
		shr eax,TFTP_BLOCKSIZE_LG2
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
; This function unloads the PXE and UNDI stacks and unclaims
; the memory.  Assumes CS == DS == ES.
;
unload_pxe:
		test byte [KeepPXE],01h		; Should we keep PXE around?
		jnz reset_pxe

		mov si,new_api_unload
		cmp byte [APIVer+1],2		; Major API version >= 2?
		jae .new_api
		mov si,old_api_unload
.new_api:
		
.call_loop:	xor ax,ax
		lodsb
		and ax,ax
		jz .call_done
		xchg bx,ax
		mov di,pxe_unload_stack_pkt
		push di
		xor ax,ax
		mov cx,pxe_unload_stack_pkt_len >> 1
		rep stosw
		pop di
		call far [PXENVEntry]
		jc .cant_free
		cmp word [pxe_unload_stack_pkt.status],PXENV_STATUS_SUCCESS
		jne .cant_free
		jmp .call_loop

.call_done:
		mov bx,0FF00h

		mov dx,[RealBaseMem]
		cmp dx,[BIOS_fbm]		; Sanity check
		jna .cant_free
		inc bx

		; Check that PXE actually unhooked the INT 1Ah chain
		movzx eax,word [4*0x1a]
		movzx ecx,word [4*0x1a+2]
		shl ecx,4
		add eax,ecx
		shr eax,10
		cmp ax,dx			; Not in range
		jae .ok
		cmp ax,[BIOS_fbm]
		jae .cant_free
		; inc bx

.ok:
		mov [BIOS_fbm],dx
		ret
		
.cant_free:
		mov si,cant_free_msg
		call writestr
		xchg ax,bx
		call writehex4
		mov al,'-'
		call writechr
		mov eax,[4*0x1a]
		call writehex8
		jmp crlf

		; We want to keep PXE around, but still we should reset
		; it to the standard bootup configuration
reset_pxe:
		mov bx,PXENV_UDP_CLOSE
		mov di,pxe_udp_close_pkt
		call far [PXENVEntry]
		ret

;
; gendotquad
;
; Take an IP address (in network byte order) in EAX and
; output a dotted quad string to ES:DI.
; DI points to terminal null at end of string on exit.
;
; CX is destroyed.
;
gendotquad:
		push eax
		mov cx,4
.genchar:
		push eax
		aam 100
		; Now AH = 100-digit; AL = remainder
		cmp ah, 0
		je .lt100
		add ah,'0'
		mov [es:di],ah
		inc di
		aam 10
		; Now AH = 10-digit; AL = remainder
		jmp short .tendigit
.lt100:
		aam 10
		; Now AH = 10-digit; AL = remainder
		cmp ah, 0
		je .lt10
.tendigit:
		add ah,'0'
		mov [es:di],ah		
		inc di
.lt10:
		add al,'0'
		stosb
		mov al,'.'
		stosb
		pop eax
		ror eax,8	; Move next char into LSB
		loop .genchar
		dec di
		mov [es:di], byte 0
		pop eax
		ret

;
; parse_dhcp
;
; Parse a DHCP packet.  This includes dealing with "overloaded"
; option fields (see RFC 2132, section 9.3)
;
; This should fill in the following global variables, if the
; information is present:
;
; MyIP		- client IP address
; ServerIP	- boot server IP address
; Netmask	- network mask
; Gateway	- default gateway router IP
; BootFile	- boot file name
;
; This assumes the DHCP packet is in "trackbuf" and the length
; of the packet in in CX on entry.
;

parse_dhcp:
		mov byte [OverLoad],0		; Assume no overload
		mov eax, [trackbuf+bootp.yip]
		and eax, eax
		jz .noyip
		cmp al,224			; Class D or higher -> bad
		jae .noyip
		mov [MyIP], eax
.noyip:
		mov eax, [trackbuf+bootp.sip]
		and eax, eax
		jz .nosip
		cmp al,224			; Class D or higher -> bad
		jae .nosip
		mov [ServerIP], eax
.nosip:
		sub cx, bootp.options
		jbe .nooptions
		mov si, trackbuf+bootp.option_magic
		lodsd
		cmp eax, BOOTP_OPTION_MAGIC
		jne .nooptions
		call parse_dhcp_options
.nooptions:
		mov si, trackbuf+bootp.bootfile
		test byte [OverLoad],1
		jz .nofileoverload
		mov cx,128
		call parse_dhcp_options
		jmp short .parsed_file
.nofileoverload:
		cmp byte [si], 0
		jz .parsed_file			; No bootfile name
		mov di,BootFile
		mov cx,32
		rep movsd
		xor al,al
		stosb				; Null-terminate
.parsed_file:
		mov si, trackbuf+bootp.sname
		test byte [OverLoad],2
		jz .nosnameoverload
		mov cx,64
		call parse_dhcp_options
.nosnameoverload:
		ret

;
; Parse a sequence of DHCP options, pointed to by DS:SI; the field
; size is CX -- some DHCP servers leave option fields unterminated
; in violation of the spec.
;
parse_dhcp_options:
.loop:
		and cx,cx
		jz .done

		lodsb
		dec cx
		jz .done	; Last byte; must be PAD, END or malformed
		cmp al, 0	; PAD option
		je .loop
		cmp al,255	; END option
		je .done

		; Anything else will have a length field
		mov dl,al	; DL <- option number
		xor ax,ax
		lodsb		; AX <- option length
		dec cx
		sub cx,ax	; Decrement bytes left counter
		jb .done	; Malformed option: length > field size

		cmp dl,1	; SUBNET MASK option
		jne .not_subnet
		mov edx,[si]
		mov [Netmask],edx
		jmp .opt_done
.not_subnet:

		cmp dl,3	; ROUTER option
		jne .not_router
		mov edx,[si]
		mov [Gateway],edx
		jmp .opt_done
.not_router:

		cmp dl,52	; OPTION OVERLOAD option
		jne .not_overload
		mov dl,[si]
		mov [OverLoad],dl
		jmp .opt_done
.not_overload:

		cmp dl,67	; BOOTFILE NAME option
		jne .not_bootfile
		mov di,BootFile
		jmp short .copyoption
.done:
		ret		; This is here to make short jumps easier
.not_bootfile:

		cmp dl,208	; PXELINUX MAGIC option
		jne .not_pl_magic
		cmp al,4	; Must have length == 4
		jne .opt_done
		cmp dword [si], htonl(0xF100747E)	; Magic number
		jne .opt_done
		or byte [DHCPMagic], byte 1		; Found magic #
		jmp short .opt_done
.not_pl_magic:

		cmp dl,209	; PXELINUX CONFIGFILE option
		jne .not_pl_config
		mov di,ConfigName
		or byte [DHCPMagic], byte 2	; Got config file
		jmp short .copyoption
.not_pl_config:

		cmp dl,210	; PXELINUX PATHPREFIX option
		jne .not_pl_prefix
		mov di,PathPrefix
		or byte [DHCPMagic], byte 4	; Got path prefix
		jmp short .copyoption
.not_pl_prefix:

		cmp dl,211	; PXELINUX REBOOTTIME option
		jne .not_pl_timeout
		cmp al,4
		jne .opt_done
		mov edx,[si]
		xchg dl,dh	; Convert to host byte order
		rol edx,16
		xchg dl,dh
		mov [RebootTime],edx
		or byte [DHCPMagic], byte 8	; Got RebootTime
		; jmp short .opt_done
.not_pl_timeout:

		; Unknown option.  Skip to the next one.
.opt_done:
		add si,ax
.opt_done_noskip:
		jmp .loop

		; Common code for copying an option verbatim
.copyoption:
		xchg cx,ax
		rep movsb
		xchg cx,ax	; Now ax == 0
		stosb		; Null-terminate
		jmp short .opt_done_noskip

;
; genipopt
;
; Generate an ip=<client-ip>:<boot-server-ip>:<gw-ip>:<netmask>
; option into IPOption based on a DHCP packet in trackbuf.
; Assumes CS == DS == ES.
;
genipopt:
		pushad
		mov di,IPOption
		mov eax,'ip='
		stosd
		dec di
		mov eax,[MyIP]
		call gendotquad
		mov al,':'
		stosb
		mov eax,[ServerIP]
		call gendotquad
		mov al,':'
		stosb
		mov eax,[Gateway]
		call gendotquad
		mov al,':'
		stosb
		mov eax,[Netmask]
		call gendotquad	; Zero-terminates its output
		sub di,IPOption
		mov [IPOptionLen],di
		popad
		ret

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "writestr.inc"		; String output
writestr	equ cwritestr
%include "writehex.inc"		; Hexadecimal output
%include "parseconfig.inc"	; High-level config file handling
%include "parsecmd.inc"		; Low-level config file handling
%include "bcopy32.inc"		; 32-bit bcopy
%include "loadhigh.inc"		; Load a file into high memory
%include "font.inc"		; VGA font stuff
%include "graphics.inc"		; VGA graphics
%include "highmem.inc"		; High memory sizing

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

CR		equ 13		; Carriage Return
LF		equ 10		; Line Feed
FF		equ 12		; Form Feed
BS		equ  8		; Backspace

copyright_str   db ' Copyright (C) 1994-', year, ' H. Peter Anvin'
		db CR, LF, 0
boot_prompt	db 'boot: ', 0
wipe_char	db BS, ' ', BS, 0
err_notfound	db 'Could not find kernel image: ',0
err_notkernel	db CR, LF, 'Invalid or corrupt kernel image.', CR, LF, 0
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
err_bssimage	db 'BSS images not supported.', CR, LF, 0
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
undi_data_msg	  db 'UNDI data segment at:   ',0
undi_data_len_msg db 'UNDI data segment size: ',0 
undi_code_msg	  db 'UNDI code segment at:   ',0
undi_code_len_msg db 'UNDI code segment size: ',0 
cant_free_msg	db 'Failed to free base memory, error ', 0
notfound_msg	db 'not found', CR, LF, 0
myipaddr_msg	db 'My IP address seems to be ',0
tftpprefix_msg	db 'TFTP prefix: ', 0
localboot_msg	db 'Booting from local disk...', CR, LF, 0
cmdline_msg	db 'Command line: ', CR, LF, 0
ready_msg	db 'Ready.', CR, LF, 0
trying_msg	db 'Trying to load: ', 0
crlfloading_msg	db CR, LF			; Fall through
loading_msg     db 'Loading ', 0
dotdot_msg      db '.'
dot_msg         db '.', 0
fourbs_msg	db BS, BS, BS, BS, 0
aborted_msg	db ' aborted.'			; Fall through to crlf_msg!
crlf_msg	db CR, LF
null_msg	db 0
crff_msg	db CR, FF, 0
default_str	db 'default', 0
default_len	equ ($-default_str)
syslinux_banner	db CR, LF, 'PXELINUX ', version_str, ' ', date, ' ', 0
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
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
; (.bs and .bss are disabled for PXELINUX, since they are not supported)
;
		align 4, db 0
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.0', 0, 0		; PXE bootstrap program
		db '.com'		; COMBOOT (same as DOS)
exten_table_end:
		dd 0, 0			; Need 8 null bytes here

;
; PXENV entry point.  If we use the !PXE API, this will point to a thunk
; function that converts to the !PXE calling convention.
;
PXENVEntry	dw pxe_thunk,0

;
; PXE unload sequences
;
new_api_unload:
		db PXENV_UDP_CLOSE
		db PXENV_UNDI_SHUTDOWN
		db PXENV_UNLOAD_STACK
		db PXENV_STOP_UNDI
		db 0
old_api_unload:
		db PXENV_UDP_CLOSE
		db PXENV_UNDI_SHUTDOWN
		db PXENV_UNLOAD_STACK
		db PXENV_UNDI_CLEANUP
		db 0

;
; PXE query packets partially filled in
;
pxe_bootp_query_pkt_2:
.status:	dw 0			; Status
.packettype:	dw 2			; DHCPACK packet
.buffersize:	dw trackbufsize		; Packet size
.buffer:	dw trackbuf, 0		; seg:off of buffer
.bufferlimit:	dw trackbufsize		; Unused

pxe_bootp_query_pkt_3:
.status:	dw 0			; Status
.packettype:	dw 3			; Boot server packet
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

;
; Misc initialized (data) variables
;
AppendLen       dw 0                    ; Bytes in append= command
KbdTimeOut      dw 0                    ; Keyboard timeout (if any)
CmdLinePtr	dw cmd_line_here	; Command line advancing pointer
initrd_flag	equ $
initrd_ptr	dw 0			; Initial ramdisk pointer/flag
VKernelCtr	dw 0			; Number of registered vkernels
ForcePrompt	dw 0			; Force prompt
AllowImplicit   dw 1                    ; Allow implicit kernels
SerialPort	dw 0			; Serial port base (or 0 for no serial port)
NextSocket	dw 49152		; Counter for allocating socket numbers
VGAFontSize	dw 16			; Defaults to 16 byte font
UserFont	db 0			; Using a user-specified font
ScrollAttribute	db 07h			; White on black (for text mode)
KeepPXE		db 0			; Should PXE be kept around?

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

		alignb 4, db 0
ack_packet_buf:	dw TFTP_ACK, 0				; TFTP ACK packet

;
; IP information (initialized to "unknown" values)
MyIP		dd 0			; My IP address
ServerIP	dd 0			; IP address of boot server
Netmask		dd 0			; Netmask of this subnet
Gateway		dd 0			; Default router
ServerPort	dw TFTP_PORT		; TFTP server port

;
; Variables that are uninitialized in SYSLINUX but initialized here
;
		alignb 4, db 0
ClustSize	dd TFTP_BLOCKSIZE	; Bytes/cluster
ClustPerMoby	dd 65536/TFTP_BLOCKSIZE	; Clusters per 64K
SecPerClust	dw TFTP_BLOCKSIZE/512	; Same as bsSecPerClust, but a word
BufSafe		dw trackbufsize/TFTP_BLOCKSIZE	; Clusters we can load into trackbuf
BufSafeSec	dw trackbufsize/512	; = how many sectors?
BufSafeBytes	dw trackbufsize		; = how many bytes?
EndOfGetCBuf	dw getcbuf+trackbufsize	; = getcbuf+BufSafeBytes
%ifndef DEPEND
%if ( trackbufsize % TFTP_BLOCKSIZE ) != 0
%error trackbufsize must be a multiple of TFTP_BLOCKSIZE
%endif
%endif
IPAppend	db 0			; Default IPAPPEND option
DHCPMagic	db 0			; DHCP site-specific option info

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
%ifndef DEPEND
%if (getcbuf+trackbufsize) > vgafontbuf
%error "Out of memory, better reorganize something..."
%endif
%endif
