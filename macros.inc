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
;; macros.inc
;; 
;; Convenient macros
;;

%ifndef _MACROS_INC
%define _MACROS_INC

;
; Identify the module we're compiling; the "correct" should be defined
; in the module itself to 1
;
%ifndef IS_SYSLINUX
%define IS_SYSLINUX 0
%endif
%ifndef IS_PXELINUX
%define IS_PXELINUX 0
%endif
%ifndef IS_ISOLINUX
%define IS_ISOLINUX 0
%endif

;
; For our convenience: define macros for jump-over-unconditinal jumps
;
%macro	jmpz	1
	jnz %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnz	1
	jz %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpe	1
	jne %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpne	1
	je %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpc	1
	jnc %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnc	1
	jc %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpb	1
	jnb %%skip
	jmp %1
%%skip:
%endmacro

%macro	jmpnb	1
	jb %%skip
	jmp %1
%%skip:
%endmacro

;
; Macros similar to res[bwd], but which works in the code segment (after
; section .text)
;
%macro	zb	1
	times %1 db 0
%endmacro

%macro	zw	1
	times %1 dw 0
%endmacro

%macro	zd	1
	times %1 dd 0
%endmacro

;
; Macros for network byte order of constants
;
%define htons(x)  ( ( ((x) & 0FFh) << 8 ) + ( ((x) & 0FF00h) >> 8 ) )
%define ntohs(x) htons(x)
%define htonl(x)  ( ( ((x) & 0FFh) << 24) + ( ((x) & 0FF00h) << 8 ) + ( ((x) & 0FF0000h) >> 8 ) + ( ((x) & 0FF000000h) >> 24) )
%define ntohl(x) htonl(x)

%endif ; _MACROS_INC
