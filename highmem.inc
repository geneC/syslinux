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
;; highmem.inc
;; 
;; Probe for the size of high memory.  This can be overridden by a
;; mem= command on the command line while booting a new kernel.
;;

;
; This is set up as a subroutine; it will set up the global variable
; HighMemSize.  All registers are preserved.  Assumes DS == CS.
;
highmemsize:
		push es
		pushad

;
; First, try INT 15:E820 (get BIOS memory map)
;
get_e820:
		xor ebx,ebx			; Start with first record
		mov es,bx			; Need ES = DS = 0 for now
		jmp short .do_e820		; Skip "at end" check first time!
.int_loop:	and ebx,ebx			; If we're back at beginning...
		jz no_e820			; ... bail; nothing found
.do_e820:	mov eax,0000E820h
		mov edx,534D4150h		; "SMAP" backwards
		xor ecx,ecx
		mov cl,20			; ECX <- 20
		mov di,E820Buf
		int 15h
		jc no_e820
		cmp eax,534D4150h
		jne no_e820
;
; Look for a memory block starting at <= 1 MB and continuing upward
;
		cmp dword [E820Buf+4], byte 0
		ja .int_loop			; Start >= 4 GB?
		mov edx, (1 << 20)
		sub edx, [E820Buf]
		jb .int_loop			; Start >= 1 MB?
		stc
		sbb eax,eax			; eax <- 0xFFFFFFFF
		cmp dword [E820Buf+12], byte 0
		ja .huge			; Size >= 4 GB
		mov eax, [E820Buf+8]
.huge:		sub eax, edx			; Adjust size to start at 1 MB
		jbe .int_loop			; Completely below 1 MB?

		; Now EAX contains the size of memory 1 MB...up
		cmp dword [E820Buf+16], byte 1
		jne near err_nohighmem		; High memory isn't usable memory!!!!

		; We're good!
		jmp short got_highmem_add1mb	; Still need to add low 1 MB

;
; INT 15:E820 failed.  Try INT 15:E801.
;
no_e820:
		mov ax,0e801h			; Query high memory (semi-recent)
		int 15h
		jc no_e801
		cmp ax,3c00h
		ja no_e801			; > 3C00h something's wrong with this call
		jb e801_hole			; If memory hole we can only use low part

		mov ax,bx
		shl eax,16			; 64K chunks
		add eax,(16 << 20)		; Add first 16M
		jmp short got_highmem				

;
; INT 15:E801 failed.  Try INT 15:88.
;
no_e801:
		mov ah,88h			; Query high memory (oldest)
		int 15h
		cmp ax,14*1024			; Don't trust memory >15M
		jna e801_hole
		mov ax,14*1024
e801_hole:
		and eax,0ffffh
		shl eax,10			; Convert from kilobytes
got_highmem_add1mb:
		add eax,(1 << 20)		; First megabyte
got_highmem:
%if HIGHMEM_SLOP != 0
		sub eax,HIGHMEM_SLOP
%endif
		mov [HighMemSize],eax
		popad
		pop es
		ret				; Done!
