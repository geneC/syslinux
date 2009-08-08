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
	extern fs_init, searchdir, getfssec, mangle_name, load_config
        extern unmangle_name

%if IS_SYSLINUX
        ; fat.c
        extern alloc_fill_dir, readdir
%endif

%endif ; EXTERN_INC
