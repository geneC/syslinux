;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; comboot.inc
;;
;; Common code for running a COMBOOT image
;;
		section .text16

; Parameter registers definition; this is the definition
; of the stack frame used by INT 21h and INT 22h.
%define		P_FLAGS		word [bp+44]
%define		P_FLAGSL	byte [bp+44]
%define		P_FLAGSH	byte [bp+45]
%define		P_CS		word [bp+42]
%define		P_IP		word [bp+40]
%define		P_CSIP		dword [bp+40]
%define		P_DS		word [bp+38]
%define		P_ES		word [bp+36]
%define		P_FS		word [bp+34]
%define		P_GS		word [bp+32]
%define		P_EAX		dword [bp+28]
%define		P_AX		word [bp+28]
%define		P_HAX		word [bp+30]
%define		P_AL		byte [bp+28]
%define		P_AH		byte [bp+29]
%define		P_ECX		dword [bp+24]
%define		P_CX		word [bp+24]
%define		P_HCX		word [bp+26]
%define		P_CL		byte [bp+24]
%define		P_CH		byte [bp+25]
%define		P_EDX		dword [bp+20]
%define		P_DX		word [bp+20]
%define		P_HDX		word [bp+22]
%define		P_DL		byte [bp+20]
%define		P_DH		byte [bp+21]
%define		P_EBX		dword [bp+16]
%define		P_BX		word [bp+16]
%define		P_HBX		word [bp+18]
%define		P_BL		byte [bp+16]
%define		P_BH		byte [bp+17]
%define		P_EBP		dword [bp+8]
%define		P_BP		word [bp+8]
%define		P_HBP		word [bp+10]
%define		P_ESI		dword [bp+4]
%define		P_SI		word [bp+4]
%define		P_HSI		word [bp+6]
%define		P_EDI		dword [bp]
%define		P_DI		word [bp]
%define		P_HDI		word [bp+2]

;
; Set up the COMBOOT API interrupt vectors.  This is now done at
; initialization time.
;
comboot_setup_api:
		mov di,DOSErrTramp	; Error trampolines
		mov cx,32
		push cx
		mov eax,02EB206Ah	; push 20h; jmp $+4
.loop1:		stosd
		inc ah
		loop .loop1
		dec di
		mov byte [di-1],0E9h
		mov ax,comboot_bogus-2
		sub ax,di
		stosw

		pop cx			; CX <- 32
		mov si,4*20h		; DOS interrupt vectors
		mov bx,comboot_vectors
		mov di,DOSSaveVectors
.loop2:
		movsd
		movzx eax,word [bx]
		inc bx
		inc bx
		mov [si-4],eax
		loop .loop2
		ret

;
; Restore the original state of the COMBOOT API vectors, and free
; any low memory allocated by the comboot module.
;
		global comboot_cleanup_api
comboot_cleanup_api:
		pusha
		mov si,DOSSaveVectors
		mov di,4*20h
		mov cx,20h
		rep movsd		; Restore DOS-range vectors
		popa
		ret

		section .bss16
		alignb 4
DOSSaveVectors	resd 32

		section .data16
%define comboot_err(x) (DOSErrTramp+4*((x)-20h))

comboot_vectors:
		dw comboot_return	; INT 20 = exit
		dw comboot_int21	; INT 21 = DOS-compatible system calls
		dw comboot_int22	; INT 22 = native system calls
		dw comboot_err(23h)	; INT 23 = DOS Ctrl-C handler
		dw comboot_err(24h)	; INT 24 = DOS critical error handler
		dw comboot_err(25h)	; INT 25 = DOS absolute disk read
		dw comboot_err(26h)	; INT 26 = DOS absolute disk write
		dw comboot_err(27h)	; INT 27 = DOS TSR
		dw comboot_int28	; INT 28 = DOS idle interrupt
		dw comboot_int29	; INT 29 = DOS fast console output
		dw comboot_err(2Ah)	; INT 2A = DOS network API (NetBIOS)
		dw comboot_err(2Bh)	; INT 2B = DOS reserved
		dw comboot_err(2Ch)	; INT 2C = DOS reserved
		dw comboot_iret		; INT 2D = DOS reserved, AMIS
		dw comboot_err(2Eh)	; INT 2E = DOS run command
		dw comboot_iret		; INT 2F = DOS multiplex interrupt
		dw comboot_err(30h)	; INT 30 = DOS CP/M system calls
		dw comboot_err(31h)	; INT 31 = DPMI
		dw comboot_err(32h)	; INT 32 = DOS reserved
		dw comboot_iret		; INT 33 = DOS mouse API
		dw comboot_err(34h)	; INT 34 = DOS FPU emulation
		dw comboot_err(35h)	; INT 35 = DOS FPU emulation
		dw comboot_err(36h)	; INT 36 = DOS FPU emulation
		dw comboot_err(37h)	; INT 37 = DOS FPU emulation
		dw comboot_err(38h)	; INT 38 = DOS FPU emulation
		dw comboot_err(39h)	; INT 39 = DOS FPU emulation
		dw comboot_err(3Ah)	; INT 3A = DOS FPU emulation
		dw comboot_err(3Bh)	; INT 3B = DOS FPU emulation
		dw comboot_err(3Ch)	; INT 3C = DOS FPU emulation
		dw comboot_err(3Dh)	; INT 3D = DOS FPU emulation
		dw comboot_err(3Eh)	; INT 3E = DOS FPU emulation
		dw comboot_err(3Fh)	; INT 3F = DOS overlay manager

		section .text16

