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

;;
;; runkernel.inc
;;
;; Common code for running a Linux kernel
;;

;
; Hook macros, that may or may not be defined
;
%ifndef HAVE_UNLOAD_PREP
%macro UNLOAD_PREP 0
%endmacro
%endif

;
; A Linux kernel consists of three parts: boot sector, setup code, and
; kernel code.	The boot sector is never executed when using an external
; booting utility, but it contains some status bytes that are necessary.
;
; First check that our kernel is at least 1K, or else it isn't long
; enough to have the appropriate headers.
;
; We used to require the kernel to be 64K or larger, but it has gotten
; popular to use the Linux kernel format for other things, which may
; not be so large.
;
; Additionally, we used to have a test for 8 MB or smaller.  Equally
; obsolete.
;
is_linux_kernel:
		push si				; <A> file pointer

;
; Now start transferring the kernel
;
		push word real_mode_seg
		pop es

;
; Start by loading the bootsector/setup code, to see if we need to
; do something funky.  It should fit in the first 32K (loading 64K won't
; work since we might have funny stuff up near the end of memory).
;
		call abort_check		; Check for abort key
		mov cx,8000h			; Half a moby (32K)
		xor bx,bx
                pop si                          ; <A> file pointer
		pm_call getfsbytes
		cmp cx,1024
		jb kernel_corrupt
                cmp word [es:bs_bootsign],0AA55h
		jne kernel_corrupt		; Boot sec signature missing

;
; Save the file pointer for later...
;
		push si				; <A> file pointer

;
; Construct the command line (append options have already been copied)
;
construct_cmdline:
		mov di,[CmdLinePtr]
                mov si,boot_image		; BOOT_IMAGE=
                mov cx,boot_image_len
                rep movsb
                mov si,KernelName		; Unmangled kernel name
		call strcpy
		mov byte [es:di-1],' '		; Follow by space

		call do_ip_append		; Handle IPAppend

                mov si,[CmdOptPtr]              ; Options from user input
		call strcpy

;
; Scan through the command line for anything that looks like we might be
; interested in.  The original version of this code automatically assumed
; the first option was BOOT_IMAGE=, but that is no longer certain.
;
parse_cmdline:
		mov di,cmd_line_here
.skipspace:	mov al,[es:di]
		inc di
.skipspace_loaded:
		and al,al
		jz cmdline_end
		cmp al,' '
		jbe .skipspace
		dec di

		; ES:DI now points to the beginning of an option
		mov si,options_list
.next_opt:
		movzx cx,byte [si]
		jcxz .skip_opt
		push di
		inc si
		repe cmpsb
		jne .no_match

		; This either needs to have been an option with parameter,
		; or be followed by EOL/whitespace
		mov ax,[es:di-1]		; AL = last chr; AH = following
		cmp al,'='
		je .is_match
		cmp ah,' '
		ja .no_match
.is_match:
		pop ax				; Drop option pointer on stack
		call [si]
.skip_opt:
		mov al,[es:di]
		inc di
		cmp al,' '
		ja .skip_opt
		jmp .skipspace_loaded
.no_match:
		pop di
		add si,cx			; Skip remaining bytes
		inc si				; Skip function pointer
		inc si
		jmp .next_opt

opt_vga:
		mov ax,[es:di-1]
		mov bx,-1
		cmp ax,'=n'			; vga=normal
		je .vc0
		dec bx				; bx <- -2
		cmp ax,'=e'			; vga=ext
		je .vc0
		dec bx				; bx <- -3
		cmp ax,'=a'			; vga=ask
		je .vc0
		mov bx,0x0f04			; bx <- 0x0f04 (current mode)
		cmp ax,'=c'			; vga=current
		je .vc0
		call parseint_esdi		; vga=<number>
		jc .skip			; Not an integer
.vc0:		mov [es:bs_vidmode],bx		; Set video mode
.skip:
		ret

opt_mem:
		call parseint_esdi
		jc .skip
%if HIGHMEM_SLOP != 0
		sub ebx,HIGHMEM_SLOP
%endif
		mov [MyHighMemSize],ebx
.skip:
		ret

opt_quiet:
		mov byte [QuietBoot],QUIET_FLAG
		ret

%if IS_PXELINUX
opt_keeppxe:
		or byte [KeepPXE],1		; KeepPXE set by command line
		ret
%endif

opt_initrd:
		mov ax,di
		cmp byte [es:di],' '
		ja .have_initrd
		xor ax,ax
.have_initrd:
		mov [InitRDPtr],ax
		ret

;
; After command line parsing...
;
cmdline_end:
		sub di,cmd_line_here
		mov [CmdLineLen],di		; Length including final null

