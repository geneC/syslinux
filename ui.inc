;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2005 H. Peter Anvin - All Rights Reserved
;;
;;   This program is free software; you can redistribute it and/or modify
;;   it under the terms of the GNU General Public License as published by
;;   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
;;   Boston MA 02111-1307, USA; either version 2 of the License, or
;;   (at your option) any later version; incorporated herein by reference.
;;
;; -----------------------------------------------------------------------

;
; This file should be entered with the config file open (for getc)
;
load_config_file:
		call parse_config		; Parse configuration file
no_config_file:

;
; Check whether or not we are supposed to display the boot prompt.
;
check_for_key:
		cmp word [ForcePrompt],0	; Force prompt?
		jnz enter_command
		test byte [KbdFlags],5Bh	; Shift Alt Caps Scroll
		jz auto_boot			; If neither, default boot

enter_command:
		cmp word [NoEscape],0		; If NOESCAPE, no prompt,
		jne auto_boot			; always run default cmd

		mov si,boot_prompt
		call cwritestr

		mov byte [FuncFlag],0		; <Ctrl-F> not pressed
		mov di,command_line

;
; get the very first character -- we can either time
; out, or receive a character press at this time.  Some dorky BIOSes stuff
; a return in the buffer on bootup, so wipe the keyboard buffer first.
;
clear_buffer:	mov ah,11h			; Check for pending char
		int 16h
		jz get_char_time
		mov ah,10h			; Get char
		int 16h
		jmp short clear_buffer

		; For the first character, both KbdTimeout and
		; TotalTimeout apply; after that, only TotalTimeout.

get_char_time:
		mov eax,[TotalTimeout]
		mov [ThisTotalTo],eax
		mov eax,[KbdTimeout]
		mov [ThisKbdTo],eax

get_char:
		call getchar_timeout
		and dword [ThisKbdTo],0		; For the next time...

		and al,al
		jz func_key

got_ascii:	cmp al,7Fh			; <DEL> == <BS>
		je backspace
		cmp al,' '			; ASCII?
		jb not_ascii
		ja enter_char
		cmp di,command_line		; Space must not be first
		je short get_char
enter_char:	test byte [FuncFlag],1
		jz .not_ctrl_f
		mov byte [FuncFlag],0
		cmp al,'0'
		jb .not_ctrl_f
		je ctrl_f_0
		cmp al,'9'
		jbe ctrl_f
.not_ctrl_f:	cmp di,max_cmd_len+command_line ; Check there's space
		jnb short get_char
		stosb				; Save it
		call writechr			; Echo to screen
		jmp short get_char
not_ascii:	mov byte [FuncFlag],0
		cmp al,0Dh			; Enter
		je command_done
		cmp al,'F' & 1Fh		; <Ctrl-F>
		je set_func_flag
		cmp al,'U' & 1Fh		; <Ctrl-U>
		je kill_command			; Kill input line
		cmp al,'V' & 1Fh		; <Ctrl-V>
		je print_version
		cmp al,'X' & 1Fh		; <Ctrl-X>
		je force_text_mode
		cmp al,08h			; Backspace
		jne get_char
backspace:	cmp di,command_line		; Make sure there is anything
		je get_char			; to erase
		dec di				; Unstore one character
		mov si,wipe_char		; and erase it from the screen
		call cwritestr
		jmp short get_char_2

kill_command:
		call crlf
		jmp enter_command

force_text_mode:
		call vgaclearmode
		jmp enter_command

set_func_flag:
		mov byte [FuncFlag],1
get_char_2:
		jmp short get_char

ctrl_f_0:	add al,10			; <Ctrl-F>0 == F10
ctrl_f:		sub al,'1'
		xor ah,ah
		jmp short show_help

func_key:
		; AL = 0 if we get here
		xchg al,ah
		cmp al,68			; F10
		ja short get_char_2
		sub al,59			; F1
		jb short get_char_2
show_help:	; AX = func key # (0 = F1, 9 = F10)
		push di				; Save end-of-cmdline pointer
		shl ax,FILENAME_MAX_LG2		; Convert to pointer
		add ax,FKeyName
		xchg di,ax
		cmp byte [di+NULLOFFSET],NULLFILE
		je short fk_nofile		; Undefined F-key
		call searchdir
		jz short fk_nofile		; File not found
		push si
		call crlf
		pop si
		call get_msg_file
		jmp short fk_wrcmd

