;; -----------------------------------------------------------------------
;;
;;   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
;;   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
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

		call adv_init
;
; Check for an ADV boot-once entry
;
		mov dl,ADV_BOOTONCE
		call adv_get
		jcxz .no_bootonce

.have_bootone:
		; We apparently have a boot-once set; clear it and
		; then execute the boot-once...

		; Save the boot-once data; SI = data, CX = length
		mov di,command_line
		rep movsb
		xor ax,ax
		stosb

		; Clear the boot-once data from the ADV
		xor cx,cx			; Set to zero = delete
		call adv_set
		jc .err
		call adv_write
.err:		jmp load_kernel

.no_bootonce:

;
; Check whether or not we are supposed to display the boot prompt.
;
check_for_key:
		test byte [KbdFlags],5Bh	; Shift Alt Caps Scroll
		jnz enter_command
		cmp word [ForcePrompt],0	; Force prompt?
		jz auto_boot
		cmp word [DefaultLevel],1	; Active UI statement?
		ja auto_boot

enter_command:
		cmp word [NoEscape],0		; If NOESCAPE, no prompt,
		jne auto_boot			; always run default cmd

		mov si,boot_prompt
		call writestr

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
		jnz ctrl_f			; Keystroke after <Ctrl-F>
		cmp di,max_cmd_len+command_line ; Check there's space
		jnb short get_char
		stosb				; Save it
		call writechr			; Echo to screen
		jmp short get_char
not_ascii:
		cmp al,0Dh			; Enter
		je command_done
		cmp al,09h			; Tab
		je display_labels
		cmp al,'F' & 1Fh		; <Ctrl-F>
		je set_func_flag
%if IS_PXELINUX
		cmp al,'N' & 1Fh		; <Ctrl-N>
		je show_network_info
%endif
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
		call writestr
get_char_2:
		jmp short get_char

kill_command:
		call crlf
		jmp enter_command

force_text_mode:
		call vgaclearmode
		jmp enter_command

set_func_flag:
		mov byte [FuncFlag],1
		jmp short get_char_2

display_labels:
		cmp word [NoComplete],0		; Label completion enabled?
		jne get_char_2
		push di				; Save pointer
		mov cx,di
		sub cx,command_line
		call crlf
		mov esi,[HighMemSize]		; Start from top of memory
.scan:
		cmp esi,[VKernelEnd]
		jbe .not_vk

		push cx				; save command line size

		mov edi,VKernelBuf
		pm_call rllunpack
		; ESI updated on return

		sub di,cx			; Return to beginning of buf
		pop cx				; restore command line size
		push si				; save SI
		cmp cx,0
		jz .print
		push di
		push cx
		mov si,command_line
		es repe cmpsb
		pop cx
		pop di
		jne .next
.print:
		mov al,' '
		call writechr

		mov si,di
		call writestr
.next:
		pop si				; restore SI
		jmp .scan
.not_vk:
		call crlf
		jmp fk_wrcmd

ctrl_f:
		xor ah,ah
		mov [FuncFlag],ah
		cmp al,'0'
		jb get_char_2
		je .zero			; <Ctrl-F>0 = F10
		or al,20h			; Lower case
		cmp al,'9'
		jna .digit
		cmp al,'a'			; F10-F12 = <Ctrl-F>A, B, C
		jb get_char_2
		cmp al,'c'
		ja get_char_2
		sub al,'a'-10
		jmp show_help
.zero:
		mov al,10
		jmp show_help
.digit:
		sub al,'1'
		jmp show_help

func_key:
		; AL = 0 if we get here
		xchg al,ah
		cmp al,44h			; F10
		ja .f11_f12
		sub al,3Bh			; F1
		jb get_char_2
		jmp show_help
.f11_f12:
		cmp al,85h			; F11
		jb get_char_2
		cmp al,86h			; F12
		ja get_char_2
		sub al,85h-10

