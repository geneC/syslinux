;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2008 H. Peter Anvin - All Rights Reserved
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
;; conio.inc
;;
;; Console I/O code, except:
;;   writechr, writestr_early	- module-dependent
;;   writestr, crlf		- writestr.inc
;;   writehex*			- writehex.inc
;;

;
; loadkeys:	Load a LILO-style keymap; file is open on the top of the
;		getc stack.
;
		section .text16

loadkeys:
		mov cx,256
		mov di,trackbuf
		call readc
		jc .done				; EOF already?

		; Make sure we are at EOF now...
		call getc
		jnc .done			; We should be at EOF now!

		; It was okay, we can now move it into the KbdMap
		mov si,trackbuf
		mov di,KbdMap
		mov cx,256 >> 2
		rep movsd

.done:
		call close
		ret

;
; get_msg_file: Load a text file and write its contents to the screen,
;               interpreting color codes.  Call with the file already
;		on the top of the open/getc stack.
;
;		Assumes CS == DS == ES.
;
get_msg_file:
                mov byte [TextAttribute],07h	; Default grey on white
		mov byte [DisplayMask],07h	; Display text in all modes
		call msg_initvars

print_msg_file:
.getc:
		call getc
		jc .done
                cmp al,1Ah                      ; DOS EOF?
		je .done
		movzx cx,byte [UsingVGA]
		and cl,01h
		inc cx				; CL <- 01h = text mode,
						;       02h = graphics mode
                call [NextCharJump]		; Do what shall be done
		jmp .getc
.done:
		jmp close			; Tailcall!

msg_putchar:                                    ; Normal character
                cmp al,0Fh                      ; ^O = color code follows
                je msg_ctrl_o
                cmp al,0Dh                      ; Ignore <CR>
                je msg_ignore
                cmp al,0Ah                      ; <LF> = newline
                je msg_newline
                cmp al,0Ch                      ; <FF> = clear screen
                je msg_formfeed
		cmp al,07h			; <BEL> = beep
		je msg_beep
		cmp al,19h			; <EM> = return to text mode
		je msg_novga
		cmp al,18h			; <CAN> = VGA filename follows
		je msg_vga
		jnb .not_modectl
		cmp al,10h			; 10h to 17h are mode controls
		jae msg_modectl
.not_modectl:

msg_normal:	call write_serial_displaymask	; Write to serial port
		test [DisplayMask],cl
		jz msg_ignore			; Not screen
		test byte [DisplayCon],01h
		jz msg_ignore
                mov bl,[TextAttribute]
		mov bh,[BIOS_page]
                mov ah,09h                      ; Write character/attribute
                mov cx,1                        ; One character only
                int 10h                         ; Write to screen
                mov al,[CursorCol]
                inc ax
                cmp al,[VidCols]
                ja msg_line_wrap		; Screen wraparound
                mov [CursorCol],al

msg_gotoxy:     mov bh,[BIOS_page]
                mov dx,[CursorDX]
                mov ah,02h                      ; Set cursor position
                int 10h
msg_ignore:     ret

msg_beep:	mov ax,0E07h			; Beep
		xor bx,bx
		int 10h
		ret

msg_ctrl_o:                                     ; ^O = color code follows
                mov word [NextCharJump],msg_setbg
                ret
msg_newline:                                    ; Newline char or end of line
		mov si,crlf_msg
		call write_serial_str_displaymask
msg_line_wrap:					; Screen wraparound
		test [DisplayMask],cl
		jz msg_ignore
                mov byte [CursorCol],0
                mov al,[CursorRow]
                inc ax
                cmp al,[VidRows]
                ja msg_scroll
                mov [CursorRow],al
                jmp short msg_gotoxy
msg_scroll:     xor cx,cx                       ; Upper left hand corner
                mov dx,[ScreenSize]
                mov [CursorRow],dh		; New cursor at the bottom
                mov bh,[ScrollAttribute]
                mov ax,0601h                    ; Scroll up one line
                int 10h
                jmp short msg_gotoxy
