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
;; bcopy32.inc
;;
;; 32-bit bcopy routine for real mode
;;

;
; 32-bit bcopy routine for real mode
;
; We enter protected mode, set up a flat 32-bit environment, run rep movsd
; and then exit.  IMPORTANT: This code assumes cs == 0.
;
; This code is probably excessively anal-retentive in its handling of
; segments, but this stuff is painful enough as it is without having to rely
; on everything happening "as it ought to."
;

		bits 16
		section .text

;
; bcopy:
;	32-bit copy, overlap safe
;
; Inputs:
;	ESI	- source pointer (-1 means do bzero rather than bcopy)
;	EDI	- target pointer
;	ECX	- byte count
;	DF	- zero
;
; Outputs:
;	ESI	- first byte after source (garbage if ESI == -1 on entry)
;	EDI	- first byte after target
;
bcopy:		jecxz .ret
		pushad
		push word pm_bcopy
		call simple_pm_call
		popad
		add edi,ecx
		add esi,ecx
.ret:		ret

;
; shuffle_and_boot_raw:
;	The new version of shuffle and boot.
; Inputs:
;	ESI		-> Pointer to list of (dst, src, len) pairs(*)
;	EDI		-> Pointer to safe area for list + shuffler
;			   (must not overlap this code nor the RM stack)
;	ECX		-> Byte count of list area (for initial copy)
;
;     If src == -1: then the memory pointed to by (dst, len) is bzeroed;
;		    this is handled inside the bcopy routine.
;
;     If len == 0:  this marks the end of the list; dst indicates
;		    the entry point and src the mode (0 = pm, 1 = rm)
;
shuffle_and_boot_raw:
		push word pm_shuffle
		call simple_pm_call
		; Never returns...
		jmp kaboom

;
; This routine is used to invoke a simple routine in 32-bit protected
; mode (with 32-bit zero-based CS, DS, ES, and SS, with ESP pointing to the
; real-mode stack even if the real-mode stack was in a nonzero SS.)
;
; No interrupt thunking services are provided; interrupts are disabled
; for the duration of the routine.  Don't run for too long at a time
; unless you really mean it.
;
; Inputs:
;	On stack	- pm entrypoint (IP only)
;	EAX, EBP preserved until real-mode exit
;	EBX, ECX, EDX, ESI and EDI passed to the called routine
;
; Outputs:
;	EAX, EBP restored from real-mode entry
;	All other registers as returned from called function
;	PM entrypoint cleaned off stack
;
simple_pm_call:
		push eax
		push ebp
		movzx ebp,sp		; BP is used as frame pointer
		pushfd			; Saves, among others, the IF flag
		push ds
		push es
		push fs
		push gs

		cli
		call enable_a20

		mov byte [cs:bcopy_gdt.TSS+5],89h	; Mark TSS unbusy

		; Convert the stack segment to a base
		xor eax,eax
		mov ax,ss
		shl eax,4
		add ebp,eax		; EBP is now an absolute frame ptr

		; Save the old segmented stack pointer
		mov [cs:.rm_esp],esp
		mov [cs:.rm_ss],ss

		o32 lgdt [cs:bcopy_gdt]
		mov eax,cr0
		or al,1
		mov cr0,eax		; Enter protected mode
		jmp PM_CS32:.in_pm

		bits 32
.in_pm:
		mov ax,PM_DS32
		mov ss,eax
		lea esp,[ebp-8*4-2*4]	; Flat mode stack
		mov es,eax
		mov ds,eax

		; Set fs, gs, tr, and ldtr in case we're on a virtual
		; machine running on Intel VT hardware -- it can't
		; deal with a partial transition, for no good reason.

		mov al,PM_DS16		; Real-mode-like segment
		mov fs,eax
		mov gs,eax
		mov al,PM_TSS		; Intel VT really doesn't want
		ltr ax			; an invalid TR and LDTR, so give
		xor eax,eax		; it something that it can use...
		lldt ax			; (sigh)

		movzx eax,word [ebp+2*4+2]
		call eax		; Call actual routine

		jmp PM_CS16:.exit
		bits 16
.exit:
		mov ax,PM_DS16		; "Real-mode-like" data segment
		mov es,eax
		mov ds,eax
		mov ss,eax

		mov eax,cr0
		and al,~1
		mov cr0,eax		; Disable protected mode
		jmp 0:.in_rm

.in_rm:		; Back in real mode
		lss esp,[cs:.rm_esp]	; Restore the stack
		pop gs
		pop fs
		pop es
		pop ds

		popfd			; Re-enables interrupts
		pop ebp
		pop eax
		ret 2			; Drops the pm entry

		section .bss
		alignb 4
.rm_esp		resd 1
.rm_ss		resw 1


		section .text
