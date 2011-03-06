;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; bootsect.inc
;;
;; Load a boot sector (or other bootstrap program.)
;;
;; Unlike previous versions of this software, this doesn't require that
;; the length is 512 bytes.  This allows PXE bootstraps and WinNT
;; "CD boot sectors" to be invoked.
;;

;
; Load a boot sector
;
is_bootsector:
%if IS_SYSLINUX
		; Transfer zero bytes
		push word 0
		jmp short load_bootsec

is_bss_sector:
		; Transfer the superblock
SuperSize	equ $+1
		push word superblock_len_fat16
%endif
load_bootsec:
		mov edi,free_high_memory
		mov [trackbuf+4],edi	; Copy from this address
		xor dx,dx		; No padding
		mov bx,abort_check	; Don't print dots, but allow abort
		call load_high

		sub edi,free_high_memory
		mov [trackbuf+8],edi	; Save length

		mov eax,7C00h		; Entry point
		mov [trackbuf],eax	; Copy to this address

%if IS_SYSLINUX
		xor ecx,ecx
		pop cx

		; For a BSS boot sector we have to patch.
		mov esi,superblock
		mov edi,free_high_memory+(superblock-bootsec)
		call bcopy
%endif
		push eax		; Save entry point

		xor edx,edx
		xor esi,esi
%if IS_SYSLINUX || IS_EXTLINUX
		; Restore original FDC table
		mov eax,[OrigFDCTabPtr]
		mov [fdctab],eax

		mov dl,[DriveNumber]
		mov si,PartInfo		; Partition info buffer
		mov di,800h-18		; Put partition info here
		push di
		mov cx,8		; 16 bytes
		xor ax,ax
		rep movsw
		pop si			; DS:SI points to partition info
		xor bx,bx
%elif IS_ISOLINUX
		mov dl,[DriveNumber]
		xor bx,bx
%elif IS_PXELINUX
		mov byte [KeepPXE],03h	; Chainloading + keep PXE
		pm_call reset_pxe
		lfs si,[InitStack]
		; Put restore DS, EDX and ESI to the true initial values
		mov bx,[fs:si+6]
		mov edx,[fs:si+28]
		mov esi,[fs:si+12]
%endif

;
; replace_bootstrap for the special case where we have exactly one
; descriptor, based in low memory.  We will generate a second descriptor
; to clear remaining FBM.
;

replace_bootstrap_one:
		mov eax,[trackbuf]		; Base address
		add eax,[trackbuf+8]		; Length
		movzx ecx,word [BIOS_fbm]
		shl ecx,10			; Free Base Memory
		sub ecx,eax
		mov [trackbuf+12],eax
		or dword [trackbuf+16],-1	; Zero memory
		mov [trackbuf+20],ecx
		push word 2			; Length of descriptor list
		; Fall through

;
; Entrypoint for "shut down and replace bootstrap" -- also invoked by
; the COMBOOT API.  This routine expects the entry point (CS, IP) and the
; count of the descriptor sequence on the stack; the shuffle
; descriptors start at the first byte of the trackbuf.
;
; The registers EDX and ESI are passed on to the called program,
; and BX is passed on as DS.
;
replace_bootstrap:
		;
		; Prepare for shutting down
		;
		call vgaclearmode

;
; We jump here when loading a kernel image, so that we don't reset
; the screen mode in "quiet" mode
;
replace_bootstrap_noclearmode:
		call cleanup_hardware

		;
		; Set up initial stack frame (not used by PXE if keeppxe is
		; set - we use the PXE stack then.)
		;
		xor ax,ax
		mov ds,ax
		mov es,ax

%if IS_PXELINUX
		cmp byte [KeepPXE],0
		je .stdstack
		les di,[InitStack]	; Reset stack to PXE original
		jmp .stackok
%endif
.stdstack:
		; StackBuf is guaranteed to have 44 bytes free immediately
		; above it, and it will not interfere with our existing stack.
		mov di,StackBuf
		push di
		mov cx,22		; 44 bytes
		rep stosw
		pop di
.stackok:

		mov [es:di+28],edx	; New EDX
		mov [es:di+12],esi	; New ESI
		mov [es:di+6],bx	; New DS

%if IS_PXELINUX == 0
		; DON'T DO THIS FOR PXELINUX...
		; For PXE, ES:BX -> PXENV+, and this would corrupt
		; that use.

		; Restore ES:DI -> $PnP (if we were ourselves called
		; that way...)
		mov ax,[OrigESDI]
		mov bx,[OrigESDI+2]

		mov [es:di+8],ax	; New DI
		mov [es:di+4],bx	; New ES
%endif
		pop ax			; descriptor list entries count

		push di
		push es

		push ds
		pop es

		mov ebx,trackbuf
		imul di,ax,12
		push di			; length of list
		add di,bx		; DI <- end of list

		; Terminating entry...
		lea eax,[replace_stub]	; Entrypoint
		push ax
		stosd
		xor ax,ax		; EAX[31:16] == 0 already
		stosd			; 16-bit mode
		stosd			; End of list

		; Copy the stub
		pop di
		mov si,__replacestub_lma
		mov cx,__replacestub_dwords
		rep movsd

		; ECX <- final list length
		xor ecx,ecx
		pop cx			; original length in bytes
		add cx, 12		; + termination entry size

		pop word [replace_stub.ss]
		pop word [replace_stub.esp]
		pop dword [replace_stub.csip]

		cli
		mov ss,[replace_stub.ss]
		mov esp,[replace_stub.esp]

		mov edi,trackbuf
		mov esi,edi

		jmp shuffle_and_boot_raw

		; This stub gets run after the shuffle.  It is copied
		; below 0x7c00 in order to properly handle the case
		; of bootstrap replacement.
		section .replacestub
replace_stub:
		mov cr0,eax
		jmp 0:.next
.next:
		mov ax,strict word 0
.ss		equ $-2
		mov ss,ax
		mov esp,strict dword 0
.esp		equ $-4
		pop gs
		pop fs
		pop es
		pop ds
		popad
		popfd
		jmp 0:0
.csip		equ $-4

		section .text16
