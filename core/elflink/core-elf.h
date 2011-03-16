#ifndef _CORE_ELF_H
#define _coRE_ELF_H

enum kernel_type {
    /* Meta-types for internal use */
    KT_NONE,
    KT_LOCALBOOT,

    /* The ones we can pass off to SYSLINUX, in order */
    KT_KERNEL,			/* Undefined type */
    KT_LINUX,			/* Linux kernel */
    KT_BOOT,			/* Bootstrap program */
    KT_BSS,			/* Boot sector with patch */
    KT_PXE,			/* PXE NBP */
    KT_FDIMAGE,			/* Floppy disk image */
    KT_COMBOOT,			/* COMBOOT image */
    KT_COM32,			/* COM32 image */
    KT_CONFIG,			/* Configuration file */
};

extern const char *append;
extern char *ippappend;
extern const char *globaldefault;
extern short onerrorlen;

extern int new_linux_kernel(char *okernel, char *ocmdline);

extern void start_ui(char *config_file);
#endif
