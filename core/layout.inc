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

		; Use .bss for things that doesn't have to be in low memory;
		; with .bss1 and .bss2 to offload.  .earlybss should be used
		; for things that absolutely have to be below 0x7c00.
		section .bss		write nobits align=16

%if 0 ; IS_PXELINUX
		; Warning here: RBFG build 22 randomly overwrites
		; memory location [0x5680,0x576c), possibly more.  It
		; seems that it gets confused and screws up the
		; pointer to its own internal packet buffer and starts
		; writing a received ARP packet into low memory.
		section .rbfg		write nobits
RBFG_brainfuck:	resb 2048		; Bigger than an Ethernet packet...
%endif

		section .bss2		write nobits align=16

		section .text		exec write progbits align=16
		section .bcopyxx	exec write progbits align=16
		section .data		write progbits align=16

		section .adv		write nobits align=512

		; .uibss contains bss data which is guaranteed to be
		; safe to clobber during the loading of the image.  This
		; is because while loading the primary image we will clobber
		; the spillover from the last fractional sector load.
		section .uibss		write nobits align=16

		; Normal bss...
		section .bss1		write nobits align=16

		; Symbols from linker script
%macro SECINFO 1
		extern __%1_start, __%1_lma, __%1_end
		extern __%1_len, __%1_dwords
%endmacro
		SECINFO config
		SECINFO replacestub

		global _start

		section .text

;
; Segment assignments in the bottom 640K
; Keep the low-memory footprint as small as possible... overrun is a hard
; failure!
;
; 0000h - main code/data segment (and BIOS segment)

xfer_buf_seg	equ 1000h
aux_seg		equ 2000h

serial_buf_size	equ 4096		; Should be a power of 2

;
; Contents of aux_seg
;
		struc aux
.fontbuf	resb 8192
.serial		resb serial_buf_size

		alignb 4096		; Align the next segment to 4K
		endstruc

aux_seg_end	equ aux_seg + (aux_size >> 4)

;
; Bounce buffer for I/O to high mem
; Note: we keep all the segments page-aligned, even if that probably
; is somewhat excessive.  Sector alignment is obligatory, however.
;

%if IS_ISOLINUX
; ISOLINUX doesn't have a block cache yet
real_mode_seg	equ aux_seg_end
%else
cache_seg	equ aux_seg_end		; 64K area for metadata cache
real_mode_seg	equ cache_seg + 1000h

pktbuf_seg	equ cache_seg		; PXELINUX packet buffers
%endif

comboot_seg	equ real_mode_seg	; COMBOOT image loading zone
