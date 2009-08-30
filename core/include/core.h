#ifndef CORE_H
#define CORE_H

#include <klibc/compiler.h>
#include <com32.h>

extern char core_xfer_buf[65536];
extern char core_cache_buf[65536];
extern char trackbuf[];
extern char CurrentDirName[];
extern char ConfigName[];
extern char KernelName[];

/* diskstart.inc isolinux.asm*/
extern void getlinsec(void);

/* getc.inc */
extern void core_open(void);

/* hello.c */
extern void myputs(const char*);


void __cdecl core_intcall(uint8_t, const com32sys_t *, com32sys_t *);
void __cdecl core_farcall(uint32_t, const com32sys_t *, com32sys_t *);
int __cdecl core_cfarcall(uint32_t, const void *, uint32_t);

extern const com32sys_t zero_regs;
void call16(void (*)(void), const com32sys_t *, com32sys_t *);

/*
 * __lowmem is in the low 1 MB; __bss16 in the low 64K
 */
#define __lowmem __attribute((nocommon,section(".lowmem")))
#define __bss16  __attribute((nocommon,section(".bss16")))

/*
 * Death!  The macro trick is to avoid symbol conflict with
 * the real-mode symbol kaboom.
 */
__noreturn _kaboom(void);
#define kaboom() _kaboom()


#endif /* CORE_H */
