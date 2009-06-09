; -*- fundamental -*- (asm-mode sucks)
; ****************************************************************************
;
;  pxelinux.asm
;
;  A program to boot Linux kernels off a TFTP server using the Intel PXE
;  network booting API.  It is based on the SYSLINUX boot loader for
;  MS-DOS floppies.
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

%define IS_PXELINUX 1
%include "head.inc"
%include "pxe.inc"

; gPXE extensions support
%define GPXE	1

;
; Some semi-configurable constants... change on your own risk.
;
my_id		equ pxelinux_id
FILENAME_MAX_LG2 equ 7			; log2(Max filename size Including final null)
FILENAME_MAX	equ (1 << FILENAME_MAX_LG2)
NULLFILE	equ 0			; Zero byte == null file name
NULLOFFSET	equ 4			; Position in which to look
REBOOT_TIME	equ 5*60		; If failure, time until full reset
%assign HIGHMEM_SLOP 128*1024		; Avoid this much memory near the top
MAX_OPEN_LG2	equ 5			; log2(Max number of open sockets)
MAX_OPEN	equ (1 << MAX_OPEN_LG2)
PKTBUF_SIZE	equ (65536/MAX_OPEN)	; Per-socket packet buffer size
TFTP_PORT	equ htons(69)		; Default TFTP port
; Desired TFTP block size
; For Ethernet MTU is normally 1500.  Unfortunately there seems to
; be a fair number of networks with "substandard" MTUs which break.
; The code assumes TFTP_LARGEBLK <= 2K.
TFTP_MTU	equ 1440
TFTP_LARGEBLK	equ (TFTP_MTU-20-8-4)	; MTU - IP hdr - UDP hdr - TFTP hdr
; Standard TFTP block size
TFTP_BLOCKSIZE_LG2 equ 9		; log2(bytes/block)
TFTP_BLOCKSIZE	equ (1 << TFTP_BLOCKSIZE_LG2)
%assign USE_PXE_PROVIDED_STACK 1	; Use stack provided by PXE?

SECTOR_SHIFT	equ TFTP_BLOCKSIZE_LG2
SECTOR_SIZE	equ TFTP_BLOCKSIZE

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
; TFTP error codes
;
TFTP_EUNDEF	equ htons(0)		; Unspecified error
TFTP_ENOTFOUND	equ htons(1)		; File not found
TFTP_EACCESS	equ htons(2)		; Access violation
TFTP_ENOSPACE	equ htons(3)		; Disk full
TFTP_EBADOP	equ htons(4)		; Invalid TFTP operation
TFTP_EBADID	equ htons(5)		; Unknown transfer
TFTP_EEXISTS	equ htons(6)		; File exists
TFTP_ENOUSER	equ htons(7)		; No such user
TFTP_EOPTNEG	equ htons(8)		; Option negotiation failure

;
; The following structure is used for "virtual kernels"; i.e. LILO-style
; option labels.  The options we permit here are `kernel' and `append
; Since there is no room in the bottom 64K for all of these, we
; stick them in high memory and copy them down before we need them.
;
		struc vkernel
vk_vname:	resb FILENAME_MAX	; Virtual name **MUST BE FIRST!**
vk_rname:	resb FILENAME_MAX	; Real name
vk_ipappend:	resb 1			; "IPAPPEND" flag
vk_type:	resb 1			; Type of file
vk_appendlen:	resw 1
		alignb 4
vk_append:	resb max_cmd_len+1	; Command line
		alignb 4
vk_end:		equ $			; Should be <= vk_size
		endstruc

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
; HBO = host byte order; NBO = network byte order
; (*) = written by options negotiation code, must be dword sized
;
; For a gPXE connection, we set the local port number to -1 and the
; remote port number contains the gPXE file handle.
;
		struc open_file_t
tftp_localport	resw 1			; Local port number	(0 = not in use)
tftp_remoteport	resw 1			; Remote port number
tftp_remoteip	resd 1			; Remote IP address
tftp_filepos	resd 1			; Bytes downloaded (including buffer)
tftp_filesize	resd 1			; Total file size(*)
tftp_blksize	resd 1			; Block size for this connection(*)
tftp_bytesleft	resw 1			; Unclaimed data bytes
tftp_lastpkt	resw 1			; Sequence number of last packet (NBO)
tftp_dataptr	resw 1			; Pointer to available data
tftp_goteof	resb 1			; 1 if the EOF packet received
		resb 3			; Currently unusued
		; At end since it should not be zeroed on socked close
tftp_pktbuf	resw 1			; Packet buffer offset
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

		alignb open_file_t_size
Files		resb MAX_OPEN*open_file_t_size

		alignb FILENAME_MAX
BootFile	resb 256		; Boot file from DHCP packet
PathPrefix	resb 256		; Path prefix derived from boot file
DotQuadBuf	resb 16			; Buffer for dotted-quad IP address
IPOption	resb 80			; ip= option buffer
InitStack	resd 1			; Pointer to reset stack (SS:SP)
PXEStack	resd 1			; Saved stack during PXE call

		section .bss
		alignb 4
RebootTime	resd 1			; Reboot timeout, if set by option
StrucPtr	resd 1			; Pointer to PXENV+ or !PXE structure
APIVer		resw 1			; PXE API version found
LocalBootType	resw 1			; Local boot return code
RealBaseMem	resw 1			; Amount of DOS memory after freeing
OverLoad	resb 1			; Set if DHCP packet uses "overloading"
DHCPMagic	resb 1			; PXELINUX magic flags

; The relative position of these fields matter!
MAC_MAX		equ  32			; Handle hardware addresses this long
MACLen		resb 1			; MAC address len
MACType		resb 1			; MAC address type
MAC		resb MAC_MAX+1		; Actual MAC address
BOOTIFStr	resb 7			; Space for "BOOTIF="
MACStr		resb 3*(MAC_MAX+1)	; MAC address as a string

; The relative position of these fields matter!
UUIDType	resb 1			; Type byte from DHCP option
UUID		resb 16			; UUID, from the PXE stack
UUIDNull	resb 1			; dhcp_copyoption zero-terminates

;
; PXE packets which don't need static initialization
;
		alignb 4
pxe_unload_stack_pkt:
.status:	resw 1			; Status
.reserved:	resb 10			; Reserved
pxe_unload_stack_pkt_len	equ $-pxe_unload_stack_pkt

		alignb 16
		; BOOTP/DHCP packet buffer

		section .bss2
		alignb 16
packet_buf	resb 2048		; Transfer packet
packet_buf_size	equ $-packet_buf

		section .text
		;
		; PXELINUX needs more BSS than the other derivatives;
		; therefore we relocate it from 7C00h on startup.
		;
StackBuf	equ $			; Base of stack if we use our own

;
; Primary entry point.
;
bootsec		equ $
_start:
		pushfd			; Paranoia... in case of return to PXE
		pushad			; ... save as much state as possible
		push ds
		push es
		push fs
		push gs

		cld			; Copy upwards
		xor ax,ax
		mov ds,ax
		mov es,ax

		jmp 0:_start1		; Canonicalize address
_start1:
		; That is all pushed onto the PXE stack.  Save the pointer
		; to it and switch to an internal stack.
		mov [InitStack],sp
		mov [InitStack+2],ss

%if USE_PXE_PROVIDED_STACK
		; Apparently some platforms go bonkers if we
		; set up our own stack...
		mov [BaseStack],sp
		mov [BaseStack+4],ss
%endif

		lss esp,[BaseStack]
		sti			; Stack set up and ready

