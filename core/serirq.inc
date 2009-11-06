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

		section .text16
		bits 16
		align 8

		section .bss16
		alignb 8

%assign n 0
%rep 16
		section .text16
serstub_irq %+ n :
		push dword [cs:oldirq %+ n]
		jmp short irq_common

		section .bss16
oldirq %+ n	resd 1
%assign n n+1
%endrep

		section .text16
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

		section .bss16
;
; SerialIRQPort will generally track SerialPort, but will be 0 when an
; IRQ service is not installed.
;
SerialIRQPort	resw 1			; Serial port w IRQ service
SerialHead	resw 1			; Head of serial port rx buffer
SerialTail	resw 1			; Tail of serial port rx buffer

		section .bss16
IRQMask		resw 1			; PIC IRQ mask status

		section .text16

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

		;
		; Enable all ther interupt lines at the PIC.  Some BIOSes
		; only enable the timer interrupts and other interrupts
		; actively in use by the BIOS.
		;
		in al,0xA1		; Secondary PIC mask register
		mov ah,al
		in al,0x21		; Primary PIC mask register
		mov [IRQMask],ax

		io_delay

		xor ax,ax		; Remove all interrupt masks
		out 0x21,al
		out 0xA1,al

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

		; Restore PIC masks
		mov ax,[IRQMask]
		out 0x21,al
		mov al,ah
		out 0xA1,al

		; Restore the original interrupt vectors
		mov si,oldirq0
		mov di,4*08h
		mov cx,8
		rep movsd
		mov di,4*70h
		mov cx,8
		rep movsd

		xor ax,ax
		mov [SerialIRQPort],ax		; No active interrupt system

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

		section .text16
