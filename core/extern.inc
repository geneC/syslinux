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

	; fs.c
	extern fs_init, pm_searchdir, getfssec, mangle_name, load_config
        extern unmangle_name, close_file

	; chdir.c
	extern pm_realpath

        ; readdir.c
        extern opendir, readdir, closedir
%if IS_PXELINUX
	; pxe.c
	extern unload_pxe, reset_pxe
%endif

%endif ; EXTERN_INC
