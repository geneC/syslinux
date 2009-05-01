; ****************************************************************************
;
;  gfxboot.asm
;
;   Copyright 2008-2009 Sebastian Herbszt
;
;  This module is based on the gfxboot integration patch by Steffen Winterfeldt:
;
;   Copyright 2001-2008 Steffen Winterfeldt
;
;  Some parts borrowed from Syslinux core:
;
;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;
;  This program is free software; you can redistribute it and/or modify
;  it under the terms of the GNU General Public License as published by
;  the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;  Boston MA 02111-1307, USA; either version 2 of the License, or
;  (at your option) any later version; incorporated herein by reference.
;
; ****************************************************************************

		[map all gfxboot.map]

		absolute 0
pspInt20:	resw 1
pspNextP:	resw 1
		resb 124
pspCmdLen:	resb 1
pspCmdArg:	resb 127

;%define DEBUG

		section .text
		org 100h

_start:
		; Zero memory from the start of .bss to the stack
		cld
		mov di,section..bss.start
		mov cx,sp
		sub cx,di
		shr cx,2
		xor eax,eax
		rep stosd

		mov ax,2
		mov bx, msg_progname
		int 22h

		mov ax,2
		mov bx, msg_crlf
		int 22h

		push es
		mov ax,0ah
		mov cl,9
		int 22h
		pop es
		cmp al,32h
		jnz not_pxelinux

		mov dl,0 ; fake drive number
		mov cl,11 ; fake sector size 2048 bytes

not_pxelinux:
		mov [derivative_id],al
		mov [drivenumber],dl
		mov [sectorshift],cl
		mov ax,1
		shl ax,cl
		mov [sectorsize],ax
		mov ax,trackbufsize
		shr ax,cl
		mov [BufSafe],ax

		xor cx,cx
		mov cl,[pspCmdLen]
		dec cx
		and cx,cx
		jne continue

		mov ax,2
		mov bx, msg_usage
		int 22h
		ret
continue:
		mov di,pspCmdArg+1
		add di,cx
		dec di
		std
		mov al,' '
		repe scasb
		inc cx
		cld
		mov [pspCmdLen],cl
		mov si,pspCmdArg+1
		mov di,si
		add di,cx
		xor al,al
		stosb
		mov si,pspCmdArg+1

; get config file name
		mov ax,0eh
		int 22h

; open config file
		mov si,bx ; es:bx config file name
		mov ax,6
		int 22h
		jc no_config_file
		and eax,eax
		jz no_config_file
		jmp got_config_file
no_config_file:
		push es
		push bx
		push cs
		pop es
		mov bx, msg_config_file
		mov ax,2
		int 22h
		mov bx, msg_space
		mov ax,2
		int 22h
		pop bx
		pop es
		mov ax,2
		int 22h
		push cs
		pop es
		mov bx, msg_space
		mov ax,2
		int 22h
		mov bx, msg_missing
		mov ax,2
		int 22h
		mov ax,2
		mov bx, msg_crlf
		int 22h
		ret
got_config_file:
		push cs
		pop es
		call parse_config

; get_gfx_file
		mov ax,cs
		add ax,2000h
		mov word [gfx_mem_start_seg],ax
		mov ax,[pspNextP]
		mov word [gfx_mem_end_seg],ax

		call gfx_init
		jc error

		call gfx_setup_menu
		jc exit

input:
		call gfx_input
		jc exit

		cmp eax,1
		jz exit

		cmp eax,2
		jz boot

		jmp input

boot:
		call far [gfx_bc_done]
		mov ax,cs
		mov es,ax
		mov bx,command_line
		mov ax,3
		int 22h
exit:
		call far [gfx_bc_done]
error:
		ret

cb_table	dw cb_status		; 0
		dw cb_fopen		; 1
		dw cb_fread		; 2
		dw cb_getcwd		; 3
		dw cb_chdir		; 4
		dw cb_readsector	; 5
cb_len		equ ($-cb_table)/2

gfx_cb:
		push cs
		pop ds

		cmp al,cb_len
		jae gfx_cb_error

		movzx bx,al
		add bx,bx
		call word [bx+cb_table]
		jmp gfx_cb_end
gfx_cb_error:
		mov al,0ffh
gfx_cb_end:
		retf

; Return status info
;
; return:
;  edx		filename buffer (64 bytes)
;
cb_status:
		mov edx,cs
		shl edx,4
		add edx,fname_buf

		xor al,al
		ret

