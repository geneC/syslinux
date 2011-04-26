;; -----------------------------------------------------------------------
;;
;;   Copyright 2003-2009 H. Peter Anvin - All Rights Reserved
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
;; cmdline.inc
;;
;; Common routine to assemble [null-terminated] command line into
;; real_mode_seg:cmd_line_here.
;; Not used by plain kernel due to BOOT_IMAGE= etc.
;;

		section .text16

;
; Assumes DS == CS
;
make_plain_cmdline:
		push es
		; ui.inc has already copied any APPEND options
		mov ax,real_mode_seg
		mov es,ax

		mov di,[CmdLinePtr]
		pm_call do_sysappend

		mov si,[CmdOptPtr]

		call strcpy

		dec di
		mov [CmdLinePtr],di
		mov byte [es:di],0		; Null-terminate

		pop es
		ret

