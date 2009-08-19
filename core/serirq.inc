;; -----------------------------------------------------------------------
;;
;;   Copyright 2009 Intel Corporation; author: H. Peter Anvin
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;;
;; serirq.inc
;;
;; Serial port IRQ code
;;
;; We don't know what IRQ, if any, we have, so map all of them...
;;

		section .text
		bits 16
		align 8

		section .bss
		alignb 8

%assign n 0
%rep 16
		section .text
serstub_irq %+ n :
		push dword [cs:oldirq %+ n]
		jmp short irq_common

		section .bss
oldirq %+ n	resd 1
%assign n n+1
%endrep

		section .text
irq_common:
		pushf
		push ax
		push dx
		mov dx,[cs:SerialPort]
		add dx,5			; DX -> LSR
		in al,dx
		test al,1			; Received data
		jnz .data
.done:
		pop dx
		pop ax
		popf
		retf				; Chain to next handler
.data:
		push es
		push di
		mov ax,aux_seg + (aux.serial >> 4)
		mov es,ax
		mov di,[cs:SerialHead]
.loop:
		mov dx,[cs:SerialPort]		; DX -> RDR
		in al,dx
		stosb
		mov ah,[cs:FlowIgnore]
		add dx,5			; DX -> LSR
		in al,dx
		push ax
		and al,ah
		cmp al,ah
		jne .drop
		and di,serial_buf_size-1	; Wrap around if necessary
		cmp di,[cs:SerialTail]		; Would this cause overflow?
		je .drop			; If so, just drop the data
		mov [cs:SerialHead],di
.drop:
		pop ax
		test al,1			; More data?
		jnz .loop
.full:
		pop di
		pop es
		jmp .done

		section .data
;
; SerialIRQPort will generally track SerialPort, but will be 0 when an
; IRQ service is not installed.
;
SerialIRQPort	dw 0			; Serial port w IRQ service
SerialHead	dw 0			; Head of serial port rx buffer
SerialTail	dw 0			; Tail of serial port rx buffer

		section .text

sirq_install:
		pushad

		call sirq_cleanup

		; Save the old interrupt vectors
		mov si,4*08h
		mov di,oldirq0
		mov cx,8
		rep movsd
		mov si,4*70h
		mov cx,8
		rep movsd

		; Install new interrupt vectors
		mov di,4*08h
		mov cx,8
		mov eax,serstub_irq0
.pic0:
		stosd
		add ax,serstub_irq1 - serstub_irq0
		loop .pic0
		mov di,4*70h
		mov cx,8
.pic1:
		stosd
		add ax,serstub_irq1 - serstub_irq0
		loop .pic1

		mov bx,[SerialPort]
		mov [SerialIRQPort],bx

		lea dx,[bx+5]		; DX -> LCR
		mov al,03h		; Clear DLAB (should already be...)
		slow_out dx,al

		lea dx,[bx+1]		; DX -> IER
		mov al,1		; Enable receive interrupt
		slow_out dx,al

		popad
		ret

sirq_cleanup_nowipe:
		pushad
		push ds
		push es
		xor ax,ax
		mov ds,ax
		mov es,ax

		mov bx,[SerialIRQPort]
		and bx,bx
		jz .done

		lea dx,[bx+5]		; DX -> LCR
		mov al,03h		; Clear DLAB (should already be...)
		slow_out dx,al

		lea dx,[bx+1]		; DX -> IER
		xor ax,ax
		slow_out dx,al		; Clear IER

		; Restore the original interrupt vectors
		mov si,oldirq0
		mov di,4*08h
		mov cx,8
		rep movsd
		mov di,4*70h
		mov cx,8
		rep movsd

.done:
		pop es
		pop ds
		popad
		ret

sirq_cleanup:
		call sirq_cleanup_nowipe
		pushad
		push es
		; Just in case it might contain a password, erase the
		; serial port receive buffer...
		mov ax,aux_seg + (aux.serial >> 4)
		mov es,ax
		xor eax,eax
		mov [cs:SerialHead],eax
		mov cx,serial_buf_size >> 2
		xor di,di
		rep stosd
		pop es
		popad
		ret

		section .text
