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
	extern cache_init, get_cache_block

	; load_config.c
	extern load_config

%endif ; EXTERN_INC