; INT 21h: generic DOS system call
comboot_int21:	sti
		push ds
		push es
		push fs
		push gs
		pushad
		cld
		mov bp,cs
		mov ds,bp
		mov es,bp
		mov bp,sp			; Set up stack frame

		pm_call pm_adjust_screen	; The COMBOOT program might hav changed the screen

		mov cx,int21_count
		mov si,int21_table
.again:		lodsb
		cmp al,P_AH
		lodsw
		loopne .again
		; The last function in the list is the
		; "no such function" function
		clc
		call ax			; Call the invoked function
comboot_resume:
		mov bp,sp		; In case the function clobbers BP
		setc P_FLAGSL		; Propagate CF->error
		popad
		pop gs
		pop fs
		pop es
		pop ds
comboot_iret:
		iret

comboot_bad_int21:
		mov ax,P_AX
		push P_CSIP
		push 21h
		; Fall through

; Attempted to execute invalid DOS system call
; The interrupt number is on the stack.
comboot_bogus:	pop dx			; Interrupt number
		pop edi			; CS:IP
		mov cx,err_notdos
		push comboot_bogus_tail
		jmp comboot_exit_msg
comboot_bogus_tail:
		xchg ax,dx
		pm_call pm_writehex2		; Interrupt number
		mov al,' '
		pm_call pm_writechr
		xchg ax,dx
		pm_call pm_writehex4		; Function number (AX)
		mov al,' '
		pm_call pm_writechr
		mov eax,edi
		pm_call pm_writehex8		; CS:IP of the origin
		pm_call crlf
		jmp kaboom

; Proper return vector
; Note: this gets invoked both via INT 21h and directly via INT 20h.
; We don't need to cld explicitly here, because comboot_exit does that
; when invoking RESET_STACK_AND_SEGS.
comboot_return:
		cli			; May not have a safe stack
		push enter_command	; Normal return to command prompt
		; jmp comboot_exit

;
; Generic COMBOOT return to command line code
;  stack -> where to go next
;     CX -> message (for _msg version)
;
		extern comboot_cleanup_lowmem
comboot_exit:
		xor cx,cx
comboot_exit_msg:
		pop bx			; Return address
		RESET_STACK_AND_SEGS si	; Contains sti, cld
		pm_call comboot_cleanup_lowmem
		pm_call pm_adjust_screen; The COMBOOT program might have change the screen
		jcxz .nomsg
		mov si,KernelName
		pm_call pm_writestr
		mov si,cx
		pm_call pm_writestr
.nomsg:
		jmp bx

;
; INT 21h system calls
;
comboot_getkey:				; 01 = get key with echo
		pm_call vgashowcursor
		call comboot_getchar
		pm_call vgahidecursor
		pm_call pm_writechr
		clc
		ret

comboot_writechr:			; 02 = writechr
		mov al,P_DL
		pm_call pm_writechr
		clc
		ret

comboot_writeserial:			; 04 = write serial port
		mov al,P_DL
		pm_call pm_write_serial
		clc
		ret

comboot_getkeynoecho:			; 08 = get key w/o echo
		call comboot_getchar
		clc
		ret

comboot_writestr:			; 09 = write DOS string
		mov es,P_DS
		mov si,P_DX
.loop:		es lodsb
		cmp al,'$'		; End string with $ - bizarre
		je .done
		pm_call pm_writechr
		jmp short .loop
.done:		clc
		ret

comboot_checkkey:			; 0B = check keyboard status
		cmp byte [APIKeyFlag],00h
		jnz .waiting
		pm_call pm_pollchar
