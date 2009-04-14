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
%if IS_SYSLINUX || IS_MDSLINUX
		; Transfer zero bytes
		push word 0
		jmp short load_bootsec

is_bss_sector:
		; Transfer the superblock
SuperSize	equ $+1
		push word superblock_len_fat16
%endif
load_bootsec:
		mov edi, 100000h
		mov [trackbuf+4],edi	; Copy from this address
		push edi		; Save load address
		xor dx,dx		; No padding
		mov bx,abort_check	; Don't print dots, but allow abort
		call load_high

		sub edi,100000h
		mov [trackbuf+8],edi	; Save length

		mov eax,7C00h		; Entry point
		mov [trackbuf],eax	; Copy to this address
		mov [EntryPoint],eax	; Jump to this address when done

%if IS_SYSLINUX || IS_MDSLINUX
		xchg eax,ecx		; ECX[31:16] <- 0
		pop cx

		; For a BSS boot sector we have to patch.
		mov esi,superblock
		mov edi,100000h+(superblock-bootsec)
		call bcopy
%endif

		xor edx,edx
		xor esi,esi
%if IS_SYSLINUX || IS_MDSLINUX || IS_EXTLINUX
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
%elif IS_ISOLINUX
		mov dl,[DriveNumber]
%elif IS_PXELINUX
		mov byte [KeepPXE],1	; Chainloading another NBP
		call reset_pxe
%endif
		xor bx,bx

;
; replace_bootstrap for the special case where we have exactly one
; descriptor, and it's the first entry in the trackbuf
;

replace_bootstrap_one:
		push word 1			; Length of descriptor list
		push word trackbuf		; Address of descriptor list
		; Fall through

;
; Entrypoint for "shut down and replace bootstrap" -- also invoked by
; the COMBOOT API.  This routine expects two words on the stack:
; address of the copy list (versus DS) and count.  Additionally,
; the values of ESI and EDX are passed on to the new bootstrap;
; the value of BX becomes the new DS.
;
replace_bootstrap:
		;
		; Prepare for shutting down
		;
		call vgaclearmode
		call cleanup_hardware

		;
		; Set up initial stack frame (not used by PXE if keeppxe is
		; set - we use the PXE stack then.)
		; AFTER THIS POINT ONLY .earlybss IS AVAILABLE, NOT .bss
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
		mov di,7C00h-44
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
		pop bx			; Copy from...
		pop ax			; Copy list count

		cli
		mov cx,es
		mov ss,cx
		movzx esp,di

		jmp shuffle_and_boot
