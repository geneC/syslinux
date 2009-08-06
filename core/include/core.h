#ifndef CORE_H
#define CORE_H

#include <klibc/compiler.h>
#include <com32.h>

extern char core_xfer_buf[65536];
extern char core_cache_buf[65536];

extern char CurrentDirName[];
extern char ConfigName[];


/* diskstart.inc isolinux.asm*/
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

/*
 * externs for pxelinux
 */
extern void kaboom(void);
extern void dns_mangle(void);

extern uint32_t ServerIP;
extern uint32_t MyIP;
extern uint32_t Netmask;
extern uint32_t Gateway;
extern uint32_t ServerPort;

extern char MACStr[];        /* MAC address as a string */
extern char MAC[];           /* Actual MAC address */
extern char BOOTIFStr[];     /* Space for "BOOTIF=" */
extern uint8_t MACLen;       /* MAC address len */
extern uint8_t MACType;      /* MAC address type */

extern uint8_t  DHCPMagic;
extern uint8_t  OverLoad;
extern uint32_t RebootTime;

/* TFTP ACK packet */
extern uint16_t ack_packet_buf[];

extern char trackbuf[];
extern char BootFile[];
extern char PathPrefix[];
extern char LocalDomain[];

extern char packet_buf[];

extern char IPOption[];
extern char DotQuadBuf[];

extern uint32_t DNSServers[];
extern uint16_t LastDNSServer;

extern uint16_t RealBaseMem;
extern uint16_t APIVer;
extern far_ptr_t PXEEntry;

extern far_ptr_t InitStack;

extern int HaveUUID;
extern uint8_t UUIDType;
extern char UUID[];

extern volatile uint16_t BIOS_timer;


#endif /* CORE_H */
