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
NULLFILE	equ 0			; Zero byte == null file name
NULLOFFSET	equ 4			; Position in which to look
REBOOT_TIME	equ 5*60		; If failure, time until full reset
%assign HIGHMEM_SLOP 128*1024		; Avoid this much memory near the top
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
TFTP_ACK	equ htons(4)		; ACK packet
TFTP_ERROR	equ htons(5)		; ERROR packet

;
; TFTP error codes
;
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
                global kaboom
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


		section .text16
;
; Invoke INT 1Ah on the PXE stack.  This is used by the "Plan C" method
; for finding the PXE entry point.
;
                global pxe_int1a
pxe_int1a:
%if USE_PXE_PROVIDED_STACK == 0
		mov [cs:PXEStack],sp
		mov [cs:PXEStack+2],ss
		lss sp,[cs:InitStack]
%endif
		int 1Ah			; May trash registers
%if USE_PXE_PROVIDED_STACK == 0
		lss sp,[cs:PXEStack]
%endif
		ret

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
cant_free_msg	db 'Failed to free base memory, error ', 0
localboot_msg	db 'Booting from local disk...', CR, LF, 0
syslinux_banner	db CR, LF, 'PXELINUX ', VERSION_STR, ' ', DATE_STR, ' ', 0

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
pxe_udp_close_pkt:
.status:	resw 1			; Status

pxe_udp_read_pkt:
.status:        resw 1                  ; Status
.sip:           resd 1                  ; Source IP
.dip:           resd 1                  ; Destination (our) IP
.rport:         resw 1                  ; Remote port
.lport:         resw 1                  ; Local port
.buffersize:    resw 1                  ; Max packet size
.buffer:        resw 2                  ; seg:off of buffer


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
; IP information (initialized to "unknown" values)
                global MyIP
MyIP		dd 0			; My IP address 
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
