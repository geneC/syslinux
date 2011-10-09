;
; Process a PXE interrupt
;
		section .text16

		global pxe_isr
pxe_isr:
		pusha
		push ds
		push es
		push fs
		push gs

		xor ax,ax
		mov ds,ax	
		mov es,ax

		mov bx,PXENV_UNDI_ISR
		mov di,pxenv_undi_isr_buf

		mov cx,pxenv_undi_isr_buf.size/2
		push di
		rep stosw
		pop di
		inc dword [pxe_irq_count]
		
		mov byte [pxenv_undi_isr_buf.funcflag],PXENV_UNDI_ISR_IN_START

		call pxenv
		jc .notus

		cmp word [pxenv_undi_isr_buf.funcflag],PXENV_UNDI_ISR_OUT_OURS
		jne .notus

		inc dword [pxe_irq_count_us]
		; Its ours - set the flag for the return to PM.
		; We need to EOI this ourselves, so that the
		; leftover BC doesn't get control.
		mov byte [pxe_irq_pending],1

		mov al,[pxe_irq_vector]
		cmp al,8
		jae .aux_pic

		add al,0x60		; Specific EOI
		jmp .pri_pic

.aux_pic:
		add al,0x60-8
		out 0xA0,al		; Secondary PIC
		mov al,0x62		; Specific EOI, cascade interrupt
.pri_pic:
		out 0x20,al		; Primary PIC

		pop gs
		pop fs
		pop es
		pop ds
		popa
		iret

.notus:
		pop gs
		pop fs
		pop es
		pop ds
		popa
		jmp 0:0
		global pxe_irq_chain
pxe_irq_chain	equ $-4

		section .bss16
		alignb 4
pxenv_undi_isr_buf:
.status:	resw 1
.funcflag:	resw 1
.bufferlength:	resw 1
.framelen:	resw 1
.framehdrlen:	resw 1
.frame:		resw 2
.prottype:	resb 1
.pkttype:	resb 1
.size		equ $-pxenv_undi_isr_buf

		global pxe_irq_vector
pxe_irq_vector	resb 1			; PXE IRQ vector
		global pxe_irq_pending
pxe_irq_pending	resb 1			; IRQ pending flag
		global pxe_irq_count
pxe_irq_count	resd 1			; How many times have we been called
		global pxe_irq_count_us
pxe_irq_count_us	resd 1			; ... and serviced the IRQ

		section .text16
