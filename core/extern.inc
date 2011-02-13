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

	;abort.c
	extern abort_load_new

	; elflink/load_env32.c
	extern load_env32

	; memscan.c
	extern highmem_init

	extern linux_kernel

	extern mp1, mp2, mp3, mp4, mp5

	extern hexdump, mydump

	extern printf_init

	extern mem_init

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

%if IS_PXELINUX
	; pxe.c
	extern unload_pxe, reset_pxe
%endif

%endif ; EXTERN_INC
