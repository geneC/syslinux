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

max_cmd_len	equ 2047		; Must be &3; 2047 is the kernel limit
HIGHMEM_MAX	equ 037FFFFFFh		; DEFAULT highest address for an initrd
DEFAULT_BAUD	equ 9600		; Default baud rate for serial port
BAUD_DIVISOR	equ 115200		; Serial port parameter
MAX_FKEYS	equ 12			; Number of F-key help files

%assign	DO_WBINVD 0			; Should we use WBINVD or not?

;
; Local boot supported
;
%assign	HAS_LOCALBOOT 1

;
; Set this to return the A20 gate to its previous state, instead of
; leaving it open.  This has caused problems, because there appear
; to be a race condition between disabling the A20 gate and trying to
; re-enter protected mode, causing the A20 gate disable to take effect
; after we have already done the A20 enabled check, with disastrous
; consequences.  Plus, there seems to be little or no demand for it.
;
%assign DISABLE_A20	0


;
; Version number definitinons
;
%include "../version.gen"

;
; Copyright year...
;
%define	year		'2008'

%endif ; _CONFIG_INC