print_version:
		push di				; Command line write pointer
		mov si,syslinux_banner
		call cwritestr
%ifdef HAVE_BIOSNAME
		mov si,[BIOSName]
		call cwritestr
%endif
		mov si,copyright_str
		call cwritestr

		; ... fall through ...

		; Write the boot prompt and command line again and
		; wait for input.  Note that this expects the cursor
		; to already have been CRLF'd, and that the old value
		; of DI (the command line write pointer) is on the stack.
fk_wrcmd:
		mov si,boot_prompt
		call cwritestr
		pop di				; Command line write pointer
		push di
		mov byte [di],0			; Null-terminate command line
		mov si,command_line
		call cwritestr			; Write command line so far
fk_nofile:	pop di
		jmp short get_char_2

;
; Jump here to run the default command line
;
auto_boot:
		mov si,default_cmd
		mov di,command_line
		mov cx,(max_cmd_len+4) >> 2
		rep movsd
		jmp short load_kernel

;
; Jump here when the command line is completed
;
command_done:
		call crlf
		cmp di,command_line		; Did we just hit return?
		je auto_boot
		xor al,al			; Store a final null
		stosb

load_kernel:					; Load the kernel now
;
; First we need to mangle the kernel name the way DOS would...
;
		mov si,command_line
                mov di,KernelName
                push si
                push di
		call mangle_name
		pop di
                pop si
