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
;; loadhigh.inc
;;
;; Load a file into high memory
;;

		section .text

;
; load_high:	loads (the remainder of) a file into high memory.
;		This routine prints dots for each 64K transferred, and
;		calls abort_check periodically.
;
;		The xfer_buf_seg is used as a bounce buffer.
;
;		Assumes CS == DS.
;
;		The input address (EDI) should be dword aligned, and the final
;		stretch is padded with zeroes if necessary.
;
; Inputs:	SI  = file handle/cluster pointer
;		EDI = target address in high memory
;		EAX = maximum number of bytes to load
;		DX  = zero-padding mask (e.g. 0003h for pad to dword)
;		BX  = subroutine to call at the top of each loop
;                     (to print status and check for abort)
;		MyHighMemSize = maximum load address
;
; Outputs:	SI  = file handle/cluster pointer
;		EBX = first untouched address (not including padding)
;		EDI = first untouched address (including padding)
;		CF  = reached EOF
;
load_high:
		push es				; <AAA> ES

		mov cx,xfer_buf_seg
		mov es,cx
		mov [PauseBird],bx

.read_loop:
		and si,si			; If SI == 0 then we have end of file
		jz .eof
		call [PauseBird]

		push eax			; <A> Total bytes to transfer
		cmp eax,(1 << 16)		; Max 64K in one transfer
		jna .size_ok
		mov eax,(1 << 16)
.size_ok:
		push eax			; <B> Bytes transferred this chunk
		add eax,SECTOR_SIZE-1
		shr eax,SECTOR_SHIFT		; Convert to sectors

		; Now (e)ax contains the number of sectors to get
		push edi			; <C> Target buffer
		mov cx,ax
		xor bx,bx			; ES:0
		call getfssec			; Load the data into xfer_buf_seg
		pop edi				; <C> Target buffer
		pushf				; <C> EOF status
		lea ebx,[edi+ecx]		; End of data
.fix_slop:
		test cx,dx
		jz .noslop
		; The last dword fractional - pad with zeroes
		; Zero-padding is critical for multi-file initramfs.
		mov byte [es:ecx],0
		inc cx
		jmp short .fix_slop
.noslop:
		lea eax,[edi+ecx]
		cmp eax,[MyHighMemSize]
		ja .overflow

		push esi			; <D> File handle/cluster pointer
		mov esi,(xfer_buf_seg << 4)	; Source address
		call bcopy			; Copy to high memory
		pop esi				; <D> File handle/cluster pointer
		popf				; <C> EOF status
		pop ecx				; <B> Byte count this round
		pop eax				; <A> Total bytes to transfer
		jc .eof
		sub eax,ecx
		jnz .read_loop			; More to read... (if ZF=1 then CF=0)
.eof:
		pop es				; <AAA> ES
		ret

.overflow:	mov si,err_nohighmem
		jmp abort_load

		section .data
err_nohighmem   db CR, LF
		db 'Not enough memory to load specified image.', CR, LF, 0

		section .bss
		alignb 2
PauseBird	resw 1

		section .text