show_help:	; AX = func key # (0 = F1, 9 = F10, 11 = F12)
		push di				; Save end-of-cmdline pointer
		shl ax,FILENAME_MAX_LG2		; Convert to pointer
		add ax,FKeyName
		xchg di,ax
		cmp byte [di+NULLOFFSET],NULLFILE
		je short fk_nofile		; Undefined F-key
		call core_open
		jz short fk_nofile		; File not found
		call crlf
		call get_msg_file
		jmp short fk_wrcmd

print_version:
		push di				; Command line write pointer
		mov si,syslinux_banner
		call writestr
%ifdef HAVE_BIOSNAME
		mov si,[BIOSName]
		call writestr
%endif
		mov si,copyright_str
		call writestr

		; ... fall through ...

		; Write the boot prompt and command line again and
		; wait for input.  Note that this expects the cursor
		; to already have been CRLF'd, and that the old value
		; of DI (the command line write pointer) is on the stack.
fk_wrcmd:
		mov si,boot_prompt
		call writestr
		pop di				; Command line write pointer
		push di
		mov byte [di],0			; Null-terminate command line
		mov si,command_line
		call writestr			; Write command line so far
fk_nofile:	pop di
		jmp get_char

;
; Show network info (in the form of the ipappend strings)
;
%if IS_PXELINUX
show_network_info:
		push di				; Command line write pointer
		call crlf
		mov si,IPAppends		; See comboot.doc
		mov cx,numIPAppends
.loop:
		lodsw
		push si
		mov si,ax
		call writestr
		call crlf
		pop si
		loop .loop
		jmp fk_wrcmd
%endif

;
; Jump here to run the default command line
;
auto_boot:
		cmp word [DefaultLevel],0	; No UI or DEFAULT?
		jne .have_default
		mov si,no_default_msg
		call writestr
		cmp word [NoEscape],0		; NOESCAPE but no DEFAULT?
		jne kaboom			; If so, we're stuck!
		jmp enter_command

.have_default:
		mov si,default_cmd
		mov di,command_line
		mov cx,(max_cmd_len+4) >> 2
		rep movsd
		jmp short load_kernel

		section .data16
no_default_msg	db 'No DEFAULT or UI configuration directive found!'
		db CR, LF, 0

		section .text16

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
		pm_call pm_mangle_name
                pop si
