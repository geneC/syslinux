;; -----------------------------------------------------------------------
;;
;;   Copyright 2002-2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; config.inc
;;
;; Common configuration options.  Some of these are imposed by the kernel.
;;

%ifndef _CONFIG_INC
%define _CONFIG_INC

max_cmd_len	equ 511			; Must be &3; 255 is the kernel limit
HIGHMEM_MAX	equ 037FFFFFFh		; DEFAULT highest address for an initrd
DEFAULT_BAUD	equ 9600		; Default baud rate for serial port
BAUD_DIVISOR	equ 115200		; Serial port parameter
MAX_FKEYS	equ 12			; Number of F-key help files

%assign	DO_WBINVD 0			; Should we use WBINVD or not?

;
; Version number definitinons
;
%ifndef DEPEND				; Generated file
%include "version.gen"
%endif

;
; Should be updated with every release to avoid bootsector/SYS file mismatch
;
%define	version_str	VERSION		; Must be 4 characters long!
%define date		DATE_STR	; Defined from the Makefile
%define	year		'2008'

%endif ; _CONFIG_INC
