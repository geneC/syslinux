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

	; cache.c
	extern cache_init

	; load_config.c
	extern load_config

	; ext2.c
	extern init_fs, searchdir, getfssec

%endif ; EXTERN_INC