msg_formfeed:                                   ; Form feed character
		mov si,crff_msg
		call write_serial_str_displaymask
		test [DisplayMask],cl
		jz msg_ignore
                xor cx,cx
                mov [CursorDX],cx		; Upper lefthand corner
                mov dx,[ScreenSize]
                mov bh,[TextAttribute]
                mov ax,0600h                    ; Clear screen region
                int 10h
                jmp msg_gotoxy
msg_setbg:                                      ; Color background character
                call unhexchar
                jc msg_color_bad
                shl al,4
		test [DisplayMask],cl
		jz .dontset
                mov [TextAttribute],al
.dontset:
                mov word [NextCharJump],msg_setfg
                ret
msg_setfg:                                      ; Color foreground character
                call unhexchar
                jc msg_color_bad
		test [DisplayMask],cl
		jz .dontset
                or [TextAttribute],al		; setbg set foreground to 0
.dontset:
		jmp short msg_putcharnext
msg_vga:
		mov word [NextCharJump],msg_filename
		mov di, VGAFileBuf
		jmp short msg_setvgafileptr

msg_color_bad:
                mov byte [TextAttribute],07h	; Default attribute
msg_putcharnext:
                mov word [NextCharJump],msg_putchar
		ret

msg_filename:					; Getting VGA filename
		cmp al,0Ah			; <LF> = end of filename
		je msg_viewimage
		cmp al,' '
		jbe msg_ret			; Ignore space/control char
		mov di,[VGAFilePtr]
		cmp di,VGAFileBufEnd
		jnb msg_ret
		mov [di],al			; Can't use stosb (DS:)
		inc di
msg_setvgafileptr:
		mov [VGAFilePtr],di
msg_ret:	ret

msg_novga:
		call vgaclearmode
		jmp short msg_initvars

msg_viewimage:
		mov si,[VGAFilePtr]
		mov byte [si],0			; Zero-terminate filename
		mov si,VGAFileBuf
		mov di,VGAFileMBuf
		pm_call pm_mangle_name
		call core_open
		jz msg_putcharnext		; Not there
		call vgadisplayfile
		; Fall through

		; Subroutine to initialize variables, also needed
		; after loading a graphics file
msg_initvars:
                pusha
                mov bh,[BIOS_page]
                mov ah,03h                      ; Read cursor position
                int 10h
                mov [CursorDX],dx
                popa
		jmp short msg_putcharnext	; Initialize state machine

msg_modectl:
		and al,07h
		mov [DisplayMask],al
		jmp short msg_putcharnext

;
; write_serial:	If serial output is enabled, write character on serial port
; write_serial_displaymask: d:o, but ignore if DisplayMask & 04h == 0
;
write_serial_displaymask:
		test byte [DisplayMask], 04h
		jz write_serial.end
write_serial:
		pushfd
		pushad
		mov bx,[SerialPort]
		and bx,bx
		je .noserial
		push ax
		mov ah,[FlowInput]
.waitspace:
		; Wait for space in transmit register
		lea dx,[bx+5]			; DX -> LSR
		in al,dx
		test al,20h
		jz .waitspace

		; Wait for input flow control
		inc dx				; DX -> MSR
		in al,dx
		and al,ah
		cmp al,ah
		jne .waitspace
.no_flow:

		xchg dx,bx			; DX -> THR
		pop ax
		slow_out dx,al			; Send data
.noserial:	popad
		popfd
.end:		ret

;
; write_serial_str: write_serial for strings
; write_serial_str_displaymask: d:o, but ignore if DisplayMask & 04h == 0
;
write_serial_str_displaymask:
		test byte [DisplayMask], 04h
		jz write_serial_str.end

write_serial_str:
.loop		lodsb
		and al,al
		jz .end
		call write_serial
		jmp short .loop
.end:		ret

