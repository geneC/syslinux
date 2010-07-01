;
; writechr:	Write a single character in AL to the console without
;		mangling any registers; handle video pages correctly.
;
		section .text16

writechr:
		call write_serial	; write to serial port if needed
		pushfd
		test byte [cs:UsingVGA], 08h
		jz .videook
		call vgaclearmode
.videook:
		test byte [cs:DisplayCon], 01h
		jz .nothing
		pushad
		mov ah,0Eh
		mov bl,07h		; attribute
		mov bh,[cs:BIOS_page]	; current page
		int 10h
		popad
.nothing:
		popfd
		ret
