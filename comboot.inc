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
		lss sp,[cs:SavedSSSP]
		mov ds,ax
		mov es,ax
		sti
		cld
		jmp short cb_enter

; Attempted to execute DOS system call
comboot_bogus:	cli			; Don't trust anyone
		lss sp,[cs:SavedSSSP]
		mov ds,ax
		mov es,ax
		sti
		cld
		mov si,KernelCName
		call cwritestr
		mov si,err_notdos
		call cwritestr
		jmp short cb_enter