.waiting:	setz al
		dec al			; AL = 0FFh if present, 0 if not
		mov P_AL,al
		clc
		ret

comboot_checkver:			; 30 = check DOS version
		; We return 0 in all DOS-compatible version registers,
		; but the high part of eax-ebx-ecx-edx spell "SYSLINUX"
		mov P_EAX,'SY' << 16
		mov P_EBX,'SL' << 16
		mov P_ECX,'IN' << 16
		mov P_EDX,'UX' << 16
		ret

comboot_getchar:
		cmp byte [APIKeyFlag],00h
		jne .queued
		pm_call pm_getchar	; If not queued get input
		and al,al		; Function key?  (CF <- 0)
		jnz .done
		mov [APIKeyWait],ah	; High part of key
		inc byte [APIKeyFlag]	; Set flag
.done:		mov P_AL,al
		ret
.queued:	mov al,[APIKeyWait]
		dec byte [APIKeyFlag]
		jmp .done

;
; INT 28h - DOS idle
;
comboot_int28:
		sti
		cld
		call do_idle
		iret

;
; INT 29h - DOS fast write character
;
comboot_int29:
		sti
		cld
		pm_call pm_writechr
		iret

;
; INT 22h - SYSLINUX-specific system calls
;           System call number in ax
;
comboot_int22:
		sti
		push ds
		push es
		push fs
		push gs
		pushad
		cld
		mov bp,cs
		mov ds,bp
		mov es,bp
		mov bp,sp			; Set up stack frame

		pm_call pm_adjust_screen	; The COMBOOT program might hav changed the screen

		cmp ax,int22_count
		jb .ok
		xor ax,ax			; Function 0 -> unimplemented
.ok:
		xchg ax,bx
		add bx,bx			; CF <- 0
		call [bx+int22_table]
		jmp comboot_resume		; On return

;
; INT 22h AX=0000h	Unimplemented call
;
comapi_err:
		stc
		ret

;
; INT 22h AX=0001h	Get SYSLINUX version
;
comapi_get_version:
		; Number of API functions supported
		mov P_AX,int22_count
		; SYSLINUX version
		mov P_CX,(VERSION_MAJOR << 8)+VERSION_MINOR
		; SYSLINUX derivative ID byte
		mov P_DX,my_id
		; For future use
		mov P_BX,cs	; cs == 0

		mov P_ES,ds
		; ES:SI -> version banner
		mov P_SI,syslinux_banner + 2	; Skip leading CR LF
		; ES:DI -> copyright string
		mov P_DI,copyright_str + 1	; Skip leading space

comapi_nop:
		clc
		ret

;
; INT 22h AX=0002h	Write string
;
; Write null-terminated string in ES:BX
;
comapi_writestr:
		mov ds,P_ES
		mov si,P_BX
		pm_call pm_writestr
		clc
		ret

;
; INT 22h AX=0003h	Run command
;
; Terminates the COMBOOT program and executes the command line in
; ES:BX as if it had been entered by the user.
;
comapi_run:
		mov es,P_ES
		mov bx,P_BX
		pm_call pm_env32_run
		ret

;
; INT 22h AX=0004h	Run default command
;
; Terminates the COMBOOT program and executes the default command line
; as if a timeout had happened or the user pressed <Enter>.
;
comapi_run_default:
		push auto_boot
		jmp comboot_exit

;
; INT 22h AX=0005h	Force text mode
;
; Puts the video in standard text mode
;
comapi_textmode:
		pm_call syslinux_force_text_mode
		clc
		ret

;
; INT 22h AX=0006h	Open file
;
comapi_open:
		mov es,P_ES
		mov si,P_SI
		pm_call pm_open_file
		mov P_EAX,eax
		mov P_CX,cx
		mov P_SI,si
		ret

;
; INT 22h AX=0007h	Read file
;
comapi_read:
		mov es,P_ES
		mov bx,P_BX
		mov si,P_SI
		mov cx,P_CX
		pm_call getfssec
		jnc .noteof
		xor si,si		; SI <- 0 on EOF, CF <- 0
.noteof:	mov P_SI,si
		mov P_ECX,ecx
		ret

;
; INT 22h AX=0008h	Close file
;
comapi_close:
		mov si,P_SI
		pm_call pm_close_file
		clc
		ret

;
; INT 22h AX=0009h	Call PXE stack
;
%if IS_PXELINUX
comapi_pxecall:
		mov bx,P_BX
		mov es,P_ES
		mov di,P_DI
		call pxenv
		mov ax,[PXEStatus]
		mov P_AX,ax
		ret