;
; Fast-forward to first option (we start over from the beginning, since
; mangle_name doesn't necessarily return a consistent ending state.)
;
clin_non_wsp:   lodsb
                cmp al,' '
                ja clin_non_wsp
clin_is_wsp:    and al,al
                jz clin_opt_ptr
                lodsb
                cmp al,' '
                jbe clin_is_wsp
clin_opt_ptr:   dec si                          ; Point to first nonblank
                mov [CmdOptPtr],si		; Save ptr to first option
;
; If "allowoptions 0", put a null character here in order to ignore any
; user-specified options.
;
		mov ax,[AllowOptions]
		and ax,ax
		jnz clin_opt_ok
		mov [si],al
clin_opt_ok:

;
; Now check if it is a "virtual kernel"
;
vk_check:
		xor si,si			; Beginning of vk_seg
.scan:
		cmp si,[VKernelBytes]
		jae .not_vk

		push ds
		push word vk_seg
		pop ds

		mov di,VKernelBuf
		call rllunpack
		pop ds
		; SI updated on return

		sub di,cx			; Return to beginning of buf
		push si
		mov si,KernelName
		mov cx,FILENAME_MAX
		es repe cmpsb
		pop si
		je .found
		jmp .scan

;
; We *are* using a "virtual kernel"
;
.found:
		push es
		push word real_mode_seg
		pop es
		mov di,cmd_line_here
		mov si,VKernelBuf+vk_append
		mov cx,[VKernelBuf+vk_appendlen]
		rep movsb
		mov [CmdLinePtr],di		; Where to add rest of cmd
		pop es
		mov di,KernelName
		push di
		mov si,VKernelBuf+vk_rname
		mov cx,FILENAME_MAX		; We need ECX == CX later
		rep movsb
		pop di
%if IS_PXELINUX
		mov al,[VKernelBuf+vk_ipappend]
		mov [IPAppend],al
%endif
		xor bx,bx			; Try only one version

		mov al, [VKernelBuf+vk_type]
		mov [KernelType], al

%if IS_PXELINUX || IS_ISOLINUX
		; Is this a "localboot" pseudo-kernel?
%if IS_PXELINUX
		cmp byte [VKernelBuf+vk_rname+4], 0
%else
		cmp byte [VKernelBuf+vk_rname], 0
%endif
		jne get_kernel		; No, it's real, go get it

		mov ax, [VKernelBuf+vk_rname+1]
		jmp local_boot
%else
		jmp get_kernel
%endif

.not_vk:

;
; Not a "virtual kernel" - check that's OK and construct the command line
;
                cmp word [AllowImplicit],byte 0
                je bad_implicit
                push es
                push si
                push di
                mov di,real_mode_seg
                mov es,di
                mov si,AppendBuf
                mov di,cmd_line_here
                mov cx,[AppendLen]
                rep movsb
                mov [CmdLinePtr],di
                pop di
                pop si
                pop es

		mov [KernelType], cl		; CL == 0 here

;
; Find the kernel on disk
;
get_kernel:     mov byte [KernelName+FILENAME_MAX],0	; Zero-terminate filename/extension
%if IS_SYSLINUX || IS_MDSLINUX			; SYSLINUX has to deal with DOS mangled names...
		mov eax,[KernelName+8]		; Save initial extension
		mov [exten_table_end],eax	; Last case == initial ext.
%else
		mov di,KernelName+4*IS_PXELINUX
		xor al,al
		mov cx,FILENAME_MAX-5		; Need 4 chars + null
		repne scasb			; Scan for final null
		jne .no_skip
		dec di				; Point to final null
.no_skip:	mov [KernelExtPtr],di
%endif
		mov bx,exten_table
.search_loop:	push bx
                mov di,KernelName	      	; Search on disk
                call searchdir
		pop bx
                jnz kernel_good
		mov eax,[bx]			; Try a different extension
%if IS_SYSLINUX || IS_MDSLINUX
		mov [KernelName+8],eax
%else
		mov si,[KernelExtPtr]
		mov [si],eax
		mov byte [si+4],0
%endif
		add bx,byte 4
		cmp bx,exten_table_end
		jna .search_loop		; allow == case (final case)
		; Fall into bad_kernel
;
; bad_kernel: Kernel image not found
; bad_implicit: The user entered a nonvirtual kernel name, with "implicit 0"
;
bad_implicit:
bad_kernel:
		mov cx,[OnerrorLen]
		and cx,cx
		jnz on_error
.really:
		mov si,KernelName
                mov di,KernelCName
		push di
                call unmangle_name              ; Get human form
		mov si,err_notfound		; Complain about missing kernel
		call cwritestr
		pop si				; KernelCName
                call cwritestr
                mov si,crlf_msg
                jmp abort_load                  ; Ask user for clue

;
; on_error: bad kernel, but we have onerror set
;
on_error:
		mov si,Onerror
		mov di,command_line
		push si				; <A>
		push di				; <B>
		push cx				; <C>
		push cx				; <D>
		push di				; <E>
		repe cmpsb
		pop di				; <E> di == command_line
		pop bx				; <D> bx == [OnerrorLen]
		je bad_kernel.really		; Onerror matches command_line already
		neg bx				; bx == -[OnerrorLen]
		lea cx,[max_cmd_len+bx]
		; CX == max_cmd_len-[OnerrorLen]
		mov di,command_line+max_cmd_len-1
		mov byte [di+1],0		; Enforce null-termination
		lea si,[di+bx]
		std
		rep movsb			; Make space in command_line
		cld
		pop cx				; <C> cx == [OnerrorLen]
		pop di				; <B> di == command_line
		pop si				; <A> si  == Onerror
		rep movsb
		jmp load_kernel

;
; kernel_corrupt: Called if the kernel file does not seem healthy
;
kernel_corrupt: mov si,err_notkernel
                jmp abort_load

;
; Get a key, observing ThisKbdTO and ThisTotalTO -- those are timeouts
; which can be adjusted by the caller based on the corresponding
; master variables; on return they're updated.
;
; This cheats.  If we say "no timeout" we actually get a timeout of
; 7.5 years.
;
getchar_timeout:
		call vgashowcursor
		RESET_IDLE

.loop:
		push word [BIOS_timer]
		call pollchar
		jnz .got_char
		pop ax
		cmp ax,[BIOS_timer]		; Has the timer advanced?
		je .loop
		DO_IDLE

		dec dword [ThisKbdTo]
		jz .timeout
		dec dword [ThisTotalTo]
		jnz .loop

.timeout:
		; Timeout!!!!
		pop cx				; Discard return address
		call vgahidecursor
		mov si,Ontimeout		; Copy ontimeout command
		mov di,command_line
		mov cx,[OntimeoutLen]		; if we have one...
		rep movsb
		jmp command_done

.got_char:
		pop cx				; Discard
		call getchar
		call vgahidecursor
		ret

;
; This is it!  We have a name (and location on the disk)... let's load
; that sucker!!  First we have to decide what kind of file this is; base
; that decision on the file extension.  The following extensions are
; recognized; case insensitive:
;
; .com 	- COMBOOT image
; .cbt	- COMBOOT image
; .c32  - COM32 image
; .bs	- Boot sector
; .0	- PXE bootstrap program (PXELINUX only)
; .bin  - Boot sector
; .bss	- Boot sector, but transfer over DOS superblock (SYSLINUX only)
; .img  - Floppy image (ISOLINUX only)
;
; Anything else is assumed to be a Linux kernel.
;
		section .bss
		alignb 4
Kernel_EAX	resd 1
Kernel_SI	resw 1

		section .text
kernel_good_saved:
		; Alternate entry point for which the return from
		; searchdir is stored in memory.  This is used for
		; COMBOOT function INT 22h, AX=0016h.
		mov si,[Kernel_SI]
		mov eax,[Kernel_EAX]
		mov dx,[Kernel_EAX+2]

kernel_good:
		pusha
		mov si,KernelName
		mov di,KernelCName
		call unmangle_name
		sub di,KernelCName
		mov [KernelCNameLen],di
		popa

%if IS_SYSLINUX || IS_MDSLINUX
		mov ecx,[KernelName+7]
		mov cl,'.'
%else
		push di
		push ax
		mov di,KernelName+4*IS_PXELINUX
		xor al,al
		mov cx,FILENAME_MAX
		repne scasb
		jne .one_step
		dec di
.one_step:	mov ecx,[di-4]			; 4 bytes before end
		pop ax
		pop di
%endif

;
; At this point, DX:AX contains the size of the kernel, SI contains
; the file handle/cluster pointer, and ECX contains the extension (if any.)
;
		movzx di,byte [KernelType]
		add di,di
		jmp [kerneltype_table+di]

is_unknown_filetype:
		or ecx,20202000h		; Force lower case (except dot)

		cmp ecx,'.com'
		je is_comboot_image
		cmp ecx,'.cbt'
		je is_comboot_image
		cmp ecx,'.c32'
		je is_com32_image
%if IS_ISOLINUX
		cmp ecx,'.img'
		je is_disk_image
%endif
		cmp ecx,'.bss'
		je is_bss_sector
		cmp ecx,'.bin'
		je is_bootsector
%if IS_SYSLINUX || IS_MDSLINUX
		cmp ecx,'.bs '
		je is_bootsector
		cmp ecx,'.0  '
		je is_bootsector
%else
		shr ecx,8
		cmp ecx,'.bs'
		je is_bootsector
		shr ecx,8
		cmp cx,'.0'
		je is_bootsector
%endif
		; Otherwise Linux kernel
		jmp is_linux_kernel

is_config_file:
		call openfd
		call reset_config
		jmp load_config_file

; This is an image type we can't deal with
is_bad_image:
		mov si,err_badimage
		call cwritestr
		jmp enter_command

%if IS_SYSLINUX || IS_MDSLINUX
		; ok
%else
is_bss_sector	equ is_bad_image
%endif
%if IS_ISOLINUX
		; ok
%else
is_disk_image	equ is_bad_image
%endif

		section .data
err_badimage	db 'Invalid image type for this media type!', CR, LF, 0

		align 2, db 0
kerneltype_table:
		dw is_unknown_filetype	; VK_KERNEL
		dw is_linux_kernel	; VK_LINUX
		dw is_bootsector	; VK_BOOT
		dw is_bss_sector	; VK_BSS
		dw is_disk_image	; VK_FDIMAGE
		dw is_comboot_image	; VK_COMBOOT
		dw is_com32_image	; VK_COM32
		dw is_config_file	; VK_CONFIG

		section .bss
		alignb 4
ThisKbdTo	resd 1			; Temporary holder for KbdTimeout
ThisTotalTo	resd 1			; Temporary holder for TotalTimeout
KernelExtPtr	resw 1			; During search, final null pointer
CmdOptPtr       resw 1			; Pointer to first option on cmd line
KbdFlags	resb 1			; Check for keyboard escapes
FuncFlag	resb 1			; Escape sequences received from keyboard
KernelType	resb 1			; Kernel type, from vkernel, if known

		section .text
