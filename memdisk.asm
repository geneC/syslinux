; -*- fundamental -*- (asm-mode sucks)
; $Id$
; ****************************************************************************
;
;  memdisk.asm
;
;  A program to emulate an INT 13h disk BIOS from a "disk" in extended
;  memory.
;
;   Copyright (C) 2001  H. Peter Anvin
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
;  USA; either version 2 of the License, or (at your option) any later
;  version; incorporated herein by reference.
; 
; ****************************************************************************

		org 0h

MyStack		equ 1024

		; Parameter registers definition; this is the definition
		; of the stack frame.
%define		P_EAX		dword [bp+28]
%define		P_AX		word [bp+28]
%define		P_AL		byte [bp+28]
%define		P_AH		byte [bp+29]
%define		P_ECX		dword [bp+24]
%define		P_CX		word [bp+24]
%define		P_CL		byte [bp+24]
%define		P_CH		byte [bp+25]
%define		P_EDX		dword [bp+20]
%define		P_DX		word [bp+20]
%define		P_DL		byte [bp+20]
%define		P_DH		byte [bp+21]
%define		P_EBX		dword [bp+16]
%define		P_BX		word [bp+16]
%define		P_BL		byte [bp+16]
%define		P_BH		byte [bp+17]
%define		P_EBP		dword [bp+8]
%define		P_BP		word [bp+8]
%define		P_ESI		dword [bp+4]
%define		P_SI		word [bp+4]
%define		P_EDI		dword [bp]
%define		P_DI		word [bp]

		section .text
Int13Start:
		; See if DL points to our class of device (FD, HD)
		push dx
		xor dl,[cs:DriveNo]
		pop dx
		js .nomatch		; If SF=0, we have a match here
		cmp dl,[cs:DriveNo]
		je .our_drive
		jb .nomatch		; Drive < Our drive
		dec dl			; Drive > Our drive, adjust drive #
.nomatch:
		jmp far [OldInt13]

.our_drive:
		mov [cs:Stack],sp
		mov [cs:SavedAX],ax
		mov ax,ss
		mov [cs:Stack+2],ax
		mov ax,cs
		mov ss,ax
		mov sp,MyStack
		push ds
		push es
		mov ds,ax
		mov es,ax
		mov ax,[SavedAX]
		pushad
		mov bp,sp
		cmp ah,Int13FuncsMax
		jae Invalid
		xor al,al		; AL = 0 is standard entry condition
		mov di,ax
		shr di,7
		call [Int13Funcs+di]

Done:		; Standard routine for return
		mov [LastStatus],ah
		mov P_AX,ax
		cmp ah,1
		setnb al		; AL <- (AH > 0) ? 1 : 0 (CF)
		lds bx,[Stack]		; DS:BX <- Old stack pointer
		mov [bx+4],al		; Low byte of old FLAGS -> arithmetric flags
		popad
		pop es
		pop ds
		lss sp,[cs:Stack]
		iret

Reset:
		; Reset affects multiple drives, so we need to pass it on
		pop ax			; Drop return address
		mov [LastStatus], byte 0
		popad
		pop es
		pop ds
		lss sp,[cs:Stack]
		and dl,80h		; Clear all but the type bit
		jmp far [OldInt13]

GetStatus:
		mov ah,[LastStatus]	; Copy last status
		ret

CheckIfReady:
Recalibrate:
		xor ah,ah		; Always successful
		ret

Read:
Write:
Verify:
Format:
GetParms:
InitWithParms:
Seek:
GetDriveType:
DetectChange:

Invalid:
		mov ah,01h		; Unsupported function
		ret

		section .data
Int13Funcs	dw Reset		; 00h - RESET
		dw GetStatus		; 01h - GET STATUS
		dw Read			; 02h - READ
		dw Write		; 03h - WRITE
		dw Verify		; 04h - VERIFY
		dw Format		; 05h - FORMAT TRACK
		dw Format		; 06h - FORMAT TRACK AND SET BAD FLAGS
		dw Format		; 07h - FORMAT DRIVE AT TRACK
		dw GetParms		; 08h - GET PARAMETERS
		dw InitWithParms	; 09h - INITIALIZE CONTROLLER WITH DRIVE PARAMETERS
		dw Invalid		; 0Ah
		dw Invalid		; 0Bh
		dw Seek			; 0Ch - SEEK TO CYLINDER
		dw Reset		; 0Dh - RESET HARD DISKS
		dw Invalid		; 0Eh
		dw Invalid		; 0Fh
		dw CheckIfReady		; 10h - CHECK IF READY
		dw Recalibrate		; 11h - RECALIBRATE
		dw Invalid		; 12h
		dw Invalid		; 13h
		dw Invalid		; 14h
		dw GetDriveType		; 15h - GET DRIVE TYPE
		dw DetectChange		; 16h - DETECT DRIVE CHANGE
Int13FuncsEnd	equ $
Int13FuncsMax	equ (Int13FuncsEnd-Int13Funcs) >> 1

DriveNo		db 0			; Our drive number
LastStatus	db 0			; Last return status

		section .bss
OldInt13	resd 1			; INT 13h in chain
Stack		resd 1			; Saved SS:SP on invocation
SavedAX		resw 1			; AX saved during initialization