%else
comapi_pxecall	equ comapi_err			; Not available
%endif

;
; INT 22h AX=000Bh	Get Serial Console Configuration
;
comapi_serialcfg:
		pm_call pm_serialcfg
		mov P_DX,ax
		mov P_CX,cx
		mov P_BX,bx
		clc
		ret

;
; INT 22h AX=000Ch	Perform final cleanup
;
comapi_cleanup:
%if IS_PXELINUX
		; Unload PXE if requested
		test dl,3
		setnz [KeepPXE]
		sub bp,sp		; unload_pxe may move the stack around
		pm_call unload_pxe
		add bp,sp		; restore frame pointer...
%elif IS_SYSLINUX || IS_EXTLINUX
		; Restore original FDC table
		mov eax,[OrigFDCTabPtr]
		mov [fdctab],eax
%endif
		pm_call cleanup_hardware
		clc
		ret

;
; INT 22h AX=000Dh	Clean up then replace bootstrap
;
comapi_chainboot:
		ret

;
; INT 22h AX=000Eh	Get configuration file name
;
comapi_configfile:
		mov P_ES,cs
		mov P_BX,ConfigName
		clc
		ret

;
; INT 22h AX=000Fh	Get IPAPPEND strings
;
comapi_ipappend:
		mov P_ES,cs
		mov P_CX,numIPAppends
		mov P_BX,IPAppends
		clc
		ret

;
; INT 22h AX=0010h	Resolve hostname
;
%if IS_PXELINUX
                extern pm_pxe_dns_resolv
comapi_dnsresolv:
		mov ds,P_ES
		mov si,P_BX
		pm_call pm_pxe_dns_resolv
		mov P_EAX,eax
		clc
		ret
%else
comapi_dnsresolv equ comapi_err
%endif

		section .text16

;
; INT 22h AX=0011h	Obsolete
;

;
; INT 22h AX=0012h	Obsolete
;

;
; INT 22h AX=0013h	Idle call
;
comapi_idle:
		call do_idle
		clc
		ret

;
; INT 22h AX=0014h	Local boot
;
comapi_localboot:
		mov ax,P_DX
		pm_call pm_local_boot
		ret

;
; INT 22h AX=0015h	Feature flags
;
comapi_features:
		mov P_ES,cs
		mov P_BX,feature_flags
		mov P_CX,feature_flags_len
		clc
		ret

;
; INT 22h AX=0016h	Run kernel image
;
comapi_runkernel:
		ret

;
; INT 22h AX=0017h  Report video mode change
;
comapi_usingvga:
		mov ax,P_BX
		cmp ax,0Fh		; Unknown flags = failure
		ja .error
		mov cx,P_CX
		mov dx,P_DX
		pm_call pm_using_vga
		clc
		ret
.error:
		stc
		ret

;
; INT 22h AX=0018h  Query custom font
;
comapi_userfont:
		mov al,[UserFont]
		and al,al
		jz .done
		mov al,[VGAFontSize]
		pm_call pm_userfont
		mov P_ES,es
		mov P_BX,bx

.done:		; CF=0 here
		mov P_AL,al
		ret

;
; INT 22h AX=0019h  Read disk
;
%if IS_SYSLINUX || IS_ISOLINUX || IS_EXTLINUX
comapi_readdisk:
		cmp P_EDI,0		; Reserved for future expansion
		jnz .err
		mov eax,P_EDX
		mov edx,P_ESI
		mov es,P_ES
		mov bx,P_BX
		mov bp,P_CX		; WE CANNOT use P_* after touching bp!
		call getlinsec
		clc
		ret
.err:
		stc
		ret
%else
comapi_readdisk	equ comapi_err
%endif

;
; INT 22h AX=001Ah	Obsolete
;

;
; INT 22h AX=001Bh	Obsolete
;

;
; INT 22h AX=001Ch	Get pointer to auxillary data vector
;
comapi_getadv:
		mov P_ES,ds
		mov P_BX,adv0.data
		mov P_CX,ADV_LEN
		ret

;
; INT 22h AX=001Dh	Write auxillary data vector
;
comapi_writeadv	equ adv_write

;
; INT 22h AX=001Eh	Keyboard remapping table
comapi_kbdtable:
		cmp P_DX,0
		jne .err
		mov P_AX,1			; Version
		mov P_CX,256			; Length
		mov P_ES,cs
		mov P_BX,KbdMap
		ret
.err:
		stc
		ret