; Open file
;
; return:
;    al		0: ok, 1: file not found
;   ecx		file length (al = 0)
;
cb_fopen:
		push ds
		pop es
		mov ax,6
		mov si,fname_buf
		int 22h
		jnc cb_fopen_ok
cb_fopen_failed:
		mov al,1
		jmp cb_fopen_end
cb_fopen_ok:
		mov ecx,eax
		mov [f_handle],si
		mov [f_size],ecx
		xor al,al
cb_fopen_end:
		ret

; Read next chunk
;
; return:
;   edx		buffer address (linear)
;   ecx		data length (< 64k)
;
cb_fread:
		cmp dword [f_size],0
		jz cb_fread_eof
		push ds
		pop es
		mov ax,7
		mov si,[f_handle]
		mov bx,trackbuf
		mov cx,[BufSafe]
		int 22h
		mov al,1
		jc cb_fread_end
		sub [f_size], ecx
		or si,si
		jnz cb_fread_noeof
		and dword [f_size],0
cb_fread_noeof:
		mov edx,cs
		shl edx,4
		add edx,trackbuf
		jmp cb_fread_ok
cb_fread_eof:
		xor ecx,ecx
cb_fread_ok:
		xor al,al
cb_fread_end:
		ret

; Return current working directory
;
; return:
;  edx		filename
;
cb_getcwd:
		mov edx,cs
		shl edx,4
		add edx,gfx_slash
		xor al,al
		ret

; Set current working directory
;
cb_chdir:
		xor al,al
		ret

; Read sector
;
;   edx		sector
;
; return:
;  edx		buffer (linear address)
;
;  Note: does not return on error!
;
cb_readsector:
		push esi
		push edi
		push ds
		pop es
		mov ax,19h
		xor esi,esi
		xor edi,edi
		mov cx,1
		mov bx,trackbuf
		int 22h
		pop edi
		pop esi
		mov al,1
		jc cb_readsector_end
		mov edx,ds
		shl dx,4
		add edx,trackbuf
		xor al,al
cb_readsector_end:
		ret

gfx_init:
		mov ax,0e801h
		xor bx,bx
		xor cx,cx
		xor dx,dx
		int 15h
		jnc got_e801

		mov ax,2
		mov bx, msg_memory
		int 22h
		stc
		ret

got_e801:
		cmp ax,3c00h
		jb mem_below_16mb
		shl ebx,6
		add eax,ebx

mem_below_16mb:
		shl eax,10
		mov [gfx_bios_mem_size],eax
		shr eax,20
		cmp ax,16
		jb skip_extended

		mov word [gfx_xmem_0],88h       ; 8MB at 8MB
		mov dword [gfx_save_area1],7f0000h      ; 8MB-64k

skip_extended:
		movzx ebx,word [gfx_mem_start_seg]
		shl ebx,4

		movzx ecx,word [gfx_mem_end_seg]
		shl ecx,4

		mov dword [gfx_mem],ebx
		mov dword [gfx_mem0_start],ebx
		mov dword [gfx_mem0_end],ecx

		call gfx_read_file
		jc gfx_init_end

		call gfx_get_sysconfig

		; align 4
		mov eax,[gfx_mem0_start]
		add eax,3
		and eax,~3
		mov [gfx_mem0_start],eax

; setup jump table
		les bx,[gfx_bc_jt]

		mov ax,[es:bx]
		mov [gfx_bc_init],ax
		mov [gfx_bc_init+2],es

		mov ax,[es:bx+2]
		mov [gfx_bc_done],ax
		mov [gfx_bc_done+2],es

		mov ax,[es:bx+4]
		mov [gfx_bc_input],ax
		mov [gfx_bc_input+2],es

		mov ax,[es:bx+6]
		mov [gfx_bc_menu_init],ax
		mov [gfx_bc_menu_init+2],es

; ...

		mov esi,cs
		shl esi,4
		add esi,gfx_sysconfig
		call far [gfx_bc_init]

gfx_init_end:
		ret

gfx_read_file:
; open file
; es:si - file name

		push cs
		pop es
		mov ax,6
		mov si,pspCmdArg+1
		int 22h
		jnc gfx_file_read
		stc
		ret

gfx_file_read:
; si - file handle
; eax - length of file in bytes, or -1
; cx - file block size

		mov edx,[gfx_mem0_end]
		sub edx,[gfx_mem0_start]
		sub edx,0fh		; space to allow for aligning later
		; edx: max allowed size

		cmp eax,-1		; unknown file size -> set to max allowed size
		jnz .has_size
		mov eax,edx
