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

%define SECTORSIZE_LG2	9		; log2(sector size)
%define	SECTORSIZE	(1 << SECTORSIZE_LG2)

MyStack		equ 1024

		; Parameter registers definition; this is the definition
		; of the stack frame.
%define		P_DS		word [bp+34]
%define		P_ES		word [bp+32]
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
DoneWeird:
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

Invalid:
		mov ax,0100h		; Unsupported function
		ret

GetDriveType:
		pop ax			; Drop return address
		mov ah,[DriveNo]
		shr ah,7
		or ah,02h		; CF = 0
		mov P_AH,ah
		mov [LastStatus],byte 0	; Success, but AH returns a value
		jmp short DoneWeird

GetStatus:
		mov ah,[LastStatus]	; Copy last status
		ret

CheckIfReady:				; These are always-successful noop functions
Recalibrate:
InitWithParms:
DetectChange:
success:
		xor ax,ax		; Always successful
		ret

Read:
		call setup_regs
do_copy:
		call bcopy
		movzx ax,P_AL		; AH = 0, AL = transfer count
		ret

Write:
		call setup_regs
		xchg esi,edi
		jmp short do_copy

		; These verify one sector only
Seek:
		mov P_AL,1

		; Verify integrity; just bounds-check
Verify:
		call setup_regs		; Returns error if appropriate
		jmp short success	

GetParms:
		; We need to get the "number of drives" from the BIOS
		mov dl,P_DL
		inc dl			; The drive whose number we're stealing
		mov ah,08h
		int 13h
		inc dl			; Add ourselves to the count
		mov P_DL,dl		; Drive count
		mov P_DI,di		; Steal the diskette parameter table if applicable
		mov ax,es
		mov P_ES,ax
		mov bl,[DriveType]
		mov P_BL,bl
		mov ax,[Cylinders]
		dec ax			; We report the highest #, not the count
		or ah,[Sectors]
		xchg al,ah
		mov P_CX,ax
		mov al,[Heads]
		dec al
		mov P_DH,al
		xor ax,ax
		ret

		; Convert a CHS address in CX/DH into an LBA in EAX
chstolba:
		xor ebx,ebx
		mov bl,cl		; Sector number
		and bl,3Fh
		dec bx
		mov si,dx
		mov ax,[Heads]
		shr cl,6
		xchg cl,ch		; Now CX <- cylinder number
		mul cx			; DX:AX <- AX*CX
		shr si,8		; SI <- head number
		add ax,si
		adc dx,byte 0
		shl edx,16
		or eax,edx
		mul dword [Sectors]
		add eax,ebx
		ret

		; Set up registers as for a "Read", and compares against disk size
setup_regs:
		call chstolba
		movzx edi,P_BX		; Get linear address of target buffer
		movzx ecx,P_ES
		shr ecx,4
		add edi,ecx
		movzx ecx,P_AL
		lea ebx,[eax+ecx]
		mov esi,eax
		shr esi,SECTORSIZE_LG2
		add esi,[DiskBuf]
		cmp ebx,[DiskSize]
		jae .overrun
		shr ecx,SECTORSIZE_LG2-1
		ret

.overrun:	pop ax			; Drop return address
		mov ax,0400h		; Sector not found
		ret

int15_e820:
		cmp edx,534D4150h
		jne near oldint15
		cmp ecx,20		; Need 20 bytes
		jb err86
		push edx		; "SMAP"
		push esi
		push edi
		and ebx,ebx
		jne .renew
		mov ebx,[E820Table]
.renew:		mov esi,ebx
		xor edi,edi
		mov di,cs
		shr di,4
		add edi,E820Buf
		mov ecx,24/2
		call bcopy
		add ebx, byte 12
		pop edi
		pop esi
		mov eax,[cs:E820Buf]
		mov [es:di],eax
		mov eax,[cs:E820Buf+4]
		mov [es:di+4],eax
		mov eax,[cs:E820Buf+12]
		mov [es:di+8],eax
		mov eax,[cs:E820Buf+16]
		mov [es:di+12],eax
		mov eax,[cs:E820Buf+8]
		mov [es:di+16],eax
		cmp dword [cs:E820Buf+20], byte -1
		jne .notdone
		xor ebx,ebx		; Done with table
