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
NULLOFFSET	equ 0			; Position in which to look
REBOOT_TIME	equ 5*60		; If failure, time until full reset
%assign HIGHMEM_SLOP 128*1024		; Avoid this much memory near the top
TFTP_BLOCKSIZE_LG2 equ 9		; log2(bytes/block)
TFTP_BLOCKSIZE	equ (1 << TFTP_BLOCKSIZE_LG2)

SECTOR_SHIFT	equ TFTP_BLOCKSIZE_LG2
SECTOR_SIZE	equ TFTP_BLOCKSIZE

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
PXEStack	resd 1			; Saved stack during PXE call

		alignb 4
                global DHCPMagic, RebootTime, APIVer
RebootTime	resd 1			; Reboot timeout, if set by option
StrucPtr	resw 2			; Pointer to PXENV+ or !PXE structure
APIVer		resw 1			; PXE API version found
LocalBootType	resw 1			; Local boot return code
DHCPMagic	resb 1			; PXELINUX magic flags

		section .text16
StackBuf	equ STACK_TOP-44	; Base of stack if we use our own
StackHome	equ StackBuf

		; PXE loads the whole file, but assume it can't be more
		; than (384-31)K in size.
MaxLMA		equ 384*1024

;
; Primary entry point.
;
bootsec		equ $
_start:
		jmp 0:_start1		; Canonicalize the address and skip
					; the patch header

;
; Patch area for adding hardwired DHCP options
;
		align 4

hcdhcp_magic	dd 0x2983c8ac		; Magic number
hcdhcp_len	dd 7*4			; Size of this structure
hcdhcp_flags	dd 0			; Reserved for the future
		; Parameters to be parsed before the ones from PXE
bdhcp_offset	dd 0			; Offset (entered by patcher)
bdhcp_len	dd 0			; Length (entered by patcher)
		; Parameters to be parsed *after* the ones from PXE
adhcp_offset	dd 0			; Offset (entered by patcher)
adhcp_len	dd 0			; Length (entered by patcher)

_start1:
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

%if 0 ; debugging code only... not intended for production use
		; Clobber the stack segment, to test for specific pathologies
		mov di,STACK_BASE
		mov cx,STACK_LEN >> 1
		mov ax,0xf4f4
		rep stosw

		; Clobber the tail of the 64K segment, too
		extern __bss1_end
		mov di,__bss1_end
		sub cx,di		; CX = 0 previously
		shr cx,1
		rep stosw
%endif

		; That is all pushed onto the PXE stack.  Save the pointer
		; to it and switch to an internal stack.
		mov [InitStack],sp
		mov [InitStack+2],ss

		lss esp,[BaseStack]
		sti			; Stack set up and ready
;
; Move the hardwired DHCP options (if present) to a safe place...
;
bdhcp_copy:
		mov cx,[bdhcp_len]
		mov ax,trackbufsize/2
		jcxz .none
		cmp cx,ax
		jbe .oksize
		mov cx,ax
		mov [bdhcp_len],ax
.oksize:
		mov eax,[bdhcp_offset]
		add eax,_start
		mov si,ax
		and si,000Fh
		shr eax,4
		push ds
		mov ds,ax
		mov di,trackbuf
		add cx,3
		shr cx,2
		rep movsd
		pop ds
.none:

adhcp_copy:
		mov cx,[adhcp_len]
		mov ax,trackbufsize/2
		jcxz .none
		cmp cx,ax
		jbe .oksize
		mov cx,ax
		mov [adhcp_len],ax
.oksize:
		mov eax,[adhcp_offset]
		add eax,_start
		mov si,ax
		and si,000Fh
		shr eax,4
		push ds
		mov ds,ax
		mov di,trackbuf+trackbufsize/2
		add cx,3
		shr cx,2
		rep movsd
		pop ds
.none:

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
	        mov eax,ROOT_FS_OPS
		xor ebp,ebp
                pm_call fs_init

		section .rodata
		alignz 4
ROOT_FS_OPS:
                extern pxe_fs_ops
		dd pxe_fs_ops
		dd 0


		section .text16
;
; Initialize the idle mechanism
;
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
; Linux kernel loading code is common.  However, we need to define
; a couple of helper macros...
;

; Unload PXE stack
%define HAVE_UNLOAD_PREP
%macro	UNLOAD_PREP 0
		pm_call unload_pxe
%endmacro

;
; Load configuration file
;
                pm_call pm_load_config
		jz no_config_file

;
; Now we have the config file open.  Parse the config file and
; run the user interface.
;
%include "ui.inc"

