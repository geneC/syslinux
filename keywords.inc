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
;; keywords.inc
;;
;; Common header file for the handling of keyword hash and macros
;;

%ifndef DEPEND		; Generated file
%include "kwdhash.gen"
%endif

%macro keyword 2
	dd hash_%1	; Hash value
	dw %2		; Entrypoint
%endmacro

keywd_size	equ 6	; Bytes per keyword
