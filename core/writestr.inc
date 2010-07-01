;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; writestr.inc
;;
;; Code to write a simple string.
;;

;
; crlf: Print a newline
;
crlf:		push ax
		mov al,CR
		call writechr
		mov al,LF
		call writechr
		pop ax
		ret

;
; writestr: write a null-terminated string to the console, saving
;            registers on entry.
;
; Note: writestr_early and writestr are distinct in
; SYSLINUX and EXTLINUX, but not PXELINUX and ISOLINUX
;
writestr:
		pushfd
                pushad
.top:		lodsb
		and al,al
                jz .end
		call writechr
                jmp short .top
.end:		popad
		popfd
                ret
