;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009-2011 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
;;   Boston MA 02110-1301, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

%ifndef _VKERNEL_INC
%define _VKERNEL_INC

;
; The following structure is used for "virtual kernels"; i.e. LILO-style
; option labels.  The options we permit here are `kernel' and `append
; Since there is no room in the bottom 64K for all of these, we
; stick them in high memory and copy them down before we need them.
;
		struc vkernel
vk_vname:	resb FILENAME_MAX	; Virtual name **MUST BE FIRST!**
vk_rname:	resb FILENAME_MAX	; Real name
vk_sysappend:	resd 1			; Sysappend option
vk_appendlen:	resw 1
vk_type:	resb 1			; Type of file
		alignb 4
vk_append:	resb max_cmd_len+1	; Command line
		alignb 4
vk_end:		equ $			; Should be <= vk_size
		endstruc

%endif ; _VKERNEL_INC
