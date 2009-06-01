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
;; highmem.inc
;;
;; Probe for the size of high memory.  This can be overridden by a
;; mem= command on the command line while booting a new kernel.
;;

		section .text16

;
; This is set up as a subroutine; it will set up the global variable
; HighMemSize.  All registers are preserved.
;
highmemsize:
		push es
		pushfd
		pushad

		push cs
		pop es

;
; First, try INT 15:E820 (get BIOS memory map)
;
; Note: we may have to scan this multiple times, because some (daft) BIOSes
; report main memory as multiple contiguous ranges...
;
get_e820:
		mov dword [E820Max],-(1 << 20)	; Max amount of high memory
		mov dword [E820Mem],(1 << 20)	; End of detected high memory
.start_over:
		mov di,E820Buf
		xor ax,ax
		mov cx,10
		rep stosw			; Clear buffer
		xor ebx,ebx			; Start with first record
		jmp short .do_e820		; Skip "at end" check first time!
.int_loop:	and ebx,ebx			; If we're back at beginning...
		jz .e820_done			; ... we're done
.do_e820:	mov eax,0000E820h
		mov edx,534D4150h		; "SMAP" backwards
		xor ecx,ecx
		mov cl,20			; ECX <- 20 (size of buffer)
		mov di,E820Buf
		int 15h
		jnc .no_carry
		; If carry, ebx == 0 means error, ebx != 0 means we're done
		and ebx,ebx
		jnz .e820_done
		jmp no_e820
.no_carry:
		cmp eax,534D4150h
		jne no_e820
		cmp cx,20
		jb no_e820

;
; Look for a memory block starting at <= 1 MB and continuing upward
;
		cmp dword [E820Buf+4], byte 0
		ja .int_loop			; Start >= 4 GB?
		mov eax, [E820Buf]
		cmp dword [E820Buf+16],1
		je .is_ram			; Is it memory?
		;
		; Non-memory range.  Remember this as a limit; some BIOSes get the length
		; of primary RAM incorrect!
		;
.not_ram:
		cmp eax, (1 << 20)
		jb .int_loop			; Starts in lowmem region
		cmp eax,[E820Max]
		jae .int_loop			; Already above limit
		mov [E820Max],eax		; Set limit
		jmp .int_loop

.is_ram:
		cmp eax,[E820Mem]
		ja .int_loop			; Not contiguous with our starting point
		add eax,[E820Buf+8]
		jc .overflow
		cmp dword [E820Buf+12],0
		je .nooverflow
.overflow:
		or eax,-1
.nooverflow:
		cmp eax,[E820Mem]
		jbe .int_loop			; All is below our baseline
		mov [E820Mem],eax
		jmp .start_over			; Start over in case we find an adjacent range

.e820_done:
		mov eax,[E820Mem]
		cmp eax,[E820Max]
		jna .not_limited
		mov eax,[E820Max]
.not_limited:
		cmp eax,(1 << 20)
		ja got_highmem			; Did we actually find memory?
		; otherwise fall through

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
		add eax,(1 << 20)		; First megabyte
got_highmem:
%if HIGHMEM_SLOP != 0
		sub eax,HIGHMEM_SLOP
%endif
		mov [HighMemSize],eax
		popad
		popfd
		pop es
		ret				; Done!

		section .bss16
		alignb 4
E820Buf		resd 5			; INT 15:E820 data buffer
E820Mem		resd 1			; Memory detected by E820
E820Max		resd 1			; Is E820 memory capped?
; HighMemSize is defined in com32.inc