;
; Now check if we have a large kernel, which needs to be loaded high
;
prepare_header:
		mov dword [RamdiskMax], HIGHMEM_MAX	; Default initrd limit
		cmp dword [es:su_header],HEADER_ID	; New setup code ID
		jne old_kernel			; Old kernel, load low
		mov ax,[es:su_version]
		mov [KernelVersion],ax
		cmp ax,0200h			; Setup code version 2.0
		jb old_kernel			; Old kernel, load low
		cmp ax,0201h			; Version 2.01+?
                jb new_kernel                   ; If 2.00, skip this step
		; Set up the heap (assuming loading high for now)
                mov word [es:su_heapend],linux_stack-512
                or byte [es:su_loadflags],80h	; Let the kernel know we care
		cmp ax,0203h			; Version 2.03+?
		jb new_kernel			; Not 2.03+
		mov eax,[es:su_ramdisk_max]
		mov [RamdiskMax],eax		; Set the ramdisk limit

;
; We definitely have a new-style kernel.  Let the kernel know who we are,
; and that we are clueful
;
new_kernel:
		mov byte [es:su_loader],my_id	; Show some ID
		xor eax,eax
		mov [es:su_ramdisklen],eax	; No initrd loaded yet

;
; About to load the kernel.  This is a modern kernel, so use the boot flags
; we were provided.
;
                mov al,[es:su_loadflags]
		or al,[QuietBoot]		; Set QUIET_FLAG if needed
		mov [es:su_loadflags],al
		mov [LoadFlags],al

any_kernel:
		mov si,loading_msg
                call writestr_qchk
                mov si,KernelName		; Print kernel name part of
                call writestr_qchk		; "Loading" message

;
; Load the kernel.  We always load it at 100000h even if we're supposed to
; load it "low"; for a "low" load we copy it down to low memory right before
; jumping to it.
;
read_kernel:
		movzx ax,byte [es:bs_setupsecs]	; Setup sectors
		and ax,ax
		jnz .sects_ok
		mov al,4			; 0 = 4 setup sectors
.sects_ok:
		inc ax				; Including the boot sector
		mov [SetupSecs],ax

		call dot_pause

;
; Move the stuff beyond the setup code to high memory at 100000h
;
		movzx esi,word [SetupSecs]	; Setup sectors
                shl si,9			; Convert to bytes
                mov ecx,8000h			; 32K
		sub ecx,esi			; Number of bytes to copy
		add esi,core_real_mode		; Pointer to source
                mov edi,free_high_memory	; Copy to free high memory

                call bcopy			; Transfer to high memory

		pop si				; <A> File pointer
		and si,si			; EOF already?
		jz high_load_done

		; On exit EDI -> where to load the rest

		mov bx,dot_pause
		or eax,-1			; Load the whole file
		mov dx,3			; Pad to dword
		call load_high

high_load_done:
		mov [KernelEnd],edi
                mov ax,real_mode_seg		; Set to real mode seg
                mov es,ax

                mov si,dot_msg
                call writestr_qchk

;
; Some older kernels (1.2 era) would have more than 4 setup sectors, but
; would not rely on the boot protocol to manage that.  These kernels fail
; if they see protected-mode kernel data after the setup sectors, so
; clear that memory.
;
		push di
		mov di,[SetupSecs]
		shl di,9
		xor eax,eax
		mov cx,cmd_line_here
		sub cx,di
		shr cx,2
		rep stosd
		pop di

;
; Now see if we have an initial RAMdisk; if so, do requisite computation
; We know we have a new kernel; the old_kernel code already will have objected
; if we tried to load initrd using an old kernel
;
load_initrd:
		; Cap the ramdisk memory range if appropriate
		mov eax,[RamdiskMax]
		cmp eax,[MyHighMemSize]
		ja .ok
		mov [MyHighMemSize],eax
.ok:
		xor eax,eax
                cmp [InitRDPtr],ax
                jz .noinitrd
		call parse_load_initrd
.noinitrd:

;
; Abandon hope, ye that enter here!  We do no longer permit aborts.
;
                call abort_check		; Last chance!!

		mov si,ready_msg
		call writestr_qchk

		UNLOAD_PREP			; Module-specific hook

;
; Now, if we were supposed to load "low", copy the kernel down to 10000h
; and the real mode stuff to 90000h.  We assume that all bzImage kernels are
; capable of starting their setup from a different address.
;
		mov ax,real_mode_seg
		mov es,ax
		mov fs,ax

;
; If the default root device is set to FLOPPY (0000h), change to
; /dev/fd0 (0200h)
;
		cmp word [es:bs_rootdev],byte 0
		jne root_not_floppy
		mov word [es:bs_rootdev],0200h
root_not_floppy:

