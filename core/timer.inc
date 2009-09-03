;; -----------------------------------------------------------------------
;;
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;;   Boston MA 02110-1301, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; timer.inc
;;
;; Very simple counting timer
;;
;; This lets us have a simple incrementing variable without worrying
;; about the BIOS_timer variable wrapping around at "midnight" and other
;; weird things.
;;

		section .text16

timer_init:
		; Hook INT 1Ch
		mov eax,[BIOS_timer_hook]
		mov [BIOS_timer_next],eax
		mov dword [BIOS_timer_hook],timer_irq
		ret

timer_cleanup:
		; Unhook INT 1Ch
		mov eax,[BIOS_timer_next]
		mov [BIOS_timer_hook],eax
		ret

timer_irq:
		inc dword [cs:__jiffies]
		jmp 0:0
BIOS_timer_next	equ $-4

		section .bss16
		global __jiffies
__jiffies	resd 1			; The actual timer variable