;
; Routines to enable and disable (yuck) A20.  These routines are gathered
; from tips from a couple of sources, including the Linux kernel and
; http://www.x86.org/.  The need for the delay to be as large as given here
; is indicated by Donnie Barnes of RedHat, the problematic system being an
; IBM ThinkPad 760EL.
;

		section .data
		alignz 2
A20Ptr		dw a20_dunno

		section .bss
		alignb 4
A20Test		resd 1			; Counter for testing A20 status
A20Tries	resb 1			; Times until giving up on A20

		section .text
enable_a20:
		pushad
		mov byte [cs:A20Tries],255 ; Times to try to make this work

try_enable_a20:

;
; First, see if we are on a system with no A20 gate, or the A20 gate
; is already enabled for us...
;
a20_none:
		call a20_test
		jnz a20_done
		; Otherwise, see if we had something memorized...
		jmp word [cs:A20Ptr]

;
; Next, try the BIOS (INT 15h AX=2401h)
;
a20_dunno:
a20_bios:
		mov word [cs:A20Ptr], a20_bios
		mov ax,2401h
		pushf				; Some BIOSes muck with IF
		int 15h
		popf

		call a20_test
		jnz a20_done

;
; Enable the keyboard controller A20 gate
;
a20_kbc:
		mov dl, 1			; Allow early exit
		call empty_8042
		jnz a20_done			; A20 live, no need to use KBC

		mov word [cs:A20Ptr], a20_kbc	; Starting KBC command sequence

		mov al,0D1h			; Write output port
		out 064h, al
		call empty_8042_uncond

		mov al,0DFh			; A20 on
		out 060h, al
		call empty_8042_uncond

		; Apparently the UHCI spec assumes that A20 toggle
		; ends with a null command (assumed to be for sychronization?)
		; Put it here to see if it helps anything...
		mov al,0FFh			; Null command
		out 064h, al
		call empty_8042_uncond

		; Verify that A20 actually is enabled.  Do that by
		; observing a word in low memory and the same word in
		; the HMA until they are no longer coherent.  Note that
		; we don't do the same check in the disable case, because
		; we don't want to *require* A20 masking (SYSLINUX should
		; work fine without it, if the BIOS does.)
.kbc_wait:	push cx
		xor cx,cx
.kbc_wait_loop:
		call a20_test
		jnz a20_done_pop
		loop .kbc_wait_loop

		pop cx
;
; Running out of options here.  Final attempt: enable the "fast A20 gate"
;
a20_fast:
		mov word [cs:A20Ptr], a20_fast
		in al, 092h
		or al,02h
		and al,~01h			; Don't accidentally reset the machine!
		out 092h, al

.fast_wait:	push cx
		xor cx,cx
.fast_wait_loop:
		call a20_test
		jnz a20_done_pop
		loop .fast_wait_loop

		pop cx

;
; Oh bugger.  A20 is not responding.  Try frobbing it again; eventually give up
; and report failure to the user.
;
		dec byte [cs:A20Tries]
		jnz a20_dunno		; Did we get the wrong type?

		mov si, err_a20
		jmp abort_load

		section .data
err_a20		db CR, LF, 'A20 gate not responding!', CR, LF, 0
		section .text

;
; A20 unmasked, proceed...
;
a20_done_pop:	pop cx
a20_done:	popad
		ret

;
; This routine tests if A20 is enabled (ZF = 0).  This routine
; must not destroy any register contents.
;
; The no-write early out avoids the io_delay in the (presumably common)
; case of A20 already enabled (e.g. from a previous call.)
;
a20_test:
		push es
		push cx
		push eax
		mov cx,0FFFFh			; HMA = segment 0FFFFh
		mov es,cx
		mov eax,[cs:A20Test]
		mov cx,32			; Loop count
		jmp .test			; First iteration = early out
.wait:		add eax,0x430aea41		; A large prime number
		mov [cs:A20Test],eax
		io_delay			; Serialize, and fix delay
.test:		cmp eax,[es:A20Test+10h]
		loopz .wait
.done:		pop eax
		pop cx
		pop es
		ret

;
; Routine to empty the 8042 KBC controller.  If dl != 0
; then we will test A20 in the loop and exit if A20 is
; suddenly enabled.
;
empty_8042_uncond:
		xor dl,dl
empty_8042:
		call a20_test
		jz .a20_on
		and dl,dl
		jnz .done
.a20_on:	io_delay
		in al, 064h		; Status port
		test al,1
		jz .no_output
		io_delay
		in al, 060h		; Read input
		jmp short empty_8042
.no_output:
		test al,2
		jnz empty_8042
		io_delay
.done:		ret

;
; The 32-bit copy and shuffle code is "special", so it is in its own file
;
%include "bcopyxx.inc"
