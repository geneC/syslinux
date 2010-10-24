; ****************************************************************************
;
;  ver.asm
;
;  A COMBOOT/DOS COM program to display the version of the system (Syslinux or DOS)
;
;   Copyright (C) 2009  Gene Cumm
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;  Boston MA 02111-1307, USA; either version 2 of the License, or
;  (at your option) any later version; incorporated herein by reference.
;
; ****************************************************************************

; %define DEBUG

		section .text
		org	0x100

_start:
; 		push ds
; 		push cs
; 		pop ds
		call crlf
		mov si,info_str
		call writestr
		call getdosver
		call chkprn_dosver
		jnz .end
		call chkprn_syslinux
		call crlf
.end:
; 		pop ds
		ret


; chkprn_syslinux
;	EAX=59530000h EBX=4C530000h ECX=4E490000h EDX=58550000h
chkprn_syslinux:
%ifdef DEBUG
		mov si,may_sysl_str
		call writestr
%endif
		cmp eax,59530000h
		jne .end
		cmp ebx,4C530000h
		jne .end
		cmp ecx,4E490000h
		jne .end
		cmp edx,58550000h
		jne .end
.is_syslinux:
		push es
%ifdef DEBUG
		mov si,is_sysl_str
		call writestr
%endif
.get_sysl_ver:
		mov ax,0001h
		int 22h
; AX=0001h [2.00]	Get Version
; 
; 	Input:	AX	0001h
; 	Output:	AX	number of INT 22h API functions available
; 		CH	Syslinux major version number
; 		CL	Syslinux minor version number
; 		DL	Syslinux derivative ID (e.g. 32h = PXELINUX)
; 		ES:SI	Syslinux version string
; 		ES:DI	Syslinux copyright string
%ifdef DEBUG
		push si
		push cs
		pop ds
		mov si,gotver_str
		call writestr
		pop si
%endif

.prn_ver_str:
		push ds
		push es
		pop ds
		call writestr
		call crlf
		pop ds
.prn_var:
		cmp dl,31h
		je .var_sysl
		cmp dl,32h
		je .var_pxel
		cmp dl,33h
		je .var_isol
		cmp dl,34h
		je .var_extl
		jmp .var_unk
.var_sysl:
		mov si,sysl_str
		call writestr
		jmp .prn_ver
.var_pxel:
		mov si,pxel_str
		call writestr
		jmp .prn_ver
.var_isol:
		mov si,isol_str
		call writestr
		jmp .prn_ver
.var_extl:
		mov si,extl_str
		call writestr
		jmp .prn_ver
.var_unk:
		mov si,unkvar_str
		call writestr
; 		jmp .prn_ver
.prn_ver:
%ifdef DEBUG
		push si
		push cs
		pop ds
		mov si,prn_ver_str
		call writestr
		pop si
%endif
.prn_ver_maj:
		mov al,ch
		call writedecb
		mov dl,'.'
		call writechr_dl
.prn_ver_min:
		mov al,cl
		call writedecb

.end_prn:
		pop es
.end:
		ret

; chkprn_dosver	Check and print DOS version;
;	Input	Data from INT21 AH=30h
;	AH	Major version of DOS or 0
;	AL	Minor Version
;	BH	DOS type
;	BL:CX	24-bit OEM serial number
;	Return
;	ZF	Unset if DOS, Set if not DOS (AX=0)
chkprn_dosver:
		and ax,ax	; cmp ax,0
		jz .end
.is_dos:
		push eax
		push edx
		push si
%ifdef DEBUG
		mov si,is_dos_str
		call writestr
		call crlf
		call prnreg_gp_l
		call crlf
%endif
.var_prn:
		cmp bh,0
		je .var_pcdos
		cmp bh,0FFh
		je .var_msdos
		cmp bh,0FDh
		je .var_freedos
		jmp .var_unk
.var_pcdos:
		mov si,pcdos_str
		call writestr
		jmp .var_end
.var_msdos:
		mov si,msdos_str
		call writestr
		jmp .var_end
.var_freedos:
		mov si,freedos_str
		call writestr
		jmp .var_end
.var_unk:
		mov si,unkdos_str
		call writestr
		mov si,spparen_str
		call writestr
		push eax
		mov al,bh
		call writehex2
		pop eax
		mov si,parensp_str
		call writestr
; 		jmp .var_end
.var_end:
		call prn_dosver_num
		call crlf
.subver:
		pop si
		pop edx
		pop eax
		cmp bh,0FFh
		jne .end_ver
		cmp al,5
		jb .end_ver
		call getprn_msdosver
.end_ver:
		and ax,ax		; Unset ZF
.end:
		ret

