	bits 16
	org 100h
_start:
	mov ax,5
	int 22h
	mov ah,09h
	mov dx,msg
	int 21h
	mov ax,000Ch
	xor dx,dx
	int 22h
	int 18h
	jmp 0F000h:0FFF0h	; INT 18h should not return...

	section .data
msg:	db 'Local boot via INT 18...', 13, 10, '$'