;
; Boot to the local disk by returning the appropriate PXE magic.
; AX contains the appropriate return code.
;
local_boot:
		push cs
		pop ds
		mov [LocalBootType],ax
		call vgaclearmode
		mov si,localboot_msg
		call writestr_early
		; Restore the environment we were called with
		pm_call reset_pxe
		call cleanup_hardware
		lss sp,[InitStack]
		pop gs
		pop fs
		pop es
		pop ds
		popad
		mov ax,[cs:LocalBootType]
		cmp ax,-1			; localboot -1 == INT 18h
		je .int18
		popfd
		retf				; Return to PXE
.int18:
		popfd
		int 18h
		jmp 0F000h:0FFF0h
		hlt

;
; kaboom: write a message and bail out.  Wait for quite a while,
;	  or a user keypress, then do a hard reboot.
;
;         Note: use BIOS_timer here; we may not have jiffies set up.
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
		call do_idle
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

		; We may be removing ourselves from memory
		cmp bx,0073h		; PXENV_RESTART_TFTP
		jz .disable_timer
		cmp bx,00E5h		; gPXE PXENV_FILE_EXEC
		jnz .store_stack

.disable_timer:
		call timer_cleanup

.store_stack:
		mov [cs:PXEStack],sp
		mov [cs:PXEStack+2],ss
		lss sp,[cs:InitStack]

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

		lss sp,[cs:PXEStack]

		mov bp,sp
		and ax,ax
		setnz [bp+32]			; If AX != 0 set CF on return

		; This clobbers the AX return, but we already saved it into
		; the PXEStatus variable.
		popad

		; If the call failed, it could return.
		cmp bx,0073h
		jz .enable_timer
		cmp bx,00E5h
		jnz .pop_flags

.enable_timer:
		call timer_init

.pop_flags:
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
		mov [cs:PXEStack],sp
		mov [cs:PXEStack+2],ss
		lss sp,[cs:InitStack]

		int 1Ah			; May trash registers

		lss sp,[cs:PXEStack]
		ret

;
; Special unload for gPXE: this switches the InitStack from
; gPXE to the ROM PXE stack.
;
%if GPXE
		global gpxe_unload
gpxe_unload:
		mov bx,PXENV_FILE_EXIT_HOOK
		mov di,pxe_file_exit_hook
		call pxenv
		jc .plain

		; Now we actually need to exit back to gPXE, which will
		; give control back to us on the *new* "original stack"...
		pushfd
		push ds
		push es
		mov [PXEStack],sp
		mov [PXEStack+2],ss
		lss sp,[InitStack]
		pop gs
		pop fs
		pop es
		pop ds
		popad
		popfd
		xor ax,ax
		retf
.resume:
		cli

		; gPXE will have a stack frame looking much like our
		; InitStack, except it has a magic cookie at the top,
		; and the segment registers are in reverse order.
		pop eax
		pop ax
		pop bx
		pop cx
		pop dx
		push ax
		push bx
		push cx
		push dx
		mov [cs:InitStack],sp
		mov [cs:InitStack+2],ss
		lss sp,[cs:PXEStack]
		pop es
		pop ds
		popfd

.plain:
		ret

		section .data16
		alignz 4
pxe_file_exit_hook:
.status:	dw 0
.offset:	dw gpxe_unload.resume
.seg:		dw 0
%endif

		section .text16

; -----------------------------------------------------------------------------
;  Common modules
; -----------------------------------------------------------------------------

%include "common.inc"		; Universal modules
%include "writestr.inc"		; String output
writestr_early	equ writestr
%include "writehex.inc"		; Hexadecimal output
%include "rawcon.inc"		; Console I/O w/o using the console functions

; -----------------------------------------------------------------------------
;  Begin data section
; -----------------------------------------------------------------------------

		section .data16

copyright_str   db ' Copyright (C) 1994-'
		asciidec YEAR
		db ' H. Peter Anvin et al', CR, LF, 0
err_bootfailed	db CR, LF, 'Boot failed: press a key to retry, or wait for reset...', CR, LF, 0
bailmsg		equ err_bootfailed
localboot_msg	db 'Booting from local disk...', CR, LF, 0
syslinux_banner	db CR, LF, MY_NAME, ' ', VERSION_STR, ' ', DATE_STR, ' ', 0

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
; Misc initialized (data) variables
;
		section .data16
                global KeepPXE
KeepPXE		db 0			; Should PXE be kept around?

;
; IP information.  Note that the field are in the same order as the
; Linux kernel expects in the ip= option.
;
		section .bss16
		alignb 4
		global IPInfo
IPInfo:
.IPv4		resd 1			; IPv4 information
.MyIP		resd 1			; My IP address 
.ServerIP	resd 1
.GatewayIP	resd 1
.Netmask	resd 1
