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
;; loadhigh.inc
;;
;; Load a file into high memory
;;

		section .text16

;
; load_high:	loads (the remainder of) a file into high memory.
;
;		Assumes CS == DS.
;
; Inputs:	SI  = file handle/cluster pointer
;		EDI = target address in high memory
;		EAX = maximum number of bytes to load
;		DX  = zero-padding mask (e.g. 0003h for pad to dword)
;		BX  = subroutine to call at the top of each loop
;                     (to print status and check for abort)
;		[MyHighMemSize] = maximum load address
;
; Outputs:	SI  = file handle/cluster pointer
;		EBX = first untouched address (not including padding)
;		EDI = first untouched address (including padding)
;		CF  = reached EOF
;
		extern pm_load_high
load_high:
		push ebp
		mov ebp,[MyHighMemSize]
		pm_call pm_load_high
		pop ebp
		jo .overflow
		ret

.overflow:	mov si,err_nohighmem
		jmp abort_load

		section .data16
err_nohighmem   db CR, LF
		db 'Not enough memory to load specified image.', CR, LF, 0

		section .bss16
		alignb 2
PauseBird	resw 1

		section .text16