;
; Copy command line.  Unfortunately, the old kernel boot protocol requires
; the command line to exist in the 9xxxxh range even if the rest of the
; setup doesn't.
;
setup_command_line:
		mov dx,[KernelVersion]
		test byte [LoadFlags],LOAD_HIGH
		jz .need_high_cmdline
		cmp dx,0202h			; Support new cmdline protocol?
		jb .need_high_cmdline
		; New cmdline protocol
		; Store 32-bit (flat) pointer to command line
		; This is the "high" location, since we have bzImage
		mov dword [fs:su_cmd_line_ptr],cmd_line
		mov word [HeapEnd],linux_stack
		mov word [fs:su_heapend],linux_stack-512
		jmp .setup_done

.need_high_cmdline:
;
; Copy command line down to fit in high conventional memory
; -- this happens if we have a zImage kernel or the protocol
; is less than 2.02.
;
		mov si,cmd_line_here
		mov di,old_cmd_line_here
		mov [fs:kern_cmd_magic],word CMD_MAGIC ; Store magic
		mov [fs:kern_cmd_offset],di	; Store pointer
		mov word [HeapEnd],old_linux_stack
		mov ax,255			; Max cmdline limit
		cmp dx,0201h
		jb .adjusted
		; Protocol 2.01+
		mov word [fs:su_heapend],old_linux_stack-512
		jbe .adjusted
		; Protocol 2.02+
		; Note that the only reason we would end up here is
		; because we have a zImage, so we anticipate the move
		; to 90000h already...
		mov dword [fs:su_cmd_line_ptr],0x90000+old_cmd_line_here
		mov ax,old_max_cmd_len		; 2.02+ allow a higher limit
.adjusted:

		mov cx,[CmdLineLen]
		cmp cx,ax
		jna .len_ok
		mov cx,ax			; Truncate the command line
.len_ok:
		fs rep movsb
		stosb				; Final null, note AL=0 already
		mov [CmdLineEnd],di
		cmp dx,0200h
		jb .nomovesize
		mov [es:su_movesize],di		; Tell the kernel what to move
.nomovesize:
.setup_done:

;
; Time to start setting up move descriptors
;
setup_move:
		mov di,trackbuf
		xor cx,cx			; Number of descriptors

		mov bx,es			; real_mode_seg
		mov fs,bx
		push ds				; We need DS == ES == CS here
		pop es

		mov edx,100000h
		test byte [LoadFlags],LOAD_HIGH
		jnz .loading_high

; Loading low: move real_mode stuff to 90000h, then move the kernel down
		mov eax,90000h
		stosd
		mov eax,core_real_mode
		stosd
		movzx eax,word [CmdLineEnd]
		stosd
		inc cx
		mov edx,10000h			; Revised target address
		mov bx,9000h			; Revised real mode segment

.loading_high:
		mov eax,edx			; Target address of kernel
		stosd
		mov eax,free_high_memory	; Where currently loaded
		stosd
		neg eax
		add eax,[KernelEnd]
		stosd
		inc cx

		cmp word [InitRDPtr],0		; Did we have an initrd?
		je .no_initrd

		mov eax,[fs:su_ramdiskat]
		stosd
		mov eax,[InitRDStart]
		stosd
		mov eax,[fs:su_ramdisklen]
		stosd
		inc cx

.no_initrd:
		push dword run_linux_kernel
		push cx				; descriptor list entries count

		; BX points to the final real mode segment, and will be loaded
		; into DS.

		test byte [QuietBoot],QUIET_FLAG
		jz replace_bootstrap
		jmp replace_bootstrap_noclearmode

run_linux_kernel:
;
; Set up segment registers and the Linux real-mode stack
; Note: ds == the real mode segment
;
		cli
		mov ax,ds
		mov ss,ax
		mov sp,strict word linux_stack
		; Point HeapEnd to the immediate of the instruction above
HeapEnd		equ $-2			; Self-modifying code!  Fun!
		mov es,ax
		mov fs,ax
		mov gs,ax

;
; We're done... now RUN THAT KERNEL!!!!
; Setup segment == real mode segment + 020h; we need to jump to offset
; zero in the real mode segment.
;
		add ax,020h
		push ax
		push word 0h
		retf

;
; Load an older kernel.  Older kernels always have 4 setup sectors, can't have
; initrd, and are always loaded low.
;
old_kernel:
		xor ax,ax
		cmp word [InitRDPtr],ax		; Old kernel can't have initrd
                je .load
                mov si,err_oldkernel
                jmp abort_load
.load:
		mov byte [LoadFlags],al		; Always low
		mov word [KernelVersion],ax	; Version 0.00
		jmp any_kernel

;
; parse_load_initrd
;
; Parse an initrd= option and load the initrds.  This sets
; InitRDStart and InitRDEnd with dword padding between; we then
; do a global memory shuffle to move it to the end of memory.
;
; On entry, EDI points to where to start loading.
;
parse_load_initrd:
		push es
		push ds
		mov ax,real_mode_seg
		mov ds,ax
		push cs
		pop es			; DS == real_mode_seg, ES == CS

		mov [cs:InitRDStart],edi
		mov [cs:InitRDEnd],edi

		mov si,[cs:InitRDPtr]

