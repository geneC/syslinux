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
	extern load_env32

	; fs.c
	extern fs_init, searchdir, getfssec, mangle_name, load_config
        extern unmangle_name, close_file

        ; dir.c
        extern vfs_opendir, vfs_readdir, vfs_closedir
%if IS_PXELINUX
	; pxe.c
	extern unload_pxe, reset_pxe
%endif

%endif ; EXTERN_INC
