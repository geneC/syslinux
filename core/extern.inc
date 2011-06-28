;
; extern.inc
;
; Prototypes for external functions

%ifndef EXTERN_INC
%define EXTERN_INC

	; rllpack.c
	extern rllpack, rllunpack

	; fs.c
	extern fs_init, pm_searchdir, getfssec, getfsbytes
	extern pm_mangle_name, pm_load_config
        extern pm_open_file, pm_close_file
	extern SectorSize, SectorShift

	; chdir.c
	extern pm_realpath

        ; readdir.c
        extern opendir, readdir, closedir

	; newconfig.c
	extern pm_is_config_file

	; mem/init.c
	extern MallocStart, mem_init

	; sysappend.c
	extern do_sysappend, print_sysappend

	; dmi.c
	extern dmi_init

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
	extern unload_pxe, reset_pxe, http_bake_cookies
%endif

%endif ; EXTERN_INC