.has_size:
		cmp eax,edx
		jbe read_bootlogo

gfx_file_too_big:
		mov ax,2
		mov bx,msg_bootlogo_toobig
		int 22h
		stc
		ret

read_bootlogo:
		mov [file_length],eax
		mov edi,[gfx_mem]

; read file
; si - file handle
; es:bx - buffer
; cx - number of blocks to read

read:
		push eax
		mov ax,7
		mov bx,trackbuf
		mov cx,[BufSafe]
		int 22h

		push edi
		push ecx
		push si
		push es

		mov si,trackbuf
		push edi
		call gfx_l2so
		pop di
		pop es

		rep movsb ; move ds:si -> es:di, length ecx
		pop es
		pop si
		pop ecx
		pop edi

		pop eax

		; si == 0: EOF
		or si,si
		jz gfx_read_done
		add edi,ecx
		sub eax,ecx
		ja read
		jmp gfx_file_too_big
gfx_read_done:
		sub eax,ecx
		mov edx,[file_length]
		sub edx,eax
		; edx = real file size
		mov [gfx_archive_end],edx
		add edx,[gfx_mem0_start]
		add edx,0fh		; for alignment
		mov [gfx_mem0_start],edx

bootlogo_read_done:
		call find_file
		or eax,eax
		jnz found_bootlogo
		stc
		ret

found_bootlogo:
		push edi
		push eax
		add eax,edi
		push dword [gfx_mem]
		pop dword [gfx_archive_start]
		neg al
		and eax,byte 0fh
		jz no_align
		add [gfx_archive_start],eax

no_align:
		pop eax
		pop edi
		sub edi,[gfx_mem]
		mov ecx,[gfx_archive_start]
		add edi,ecx
		mov [gfx_file],edi
		add [gfx_archive_end],ecx
		add eax,edi
		shr eax,4
		mov [gfx_bc_jt+2],ax
		ret

gfx_get_sysconfig:
		mov ah,0
		cmp byte [derivative_id],33h
		jnz not_isolinux
		mov ah,2
not_isolinux:
		mov al,[drivenumber]
		mov [gfx_boot_drive],al
		cmp al,80h	; floppy ?
		jae not_floppy
		mov ah,1
not_floppy:
		mov byte [gfx_media_type],ah
		mov ah,[sectorshift]
		mov byte [gfx_sector_shift],ah
		mov ax,cs
		mov [gfx_bootloader_seg],ax
		ret

gfx_setup_menu:
		push es
		push ds
		pop es

		mov word [menu_desc+menu_ent_list],0
		mov di,[menu_seg]
		mov [menu_desc+menu_ent_list+2],di

		mov word [menu_desc+menu_default],dentry_buf
		mov [menu_desc+menu_default+2],cs

		mov di,256
		mov [menu_desc+menu_arg_list],di
		mov di,[menu_seg]
		mov [menu_desc+menu_arg_list+2],di

		mov cx,[label_cnt]
		mov [menu_desc+menu_entries],cx

		mov cx,256*2
		mov [menu_desc+menu_ent_size],cx
		mov [menu_desc+menu_arg_size],cx

		mov esi,ds
		shl esi,4
		add esi,menu_desc

		call far [gfx_bc_menu_init]
		pop es
		ret

magic_ok:
		xor eax,eax
		cmp dword [es:bx],0b2d97f00h    ; header.magic_id
		jnz magic_ok_end
		cmp byte [es:bx+4],8            ; header.version
		jnz magic_ok_end
		mov eax,[es:bx+8]
magic_ok_end:
		ret

find_file:
		mov edi,[gfx_mem]
		push edi
		call gfx_l2so
		pop bx
		pop es
		call magic_ok
		or eax,eax
		jnz find_file_end

find_file_loop:
		mov ecx,[gfx_mem0_start]
		sub ecx,26 + 12                 ; min cpio header + gfx header
		cmp edi,ecx
		jae find_file_end
		push edi
		call gfx_l2so
		pop bx
		pop es
		cmp word [es:bx],71c7h
		jnz find_file_end
		mov ax,[es:bx+20]               ; file name size
		movzx esi,ax

		inc si
		and si,~1                       ; align

		mov eax,[es:bx+22]              ; data size
		rol eax,16                      ; get word order right
		mov ecx,eax

		inc ecx
		and ecx,byte ~1                 ; align

		add si,26                       ; skip header

		add edi,esi
		add bx,si
		call magic_ok
		or eax,eax
		jnz find_file_end

		add edi,ecx
		jmp find_file_loop

