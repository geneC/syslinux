		section .text
		org 8000h
;
; 32-bit bcopy routine for real mode
;
; We enter protected mode, set up a flat 32-bit environment, run rep movsd
; and then exit.  IMPORTANT: This code assumes cs == ss == 0.
;
; This code is probably excessively anal-retentive in its handling of
; segments.
;
bcopy_main:	jmp short bcopy
	
bcopy_gdt_ptr:	dw bcopy_gdt_size-1
		dd bcopy_gdt

		align 4
bcopy_gdt:	dd 0			; Null descriptor
		dd 0
		dd 0000ffffh		; Code segment, use16, readable,
		dd 00009b00h		; present, dpl 0, cover 64K
		dd 0000ffffh		; Data segment, use16, read/write,
		dd 008f9300h		; present, dpl 0, cover all 4G
		dd 0000ffffh		; Data segment, use16, read/write,
		dd 00009300h		; present, dpl 0, cover 64K
bcopy_gdt_size:	equ $-bcopy_gdt

bcopy:
		push eax
		push gs
		push fs
		push ds
		push es
		lgdt [bcopy_gdt_ptr]

		mov eax,cr0
		or eax,byte 1
		cli
		mov cr0,eax		; Enter protected mode
		jmp 8:.in_pm

.in_pm:		xor ax,ax		; Null selector
		mov fs,ax
		mov gs,ax

		mov al,16		; Data segment selector
		mov es,ax
		mov ds,ax

		mov al,24		; "Real-mode-like" data segment
		mov ss,ax
	
		a32 rep movsd		; Do our business
		
		mov es,ax		; Set to "real-mode-like"
		mov ds,ax
		mov fs,ax
		mov gs,ax	
	
		mov eax,cr0
		and eax,0fffffffeh
		mov cr0,eax		; Disable protected mode
		jmp 0:.in_rm

.in_rm:		xor ax,ax		; Back in real mode
		mov ss,ax
		sti
		pop es
		pop ds
		pop fs
		pop gs
		pop eax
		ret
