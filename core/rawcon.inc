;
; writechr:	Write a single character in AL to the console without
;		mangling any registers.  This does raw console writes,
;		since some PXE BIOSes seem to interfere regular console I/O.
;
%if IS_ISOLINUX
writechr_full:
%else
writechr:
%endif
		pushfd
		push ds
		push cs
		pop ds
		test byte [UsingVGA], 08h
		jz .videook
		call vgaclearmode
.videook:
		call write_serial	; write to serial port if needed
		test byte [DisplayCon],01h	; Write to screen?
		jz .nothing

		pushad
		mov bh,[BIOS_page]
		push ax
                mov ah,03h              ; Read cursor position
                int 10h
		pop ax
		cmp al,8
		je .bs
		cmp al,13
		je .cr
		cmp al,10
		je .lf
		push dx
                mov bh,[BIOS_page]
		mov bl,07h		; White on black
		mov cx,1		; One only
		mov ah,09h		; Write char and attribute
		int 10h
		pop dx
		inc dl
		cmp dl,[VidCols]
		jna .curxyok
		xor dl,dl
.lf:		inc dh
		cmp dh,[VidRows]
		ja .scroll
.curxyok:	mov bh,[BIOS_page]
		mov ah,02h		; Set cursor position
		int 10h
.ret:		popad
.nothing:
		pop ds
		popfd
		ret
.scroll:	dec dh
		mov bh,[BIOS_page]
		mov ah,02h
		int 10h
		mov ax,0601h		; Scroll up one line
		mov bh,[ScrollAttribute]
		xor cx,cx
		mov dx,[ScreenSize]	; The whole screen
		int 10h
		jmp short .ret
.cr:		xor dl,dl
		jmp short .curxyok
.bs:		sub dl,1
		jnc .curxyok
		mov dl,[VidCols]
		sub dh,1
		jnc .curxyok
		xor dh,dh
		jmp short .curxyok
