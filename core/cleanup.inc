;; -----------------------------------------------------------------------
;;
;;   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; cleanup.inc
;;
;; Some final tidying before jumping to a kernel or bootsector
;;

		section .text16
;
; cleanup_hardware:
;
;	Shut down anything transient.  *No segment assumptions*.
;	Preserves all registers.
;
cleanup_hardware:
		pushad
;
; Linux wants the floppy motor shut off before starting the kernel,
; at least bootsect.S seems to imply so.  If we don't load the floppy
; driver, this is *definitely* so!
;
		xor ax,ax
		xor dx,dx
		int 13h

%if 0		; This bug report has not been substantiated!
; Vmware crashes if we scroll in the decompressor!  Try to detect vmware
; and if it is Vmware, clear the screen...
		mov eax,'VMXh'
		xor ebx, ebx
		mov ecx, 10		; Get version
		mov dx, 'VX'
		in eax, dx
		cmp ebx, 'VMXh'
		jne .no_vmware

		mov ax,0x0003		; Set mode (clear screen/home cursor)
		int 10h
.no_vmware:
%endif

		call comboot_cleanup_api

		call timer_cleanup

		popad

		; If we enabled serial port interrupts, clean them up now
		jmp sirq_cleanup