.get_chunk:
		; DS:SI points to the start of a name

		mov bx,si
.find_end:
		lodsb
		cmp al,','
		je .got_end
		cmp al,' '
		jbe .got_end
		jmp .find_end

.got_end:
		push ax			; Terminating character
		push si			; Next filename (if any)
		mov byte [si-1],0	; Zero-terminate
		mov si,bx		; Current filename

		push di
		mov di,InitRD		; Target buffer for mangled name
		pm_call pm_mangle_name
		pop di
		call loadinitrd

		pop si
		pop ax
		mov [si-1],al		; Restore ending byte

		cmp al,','
		je .get_chunk

		; Compute the initrd target location
		; Note: we round to a page boundary twice here.  The first
		; time it is to make sure we don't use any fractional page
		; which may be valid RAM but which will be ignored by the
		; kernel (and therefore is inaccessible.)  The second time
		; it is to make sure we start out on page boundary.
		mov edx,[cs:InitRDEnd]
		sub edx,[cs:InitRDStart]
		mov [su_ramdisklen],edx
		mov eax,[cs:MyHighMemSize]
		and ax,0F000h		; Round to a page boundary
		sub eax,edx
		and ax,0F000h		; Round to a page boundary
		mov [su_ramdiskat],eax

		pop ds
		pop es
		ret

;
; Load RAM disk into high memory
;
; Input:	InitRD		- set to the mangled name of the initrd
;		EDI		- location to load
; Output:	EDI		- location for next initrd
;		InitRDEnd	- updated
;
loadinitrd:
		push ds
		push es
		mov ax,cs			; CS == DS == ES
		mov ds,ax
		mov es,ax
		push edi
                mov di,InitRD
                pm_call pm_searchdir                  ; Look for it in directory
		pop edi
		jz .notthere

		push si
		mov si,crlfloading_msg		; Write "Loading "
		call writestr_qchk
                mov si,InitRD			; Write ramdisk name
                call writestr_qchk
                mov si,dotdot_msg		; Write dots
                call writestr_qchk
		pop si

.li_skip_echo:
		mov dx,3
		mov bx,dot_pause
		call load_high
		mov [InitRDEnd],ebx

		pop es
		pop ds
		ret

.notthere:
                mov si,err_noinitrd
                call writestr
                mov si,InitRD
                call writestr
                mov si,crlf_msg
                jmp abort_load

;
; writestr_qchk: writestr, except allows output to be suppressed
;		assumes CS == DS
;
writestr_qchk:
		test byte [QuietBoot],QUIET_FLAG
		jz writestr
		ret

		section .data16
crlfloading_msg	db CR, LF
loading_msg     db 'Loading ', 0
dotdot_msg      db '.'
dot_msg         db '.', 0
ready_msg	db 'ready.', CR, LF, 0
err_oldkernel   db 'Cannot load a ramdisk with an old kernel image.'
                db CR, LF, 0
err_noinitrd    db CR, LF, 'Could not find ramdisk image: ', 0

boot_image      db 'BOOT_IMAGE='
boot_image_len  equ $-boot_image

;
; Command line options we'd like to take a look at
;
%macro cmd_opt	2
%strlen cmd_opt_len	%1
	db cmd_opt_len
	db %1
	dw %2
%endmacro
options_list:
		cmd_opt "vga=", opt_vga
		cmd_opt "mem=", opt_mem
		cmd_opt "quiet", opt_quiet
str_initrd	equ $+1			; Pointer to "initrd=" in memory
		cmd_opt "initrd=", opt_initrd
%if IS_PXELINUX
		cmd_opt "keeppxe", opt_keeppxe
%endif
		db 0

		section .bss16
		alignb 4
MyHighMemSize	resd 1			; Possibly adjusted highmem size
RamdiskMax	resd 1			; Highest address for ramdisk
KernelSize	resd 1			; Size of kernel in bytes
KernelSects	resd 1			; Size of kernel in sectors
KernelEnd	resd 1			; Ending address of the kernel image
InitRDStart	resd 1			; Start of initrd (pre-relocation)
InitRDEnd	resd 1			; End of initrd (pre-relocation)
CmdLineLen	resw 1			; Length of command line including null
CmdLineEnd	resw 1			; End of the command line in real_mode_seg
SetupSecs	resw 1			; Number of setup sectors (+bootsect)
KernelVersion	resw 1			; Kernel protocol version
;
; These are derived from the command-line parser
;
InitRDPtr	resw 1			; Pointer to initrd= option in command line
LoadFlags	resb 1			; Loadflags from kernel
QuietBoot	resb 1			; Set if a quiet boot is requested