;
; pollchar: check if we have an input character pending (ZF = 0)
;
pollchar:
		pushad
		mov ah,11h		; Poll keyboard
		int 16h
		jnz .done		; Keyboard response
		mov dx,[SerialPort]
		and dx,dx
		jz .done		; No serial port -> no input
		mov ax,[SerialTail]	; Already-queued input?
		cli
		cmp ax,[SerialHead]
		jne .done_sti		; If so, return ZF = 0
		add dx,5		; DX -> LSR
		in al,dx
		test al,1		; ZF = 0 if data pending
		jz .done_sti
		inc dx			; DX -> MSR
		mov ah,[FlowIgnore]	; Required status bits
		in al,dx
		and al,ah
		cmp al,ah
		setne al
		dec al			; Set ZF = 0 if equal
.done_sti:	sti
.done:		popad
		ret

;
; getchar: Read a character from keyboard or serial port
;
getchar.sti_again:
		sti
getchar:
.again:
		call do_idle
		mov ah,11h		; Poll keyboard
		int 16h
		jnz .kbd		; Keyboard input?
		mov bx,[SerialPort]
		and bx,bx
		jz .again
		mov ax,[SerialTail]
		cli
		cmp ax,[SerialHead]
		jne .serial_queued
		lea dx,[bx+5]		; DX -> LSR
		in al,dx
		test al,1
		jz .sti_again
		inc dx			; DX -> MSR
		mov ah,[FlowIgnore]
		in al,dx
		and al,ah
		cmp al,ah
		jne .sti_again
.serial:	xor ah,ah		; Avoid confusion
		mov dx,bx		; Data port
		in al,dx		; Read data
		sti
		jmp .done
.serial_queued:
		sti			; We already know we'll consume data
		xchg bx,ax
		push ds
		mov ax,aux_seg + (aux.serial >> 4)
		mov ds,ax
		mov al,[bx]
		pop ds
		inc bx
		and bx,serial_buf_size-1
		mov [SerialTail],bx
		jmp .done

.kbd:		mov ah,10h		; Get keyboard input
		int 16h
		cmp al,0E0h
		jnz .not_ext
		xor al,al
.not_ext:
		and al,al
		jz .func_key
		mov bx,KbdMap		; Convert character sets
		xlatb
.func_key:
.done:
		jmp reset_idle		; Character received

%ifdef DEBUG_TRACERS
;
; debug hack to print a character with minimal code impact
;
debug_tracer:	pushad
		pushfd
		mov bp,sp
		mov bx,[bp+9*4]		; Get return address
		mov al,[cs:bx]		; Get data byte
		inc word [bp+9*4]	; Return to after data byte
		call writechr
		popfd
		popad
		ret
%endif	; DEBUG_TRACERS

		section .data16
%if IS_ISOLINUX == 0			; Defined elsewhere for ISOLINUX
crlf_msg	db CR, LF
null_msg	db 0
%endif
crff_msg	db CR, FF, 0

		section .config
		; This is a word to pc_setint16 can set it
DisplayCon	dw 01h			; Console display enabled

ScrollAttribute	db 07h			; Grey on white (normal text color)

		section .bss16
		alignb 2
NextCharJump    resw 1			; Routine to interpret next print char
CursorDX        equ $
CursorCol       resb 1			; Cursor column for message file
CursorRow       resb 1			; Cursor row for message file
ScreenSize      equ $
VidCols         resb 1			; Columns on screen-1
VidRows         resb 1			; Rows on screen-1

; Serial console stuff; don't put this in .config becasue we don't want
; loading a new config file to undo this setting.
		section .data16
		alignz 4
SerialPort	dw 0			; Serial port base (or 0 for no serial port)
BaudDivisor	dw 115200/9600		; Baud rate divisor
FlowControl	equ $
FlowOutput	db 0			; Outputs to assert for serial flow
FlowInput	db 0			; Input bits for serial flow
FlowIgnore	db 0			; Ignore input unless these bits set
FlowDummy	db 0			; Unused

		section .bss16
TextAttribute   resb 1			; Text attribute for message file
DisplayMask	resb 1			; Display modes mask

		section .text16
%include "serirq.inc"