;
; Initialize screen (if we're using one)
;
%include "init.inc"

;
; Tell the user we got this far
;
		mov si,syslinux_banner
		call writestr_early

		mov si,copyright_str
		call writestr_early

;
; Assume API version 2.1, in case we find the !PXE structure without
; finding the PXENV+ structure.  This should really look at the Base
; Code ROM ID structure in have_pxe, but this is adequate for now --
; if we have !PXE, we have to be 2.1 or higher, and we don't care
; about higher versions than that.
;
		mov word [APIVer],0201h

;
; Now we need to find the !PXE structure.
; We search for the following, in order:
;
; a. !PXE structure as SS:[SP+4]
; b. PXENV+ structure at [ES:BX]
; c. INT 1Ah AX=5650h -> PXENV+
; d. Search memory for !PXE
; e. Search memory for PXENV+
;
; If we find a PXENV+ structure, we try to find a !PXE structure from
; it if the API version is 2.1 or later.
;
		; Plan A: !PXE structure as SS:[SP+4]
		lgs bp,[InitStack]	; GS:BP -> original stack
		les bx,[gs:bp+48]
		call is_pxe
		je have_pxe

		; Plan B: PXENV+ structure at [ES:BX]
		inc byte [plan]
		mov bx,[gs:bp+24]	; Original BX
		mov es,[gs:bp+4]	; Original ES
		call is_pxenv
		je have_pxenv

		; Plan C: PXENV+ structure via INT 1Ah AX=5650h
		inc byte [plan]
		mov ax, 5650h
%if USE_PXE_PROVIDED_STACK == 0
		lss sp,[InitStack]
%endif
		int 1Ah			; May trash regs
%if USE_PXE_PROVIDED_STACK == 0
		lss esp,[BaseStack]
%endif
		sti			; Work around Etherboot bug

		jc no_int1a
		cmp ax,564Eh
		jne no_int1a

		call is_pxenv
		je have_pxenv

no_int1a:
		; Plan D: !PXE memory scan
		inc byte [plan]
		call memory_scan_for_pxe_struct		; !PXE scan
		je have_pxe

		; Plan E: PXENV+ memory scan
		inc byte [plan]
		call memory_scan_for_pxenv_struct	; PXENV+ scan
		je have_pxenv

		; Found nothing at all!!
no_pxe:
		mov si,err_nopxe
		call writestr_early
		jmp kaboom

have_pxenv:
		mov [StrucPtr],bx
		mov [StrucPtr+2],es

		mov si,found_pxenv
		call writestr_early

		mov si,apiver_str
		call writestr_early
		mov ax,[es:bx+6]
		mov [APIVer],ax
		call writehex4
		call crlf

		cmp ax,0201h			; API version 2.1 or higher
		jb .old_api
		cmp byte [es:bx+8],2Ch		; Space for !PXE pointer?
		jb .pxescan
		les bx,[es:bx+28h]		; !PXE structure pointer
		call is_pxe
		je have_pxe

		; Nope, !PXE structure missing despite API 2.1+, or at least
		; the pointer is missing.  Do a last-ditch attempt to find it.
.pxescan:
		call memory_scan_for_pxe_struct
		je have_pxe

		; Otherwise, no dice, use PXENV+ structure
.old_api:
		les bx,[StrucPtr]
		push word [es:bx+22h]		; UNDI data len
		push word [es:bx+20h]		; UNDI data seg
		push word [es:bx+26h]		; UNDI code len
		push word [es:bx+24h]		; UNDI code seg
		push dword [es:bx+0Ah]		; PXENV+ entry point

		mov si,pxenventry_msg
		jmp have_entrypoint

have_pxe:
		mov [StrucPtr],bx
		mov [StrucPtr+2],es

		push word [es:bx+2Eh]		; UNDI data len
		push word [es:bx+28h]		; UNDI data seg
		push word [es:bx+36h]		; UNDI code len
		push word [es:bx+30h]		; UNDI code seg
		push dword [es:bx+10h]		; !PXE entry point

		mov si,pxeentry_msg

have_entrypoint:
		push cs
		pop es				; Restore CS == DS == ES

		call writestr_early		; !PXE or PXENV+ entry found

		pop dword [PXEEntry]
		mov ax,[PXEEntry+2]
		call writehex4
		mov al,':'
		call writechr
		mov ax,[PXEEntry]
		call writehex4

		mov si,viaplan_msg
		call writestr_early

		mov si,undi_code_msg
		call writestr_early
		pop ax				; UNDI code segment
		call writehex4
		xchg dx,ax
		mov si,len_msg
		call writestr_early
		pop ax				; UNDI code length
		call writehex4
		call crlf
		add ax,15
		shr ax,4
		add dx,ax			; DX = seg of end of UNDI code

		mov si,undi_data_msg
		call writestr_early
		pop ax				; UNDI data segment
		call writehex4
		xchg bx,ax
		mov si,len_msg
		call writestr_early
		pop ax				; UNDI data length
		call writehex4
		call crlf
		add ax,15
		shr ax,4
		add ax,bx			; AX = seg of end of UNDI data

		cmp ax,dx
		ja .data_on_top
		xchg ax,dx
.data_on_top:
		; Could we safely add 63 here before the shift?
		shr ax,6			; Convert to kilobytes
		mov [RealBaseMem],ax


;
; Network-specific initialization
;
		xor ax,ax
		mov [LocalDomain],al		; No LocalDomain received

;
; The DHCP client identifiers are best gotten from the DHCPREQUEST
; packet (query info 1).
;
query_bootp_1:
		mov si,get_packet_msg
		call writestr_early

		mov dl,1
		call pxe_get_cached_info
		call parse_dhcp

		; We don't use flags from the request packet, so
		; this is a good time to initialize DHCPMagic...
		; Initialize it to 1 meaning we will accept options found;
		; in earlier versions of PXELINUX bit 0 was used to indicate
		; we have found option 208 with the appropriate magic number;
		; we no longer require that, but MAY want to re-introduce
		; it in the future for vendor encapsulated options.
		mov byte [DHCPMagic],1

;
; Now attempt to get the BOOTP/DHCP packet that brought us life (and an IP
; address).  This lives in the DHCPACK packet (query info 2).
;
query_bootp_2:
		mov dl,2
		call pxe_get_cached_info
		call parse_dhcp			; Parse DHCP packet
;
; Save away MAC address (assume this is in query info 2.  If this
; turns out to be problematic it might be better getting it from
; the query info 1 packet.)
;
.save_mac:
		movzx cx,byte [trackbuf+bootp.hardlen]
		cmp cx,16
		jna .mac_ok
		xor cx,cx		; Bad hardware address length
.mac_ok:
		mov [MACLen],cl
		mov al,[trackbuf+bootp.hardware]
		mov [MACType],al
		mov si,trackbuf+bootp.macaddr
		mov di,MAC
		rep movsb

; Enable this if we really need to zero-pad this field...
;		mov cx,MAC+MAC_MAX+1
;		sub cx,di
;		xor ax,ax
;		rep stosb

;
; Now, get the boot file and other info.  This lives in the CACHED_REPLY
; packet (query info 3).
;
query_bootp_3:
		mov dl,3
		call pxe_get_cached_info
		call parse_dhcp			; Parse DHCP packet
		call crlf

;
; Generate the bootif string, and the hardware-based config string.
;
make_bootif_string:
		mov si,bootif_str
		mov di,BOOTIFStr
		mov cx,bootif_str_len
		rep movsb

		movzx cx,byte [MACLen]
		mov si,MACType
		inc cx
.hexify_mac:
		push cx
		mov cl,1		; CH == 0 already
		call lchexbytes
		mov al,'-'
		stosb
		pop cx
		loop .hexify_mac
		mov [di-1],cl		; Null-terminate and strip final dash
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
		call writestr_early
		call writehex8
		mov al,' '
		call writechr
		pop si				; DotQuadBuf
		call writestr_early
		call crlf

		mov si,IPOption
		call writestr_early
		call crlf

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
		call pxenv
		jc .failed
		cmp word [pxe_udp_open_pkt.status], byte 0
		je .success
.failed:	mov si,err_udpinit
		call writestr_early
		jmp kaboom
.success:

;
; Common initialization code
;
%include "cpuinit.inc"

;
; Detect NIC type and initialize the idle mechanism
;
		call pxe_detect_nic_type
		call reset_idle

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
; Store standard filename prefix
;
prefix:		test byte [DHCPMagic], 04h	; Did we get a path prefix option
		jnz .got_prefix
		mov si,BootFile
		mov di,PathPrefix
		cld
		call strcpy
		mov cx,di
		sub cx,PathPrefix+1
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
		call writestr_early
		mov si,PathPrefix
		call writestr_early
		call crlf

		; Set CurrentDirName
		push di
		mov si,PathPrefix
		mov di,CurrentDirName
		call strcpy
		pop di

;
; Load configuration file
;
find_config:

;
; Begin looking for configuration file
;
config_scan:
		test byte [DHCPMagic], 02h
		jz .no_option

		; We got a DHCP option, try it first
		call .try
		jnz .success

.no_option:
		mov di,ConfigName
		mov si,cfgprefix
		mov cx,cfgprefix_len
		rep movsb

		; Have to guess config file name...

		; Try loading by UUID.
		cmp byte [HaveUUID],0
		je .no_uuid

		push di
		mov bx,uuid_dashes
		mov si,UUID
.gen_uuid:
		movzx cx,byte [bx]
		jcxz .done_uuid
		inc bx
		call lchexbytes
		mov al,'-'
		stosb
		jmp .gen_uuid
.done_uuid:
		mov [di-1],cl		; Remove last dash and zero-terminate
		pop di
		call .try
		jnz .success
.no_uuid:

		; Try loading by MAC address
		push di
		mov si,MACStr
		call strcpy
		pop di
		call .try
		jnz .success

		; Nope, try hexadecimal IP prefixes...
.scan_ip:
		mov cx,4
		mov si,MyIP
		call uchexbytes			; Convert to hex string

		mov cx,8			; Up to 8 attempts
.tryagain:
		mov byte [di],0			; Zero-terminate string
		call .try
		jnz .success
		dec di				; Drop one character
		loop .tryagain

		; Final attempt: "default" string
		mov si,default_str		; "default" string
		call strcpy
		call .try
		jnz .success

		mov si,err_noconfig
		call writestr_early
		jmp kaboom

.try:
		pusha
		mov si,trying_msg
		call writestr_early
		mov di,ConfigName
		mov si,di
		call writestr_early
		call crlf
		mov si,di
		mov di,KernelName	;  Borrow this buffer for mangled name
		call mangle_name
		call open
		popa
		ret


.success:

;
; Linux kernel loading code is common.  However, we need to define
; a couple of helper macros...
;

; Unload PXE stack
%define HAVE_UNLOAD_PREP
%macro	UNLOAD_PREP 0
		call unload_pxe
%endmacro

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"

;
; Boot to the local disk by returning the appropriate PXE magic.
; AX contains the appropriate return code.
;
%if HAS_LOCALBOOT

local_boot:
		push cs
		pop ds
		mov [LocalBootType],ax
		call vgaclearmode
		mov si,localboot_msg
		call writestr_early
		; Restore the environment we were called with
		lss sp,[InitStack]
		pop gs
		pop fs
		pop es
		pop ds
		popad
		mov ax,[cs:LocalBootType]
		popfd
		retf				; Return to PXE

%endif

;
; kaboom: write a message and bail out.  Wait for quite a while,
;	  or a user keypress, then do a hard reboot.
;
kaboom:
		RESET_STACK_AND_SEGS AX
.patch:		mov si,bailmsg
		call writestr_early		; Returns with AL = 0
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
; memory_scan_for_pxenv_struct:
;
;	If none of the standard methods find the !PXE/PXENV+ structure,
;	look for it by scanning memory.
;
;	On exit, if found:
;		ZF = 1, ES:BX -> !PXE structure
;	Otherwise:
;		ZF = 0
;
;	Assumes DS == CS
;	Clobbers AX, BX, CX, DX, SI, ES
;
memory_scan_for_pxe_struct:
		mov dx,is_pxe
		mov ax,[BIOS_fbm]	; Starting segment
		shl ax,(10-4)		; Kilobytes -> paragraphs
		jmp memory_scan_common

memory_scan_for_pxenv_struct:
		mov ax,1000h		; Starting segment
		mov dx,is_pxenv
		; fall through

memory_scan_common:
		dec ax			; To skip inc ax
.mismatch:
		inc ax
		cmp ax,0A000h-1		; End of memory
		ja .not_found		; ZF = 0 on not found
		mov es,ax
		xor bx,bx
		call dx
		jne .mismatch
.not_found:
		ret

;
; is_pxe:
;	Validity check on possible !PXE structure in ES:BX
; is_pxenv:
;	Validity check on possible PXENV+ structure in ES:BX
;
;	Return ZF = 1 on success
;
;	Clobbers CX and SI
;
is_struc:
.pxe:
		cmp dword [es:bx],'!PXE'
		jne .bad
		movzx cx,byte [es:bx+4]
		cmp cx,58h
		jae .checksum
		ret
.pxenv:
		cmp dword [es:bx],'PXEN'
		jne .bad
		cmp word [es:bx+4],'V+'
		jne .bad
		movzx cx,[es:bx+8]
		cmp cx,28h
		jb .bad
.checksum:
		push ax
		mov si,bx
		xor ax,ax
.loop:
		es lodsb
		add ah,al
		loop .loop
		pop ax
.bad:
		ret

is_pxe		equ is_struc.pxe
is_pxenv	equ is_struc.pxenv

;
; close_file:
;	     Deallocates a file structure (pointer in SI)
;	     Assumes CS == DS.
;
; XXX: We should check to see if this file is still open on the server
; side and send a courtesy ERROR packet to the server.
;
close_file:
		and si,si
		jz .closed
		mov word [si],0		; Not in use
.closed:	ret

;
; searchdir:
;
;	Open a TFTP connection to the server
;
;	     On entry:
;		DS:DI	= mangled filename
;	     If successful:
;		ZF clear
;		SI	= socket pointer
;		EAX	= file length in bytes, or -1 if unknown
;	     If unsuccessful
;		ZF set
;

searchdir:
		push es
		push bx
		push cx
		mov ax,ds
		mov es,ax
		mov si,di
		push bp
		mov bp,sp

		call allocate_socket
		jz .ret

		mov ax,TimeoutTable	; Reset timeout

.sendreq:	push ax			; [bp-2]  - Timeout pointer
		push si			; [bp-4]  - File name

		mov di,packet_buf
		mov [pxe_udp_write_pkt.buffer],di

		mov ax,TFTP_RRQ		; TFTP opcode
		stosw

		lodsd			; EAX <- server override (if any)
		and eax,eax
		jnz .noprefix		; No prefix, and we have the server

		push si			; Add common prefix
		mov si,PathPrefix
		call strcpy
		dec di
		pop si

		mov eax,[ServerIP]	; Get default server

.noprefix:
		call strcpy		; Filename
%if GPXE
		mov si,packet_buf+2
		call is_gpxe
		jnc .gpxe
%endif

		mov [bx+tftp_remoteip],eax

		push bx			; [bp-6]  - TFTP block
		mov bx,[bx]
		push bx			; [bp-8]  - TID (local port no)

		mov [pxe_udp_write_pkt.sip],eax
		; Now figure out the gateway
		xor eax,[MyIP]
		and eax,[Netmask]
		jz .nogwneeded
		mov eax,[Gateway]
.nogwneeded:
		mov [pxe_udp_write_pkt.gip],eax
		mov [pxe_udp_write_pkt.lport],bx
		mov ax,[ServerPort]
		mov [pxe_udp_write_pkt.rport],ax
		mov si,tftp_tail
		mov cx,tftp_tail_len
		rep movsb
		sub di,packet_buf	; Get packet size
		mov [pxe_udp_write_pkt.buffersize],di

		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call pxenv
		jc .failure
		cmp word [pxe_udp_write_pkt.status],byte 0
		jne .failure

		;
		; Danger, Will Robinson!  We need to support timeout
		; and retry lest we just lost a packet...
		;

		; Packet transmitted OK, now we need to receive
.getpacket:	mov bx,[bp-2]
		movzx bx,byte [bx]
		push bx			; [bp-10] - timeout in ticks
		push word [BIOS_timer]	; [bp-12]

.pkt_loop:	mov bx,[bp-8]		; TID
		mov di,packet_buf
		mov [pxe_udp_read_pkt.buffer],di
		mov [pxe_udp_read_pkt.buffer+2],ds
		mov word [pxe_udp_read_pkt.buffersize],packet_buf_size
		mov eax,[MyIP]
		mov [pxe_udp_read_pkt.dip],eax
		mov [pxe_udp_read_pkt.lport],bx
		mov di,pxe_udp_read_pkt
		mov bx,PXENV_UDP_READ
		call pxenv
		jnc .got_packet			; Wait for packet
.no_packet:
		mov dx,[BIOS_timer]
		cmp dx,[bp-12]
		je .pkt_loop
		mov [bp-12],dx
		dec word [bp-10]
		jnz .pkt_loop
		pop ax	; Adjust stack
		pop ax
		jmp .failure

.got_packet:
		mov si,[bp-6]			; TFTP pointer
		mov bx,[bp-8]			; TID

		; Make sure the packet actually came from the server
		; This is technically not to the TFTP spec?
		mov eax,[si+tftp_remoteip]
		cmp [pxe_udp_read_pkt.sip],eax
		jne .no_packet

		; Got packet - reset timeout
		mov word [bp-2],TimeoutTable

		pop ax	; Adjust stack
		pop ax

		mov ax,[pxe_udp_read_pkt.rport]
		mov [si+tftp_remoteport],ax

		; filesize <- -1 == unknown
		mov dword [si+tftp_filesize], -1
		; Default blksize unless blksize option negotiated
		mov word [si+tftp_blksize], TFTP_BLOCKSIZE

		movzx ecx,word [pxe_udp_read_pkt.buffersize]
		sub cx,2		; CX <- bytes after opcode
		jb .failure		; Garbled reply

		mov si,packet_buf
		lodsw

		cmp ax, TFTP_ERROR
		je .bailnow		; ERROR reply: don't try again

		; If the server doesn't support any options, we'll get
		; a DATA reply instead of OACK.  Stash the data in
		; the file buffer and go with the default value for
		; all options...
		cmp ax, TFTP_DATA
		je .no_oack

		cmp ax, TFTP_OACK
		jne .err_reply		; Unknown packet type

		; Now we need to parse the OACK packet to get the transfer
		; and packet sizes.
		;  SI -> first byte of options; [E]CX -> byte count
.parse_oack:
		jcxz .done_pkt			; No options acked

.get_opt_name:
		; If we find an option which starts with a NUL byte,
		; (a null option), we're either seeing garbage that some
		; TFTP servers add to the end of the packet, or we have
		; no clue how to parse the rest of the packet (what is
		; an option name and what is a value?)  In either case,
		; discard the rest.
		cmp byte [si],0
		je .done_pkt

		mov di,si
		mov bx,si
.opt_name_loop:	lodsb
		and al,al
		jz .got_opt_name
		or al,20h			; Convert to lowercase
		stosb
		loop .opt_name_loop
		; We ran out, and no final null
		jmp .done_pkt			; Ignore runt option
.got_opt_name:	; si -> option value
		dec cx				; bytes left in pkt
		jz .done_pkt			; Option w/o value, ignore

		; Parse option pointed to by bx; guaranteed to be
		; null-terminated.
		push cx
		push si
		mov si,bx			; -> option name
		mov bx,tftp_opt_table
		mov cx,tftp_opts
.opt_loop:
		push cx
		push si
		mov di,[bx]			; Option pointer
		mov cx,[bx+2]			; Option len
		repe cmpsb
		pop si
		pop cx
		je .get_value			; OK, known option
		add bx,6
		loop .opt_loop

		pop si
		pop cx
		; Non-negotiated option returned, no idea what it means...
		jmp .err_reply

.get_value:	pop si				; si -> option value
		pop cx				; cx -> bytes left in pkt
		mov bx,[bx+4]			; Pointer to data target
		add bx,[bp-6]			; TFTP socket pointer
		xor eax,eax
		xor edx,edx
.value_loop:	lodsb
		and al,al
		jz .got_value
		sub al,'0'
		cmp al, 9
		ja .err_reply			; Not a decimal digit
		imul edx,10
		add edx,eax
		mov [bx],edx
		loop .value_loop
		; Ran out before final null, accept anyway
		jmp short .done_pkt

.got_value:
		dec cx
		jnz .get_opt_name		; Not end of packet

		; ZF == 1

		; Success, done!
.done_pkt:
		pop si			; Junk
		pop si			; We want the packet ptr in SI

		mov eax,[si+tftp_filesize]
.got_file:				; SI->socket structure, EAX = size
		and eax,eax		; Set ZF depending on file size
		jz .error_si		; ZF = 1 need to free the socket
.ret:
		leave			; SP <- BP, POP BP
		pop cx
		pop bx
		pop es
		ret


.no_oack:	; We got a DATA packet, meaning no options are
		; suported.  Save the data away and consider the length
		; undefined, *unless* this is the only data packet...
		mov bx,[bp-6]		; File pointer
		sub cx,2		; Too short?
		jb .failure
		lodsw			; Block number
		cmp ax,htons(1)
		jne .failure
		mov [bx+tftp_lastpkt],ax
		cmp cx,TFTP_BLOCKSIZE
		ja .err_reply		; Corrupt...
		je .not_eof
		; This was the final EOF packet, already...
		; We know the filesize, but we also want to ack the
		; packet and set the EOF flag.
		mov [bx+tftp_filesize],ecx
		mov byte [bx+tftp_goteof],1
		push si
		mov si,bx
		; AX = htons(1) already
		call ack_packet
		pop si
.not_eof:
		mov [bx+tftp_bytesleft],cx
		mov ax,pktbuf_seg
		push es
		mov es,ax
		mov di,tftp_pktbuf
		mov [bx+tftp_dataptr],di
		add cx,3
		shr cx,2
		rep movsd
		pop es
		jmp .done_pkt

.err_reply:	; TFTP protocol error.  Send ERROR reply.
		; ServerIP and gateway are already programmed in
		mov si,[bp-6]
		mov ax,[si+tftp_remoteport]
		mov word [pxe_udp_write_pkt.rport],ax
		mov word [pxe_udp_write_pkt.buffer],tftp_proto_err
		mov word [pxe_udp_write_pkt.buffersize],tftp_proto_err_len
		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call pxenv

		; Write an error message and explode
		mov si,err_damage
		call writestr_early
		jmp kaboom

.bailnow:
		; Immediate error - no retry
		mov word [bp-2],TimeoutTableEnd-1

.failure:	pop bx			; Junk
		pop bx
		pop si
		pop ax
		inc ax
		cmp ax,TimeoutTableEnd
		jb .sendreq		; Try again

.error:		mov si,bx		; Socket pointer
.error_si:				; Socket pointer already in SI
		call free_socket	; ZF <- 1, SI <- 0
		jmp .ret


%if GPXE
.gpxe:
		push bx			; Socket pointer
		mov di,gpxe_file_open
		mov word [di],2		; PXENV_STATUS_BAD_FUNC
		mov word [di+4],packet_buf+2	; Completed URL
		mov [di+6],ds
		mov bx,PXENV_FILE_OPEN
		call pxenv
		pop si			; Socket pointer in SI
		jc .error_si

		mov ax,[di+2]
		mov word [si+tftp_localport],-1	; gPXE URL
		mov [si+tftp_remoteport],ax
		mov di,gpxe_get_file_size
		mov [di+2],ax

%if 0
		; Disable this for now since gPXE doesn't always
		; return valid information in PXENV_GET_FILE_SIZE
		mov bx,PXENV_GET_FILE_SIZE
		call pxenv
		mov eax,[di+4]		; File size
		jnc .oksize
%endif
		or eax,-1		; Size unknown
.oksize:
		mov [si+tftp_filesize],eax
		jmp .got_file
%endif ; GPXE

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
		mov bx,Files
		mov cx,MAX_OPEN
.check:		cmp word [bx], byte 0
		je .found
		add bx,open_file_t_size
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
		and ax,((1 << (13-MAX_OPEN_LG2))-1) | 0xC000
		mov [NextSocket],ax
		shl cx,13-MAX_OPEN_LG2
		add cx,ax			; ZF = 0
		xchg ch,cl			; Convert to network byte order
		mov [bx],cx			; Socket in use
		pop ax
		pop cx
		ret

;
; Free socket: socket in SI; return SI = 0, ZF = 1 for convenience
;
free_socket:
		push es
		pusha
		xor ax,ax
		mov es,ax
		mov di,si
		mov cx,tftp_pktbuf >> 1		; tftp_pktbuf is not cleared
		rep stosw
		popa
		pop es
		xor si,si
		ret

;
; parse_dotquad:
;		Read a dot-quad pathname in DS:SI and output an IP
;		address in EAX, with SI pointing to the first
;		nonmatching character.
;
;		Return CF=1 on error.
;
;		No segment assumptions permitted.
;
parse_dotquad:
		push cx
		mov cx,4
		xor eax,eax
.parseloop:
		mov ch,ah
		mov ah,al
		lodsb
		sub al,'0'
		jb .notnumeric
		cmp al,9
		ja .notnumeric
		aad				; AL += 10 * AH; AH = 0;
		xchg ah,ch
		jmp .parseloop
.notnumeric:
		cmp al,'.'-'0'
		pushf
		mov al,ah
		mov ah,ch
		xor ch,ch
		ror eax,8
		popf
		jne .error
		loop .parseloop
		jmp .done
.error:
		loop .realerror			; If CX := 1 then we're done
		clc
		jmp .done
.realerror:
		stc
.done:
		dec si				; CF unchanged!
		pop cx
		ret

;
; is_url:      Return CF=0 if and only if the buffer pointed to by
;	       DS:SI is a URL (contains ://).  No registers modified.
;
%if GPXE
is_url:
		push si
		push eax
.loop:
		mov eax,[si]
		inc si
		and al,al
		jz .not_url
		and eax,0FFFFFFh
		cmp eax,'://'
		jne .loop
.done:
		; CF=0 here
		pop eax
		pop si
		ret
.not_url:
		stc
		jmp .done

;
; is_gpxe:     Return CF=0 if and only if the buffer pointed to by
;	       DS:SI is a URL (contains ://) *and* the gPXE extensions
;	       API is available.  No registers modified.
;
is_gpxe:
		call is_url
		jc .ret			; Not a URL, don't bother
.again:
		cmp byte [HasGPXE],1
		ja .unknown
		; CF=1 if not available (0),
		; CF=0 if known available (1).
.ret:		ret

.unknown:
		; If we get here, the gPXE status is unknown.
		push es
		pushad
		push ds
		pop es
		mov di,gpxe_file_api_check
		mov bx,PXENV_FILE_API_CHECK	; BH = 0
		call pxenv
		jc .nogood
		cmp dword [di+4],0xe9c17b20
		jne .nogood
		mov ax,[di+12]		; Don't care about the upper half...
		not ax			; Set bits of *missing* functions...
		and ax,01001011b	; The functions we care about
		setz bh
		jz .done
.nogood:
		mov si,gpxe_warning_msg
		call writestr_early
.done:
		mov [HasGPXE],bh
		popad
		pop es
		jmp .again

		section .data
gpxe_warning_msg:
		db 'URL syntax, but gPXE extensions not detected, '
		db 'trying plain TFTP...', CR, LF, 0
HasGPXE		db -1			; Unknown
		section .text

%endif

;
; mangle_name: Mangle a filename pointed to by DS:SI into a buffer pointed
;	       to by ES:DI; ends on encountering any whitespace.
;	       DI is preserved.
;
;	       This verifies that a filename is < FILENAME_MAX characters
;	       and doesn't contain whitespace, and zero-pads the output buffer,
;	       so "repe cmpsb" can do a compare.
;
;	       The first four bytes of the manged name is the IP address of
;	       the download host, 0 for no host, or -1 for a gPXE URL.
;
;              No segment assumptions permitted.
;
mangle_name:
		push di
%if GPXE
		call is_url
		jc .not_url
		or eax,-1			; It's a URL
		jmp .prefix_done
.not_url:
%endif ; GPXE
		push si
		mov eax,[cs:ServerIP]
		cmp byte [si],0
		je .noip			; Null filename?!?!
		cmp word [si],'::'		; Leading ::?
		je .gotprefix

.more:
		inc si
		cmp byte [si],0
		je .noip
		cmp word [si],'::'
		jne .more

		; We have a :: prefix of some sort, it could be either
		; a DNS name or a dot-quad IP address.  Try the dot-quad
		; first...
.here:
		pop si
		push si
		call parse_dotquad
		jc .notdq
		cmp word [si],'::'
		je .gotprefix
.notdq:
		pop si
		push si
		call dns_resolv
		cmp word [si],'::'
		jne .noip
		and eax,eax
		jnz .gotprefix

.noip:
		pop si
		xor eax,eax
		jmp .prefix_done

.gotprefix:
		pop cx				; Adjust stack
		inc si				; Skip double colon
		inc si

.prefix_done:
		stosd				; Save IP address prefix
		mov cx,FILENAME_MAX-5

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
		pop di
		ret				; Done

;
; unmangle_name: Does the opposite of mangle_name; converts a DOS-mangled
;                filename to the conventional representation.  This is needed
;                for the BOOT_IMAGE= parameter for the kernel.
;
;                NOTE: The output buffer needs to be able to hold an
;		 expanded IP address.
;
;                DS:SI -> input mangled file name
;                ES:DI -> output buffer
;
;                On return, DI points to the first byte after the output name,
;                which is set to a null byte.
;
unmangle_name:
		push eax
		lodsd
		and eax,eax
		jz .noip
		cmp eax,-1
		jz .noip			; URL
		call gendotquad
		mov ax,'::'
		stosw
.noip:
		call strcpy
		dec di				; Point to final null byte
		pop eax
		ret

;
; pxenv
;
; This is the main PXENV+/!PXE entry point, using the PXENV+
; calling convention.  This is a separate local routine so
; we can hook special things from it if necessary.  In particular,
; some PXE stacks seem to not like being invoked from anything but
; the initial stack, so humour it.
;
; While we're at it, save and restore all registers.
;
pxenv:
		pushfd
		pushad
%if USE_PXE_PROVIDED_STACK == 0
		mov [cs:PXEStack],sp
		mov [cs:PXEStack+2],ss
		lss sp,[cs:InitStack]
%endif
		; Pre-clear the Status field
		mov word [es:di],cs

		; This works either for the PXENV+ or the !PXE calling
		; convention, as long as we ignore CF (which is redundant
		; with AX anyway.)
		push es
		push di
		push bx
.jump:		call 0:0
		add sp,6
		mov [cs:PXEStatus],ax
%if USE_PXE_PROVIDED_STACK == 0
		lss sp,[cs:PXEStack]
%endif
		mov bp,sp
		and ax,ax
		setnz [bp+32]			; If AX != 0 set CF on return

		; This clobbers the AX return, but we already saved it into
		; the PXEStatus variable.
		popad
		popfd				; Restore flags (incl. IF, DF)
		ret

; Must be after function def due to NASM bug
PXEEntry	equ pxenv.jump+1

		section .bss
		alignb 2
PXEStatus	resb 2

		section .text

;
; getfssec: Get multiple clusters from a file, given the starting cluster.
;
;	In this case, get multiple blocks from a specific TCP connection.
;
;  On entry:
;	ES:BX	-> Buffer
;	SI	-> TFTP socket pointer
;	CX	-> 512-byte block count; 0FFFFh = until end of file
;  On exit:
;	SI	-> TFTP socket pointer (or 0 on EOF)
;	CF = 1	-> Hit EOF
;	ECX	-> number of bytes actually read
;
getfssec:
		push eax
		push edi
		push bx
		push si
		push fs
		mov di,bx
		mov ax,pktbuf_seg
		mov fs,ax

		xor eax,eax
		movzx ecx,cx
		shl ecx,TFTP_BLOCKSIZE_LG2	; Convert to bytes
		push ecx			; Initial request size
		jz .hit_eof			; Nothing to do?

.need_more:
		call fill_buffer
		movzx eax,word [si+tftp_bytesleft]
		and ax,ax
		jz .hit_eof

		push ecx
		cmp ecx,eax
		jna .ok_size
		mov ecx,eax
.ok_size:
		mov ax,cx			; EAX<31:16> == ECX<31:16> == 0
		mov bx,[si+tftp_dataptr]
		sub [si+tftp_bytesleft],cx
		xchg si,bx
		fs rep movsb			; Copy from packet buffer
		xchg si,bx
		mov [si+tftp_dataptr],bx

		pop ecx
		sub ecx,eax
		jnz .need_more

.hit_eof:
		call fill_buffer

		pop eax				; Initial request amount
		xchg eax,ecx
		sub ecx,eax			; ... minus anything not gotten

		pop fs
		pop si

		; Is there anything left of this?
		mov eax,[si+tftp_filesize]
		sub eax,[si+tftp_filepos]
		jnz .bytes_left

		cmp [si+tftp_bytesleft],ax	; AX == 0
		jne .bytes_left

		cmp byte [si+tftp_goteof],0
		je .done
		; I'm 99% sure this can't happen, but...
		call fill_buffer		; Receive/ACK the EOF packet
.done:
		; The socket is closed and the buffer drained
		; Close socket structure and re-init for next user
		call free_socket
		stc
		jmp .ret
.bytes_left:
		clc
.ret:
		pop bx
		pop edi
		pop eax
		ret

;
; Get a fresh packet if the buffer is drained, and we haven't hit
; EOF yet.  The buffer should be filled immediately after draining!
;
; expects fs -> pktbuf_seg and ds:si -> socket structure
;
fill_buffer:
		cmp word [si+tftp_bytesleft],0
		je .empty
		ret				; Otherwise, nothing to do

.empty:
		push es
		pushad
		mov ax,ds
		mov es,ax

		; Note: getting the EOF packet is not the same thing
		; as tftp_filepos == tftp_filesize; if the EOF packet
		; is empty the latter condition can be true without
		; having gotten the official EOF.
		cmp byte [si+tftp_goteof],0
		jne .ret			; Already EOF

%if GPXE
		cmp word [si+tftp_localport], -1
		jne .get_packet_tftp
		call get_packet_gpxe
		jmp .ret
.get_packet_tftp:
%endif ; GPXE

		; TFTP code...
.packet_loop:
		; Start by ACKing the previous packet; this should cause the
		; next packet to be sent.
		mov bx,TimeoutTable

.send_ack:	push bx				; <D> Retry pointer
		movzx cx,byte [bx]		; Timeout

		mov ax,[si+tftp_lastpkt]
		call ack_packet			; Send ACK

		; We used to test the error code here, but sometimes
		; PXE would return negative status even though we really
		; did send the ACK.  Now, just treat a failed send as
		; a normally lost packet, and let it time out in due
		; course of events.

.send_ok:	; Now wait for packet.
		mov dx,[BIOS_timer]		; Get current time

.wait_data:	push cx				; <E> Timeout
		push dx				; <F> Old time

		mov bx,[si+tftp_pktbuf]
		mov [pxe_udp_read_pkt.buffer],bx
		mov [pxe_udp_read_pkt.buffer+2],fs
		mov [pxe_udp_read_pkt.buffersize],word PKTBUF_SIZE
		mov eax,[si+tftp_remoteip]
		mov [pxe_udp_read_pkt.sip],eax
		mov eax,[MyIP]
		mov [pxe_udp_read_pkt.dip],eax
		mov ax,[si+tftp_remoteport]
		mov [pxe_udp_read_pkt.rport],ax
		mov ax,[si+tftp_localport]
		mov [pxe_udp_read_pkt.lport],ax
		mov di,pxe_udp_read_pkt
		mov bx,PXENV_UDP_READ
		call pxenv
		jnc .recv_ok

		; No packet, or receive failure
		mov dx,[BIOS_timer]
		pop ax				; <F> Old time
		pop cx				; <E> Timeout
		cmp ax,dx			; Same time -> don't advance timeout
		je .wait_data			; Same clock tick
		loop .wait_data			; Decrease timeout

		pop bx				; <D> Didn't get any, send another ACK
		inc bx
		cmp bx,TimeoutTableEnd
		jb .send_ack
		jmp kaboom			; Forget it...

.recv_ok:	pop dx				; <F>
		pop cx				; <E>

		cmp word [pxe_udp_read_pkt.buffersize],byte 4
		jb .wait_data			; Bad size for a DATA packet

		mov bx,[si+tftp_pktbuf]
		cmp word [fs:bx],TFTP_DATA	; Not a data packet?
		jne .wait_data			; Then wait for something else

		mov ax,[si+tftp_lastpkt]
		xchg ah,al			; Host byte order
		inc ax				; Which packet are we waiting for?
		xchg ah,al			; Network byte order
		cmp [fs:bx+2],ax
		je .right_packet

		; Wrong packet, ACK the packet and then try again
		; This is presumably because the ACK got lost,
		; so the server just resent the previous packet
		mov ax,[fs:bx+2]
		call ack_packet
		jmp .send_ok			; Reset timeout

.right_packet:	; It's the packet we want.  We're also EOF if the
		; size < blocksize

		pop cx				; <D> Don't need the retry count anymore

		mov [si+tftp_lastpkt],ax	; Update last packet number

		movzx ecx,word [pxe_udp_read_pkt.buffersize]
		sub cx,byte 4			; Skip TFTP header

		; Set pointer to data block
		lea ax,[bx+4]			; Data past TFTP header
		mov [si+tftp_dataptr],ax

		add [si+tftp_filepos],ecx
		mov [si+tftp_bytesleft],cx

		cmp cx,[si+tftp_blksize]	; Is it a full block?
		jb .last_block			; If not, it's EOF

.ret:
		popad
		pop es
		ret


.last_block:	; Last block - ACK packet immediately
		mov ax,[fs:bx+2]
		call ack_packet

		; Make sure we know we are at end of file
		mov eax,[si+tftp_filepos]
		mov [si+tftp_filesize],eax
		mov byte [si+tftp_goteof],1

		jmp .ret

;
; TimeoutTable: list of timeouts (in 18.2 Hz timer ticks)
;
; This is roughly an exponential backoff...
;
		section .data
TimeoutTable:
		db 2, 2, 3, 3, 4, 5, 6, 7, 9, 10, 12, 15, 18
		db 21, 26, 31, 37, 44, 53, 64, 77, 92, 110, 132
		db 159, 191, 229, 255, 255, 255, 255
TimeoutTableEnd	equ $

		section .text
;
; ack_packet:
;
; Send ACK packet.  This is a common operation and so is worth canning.
;
; Entry:
;	SI	= TFTP block
;	AX	= Packet # to ack (network byte order)
; Exit:
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
		xor eax,[MyIP]
		and eax,[Netmask]
		jz .nogw
		mov eax,[Gateway]
.nogw:
		mov [pxe_udp_write_pkt.gip],eax
		mov [pxe_udp_write_pkt.buffer],word ack_packet_buf
		mov [pxe_udp_write_pkt.buffersize], word 4
		mov di,pxe_udp_write_pkt
		mov bx,PXENV_UDP_WRITE
		call pxenv
		popad
		ret

%if GPXE
;
; Get a fresh packet from a gPXE socket; expects fs -> pktbuf_seg
; and ds:si -> socket structure
;
; Assumes CS == DS == ES.
;
get_packet_gpxe:
		mov di,gpxe_file_read

		mov ax,[si+tftp_remoteport]	; gPXE filehandle
		mov [di+2],ax
		mov ax,[si+tftp_pktbuf]
		mov [di+6],ax
		mov [si+tftp_dataptr],ax
		mov [di+8],fs

.again:
		mov word [di+4],PKTBUF_SIZE
		mov bx,PXENV_FILE_READ
		call pxenv
		jnc .ok				; Got data or EOF
		cmp word [di],PXENV_STATUS_TFTP_OPEN	; == EWOULDBLOCK
		je .again
		jmp kaboom			; Otherwise error...

.ok:
		movzx eax,word [di+4]		; Bytes read
		mov [si+tftp_bytesleft],ax	; Bytes in buffer
		add [si+tftp_filepos],eax	; Position in file

		and ax,ax			; EOF?
		mov eax,[si+tftp_filepos]

		jnz .got_stuff

		; We got EOF here, make sure the upper layers know
		mov [si+tftp_filesize],eax

.got_stuff:
		; If we're done here, close the file
		cmp [si+tftp_filesize],eax
		ja .done		; Not EOF, there is still data...

		; Reuse the previous [es:di] structure since the
		; relevant fields are all the same
		mov byte [si+tftp_goteof],1

		mov bx,PXENV_FILE_CLOSE
		call pxenv
		; Ignore return...
.done:
		ret
%endif ; GPXE

;
; unload_pxe:
;
; This function unloads the PXE and UNDI stacks and unclaims
; the memory.
;
unload_pxe:
		cmp byte [KeepPXE],0		; Should we keep PXE around?
		jne reset_pxe

		push ds
		push es

		mov ax,cs
		mov ds,ax
		mov es,ax

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
		call pxenv
		jc .cant_free
		mov ax,word [pxe_unload_stack_pkt.status]
		cmp ax,PXENV_STATUS_SUCCESS
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
.pop_ret:
		pop es
		pop ds
		ret

.cant_free:
		mov si,cant_free_msg
		call writestr_early
		push ax
		xchg bx,ax
		call writehex4
		mov al,'-'
		call writechr
		pop ax
		call writehex4
		mov al,'-'
		call writechr
		mov eax,[4*0x1a]
		call writehex8
		call crlf
		jmp .pop_ret

		; We want to keep PXE around, but still we should reset
		; it to the standard bootup configuration
reset_pxe:
		push es
		push cs
		pop es
		mov bx,PXENV_UDP_CLOSE
		mov di,pxe_udp_close_pkt
		call pxenv
		pop es
		ret

;
; gendotquad
;
; Take an IP address (in network byte order) in EAX and
; output a dotted quad string to ES:DI.
; DI points to terminal null at end of string on exit.
;
gendotquad:
		push eax
		push cx
		mov cx,4
.genchar:
		push eax
		cmp al,10		; < 10?
		jb .lt10		; If so, skip first 2 digits

		cmp al,100		; < 100
		jb .lt100		; If so, skip first digit

		aam 100
		; Now AH = 100-digit; AL = remainder
		add ah,'0'
		mov [es:di],ah
		inc di

.lt100:
		aam 10
		; Now AH = 10-digit; AL = remainder
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
		pop cx
		pop eax
		ret
;
; uchexbytes/lchexbytes
;
; Take a number of bytes in memory and convert to upper/lower-case
; hexadecimal
;
; Input:
;	DS:SI	= input bytes
;	ES:DI	= output buffer
;	CX	= number of bytes
; Output:
;	DS:SI	= first byte after
;	ES:DI	= first byte after
;	CX = 0
;
; Trashes AX, DX
;

lchexbytes:
	mov dl,'a'-'9'-1
	jmp xchexbytes
uchexbytes:
	mov dl,'A'-'9'-1
xchexbytes:
.loop:
	lodsb
	mov ah,al
	shr al,4
	call .outchar
	mov al,ah
	call .outchar
	loop .loop
	ret
.outchar:
	and al,0Fh
	add al,'0'
	cmp al,'9'
	jna .done
	add al,dl
.done:
	stosb
	ret

;
; pxe_get_cached_info
;
; Get a DHCP packet from the PXE stack into the trackbuf.
;
; Input:
;	DL = packet type
; Output:
;	CX = buffer size
;
; Assumes CS == DS == ES.
;
pxe_get_cached_info:
		pushad
		mov al,' '
		call writechr
		mov al,dl
		call writehex2
		mov di,pxe_bootp_query_pkt
		push di
		xor ax,ax
		stosw		; Status
		movzx ax,dl
		stosw		; Packet type
		mov ax,trackbufsize
		stosw		; Buffer size
		mov ax,trackbuf
		stosw		; Buffer offset
		xor ax,ax
		stosw		; Buffer segment

		pop di		; DI -> parameter set
		mov bx,PXENV_GET_CACHED_INFO
		call pxenv
		jc .err

		popad
		mov cx,[pxe_bootp_query_pkt.buffersize]
		ret

.err:
		mov si,err_pxefailed
		call writestr_early
		call writehex4
		call crlf
		jmp kaboom

		section .data
get_packet_msg	db 'Getting cached packet', 0

		section .text
;
; ip_ok
;
; Tests an IP address in EAX for validity; return with ZF=1 for bad.
; We used to refuse class E, but class E addresses are likely to become
; assignable unicast addresses in the near future.
;
ip_ok:
		push ax
		cmp eax,-1		; Refuse the all-ones address
		jz .out
		and al,al		; Refuse network zero
		jz .out
		cmp al,127		; Refuse loopback
		jz .out
		and al,0F0h
		cmp al,224		; Refuse class D
.out:
		pop ax
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
; DNSServers	- DNS server IPs
; LocalDomain	- Local domain name
; MACLen, MAC	- Client identifier, if MACLen == 0
;
; This assumes the DHCP packet is in "trackbuf" and the length
; of the packet in in CX on entry.
;

parse_dhcp:
		mov byte [OverLoad],0		; Assume no overload
		mov eax, [trackbuf+bootp.yip]
		call ip_ok
		jz .noyip
		mov [MyIP], eax
.noyip:
		mov eax, [trackbuf+bootp.sip]
		and eax, eax
		call ip_ok
		jz .nosip
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
; For parse_some_dhcp_options, DH contains the minimum value for
; the option to recognize -- this is used to restrict parsing to
; PXELINUX-specific options only.
;
parse_dhcp_options:
		xor dx,dx

parse_some_dhcp_options:
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

		cmp dl,dh	; Is the option value valid?
		jb .opt_done

		mov bx,dhcp_option_list
.find_option:
		cmp bx,dhcp_option_list_end
		jae .opt_done
		cmp dl,[bx]
		je .found_option
		add bx,3
		jmp .find_option
.found_option:
		pushad
		call [bx+1]
		popad

; Fall through
		; Unknown option.  Skip to the next one.
.opt_done:
		add si,ax
		jmp .loop
.done:
		ret

		section .data
dhcp_option_list:
		section .text

%macro dopt 2
		section .data
		db %1
		dw dopt_%2
		section .text
dopt_%2:
%endmacro

;
; Parse individual DHCP options.  SI points to the option data and
; AX to the option length.  DL contains the option number.
; All registers are saved around the routine.
;
	dopt 1, subnet_mask
		mov ebx,[si]
		mov [Netmask],ebx
		ret

	dopt 3, router
		mov ebx,[si]
		mov [Gateway],ebx
		ret

	dopt 6, dns_servers
		mov cx,ax
		shr cx,2
		cmp cl,DNS_MAX_SERVERS
		jna .oklen
		mov cl,DNS_MAX_SERVERS
.oklen:
		mov di,DNSServers
		rep movsd
		mov [LastDNSServer],di
		ret

	dopt 15, local_domain
		mov bx,si
		add bx,ax
		xor ax,ax
		xchg [bx],al	; Zero-terminate option
		mov di,LocalDomain
		call dns_mangle	; Convert to DNS label set
		mov [bx],al	; Restore ending byte
		ret

	dopt 43, vendor_encaps
		mov dh,208	; Only recognize PXELINUX options
		mov cx,ax	; Length of option = max bytes to parse
		call parse_some_dhcp_options	; Parse recursive structure
		ret

	dopt 52, option_overload
		mov bl,[si]
		mov [OverLoad],bl
		ret

	dopt 54, server
		mov eax,[si]
		cmp dword [ServerIP],0
		jne .skip		; Already have a next server IP
		call ip_ok
		jz .skip
		mov [ServerIP],eax
.skip:		ret

	dopt 61, client_identifier
		cmp ax,MAC_MAX		; Too long?
		ja .skip
		cmp ax,2		; Too short?
		jb .skip
		cmp [MACLen],ah		; Only do this if MACLen == 0
		jne .skip
		push ax
		lodsb			; Client identifier type
		cmp al,[MACType]
		pop ax
		jne .skip		; Client identifier is not a MAC
		dec ax
		mov [MACLen],al
		mov di,MAC
		jmp dhcp_copyoption
.skip:		ret

	dopt 67, bootfile_name
		mov di,BootFile
		jmp dhcp_copyoption

	dopt 97, uuid_client_identifier
		cmp ax,17		; type byte + 16 bytes UUID
		jne .skip
		mov dl,[si]		; Must have type 0 == UUID
		or dl,[HaveUUID]	; Capture only the first instance
		jnz .skip
		mov byte [HaveUUID],1	; Got UUID
		mov di,UUIDType
		jmp dhcp_copyoption
.skip:		ret

	dopt 209, pxelinux_configfile
		mov di,ConfigName
		or byte [DHCPMagic],2	; Got config file
		jmp dhcp_copyoption

	dopt 210, pxelinux_pathprefix
		mov di,PathPrefix
		or byte [DHCPMagic],4	; Got path prefix
		jmp dhcp_copyoption

	dopt 211, pxelinux_reboottime
		cmp al,4
		jne .done
		mov ebx,[si]
		xchg bl,bh		; Convert to host byte order
		rol ebx,16
		xchg bl,bh
		mov [RebootTime],ebx
		or byte [DHCPMagic],8	; Got RebootTime
.done:		ret

		; Common code for copying an option verbatim
		; Copies the option into ES:DI and null-terminates it.
		; Returns with AX=0 and SI past the option.
dhcp_copyoption:
		xchg cx,ax	; CX <- option length
		rep movsb
		xchg cx,ax	; AX <- 0
		stosb		; Null-terminate
		ret

		section .data
dhcp_option_list_end:
		section .text

		section .data
HaveUUID	db 0
uuid_dashes	db 4,2,2,2,6,0	; Bytes per UUID dashed section
		section .text

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
		popad
		ret

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "getc.inc"		; getc et al
%include "conio.inc"		; Console I/O
%include "writestr.inc"		; String output
writestr_early	equ writestr
%include "writehex.inc"		; Hexadecimal output
%include "configinit.inc"	; Initialize configuration
%include "parseconfig.inc"	; High-level config file handling
%include "parsecmd.inc"		; Low-level config file handling
%include "bcopy32.inc"		; 32-bit bcopy
%include "loadhigh.inc"		; Load a file into high memory
%include "font.inc"		; VGA font stuff
%include "graphics.inc"		; VGA graphics
%include "highmem.inc"		; High memory sizing
%include "strcpy.inc"		; strcpy()
%include "rawcon.inc"		; Console I/O w/o using the console functions
%include "dnsresolv.inc"	; DNS resolver
%include "idle.inc"		; Idle handling
%include "pxeidle.inc"		; PXE-specific idle mechanism
%include "adv.inc"		; Auxillary Data Vector

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data

copyright_str   db ' Copyright (C) 1994-'
		asciidec YEAR
		db ' H. Peter Anvin et al', CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: press a key to retry, or wait for reset...', CR, LF, 0
bailmsg		equ err_bootfailed
err_nopxe	db "No !PXE or PXENV+ API found; we're dead...", CR, LF, 0
err_pxefailed	db 'PXE API call failed, error ', 0
err_udpinit	db 'Failed to initialize UDP stack', CR, LF, 0
err_noconfig	db 'Unable to locate configuration file', CR, LF, 0
err_damage	db 'TFTP server sent an incomprehesible reply', CR, LF, 0
found_pxenv	db 'Found PXENV+ structure', CR, LF, 0
apiver_str	db 'PXE API version is ',0
pxeentry_msg	db '!PXE entry point found (we hope) at ', 0
pxenventry_msg	db 'PXENV+ entry point found (we hope) at ', 0
viaplan_msg	db ' via plan '
plan		db 'A', CR, LF, 0
trymempxe_msg	db 'Scanning memory for !PXE structure... ', 0
trymempxenv_msg	db 'Scanning memory for PXENV+ structure... ', 0
undi_data_msg	db 'UNDI data segment at ',0
undi_code_msg	db 'UNDI code segment at ',0
len_msg		db ' len ', 0
cant_free_msg	db 'Failed to free base memory, error ', 0
notfound_msg	db 'not found', CR, LF, 0
myipaddr_msg	db 'My IP address seems to be ',0
tftpprefix_msg	db 'TFTP prefix: ', 0
localboot_msg	db 'Booting from local disk...', CR, LF, 0
trying_msg	db 'Trying to load: ', 0
default_str	db 'default', 0
syslinux_banner	db CR, LF, 'PXELINUX ', VERSION_STR, ' ', DATE_STR, ' ', 0
cfgprefix	db 'pxelinux.cfg/'		; No final null!
cfgprefix_len	equ ($-cfgprefix)

; This one we make ourselves
bootif_str	db 'BOOTIF='
bootif_str_len	equ $-bootif_str
;
; Config file keyword table
;
%include "keywords.inc"

;
; Extensions to search for (in *forward* order).
; (.bs and .bss are disabled for PXELINUX, since they are not supported)
;
		alignz 4
exten_table:	db '.cbt'		; COMBOOT (specific)
		db '.0', 0, 0		; PXE bootstrap program
		db '.com'		; COMBOOT (same as DOS)
		db '.c32'		; COM32
exten_table_end:
		dd 0, 0			; Need 8 null bytes here

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
		section .bss
pxe_bootp_query_pkt:
.status:	resw 1			; Status
.packettype:	resw 1			; Boot server packet type
.buffersize:	resw 1			; Packet size
.buffer:	resw 2			; seg:off of buffer
.bufferlimit:	resw 1			; Unused

		section .data
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

%if GPXE

gpxe_file_api_check:
.status:	dw 0			; Status
.size:		dw 20			; Size in bytes
.magic:		dd 0x91d447b2		; Magic number
.provider:	dd 0
.apimask:	dd 0
.flags:		dd 0

gpxe_file_open:
.status:	dw 0			; Status
.filehandle:	dw 0			; FileHandle
.filename:	dd 0			; seg:off of FileName
.reserved:	dd 0

gpxe_get_file_size:
.status:	dw 0			; Status
.filehandle:	dw 0			; FileHandle
.filesize:	dd 0			; FileSize

gpxe_file_read:
.status:	dw 0			; Status
.filehandle:	dw 0			; FileHandle
.buffersize:	dw 0			; BufferSize
.buffer:	dd 0			; seg:off of buffer

%endif ; GPXE

;
; Misc initialized (data) variables
;
		alignz 4
BaseStack	dd StackBuf		; ESP of base stack
		dw 0			; SS of base stack
NextSocket	dw 49152		; Counter for allocating socket numbers
KeepPXE		db 0			; Should PXE be kept around?

;
; TFTP commands
;
tftp_tail	db 'octet', 0				; Octet mode
tsize_str	db 'tsize' ,0				; Request size
tsize_len	equ ($-tsize_str)
		db '0', 0
blksize_str	db 'blksize', 0				; Request large blocks
blksize_len	equ ($-blksize_str)
		asciidec TFTP_LARGEBLK
		db 0
tftp_tail_len	equ ($-tftp_tail)

		alignz 2
;
; Options negotiation parsing table (string pointer, string len, offset
; into socket structure)
;
tftp_opt_table:
		dw tsize_str, tsize_len, tftp_filesize
		dw blksize_str, blksize_len, tftp_blksize
tftp_opts	equ ($-tftp_opt_table)/6

;
; Error packet to return on TFTP protocol error
; Most of our errors are OACK parsing errors, so use that error code
;
tftp_proto_err	dw TFTP_ERROR				; ERROR packet
		dw TFTP_EOPTNEG				; ERROR 8: OACK error
		db 'TFTP protocol error', 0		; Error message
tftp_proto_err_len equ ($-tftp_proto_err)

		alignz 4
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
		alignz 4
BufSafe		dw trackbufsize/TFTP_BLOCKSIZE	; Clusters we can load into trackbuf
BufSafeBytes	dw trackbufsize		; = how many bytes?
%ifndef DEPEND
%if ( trackbufsize % TFTP_BLOCKSIZE ) != 0
%error trackbufsize must be a multiple of TFTP_BLOCKSIZE
%endif
%endif
