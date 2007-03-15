;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2006 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; keywords.inc
;;
;; Common header file for the handling of keyword hash and macros
;;

%ifndef DEPEND		; Generated file
%include "kwdhash.gen"
%endif

%macro keyword 2
	dd hash_%1	; Hash value
	dw 0		; No argument
	dw %2		; Entrypoint
%endmacro

%macro keyword 3
	dd hash_%1	; Hash value
	dw %3		; 16-bit argument
	dw %2		; Entrypoint
%endmacro

%macro keyword 4
	dd hash_%1	; Hash value
	db %3, %4	; 2 8-bit arguments
	dw %2		; Entrypoint
%endmacro

keywd_size	equ 8	; Bytes per keyword

		align 4, db 0

%define FKeyN(n) (FKeyName+(((n)-1) << FILENAME_MAX_LG2))

keywd_table:
		keyword menu,		pc_comment
		keyword text,		pc_text
		keyword append,		pc_append
		keyword default,	pc_default
		keyword display,	pc_filecmd,	get_msg_file
		keyword font,		pc_filecmd,	loadfont
		keyword implicit,	pc_setint16,	AllowImplicit
		keyword kbdmap,		pc_filecmd,	loadkeys
		keyword kernel,		pc_kernel,	VK_KERNEL
		keyword linux,		pc_kernel,	VK_LINUX
		keyword boot,		pc_kernel,	VK_BOOT
		keyword bss,		pc_kernel,	VK_BSS
		keyword pxe,		pc_kernel,	VK_PXE
		keyword fdimage,	pc_kernel,	VK_FDIMAGE
		keyword comboot,	pc_kernel,	VK_COMBOOT
		keyword com32,		pc_kernel,	VK_COM32
		keyword config,		pc_kernel,	VK_CONFIG
		keyword label,		pc_label
		keyword prompt,		pc_setint16,	ForcePrompt
		keyword say,		pc_say
		keyword serial,		pc_serial
		keyword console,	pc_setint16,	DisplayCon
		keyword timeout,	pc_timeout,	KbdTimeout
		keyword totaltimeout,	pc_timeout,	TotalTimeout
		keyword ontimeout,	pc_ontimeout
		keyword onerror,	pc_onerror
		keyword allowoptions,	pc_setint16,	AllowOptions
		keyword noescape,	pc_setint16,	NoEscape
		keyword f1,		pc_fkey,	FKeyN(1)
		keyword f2,		pc_fkey,	FKeyN(2)
		keyword f3,		pc_fkey,	FKeyN(3)
		keyword f4,		pc_fkey,	FKeyN(4)
		keyword f5,		pc_fkey,	FKeyN(5)
		keyword f6,		pc_fkey,	FKeyN(6)
		keyword f7,		pc_fkey,	FKeyN(7)
		keyword f8,		pc_fkey,	FKeyN(8)
		keyword f9,		pc_fkey,	FKeyN(9)
		keyword f10,		pc_fkey,	FKeyN(10)
		keyword f0,		pc_fkey,	FKeyN(10)
%if IS_PXELINUX
		keyword ipappend,	pc_ipappend
%endif
%if IS_PXELINUX || IS_ISOLINUX
		keyword localboot,	pc_localboot
%endif

keywd_count	equ ($-keywd_table)/keywd_size
