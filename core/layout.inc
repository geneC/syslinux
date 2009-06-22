; -----------------------------------------------------------------------
;
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Bostom MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; layout.inc
;
; Memory layout of segments
;

		; Default to 16-bit code
		bits 16

; Memory below 0800h is reserved for the BIOS and the MBR.
BSS_START	equ 0800h

; Text starts at the load address of 07C00h.
TEXT_START	equ 7C00h

; The secondary BSS section, above the text; we really wish we could
; just make it follow .bcopy32 or hang off the end,
; but it doesn't seem to work that way.
LATEBSS_START	equ 0B800h

;
; The various sections and their relationship
;
		; Use .earlybss for things that MUST be in low memory.
		section .earlybss	nobits
		section .config		write progbits align=4
		section .replacestub	exec write progbits align=16

		; Use .bss16 for things that doesn't have to be in low memory;
		; .earlybss should be used for things that absolutely have
		; to be below 0x7c00.
		section .bss16		write nobits align=16

%if 0 ; IS_PXELINUX
		; Warning here: RBFG build 22 randomly overwrites
		; memory location [0x5680,0x576c), possibly more.  It
		; seems that it gets confused and screws up the
		; pointer to its own internal packet buffer and starts
		; writing a received ARP packet into low memory.
		section .rbfg		write nobits
RBFG_brainfuck:	resb 2048		; Bigger than an Ethernet packet...
%endif

		section .init		exec write progbits align=1
		section .text16		exec write progbits align=1
		section .textnr		exec nowrite progbits align=1
		section .bcopyxx	exec write progbits align=16
		section .data16		write progbits align=16

		section .adv		write nobits align=512

		; .uibss contains bss data which is guaranteed to be
		; safe to clobber during the loading of the image.  This
		; is because while loading the primary image we will clobber
		; the spillover from the last fractional sector load.
		section .uibss		write nobits align=16

		; Symbols from linker script
%macro SECINFO 1
		extern __%1_start, __%1_lma, __%1_end
		extern __%1_len, __%1_dwords
%endmacro
		SECINFO bss16
		SECINFO uibss
		SECINFO config
		SECINFO replacestub

		SECINFO pm_code
		SECINFO high_clear

		SECINFO bss

		extern free_high_memory

		global _start

		section .text16

;
; Segment assignments in the bottom 640K
; Keep the low-memory footprint as small as possible... overrun is a hard
; failure!
;
; 0000h - main code/data segment (and BIOS segment)
;
; This stuff really should come from the linker...
;
		global	xfer_buf_seg, core_xfer_buf
xfer_buf_seg	equ 3000h
core_xfer_buf	equ xfer_buf_seg << 4

serial_buf_size	equ 4096		; Should be a power of 2

;
; Contents of aux_seg
;
		extern aux_seg		; Actual segment assigned by linker

		struc aux
.fontbuf	resb 8192
.serial		resb serial_buf_size

		alignb 4096		; Align the next segment to 4K
		endstruc

		section .auxseg		write nobits align=16
auxseg		resb aux_size

;
; Bounce buffer for I/O to high mem
; Note: we keep all the segments page-aligned, even if that probably
; is somewhat excessive.  Sector alignment is obligatory, however.
;

		global cache_seg, core_cache_buf
cache_seg	equ 4000h		; 64K area for metadata cache
core_cache_buf	equ cache_seg << 4
real_mode_seg	equ 5000h

pktbuf_seg	equ cache_seg		; PXELINUX packet buffers

comboot_seg	equ real_mode_seg	; COMBOOT image loading zone
