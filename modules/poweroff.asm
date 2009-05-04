		absolute 0
pspInt20:	resw 1
pspNextP:	resw 1
		resb 124
pspCmdLen:	resb 1
pspCmdArg:	resb 127

		section .text
		org	0x100

_start:
		mov ax,5300h	; APM Installation Check (00h)
		xor bx,bx	; APM BIOS (0000h)
		int 15h
		jnc check_sig

		mov bx, msg_notpresent
		jmp error

check_sig:
		cmp bx,504Dh	; signature 'PM'
		je check_ver

		mov bx, msg_notpresent
		jmp error

check_ver:
		cmp ax,0101h	; Need version 1.1+
		jae check_state

		mov bx, msg_notsup
		jmp error

check_state:
		test cx,8	; bit 3 APM BIOS Power Management disabled
		jz connect

		mov bx, msg_pmdisabled
		jmp error

connect:
		mov ax,5301h	; APM Real Mode Interface Connect (01h)
		xor bx,bx	; APM BIOS (0000h)
		int 15h
		jnc connect_ok

		mov bx, msg_connect
		jmp error

connect_ok:
		mov ax,530Eh	; APM Driver Version (0Eh)
		xor bx,bx	; APM BIOS (0000h)
		mov cx,0101h	; APM Driver version 1.1
		int 15h
		jnc apm0101_check

		mov bx, msg_notsup
		jmp error

apm0101_check:
		cmp cx,0101h	; APM Connection version
		jae apm0101_ok

		mov bx, msg_notsup
		jmp error

apm0101_ok:
		mov ax,5307h	; Set Power State (07h)
		mov bx,0001h	; All devices power managed by the APM BIOS
		mov cx,0003h	; Power state off
		int 15h
		jnc off

		mov bx, msg_failed

error:
		mov ax,2
		int 22h
off:
		ret

msg_notpresent:	db 'APM not present.',0dh,0ah,0
msg_notsup:	db 'APM 1.1+ not supported.',0dh,0ah,0
msg_pmdisabled:	db 'Power management disabled.',0dh,0ah,0
msg_connect:	db 'APM RM interface connect failed.',0dh,0ah,0
msg_failed:	db 'Power off failed.',0dh,0ah,0