;
; INT 22h AX=001Fh	Get current working directory
;
comapi_getcwd:
		mov P_ES,cs
		mov P_BX,CurrentDirName
		clc
		ret

;
; INT 22h AX=0023h	Query shuffler size
;
comapi_shufsize:
		; +15 is padding to guarantee alignment
		mov P_CX,__bcopyxx_len + 15
		ret

;
; INT 22h AX=0024h	Cleanup, shuffle and boot raw
;
comapi_shufraw:
		call comapi_cleanup
		mov edi,P_EDI
		mov esi,P_ESI
		mov ecx,P_ECX
		jmp shuffle_and_boot_raw

;
; INT 22h AX=0025h	Initialize the ADV structure
;
comapi_initadv:
		call adv_init
		ret

		section .data16

%macro		int21 2
		db %1
		dw %2
%endmacro

int21_table:
		int21	00h, comboot_return
		int21	01h, comboot_getkey
		int21	02h, comboot_writechr
		int21	04h, comboot_writeserial
		int21	08h, comboot_getkeynoecho
		int21	09h, comboot_writestr
		int21	0Bh, comboot_checkkey
		int21	30h, comboot_checkver
		int21	4Ch, comboot_return
		int21	 -1, comboot_bad_int21
int21_count	equ ($-int21_table)/3

		alignz 2
int22_table:
		dw comapi_err		; 0000 unimplemented syscall
		dw comapi_get_version	; 0001 get SYSLINUX version
		dw comapi_writestr	; 0002 write string
		dw comapi_run		; 0003 run specified command
		dw comapi_run_default	; 0004 run default command
		dw comapi_textmode	; 0005 force text mode
		dw comapi_open		; 0006 open file
		dw comapi_read		; 0007 read file
		dw comapi_close		; 0008 close file
		dw comapi_pxecall	; 0009 call PXE stack
		dw comapi_err		; 000A derivative-specific info
		dw comapi_serialcfg	; 000B get serial port config
		dw comapi_cleanup	; 000C perform final cleanup
		dw comapi_err		; 000D clean up then bootstrap
		dw comapi_configfile	; 000E get name of config file
		dw comapi_ipappend	; 000F get ipappend strings
		dw comapi_dnsresolv	; 0010 resolve hostname
		dw comapi_err		; 0011 maximum shuffle descriptors
		dw comapi_err		; 0012 cleanup, shuffle and boot
		dw comapi_idle		; 0013 idle call
		dw comapi_localboot	; 0014 local boot
		dw comapi_features	; 0015 feature flags
		dw comapi_runkernel	; 0016 run kernel image
		dw comapi_usingvga	; 0017 report video mode change
		dw comapi_userfont	; 0018 query custom font
		dw comapi_readdisk	; 0019 read disk
		dw comapi_err		; 001A cleanup, shuffle and boot to pm
		dw comapi_err		; 001B cleanup, shuffle and boot to rm
		dw comapi_getadv	; 001C get pointer to ADV
		dw comapi_writeadv	; 001D write ADV to disk
		dw comapi_kbdtable	; 001E keyboard remapping table
		dw comapi_getcwd	; 001F get current working directory
		dw comapi_err		; 0020 open directory
		dw comapi_err		; 0021 read directory
		dw comapi_err		; 0022 close directory
		dw comapi_shufsize	; 0023 query shuffler size
		dw comapi_shufraw	; 0024 cleanup, shuffle and boot raw
		dw comapi_initadv	; 0025 initialize adv structure
int22_count	equ ($-int22_table)/2

APIKeyWait	db 0
APIKeyFlag	db 0

zero_string	db 0			; Empty, null-terminated string

;
; This is the feature flag array for INT 22h AX=0015h
;
; Note: PXELINUX clears the idle is noop flag if appropriate
; in pxe_detect_nic_type
;
		global feature_flags, feature_flags_len
feature_flags:
		db 1			; Have local boot, idle is not noop
feature_flags_len equ ($-feature_flags)

err_notdos	db ': attempted DOS system call INT ',0
err_comlarge	db 'COMBOOT image too large.', CR, LF, 0

		global VGAFontSize, UserFont
		alignz 2
VGAFontSize	dw 16			; Defaults to 16 byte font
UserFont	db 0			; Using a user-specified font

		section .bss16
		alignb 4
DOSErrTramp	resd	33		; Error trampolines

		global ConfigName
ConfigName	resb	FILENAME_MAX
%ifndef HAVE_CURRENTDIRNAME
		global CurrentDirName
CurrentDirName	resb	FILENAME_MAX
%endif