find_file_end:
		ret

gfx_input:
		mov edi,cs
		shl edi,4
		add edi, command_line ; buffer (0: no buffer)
		mov ecx, max_cmd_len ; buffer size
;		xor eax,eax ; timeout value (0: no timeout)
		mov eax,100 ; timeout value (0: no timeout)

		call far [gfx_bc_input]
		ret

gfx_l2so:
		push eax
		mov eax,[esp + 6]
		shr eax,4
		mov [esp + 8],ax
		and word [esp + 6],byte 0fh
		pop eax
		ret

parse_config:
		mov [f_handle],si
		push es
		mov ax,cs
		add ax,1000h
		mov es,ax
		mov word [menu_seg],ax
		xor eax,eax
		mov cx,4000h
		mov di,0
		rep stosd
		pop es
.read:
		call skipspace
		jz .eof
		jc .read
		cmp al,'#'
		je .nextline
		or al,20h ; convert to lower case
		mov di,configbuf
		stosb
.read_loop:
		call getc
		jc .eof
		cmp al,' '
		jbe .done
		or al,20h ; convert to lower case
		stosb
		jmp .read_loop
.done:
		call ungetc

		xor ax,ax
		stosb
%ifdef DEBUG
		mov ax,2
		mov bx, configbuf
		int 22h

		mov ax,2
		mov bx, msg_crlf
		int 22h
%endif
		push si
		push di
		xor ecx,ecx
		mov si,configbuf
		mov di,label_keyword+1
		mov cl, byte [label_keyword]
		call memcmp
		pop di
		pop si
		jz .do_label

		push si
		push di
		xor ecx,ecx
		mov si,configbuf
		mov di,default_keyword+1
		mov cl, byte [default_keyword]
		call memcmp
		pop di
		pop si
		jz .do_default

.nextline:
		call skipline
		jmp .read

.do_label:
		call skipspace
		jz .eof
		jc .noparm
		call ungetc
		push es
		push di
		mov ax,[menu_seg]
		mov es,ax
		mov di,[menu_off]
		call getline
		mov di,[menu_off]
		add di,512
		mov [menu_off],di
		pop di
		pop es
		inc word [label_cnt]

		jmp .read

.do_default:
		call skipspace
		jz .eof
		jc .noparm
		call ungetc
		push es
		push di
		push cs
		pop es
		mov di,dentry_buf
		call getline
		pop di
		pop es

		jmp .read

.eof:
.noparm:
		ret

skipline:
		cmp al,10
		je .end
		call getc
		jc .end
		jmp skipline
.end:
		ret

skipspace:
.loop:
		call getc
		jc .eof
		cmp al,0Ah
		je .eoln
		cmp al,' '
		jbe .loop
		ret
.eof:
		cmp al,al
		stc
		ret
.eoln:
		add al,0FFh
		ret

ungetc:
		mov byte [ungetc_cnt],1
		mov byte [ungetcdata],al
		ret

getc:
		cmp byte [ungetc_cnt],1
		jne .noungetc
		mov byte [ungetc_cnt],0
		mov al,[ungetcdata]
		clc
		ret
.noungetc:
		sub word [bufbytes],1
		jc .get_data
		mov si,trackbuf
		add si,[bufdata]
		mov al,[si]
		inc word [bufdata]
		clc
		ret
.get_data:
		mov si,[f_handle]
		and si,si
		jz .empty
		mov ax,7
		mov bx,trackbuf
		mov cx,[BufSafe]
		int 22h
		mov word [bufdata],0
		jc .empty
		mov [f_handle],si
		mov [bufbytes],cx
		jmp getc
.empty:
		mov word [f_handle],0
		mov word [bufbytes],0
		stc
		ret

getline:
		call skipspace
		jz .eof
		jc .eoln
		call ungetc
.loop:
		call getc
		jc .ret
		cmp al,' '
		jna .ctrl
.store:
		stosb
		jmp .loop
.ctrl:
		cmp al,10
		je .ret
		mov al,' '
		jmp .store
.eoln:
		clc
		jmp .ret
.eof:
		stc
.ret:
		xor al,al
		stosb
		ret


memcmp:
		push si
		push di
		push ax
