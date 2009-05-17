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
;; bcopy32xx.inc
;;


;
; 32-bit bcopy routine
;
; This is the actual 32-bit portion of the bcopy and shuffle and boot
; routines.  ALL THIS CODE NEEDS TO BE POSITION-INDEPENDENT, with the
; sole exception being the actual relocation code at the beginning of
; pm_shuffle_boot.
;
; It also really needs to live all in a single segment, for the
; address calculcations to actually work.
;

		bits 32
		section .bcopyxx
		align 16
bcopyxx_start	equ $
;
; pm_bcopy:
;
;	This is the protected-mode core of the "bcopy" routine.
;	Try to do aligned transfers; if the src and dst are relatively
;	misaligned, align the dst.
;
;	ECX is guaranteed to not be zero on entry.
;
;	Clobbers ESI, EDI, ECX.
;

pm_bcopy:
		push ebx
		push edx
		push eax

		cmp esi,-1
		je .bzero

		cmp esi,edi		; If source < destination, we might
		jb .reverse		; have to copy backwards

.forward:
		; Initial alignment
		mov edx,edi
		shr edx,1
		jnc .faa1
		movsb
		dec ecx
.faa1:
		mov al,cl
		cmp ecx,2
		jb .f_tiny

		shr edx,1
		jnc .faa2
		movsw
		sub ecx,2
.faa2:

		; Bulk transfer
		mov al,cl		; Save low bits
		shr ecx,2		; Convert to dwords
		rep movsd		; Do our business
		; At this point ecx == 0

		test al,2
		jz .fab2
		movsw
.fab2:
.f_tiny:
		test al,1
		jz .fab1
		movsb
.fab1:
.done:
		pop eax
		pop edx
		pop ebx
		ret

.reverse:
		std			; Reverse copy

		lea esi,[esi+ecx-1]	; Point to final byte
		lea edi,[edi+ecx-1]

		; Initial alignment
		mov edx,edi
		shr edx,1
		jc .raa1
		movsb
		dec ecx
.raa1:

		dec esi
		dec edi
		mov al,cl
		cmp ecx,2
		jb .r_tiny
		shr edx,1
		jc .raa2
		movsw
		sub ecx,2
.raa2:

		; Bulk copy
		sub esi,2
		sub edi,2
		mov al,cl		; Save low bits
		shr ecx,2
		rep movsd

		; Final alignment
.r_final:
		add esi,2
		add edi,2
		test al,2
		jz .rab2
		movsw
.rab2:
.r_tiny:
		inc esi
		inc edi
		test al,1
		jz .rab1
		movsb
.rab1:
		cld
		jmp short .done

.bzero:
		xor eax,eax

		; Initial alignment
		mov edx,edi
		shr edx,1
		jnc .zaa1
		stosb
		dec ecx
.zaa1:

		mov bl,cl
		cmp ecx,2
		jb .z_tiny
		shr edx,1
		jnc .zaa2
		stosw
		sub ecx,2
.zaa2:

		; Bulk
		mov bl,cl		; Save low bits
		shr ecx,2
		rep stosd

		test bl,2
		jz .zab2
		stosw
.zab2:
.z_tiny:
		test bl,1
		jz .zab1
		stosb
.zab1:
		jmp short .done

;
; shuffle_and_boot:
;
; This routine is used to shuffle memory around, followed by
; invoking an entry point somewhere in low memory.  This routine
; can clobber any memory outside the bcopy special area.
;
; IMPORTANT: This routine does not set up any registers.
; It is the responsibility of the caller to generate an appropriate entry
; stub; *especially* when going to real mode.
;
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
pm_shuffle:
		mov ebx,edi		; EBX <- descriptor list
		lea edx,[edi+ecx+15]	; EDX <- where to relocate our code to
		and edx,~15		; Align 16 to benefit the GDT
		call pm_bcopy
		mov edi,edx
		mov esi,bcopyxx_start
		mov ecx,bcopyxx_dwords
		lea eax,[edx+.safe-bcopyxx_start]	; Resume point
		; Relocate this code
		rep movsd
		jmp eax			; Jump to safe location
.safe:
		; Give ourselves a safe stack
		lea esp,[edx+bcopyxx_stack+bcopyxx_end-bcopyxx_start]
		add edx,bcopy_gdt-bcopyxx_start
		mov [edx+2],edx		; GDT self-pointer
		lgdt [edx]		; Switch to local GDT

		; Now for the actual shuffling...
.loop:
		mov edi,[ebx]
		mov esi,[ebx+4]
		mov ecx,[ebx+8]
		add ebx,12
		jecxz .done
		call pm_bcopy
		jmp .loop
.done:
		push ecx		; == 0, for cleaning the flags register
		and esi,esi
		jz pm_shuffle_real_mode
		popfd			; Clean the flags
		jmp edi			; Protected mode entry

		; We have a real-mode entry point, so we need to return
		; to real mode.  Note: EDX already points to the GDT.
pm_shuffle_real_mode:
		mov eax,edi
		mov [edx+PM_CS16+2],ax
		mov [edx+PM_DS16+2],ax
		shr eax,16
		mov [edx+PM_CS16+4],al
		mov [edx+PM_CS16+7],ah
		mov [edx+PM_DS16+4],al
		mov [edx+PM_DS16+7],ah
		mov eax,cr0
		and al,~1
		popfd			; Clean the flags
		; No flag-changing instructions below...
		mov dx,PM_DS16
		mov ds,edx
		mov es,edx
		mov fs,edx
		mov gs,edx
		mov ss,edx
		jmp PM_CS16:0

		align	16
; GDT descriptor entry
%macro desc 1
bcopy_gdt.%1:
PM_%1		equ bcopy_gdt.%1-bcopy_gdt
%endmacro

bcopy_gdt:
		dw bcopy_gdt_size-1	; Null descriptor - contains GDT
		dd bcopy_gdt		; pointer for LGDT instruction
		dw 0

		; TSS segment to keep Intel VT happy.  Intel VT is
		; unhappy about anything that doesn't smell like a
		; full-blown 32-bit OS.
	desc TSS
		dw 104-1, DummyTSS	; 08h 32-bit task state segment
		dd 00008900h		; present, dpl 0, 104 bytes @DummyTSS

	desc CS16
		dd 0000ffffh		; 10h Code segment, use16, readable,
		dd 00009b00h		; present, dpl 0, cover 64K
	desc DS16
		dd 0000ffffh		; 18h Data segment, use16, read/write,
		dd 00009300h		; present, dpl 0, cover 64K
	desc CS32
		dd 0000ffffh		; 20h Code segment, use32, readable,
		dd 00cf9b00h		; present, dpl 0, cover all 4G
	desc DS32
		dd 0000ffffh		; 28h Data segment, use32, read/write,
		dd 00cf9300h		; present, dpl 0, cover all 4G

bcopy_gdt_size:	equ $-bcopy_gdt

		alignz 4
bcopyxx_end	equ $			; *Must* be dword-aligned!
bcopyxx_len	equ $-bcopyxx_start
bcopyxx_dwords	equ bcopyxx_len >> 2

bcopyxx_stack	equ 128			; We want this much stack
		; The +15 is for alignment
bcopyxx_safe	equ bcopyxx_len + bcopyxx_stack + 15

;
; Space for a dummy task state segment.  It should never be actually
; accessed, but just in case it is, point to a chunk of memory that
; has a chance to not be used for anything real...
;
DummyTSS	equ 0x580

		bits 16
		section .text