.notdone:
		pop eax			; "SMAP"
		mov ecx,20		; Bytes loaded
int15_success:
		mov byte [bp+12], 02h	; Clear CF
		pop bp
		iret

err86:
		mov byte [bp+12], 03h	; Set CF
		mov ah,86h
		pop bp
		iret

Int15Start:
		push bp
		mov bp,sp
		cmp ax,0E820h
		je near int15_e820
		cmp ax,0E801h
		je int15_e801
		cmp ax,0E881h
		je int15_e881
		cmp ah,88h
		je int15_88
oldint15:	pop bp
		jmp far [cs:OldInt15]
		
int15_e801:
		mov ax,[cs:Mem1MB]
		mov cx,ax
		mov bx,[cs:Mem16MB]
		mov dx,ax
		jmp short int15_success

int15_e881:
		mov eax,[cs:Mem1MB]
		mov ecx,eax
		mov ebx,[cs:Mem16MB]
		mov edx,eax
		jmp short int15_success

int15_88:
		mov ax,[cs:MemInt1588]
		jmp short int15_success

;
; Routine to copy in/out of high memory
; esi = linear source address
; edi = linear target address
; ecx = 16-bit word count 
;
; Assumes cs = ds = es
;
bcopy:
		push eax
		push ebx
		push edx
		push ebp
		push esi
		push edi
.copy_loop:
		push ecx
		cmp ecx,8000h
		jna .safe_size
		mov ecx,8000h
.safe_size:
		push ecx
		mov eax, esi
		mov [Mover_src1], si
		shr eax, 16
		mov [Mover_src1+2], al
		mov [Mover_src2], ah
		mov eax, edi
		mov [Mover_dst1], di
		shr eax, 16
		mov [Mover_dst1+2], al
		mov [Mover_dst2], ah
		mov si,Mover
		mov ah, 87h
		int 15h
		pop eax
		pop ecx
		pop edi
		pop esi
		jc .error
		lea esi,[esi+2*eax]
		lea edi,[edi+2*eax]
		sub ecx, eax
		jnz .copy_loop
		; CF = 0
.error:
		pop ebp
		pop edx
		pop ebx
		pop eax
		ret

		section .data
Int13Funcs	dw Reset		; 00h - RESET
		dw GetStatus		; 01h - GET STATUS
		dw Read			; 02h - READ
		dw Write		; 03h - WRITE
		dw Verify		; 04h - VERIFY
		dw Invalid		; 05h - FORMAT TRACK
		dw Invalid		; 06h - FORMAT TRACK AND SET BAD FLAGS
		dw Invalid		; 07h - FORMAT DRIVE AT TRACK
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
DriveType	db 0			; Our drive type (floppies)
LastStatus	db 0			; Last return status

		alignb 4, db 0
Cylinders	dw 0			; Cylinder count
Heads		dw 0			; Head count
Sectors		dd 0			; Sector count (zero-extended)
DiskSize	dd 0			; Size of disk in blocks
DiskBuf		dd 0			; Linear address of high memory disk

E820Table	dd 0			; E820 table in high memory
Mem1MB		dd 0			; 1MB-16MB memory amount (1K)
Mem16MB		dd 0			; 16MB-4G memory amount (64K)
MemInt1588	dw 0			; 1MB-65MB memory amount (1K)

		alignb 8, db 0
Mover		dd 0, 0, 0, 0		; Must be zero
		dw 0ffffh		; 64 K segment size
Mover_src1:	db 0, 0, 0		; Low 24 bits of source addy
		db 93h			; Access rights
		db 00h			; Extended access rights
Mover_src2:	db 0			; High 8 bits of source addy
Mover_dst1:	db 0, 0, 0		; Low 24 bits of target addy
		db 93h			; Access rights
		db 00h			; Extended access rights
Mover_dst2:	db 0			; High 8 bits of source addy

		section .bss
OldInt13	resd 1			; INT 13h in chain
OldInt15	resd 1			; INT 15h in chain
Stack		resd 1			; Saved SS:SP on invocation
E820Buf		resd 6			; E820 fetch buffer
SavedAX		resw 1			; AX saved during initialization