;
; Fast-forward to first option (we start over from the beginning, since
; pm_mangle_name doesn't necessarily return a consistent ending state.)
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
		mov esi,[HighMemSize]		; Start from top of memory
.scan:
		cmp esi,[VKernelEnd]
		jbe .not_vk

		mov edi,VKernelBuf
		pm_call rllunpack
		; ESI updated on return

		sub di,cx			; Return to beginning of buf
		push si
		mov si,command_line
.loop:
		lodsb
		cmp al,' '
		jbe .done
		scasb
		je .loop
.nomatch:
		pop si
		jmp .scan
.done:
		cmp byte [di],0			; Must match end of string
		jne .nomatch
		pop si

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
		mov byte [es:di],cl		; Null-terminate
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

		mov al,[VKernelBuf+vk_type]
		mov [KernelType],al

		; Is this a "localboot" pseudo-kernel?
		cmp al,VK_LOCALBOOT		; al == KernelType
		mov ax,[VKernelBuf+vk_rname]	; Possible localboot type
		je local_boot
		jmp get_kernel

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
		mov byte [es:di],cl		; Null-terminate
                mov [CmdLinePtr],di
                pop di
                pop si
                pop es

		mov [KernelType], cl		; CL == 0 here

;
; Find the kernel on disk
;
get_kernel:     mov byte [KernelName+FILENAME_MAX],0	; Zero-terminate filename/extension
		mov di,KernelName
		cmp byte [di],' '
		jbe bad_kernel			; Missing kernel name
		xor al,al
		mov cx,FILENAME_MAX-5		; Need 4 chars + null
		repne scasb			; Scan for final null
		jne .no_skip
		dec di				; Point to final null
.no_skip:	mov [KernelExtPtr],di
		mov bx,exten_table
.search_loop:	push bx
                mov di,KernelName		; Search on disk
                pm_call pm_searchdir
		pop bx
                jnz kernel_good
		mov eax,[bx]			; Try a different extension
		mov si,[KernelExtPtr]
		mov [si],eax
		mov byte [si+4],0
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
		mov si,err_notfound		; Complain about missing kernel
		call writestr
		mov si,KernelName
                call writestr
                mov si,crlf_msg
                jmp abort_load                  ; Ask user for clue

;
; on_error: bad kernel, but we have onerror set; CX = OnerrorLen
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
		call reset_idle

.loop:
		push word [__jiffies]
		call pollchar
		jnz .got_char
		call do_idle
		pop ax
		cmp ax,[__jiffies]		; Has the timer advanced?
		je .loop

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
; .com	- COMBOOT image
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
		section .bss16
		alignb 4
Kernel_EAX	resd 1
Kernel_SI	resw 1

		section .text16
kernel_good_saved:
		; Alternate entry point for which the return from
		; searchdir is stored in memory.  This is used for
		; COMBOOT function INT 22h, AX=0016h.
		mov si,[Kernel_SI]
		mov eax,[Kernel_EAX]

kernel_good:
		pushad
		;
		; Common initialization for all kernel types
		;
		xor ax,ax
		mov [InitRDPtr],ax
		mov [QuietBoot],al
%if IS_PXELINUX
		mov [KeepPXE],al
%endif

		; Default memory limit, can be overridden by image loaders
		mov eax,[HighMemRsvd]
		mov [MyHighMemSize],eax

		popad

		push di
		push ax
		mov di,KernelName
		xor al,al
		mov cx,FILENAME_MAX
		repne scasb
		jne .one_step
		dec di
.one_step:	mov ecx,[di-4]			; 4 bytes before end
		pop ax
		pop di

;
; At this point, EAX contains the size of the kernel, SI contains
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
		shr ecx,8
		cmp ecx,'.bs'
		je is_bootsector
		shr ecx,8
		cmp cx,'.0'
		je is_bootsector

		; Otherwise Linux kernel
		jmp is_linux_kernel

is_config_file:
		push si
		call make_plain_cmdline
		pm_call pm_is_config_file
		pop si
		call openfd
		call reset_config
		jmp load_config_file

; This is an image type we can't deal with
is_bad_image:
		mov si,err_badimage
		call writestr
		jmp enter_command

%if IS_SYSLINUX
		; ok
%else
is_bss_sector	equ is_bad_image
%endif

is_disk_image	equ is_bad_image	; No longer supported

		section .data16
boot_prompt	db 'boot: ', 0
wipe_char	db BS, ' ', BS, 0
err_badimage	db 'Invalid image type for this media type!', CR, LF, 0
err_notfound	db 'Could not find kernel image: ',0
err_notkernel	db CR, LF, 'Invalid or corrupt kernel image.', CR, LF, 0


		alignz 2
kerneltype_table:
		dw is_unknown_filetype	; VK_KERNEL
		dw is_linux_kernel	; VK_LINUX
		dw is_bootsector	; VK_BOOT
		dw is_bss_sector	; VK_BSS
		dw is_bootsector	; VK_PXE
		dw is_disk_image	; VK_FDIMAGE
		dw is_comboot_image	; VK_COMBOOT
		dw is_com32_image	; VK_COM32
		dw is_config_file	; VK_CONFIG

		section .bss16
		alignb 4
ThisKbdTo	resd 1			; Temporary holder for KbdTimeout
ThisTotalTo	resd 1			; Temporary holder for TotalTimeout
KernelExtPtr	resw 1			; During search, final null pointer
CmdOptPtr       resw 1			; Pointer to first option on cmd line
KbdFlags	resb 1			; Check for keyboard escapes
FuncFlag	resb 1			; Escape sequences received from keyboard
KernelType	resb 1			; Kernel type, from vkernel, if known

		section .text16
;
; Linux kernel loading code is common.
;
%include "runkernel.inc"

;
; COMBOOT-loading code
;
%include "comboot.inc"
%include "com32.inc"
%include "cmdline.inc"

;
; Boot sector loading code
;
%include "bootsect.inc"

;
; Abort loading code
;
%include "abort.inc"

;
; Hardware cleanup common code
;
%include "cleanup.inc"
