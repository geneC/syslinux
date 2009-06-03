#ifndef CORE_H
#define CORE_H

#include <klibc/compiler.h>
#include <com32.h>

extern char core_xfer_buf[65536];
extern char core_cache_buf[65536];

extern char CurrentDirName[];
extern char ConfigName[];


/* diskstart.inc */
extern void getlinsec(void);

/* getc.inc */
extern void core_open(void);

/* hello.c */
extern void myputs(const char*);


void __cdecl core_intcall(uint8_t, const com32sys_t *, com32sys_t *);
void __cdecl core_farcall(uint32_t, const com32sys_t *, com32sys_t *);
int __cdecl core_cfarcall(uint32_t, const void *, uint32_t);

void call16(void (*)(void), const com32sys_t *, com32sys_t *);

#define __lowmem __attribute((nocommon,section(".lowmem")))

#endif /* CORE_H */
