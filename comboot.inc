;; $Id$
;; -----------------------------------------------------------------------
;;   
;;   Copyright 1994-2002 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; comboot.inc
;; 
;; Common code for running a COMBOOT image
;;

; Looks like a COMBOOT image but too large
comboot_too_large:
		mov si,err_comlarge
		call cwritestr
		jmp enter_command

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

		call comboot_setup_api

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
		mov [SavedSSSP+2],ss	; Save away SS:SP

		mov ax,es
		mov ds,ax
		mov ss,ax
		xor sp,sp
		push word 0		; Return to address 0 -> exit

		jmp comboot_seg:100h	; Run it

; Proper return vector
comboot_return:	cli			; Don't trust anyone
		xor ax,ax
		jmp comboot_exit

;
; Set up the COMBOOT API interrupt vectors.  This is also used
; by the COM32 code.
;
comboot_setup_api:
		mov di,4*0x20		; DOS interrupt vectors
		mov eax,comboot_return	; INT 20h = exit
		stosd
		mov ax,comboot_int21	; INT 21h = DOS-compatible syscalls
		stosd
		mov ax,comboot_int22	; INT 22h = proprietary syscalls
		stosd
		mov ax,comboot_bogus
		mov cx,29		; All remaining DOS vectors
		rep stosd
		ret

; INT 21h: generic DOS system call
comboot_int21:	push ds
		push cs
		pop ds			; Set DS <- CS
		and ah,ah		; 00 = return
		je comboot_return
		cmp ah,02h
		jb comboot_getkeyecho	; 01 = get key with echo
		je comboot_writechr	; 02 = writechr
		cmp ah,08h		; 08 = get key w/o echo
		je comboot_getkey
		cmp ah,09h		; 09 = writestr
		je comboot_writestr
		cmp ah,0Bh		; 0B = check keyboard
		je comboot_checkkey
		cmp ah,30h		; 30 = check version
		je comboot_checkver
		cmp ah,4Ch		; 4C = return with status
		je comboot_return

		; Otherwise fall through to comboot_bogus

; Attempted to execute non-21h DOS system call
comboot_bogus:	cli			; Don't trust anyone
		mov ax,err_notdos
;
; Generic COMBOOT return to command line code
;  AX -> message (if any)
;  BX -> where to go next
;
comboot_exit:
		mov bx,enter_command	; Normal return to command prompt
comboot_exit_special:
		xor dx,dx
		mov ds,dx
		mov es,dx
		lss sp,[SavedSSSP]
		sti
		cld
		and ax,ax
		je .nomsg
		mov si,KernelCName
		call cwritestr
		xchg si,ax
		call cwritestr
.nomsg:		jmp bx

;
; INT 21h system calls
;
comboot_getkeyecho:			; 01 = get key with echo
		call vgashowcursor
		call getchar
		call vgahidecursor
		call writechr
		jmp comboot_resume_ok

comboot_writechr:			; 02 = writechr
		xchg ax,dx
		call writechr
		xchg ax,dx
		jmp comboot_resume_ok

comboot_getkey:				; 08 = get key w/o echo
		call vgashowcursor
		call getchar
		call vgahidecursor
		jmp comboot_resume_ok

comboot_writestr:			; 09 = write string
		pusha
		push es
		mov bp,sp
		mov es,[bp-20]		; Old DS
		mov si,dx
.loop:		es lodsb
		cmp al,'$'		; End string with $ - bizarre
		je .done
		call writechr
		jmp short .loop
.done:		pop es
		popa
		jmp comboot_resume_ok

comboot_checkkey:			; 0B = check keyboard status
		call pollchar
		setz al
		dec al			; AL = 0FFh if present, 0 if not
		jmp comboot_resume_ok

comboot_checkver:			; 30 = check DOS version
		; We return 0 in all DOS-compatible version registers,
		; but the high part of eax-ebx-ecx-edx spell "SYSLINUX"
		mov eax,'SY' << 16
		mov ebx,'SL' << 16
		mov ecx,'IN' << 16
		mov edx,'UX' << 16
		;jmp comboot_resume_ok

;
; Resume comboot execution
;
comboot_resume_ok:
		clc
comboot_resume:
		pop ds
		iret

comboot_apierr:
		stc
		jmp comboot_resume

;
; INT 22h - SYSLINUX-specific system calls
;           System call number in ax
;
comboot_int22:
		push ds
		push cs
		pop ds

		cmp ax,int22_count
		jae comboot_apierr

		xchg ax,bx
		add bx,bx
		call [bx+int22_table]
		jmp comboot_resume		; On return

;
; INT 22h AX=0000h	Null system call
; INT 22h AX=0001h	Get SYSLINUX version
;
comapi_get_version:
		; Number of API functions supported
		mov ax,int22_count
		; SYSLINUX version
		mov cx,(VER_MAJOR << 8)+VER_MINOR
		; SYSLINUX derivative ID byte
		mov dx,my_id
		; For future use
		xor bx,bx

		push cs
		pop es
		; ES:SI -> version banner
		mov si,syslinux_banner
		; ES:DI -> copyright string
		mov di,copyright_str

comapi_nop:
		clc
		ret

;
; INT 22h AX=0002h	Write string
;
; Write null-terminated string in ES:BX
;
comapi_writestr:
		push es
		pop ds
		mov si,ax
		jmp writestr			; Write string from ES:BX

;
; INT 22h AX=0003h	Run command
;
; Terminates the COMBOOT program and executes the command line in
; ES:BX as if it had been entered by the user.
;
comapi_run:
		push es
		push ds
		pop es
		pop ds
		mov si,ax
		mov di,command_line
.copyloop:
		lodsb
		stosb
		and al,al
		jnz .copyloop
		xor ax,ax
		mov bx,load_kernel		; Run a new kernel
		jmp comboot_exit_special	; Terminate task, clean up

;
; INT 22h AX=0004h	Run default command		
;
; Terminates the COMBOOT program and executes the default command line
; as if a timeout had happened or the user pressed <Enter>.
;
comapi_run_default:
		mov bx,auto_boot
		jmp comboot_exit_special

;
; INT 22h AX=0005h	Force text mode
;
; Puts the video in standard text mode
;
comapi_textmode:
		call vgaclearmode
		clc
		ret

;
; INT 22h AX=0006h	Open file
;
comapi_open:
		push ds
		push ds
		push es
		pop ds
		pop es
		mov di,InitRD
		push di
		call mangle_name
		pop di
		pop ds
		call searchdir
		jz .err
		xchg eax,edx
		shr eax,16
		xchg ax,dx
		mov cx,[ClustSize]
		clc
		ret
.err:
		stc
		ret


;
; INT 22h AX=0007h	Read file
;
comapi_read:
		xchg ax,bx
		call getfssec
		jnc .noteof
		xor si,si		; SI <- 0 on EOF, CF <- 0
.noteof:	ret

;
; INT 22h AX=0008h	Close file
;
comapi_close:
		; Do nothing for now.  Eventually implement
		; an internal API for this.
		clc
		ret


		align 2, db 0
int22_table:
		dw comapi_nop			; 0000 null syscall
		dw comapi_get_version		; 0001 get SYSLINUX version
		dw comapi_writestr		; 0002 write string
		dw comapi_run			; 0003 run specified command
		dw comapi_run_default		; 0004 run default command
		dw comapi_textmode		; 0005 force text mode
		dw comapi_open			; 0006 open file
		dw comapi_read			; 0007 read file
		dw comapi_close			; 0008 close file
int22_count	equ ($-int22_table)/2
