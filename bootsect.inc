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
		xor cx,cx
		jmp short load_bootsec
is_bss_sector:
		; Transfer the superblock
		mov cx,superblock_len
load_bootsec:
		push cx
%endif
		xchg dx,ax
		shr eax,16
		xchg dx,ax		; Now EAX = file length
		mov edi, 100000h
		call load_high
		call crlf

%if IS_SYSLINUX || IS_MDSLINUX
		pop cx
%endif

		sub edi,100000h
		push edi		; Save length

%if IS_SYSLINUX || IS_MDSLINUX
		jcxz .not_bss

		; For a BSS boot sector we have to patch.
		mov esi,superblock
		mov edi,100000h+(superblock-bootsec)
		movzx ecx,cx
		call bcopy

.not_bss:
%endif
;
; Prepare for shutting down
;
		call vgaclearmode
		xor edx,edx
		xor eax,eax
%if IS_SYSLINUX || IS_MDSLINUX
		mov dl,[bsDriveNumber]

		mov si,PartInfo
		mov di,800h-18		; Put partition info here
		push di
		mov cx,8		; 16 bytes
		rep movsw
		pop si			; DS:SI points to partition info
%elif IS_ISOLINUX
		mov dl,[DriveNo]
%elif IS_PXELINUX
		; Close the UDP stack so the PXE stack is in a known state for
		; the new NBP
		call reset_pxe
%endif

		pop ecx			; Byte count to copy

		cli
		xor ebx,ebx
		mov ds,bx
		mov es,bx
%if IS_PXELINUX
		lss sp,[InitStack]	; Reset stack to PXE original
%else
		mov esp,7C00h
		pushfd
		pushad
		push bx	; ds
		push bx ; es
		push bx ; fs
		push bx	; gs
%endif

		mov esi,100000h		; Copy from...
		mov edi,7C00h		; Copy to...

		jmp bcopy_over_self

%if IS_SYSLINUX || IS_MDSLINUX
		; Nothing
%else
is_bss_sector:
		mov si,err_bssimage
		call cwritestr
		jmp enter_command
%endif
