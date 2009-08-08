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

;
; Set to 1 to disable switching to a private stack
;
%assign USE_PXE_PROVIDED_STACK 0	; Use stack provided by PXE?

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
                global trackbuf
trackbufsize	equ 8192
trackbuf	resb trackbufsize	; Track buffer goes here
		; ends at 2800h

		; These fields save information from before the time
		; .bss is zeroed... must be in .earlybss
		global InitStack
InitStack	resd 1

		section .bss16
                global Files
		alignb open_file_t_size
Files		resb MAX_OPEN*open_file_t_size

		alignb FILENAME_MAX
                global BootFile, PathPrefix, DotQuadBuf, IPOption
BootFile	resb 256		; Boot file from DHCP packet
PathPrefix	resb 256		; Path prefix derived from boot file
DotQuadBuf	resb 16			; Buffer for dotted-quad IP address
IPOption	resb 80			; ip= option buffer
PXEStack	resd 1			; Saved stack during PXE call

		alignb 4
                global DHCPMagic, OverLoad, RebootTime, APIVer, RealBaseMem
                global StructPtr
RebootTime	resd 1			; Reboot timeout, if set by option
StrucPtr	resw 2			; Pointer to PXENV+ or !PXE structure
APIVer		resw 1			; PXE API version found
LocalBootType	resw 1			; Local boot return code
RealBaseMem	resw 1			; Amount of DOS memory after freeing
OverLoad	resb 1			; Set if DHCP packet uses "overloading"
DHCPMagic	resb 1			; PXELINUX magic flags

; The relative position of these fields matter!
                global MACStr, MACLen, MACType, MAC, BOOTIFStr
MAC_MAX		equ  32			; Handle hardware addresses this long
MACLen		resb 1			; MAC address len
MACType		resb 1			; MAC address type
MAC		resb MAC_MAX+1		; Actual MAC address
BOOTIFStr	resb 7			; Space for "BOOTIF="
MACStr		resb 3*(MAC_MAX+1)	; MAC address as a string

; The relative position of these fields matter!
                global UUID, UUIDType
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

		section .bss16
                global packet_buf
		alignb 16
packet_buf	resb 2048		; Transfer packet
packet_buf_size	equ $-packet_buf

		section .text16
		;
		; PXELINUX needs more BSS than the other derivatives;
		; therefore we relocate it from 7C00h on startup.
		;
StackBuf	equ $-44		; Base of stack if we use our own
StackTop	equ StackBuf

		; PXE loads the whole file, but assume it can't be more
		; than (384-31)K in size.
MaxLMA		equ 384*1024

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
; do fs initialize
;
                extern pxe_fs_ops
	        mov eax,pxe_fs_ops
                pm_call fs_init
                jmp next

;
; Plan C, because it must be in real mode, we set it here
;
                global plan_c
plan_c:
                
		; Plan C: PXENV+ structure via INT 1Ah AX=5650h
		mov ax, 5650h
%if USE_PXE_PROVIDED_STACK == 0
		lss sp,[InitStack]
%endif
		int 1Ah			; May trash regs
%if USE_PXE_PROVIDED_STACK == 0
		lss esp,[BaseStack]
%endif
		sti			; Work around Etherboot bug
                ret


next:
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
; Load configuration file
;
                pm_call load_config

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
                global no_config, kaboom
; set the no_config kaboom here
no_config:
                mov si, err_noconfig
                call writestr_early
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
                global pxenv
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
                global PXEEntry
PXEEntry	equ pxenv.jump+1

		section .bss16
		alignb 2
PXEStatus	resb 2


;
; TimeoutTable: list of timeouts (in 18.2 Hz timer ticks)
;
; This is roughly an exponential backoff...
;
		section .data16
TimeoutTable:
		db 2, 2, 3, 3, 4, 5, 6, 7, 9, 10, 12, 15, 18
		db 21, 26, 31, 37, 44, 53, 64, 77, 92, 110, 132
		db 159, 191, 229, 255, 255, 255, 255
TimeoutTableEnd	equ $

		section .text16


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



; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "common.inc"		; Universal modules
%include "writestr.inc"		; String output
writestr_early	equ writestr
%include "writehex.inc"		; Hexadecimal output
%include "rawcon.inc"		; Console I/O w/o using the console functions
%include "dnsresolv.inc"	; DNS resolver
%include "pxeidle.inc"		; PXE-specific idle mechanism

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data16

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
; (.bs and .bss16 are disabled for PXELINUX, since they are not supported)
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
		section .bss16
                global pxe_bootp_query_pkt, pxe_udp_write_pkt
                global pxe_udp_open_pkt, pxe_udp_read_pkt
pxe_bootp_query_pkt:
.status:	resw 1			; Status
.packettype:	resw 1			; Boot server packet type
.buffersize:	resw 1			; Packet size
.buffer:	resw 2			; seg:off of buffer
.bufferlimit:	resw 1			; Unused

pxe_udp_open_pkt:
.status:	resw 1			; Status
.sip:		resd 1			; Source (our) IP

pxe_udp_close_pkt:
.status:	resw 1			; Status

pxe_udp_write_pkt:
.status:	resw 1			; Status
.sip:		resd 1			; Server IP
.gip:		resd 1			; Gateway IP
.lport:		resw 1			; Local port
.rport:		resw 1			; Remote port
.buffersize:	resw 1			; Size of packet
.buffer:	resw 2			; seg:off of buffer

pxe_udp_read_pkt:
.status:	resw 1			; Status
.sip:		resd 1			; Source IP
.dip:		resd 1			; Destination (our) IP
.rport:		resw 1			; Remote port
.lport:		resw 1			; Local port
.buffersize:	resw 1			; Max packet size
.buffer:	resw 2			; seg:off of buffer

%if GPXE

		section .data16
                global gpxe_file_api_check
gpxe_file_api_check:
.status:	dw 0			; Status
.size:		dw 20			; Size in bytes
.magic:		dd 0x91d447b2		; Magic number
.provider:	dd 0
.apimask:	dd 0
.flags:		dd 0

		section .bss16
                global gpxe_file_read, gpxe_get_file_size
                global gpxe_file_open

gpxe_file_open:
.status:	resw 1			; Status
.filehandle:	resw 1			; FileHandle
.filename:	resd 1			; seg:off of FileName
.reserved:	resd 1

gpxe_get_file_size:
.status:	resw 1			; Status
.filehandle:	resw 1			; FileHandle
.filesize:	resd 1			; FileSize

gpxe_file_read:
.status:	resw 1			; Status
.filehandle:	resw 1			; FileHandle
.buffersize:	resw 1			; BufferSize
.buffer:	resd 1			; seg:off of buffer

%endif ; GPXE

;
; Misc initialized (data) variables
;
		section .data16

		alignz 4
                global BaseStack
BaseStack	dd StackTop		; ESP of base stack
		dw 0			; SS of base stack
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
                global tftp_proto_err, tftp_proto_err_len
tftp_proto_err	dw TFTP_ERROR				; ERROR packet
		dw TFTP_EOPTNEG				; ERROR 8: OACK error
		db 'TFTP protocol error', 0		; Error message
tftp_proto_err_len equ ($-tftp_proto_err)

		alignz 4
                global ack_packet_buf
ack_packet_buf:	dw TFTP_ACK, 0				; TFTP ACK packet

;
; IP information (initialized to "unknown" values)
                global MyIP, ServerIP, Netmask, Gateway, ServerPort
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
