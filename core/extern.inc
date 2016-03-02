;
; extern.inc
;
; Prototypes for external functions

%ifndef EXTERN_INC
%define EXTERN_INC

	; rllpack.c
	extern rllpack, rllunpack

	; hello.c
	extern hello

	; elflink/load_env32.c
	extern load_env32, pm_env32_run

	extern mp1, mp2, mp3, mp4, mp5

	extern hexdump

	extern mem_init

	; fs.c
	extern pm_fs_init
	extern SectorSize, SectorShift

	; chdir.c
	extern pm_realpath

        ; readdir.c
        extern opendir, readdir, closedir

	; idle.c
	extern __idle

%ifdef DEBUG
	; debug.c
	extern pm_debug_msg

  %macro dprint 1+
	push ax
	call %%fwd
	db %1
	db 0
%%fwd:	pop ax
	pm_call pm_debug_msg
	pop ax
  %endmacro
%else
  %macro dprint 1+
  %endmacro
%endif

%if IS_PXELINUX
	; pxe.c
	extern unload_pxe, reset_pxe
%endif

	; plaincon.c
	extern pm_writechr

	; cleanup.c
	extern cleanup_hardware

	; writestr.c
	extern pm_writestr, crlf

	; writehex.c
	extern pm_writehex2, pm_writehex4, pm_writehex8

	; graphics.c
	extern syslinux_force_text_mode, vgashowcursor, vgahidecursor, pm_using_vga

	; conio.c
	extern pm_pollchar, pm_write_serial, pm_serialcfg

	; font.c
	extern pm_getchar, pm_adjust_screen, pm_userfont

%endif ; EXTERN_INC
