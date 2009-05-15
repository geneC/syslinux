; -*- fundamental -*- ---------------------------------------------------
;
;   Copyright 2007-2009 H. Peter Anvin - All Rights Reserved
;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;   Boston MA 02111-1307, USA; either version 2 of the License, or
;   (at your option) any later version; incorporated herein by reference.
;
; -----------------------------------------------------------------------

;
; rllpack.inc
;
; Very simple RLL compressor/decompressor, used to pack binary structures
; together.
;
; Format of leading byte
; 1-128		= x verbatim bytes follow
; 129-223	= (x-126) times subsequent byte
; 224-255	= (x-224)*256+(next byte) times the following byte
; 0		= end of data
;
; These structures are stored *in reverse order* in high memory.
; High memory pointers point to one byte beyond the end.
;

		section .text16

;
; rllpack:
;	Pack ECX bytes from ESI into EDI.
;	Returns updated ESI and EDI.
;
rllpack:
		extern pm_rllpack
		push dword pm_rllpack
		call pm_call
		ret
;
; rllunpack:
;	Unpack bytes from ESI into EDI
;	On return ESI, EDI are updated and
;	ECX contains number of bytes output.
;
rllunpack:
		extern pm_rllunpack
		push dword pm_rllunpack
		call pm_call
		ret