; prn_dosver_num	Print the numerical DOS version
;	Input	Data from INT21 AH=30h
;	AH	Major version of DOS or 0
;	AL	Minor Version
;	BH	DOS type
;	BL:CX	24-bit OEM serial number
prn_dosver_num:
		push eax
		push edx
		push si
.vmaj_prn:
		call writedecb
; 		call writehex2
		mov dl,'.'
		call writechr_dl
.vmin_prn:
		mov al,ah
		call writedecb
; 		call writehex2
.serial:
		mov si,spparen_str
		call writestr
		mov si,zerox_str
		call writestr
.ser_bl:
		mov al,bl
		call writehex2
.ser_cx:
		mov ax,cx
		call writehex4
.serial_end:
		mov si,parensp_str
		call writestr
.end:
		pop si
		pop edx
		pop eax
		ret

; getdosver	Get the DOS version
;	Return	Version or 0 + SYSLINUX message
;	EAX	Part 1
;	EBX	Part 2
;	ECX	Part 3
;	EDX	Part 4
getdosver:
		mov eax,0
		mov ecx,0
		mov edx,0
		mov ebx,0
		mov ah,30h
		int 21h
		ret

; getmsdosver	Get the Extended MS-DOS version
;	Returns	Version
;	EAX	Part 1
;	EBX	Part 2
;	ECX	Part 3
;	EDX	Part 4
getmsdosver:
		mov ecx,0
		mov edx,0
		mov ebx,0
		mov eax,3306h
		int 21h
		ret

; getprn_msdosver
getprn_msdosver:
		pushad
		pushfd
		call getmsdosver
%ifdef DEBUG
		call prnreg_gp_l
		call crlf
%endif
		mov si,dosext_str
		call writestr
		mov eax,ebx
		mov ebx,0
		mov ecx,edx
		call prn_dosver_num
.end:
		popfd
		popad
		ret

; writechr_dl	Write a character to the console saving AX
;	Input
;	DL	character to write
writechr_dl:
		push ax
		mov ah,02h
		int 21h
.end:
		pop ax
		ret

; writechr_al	Write a character to the console saving AX
;	Input
;	AL	character to write
writechr:
writechr_al:
		push dx
		mov dl,al
		call writechr_dl
.end:		pop dx
		ret

; writedecb2	FIXME: Fake a write decimal function
;	Input
;	AL	number to write
; writedecb:
writedecb2:
		push eax
		add al,'0'
		call writechr
.end:
		pop eax
		ret

; prnreg_gp_l	Dump GP registers (Long)
prnreg_gp_l:
		push eax
		push si
		call crlf
		mov si,sp2_str
		call writestr
		mov si,eax_str
		call writestr
		call writehex8
		mov si,sp2_str
		call writestr
		mov si,ecx_str
		call writestr
		mov eax,ecx
		call writehex8
		mov si,sp2_str
		call writestr
		mov si,edx_str
		call writestr
		mov eax,edx
		call writehex8
		mov si,sp2_str
		call writestr
		mov si,ebx_str
		call writestr
		mov eax,ebx
		call writehex8
		call crlf
		pop si
		pop eax
.end:
		ret

; is_zf
is_zf:
		push si
		jz .true
.false:
		mov si,zero_not_str
		call writestr
		jmp .end
.true:
		mov si,zero_is_str
		call writestr
.end:
		pop si
		ret

%include "../core/macros.inc"		; CR/LF
%include "../core/writestr.inc"		; String output
%include "../core/writehex.inc"		; Hexadecimal output
%include "../core/writedec.inc"		; Decimal output

		section .data
info_str	db 'Ver.com b010', CR, LF, 0
is_dos_str	db 'Found DOS', CR, LF, 0
is_sysl_str	db 'Found a Syslinux variant', CR, LF, 0
may_sysl_str	db 'Maybe Syslinux variant', CR, LF, 0
gotver_str	db 'Got the version back', CR, LF, 0
prn_ver_str	db 'Printing version number', CR, LF, 0
sysl_str	db 'SYSLINUX ', 0
pxel_str	db 'PXELINUX ', 0
isol_str	db 'ISOLINUX ', 0
extl_str	db 'EXTLINUX ', 0
unkvar_str	db 'Unkown-Variant ', 0
pcdos_str	db 'PC-DOS ', 0
msdos_str	db 'MS-DOS ', 0
freedos_str	db 'FreeDOS ', 0
unkdos_str	db 'Unknown-DOS ', 0
dosext_str	db '  Extended DOS version: ', 0
spparen_str	db ' (', 0
zerox_str	db '0x', 0
parensp_str	db ') ', 0
eax_str		db 'EAX=', 0
ebx_str		db 'EBX=', 0
ecx_str		db 'ECX=', 0
edx_str		db 'EDX=', 0
sp2_str		db '  ', 0
zero_not_str	db ' NOT_Zero ',0
zero_is_str	db ' IS_Zero ',0