.loop:
		mov al,[si]
		mov ah,[di]
		inc si
		inc di
		cmp al,ah
		loope .loop
		pop ax
		pop di
		pop si
		ret

		section .data
label_keyword		db 6,'label',0
default_keyword		db 7,'default',0

msg_progname		db 'gfxboot: ',0
msg_config_file		db 'Configuration file',0
msg_missing		db 'missing',0
msg_usage		db 'Usage: gfxboot.com <bootlogo>',0dh,0ah,0
msg_memory		db 'Could not detect available memory size',0dh,0ah,0
msg_bootlogo_toobig	db 'bootlogo file too big',0dh,0ah,0
msg_pxelinux		db 'pxelinux is not supported',0dh,0ah,0
msg_unknown_file_size	db 'unknown file size',0dh,0ah,0
msg_space		db ' ',0
msg_crlf		db 0dh,0ah,0

gfx_slash		db '/', 0
db0			db 0

; menu entry descriptor
menu_entries		equ 0
menu_default		equ 2		; seg:ofs
menu_ent_list		equ 6		; seg:ofs
menu_ent_size		equ 10
menu_arg_list		equ 12		; seg:ofs
menu_arg_size		equ 16
sizeof_menu_desc	equ 18

; system config data (52 bytes)
gfx_sysconfig		equ $
gfx_bootloader		db 1			;  0: boot loader type (0: lilo, 1: syslinux, 2: grub)
gfx_sector_shift	db 9			;  1: sector shift
gfx_media_type		db 0			;  2: media type (0: disk, 1: floppy, 2: cdrom)
gfx_failsafe		db 0			;  3: turn on failsafe mode (bitmask)
						;     0: SHIFT pressed
						;     1: skip gfxboot
						;     2: skip monitor detection
gfx_sysconfig_size	db gfx_sysconfig_end-gfx_sysconfig	;  4: size of sysconfig data
gfx_boot_drive		db 0			;  5: BIOS boot drive
gfx_callback		dw gfx_cb		;  6: offset to callback handler
gfx_bootloader_seg	dw 0			;  8: code/data segment used by bootloader; must follow gfx_callback
gfx_reserved_1		dw 0			; 10
gfx_user_info_0		dd 0			; 12: data for info box
gfx_user_info_1		dd 0			; 16: data for info box
gfx_bios_mem_size	dd 0			; 20: BIOS memory size (in bytes)
gfx_xmem_0		dw 0			; 24: extended mem area 0 (start:size in MB; 12:4 bits)
gfx_xmem_1		dw 0			; 26: extended mem area 1
gfx_xmem_2		dw 0			; 28: extended mem area 2
gfx_xmem_3		dw 0			; 20: extended mem area 3
gfx_file		dd 0			; 32: start of gfx file
gfx_archive_start	dd 0			; 36: start of cpio archive
gfx_archive_end		dd 0			; 40: end of cpio archive
gfx_mem0_start		dd 0			; 44: low free memory start
gfx_mem0_end		dd 0			; 48: low free memory end
gfx_sysconfig_end	equ $

			section .bss align=4096
trackbufsize		equ 16384
trackbuf		resb trackbufsize
configbuf		resb trackbufsize

dentry_buf		resb 512
dentry_buf_len		equ $ - dentry_buf

max_cmd_len		equ 2047
command_line		resb max_cmd_len+2

			alignb 4
derivative_id		resb 1
drivenumber		resb 1
sectorshift		resb 1
			resb 1			; Pad
sectorsize		resw 1
BufSafe			resw 1
file_length		resd 1

bufbytes		resw 1
bufdata			resw 1
ungetc_cnt		resb 1
ungetcdata		resb 1

f_handle		resw 1
f_size			resd 1
fname_buf		resb 64
fname_buf_len		equ $ - fname_buf

label_cnt		resw 1

menu_desc		resb sizeof_menu_desc
menu_seg		resw 1
menu_off		resw 1

gfx_mem_start_seg	resw 1
gfx_mem_end_seg		resw 1

			alignb 4
gfx_mem			resd 1		; linear address
gfx_save_area1		resd 1		; 64k
gfx_save_area1_used	resb 1		; != 0 if area1 is in use

			alignb 4
; interface to loadable gfx extension (seg:ofs values)
gfx_bc_jt		resd 1

gfx_bc_init		resd 1
gfx_bc_done		resd 1
gfx_bc_input		resd 1
gfx_bc_menu_init	resd 1
