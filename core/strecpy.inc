;
; strecpy: Copy DS:SI -> ES:DI up to and including a null byte;
;	   on exit SI and DI point to the byte *after* the null byte.
;	   BP holds a pointer to the first byte beyond the end of the
;	   target buffer; return with CF=1 if target buffer overflows;
;	   the output is still zero-terminated.
;
		section .text

strecpy:
		push ax
		push bp
		dec bp
		dec bp
.loop:		lodsb
		stosb
		and al,al	; CF=0
		jz .done
		cmp bp,di	; CF set if BP < DI
		jnc .loop

		; Zero-terminate overflow string
		mov al,0	; Avoid changing flags
		stosb
.done:
		pop bp
		pop ax
		ret
