#ifndef _COM32_CHAIN_CHAIN_H
#define _COM32_CHAIN_CHAIN_H

#include <stdint.h>
#include <syslinux/bootrm.h>

struct options {
    unsigned int fseg;
    unsigned int foff;
    unsigned int fip;
    unsigned int sseg;
    unsigned int soff;
    unsigned int sip;
    unsigned int drvoff;
    const char *drivename;
    const char *partition;
    const char *file;
    const char *grubcfg;
    bool isolinux;
    bool cmldr;
    bool drmk;
    bool grub;
    bool grldr;
    bool maps;
    bool hand;
    bool hptr;
    bool swap;
    int hide;
    bool sethid;
    bool setgeo;
    bool setdrv;
    bool sect;
    bool save;
    bool filebpb;
    bool mbrchs;
    bool warn;
    uint16_t keeppxe;
    struct syslinux_rm_regs regs;
};

struct data_area {
    void *data;
    addr_t base;
    addr_t size;
};

extern struct options opt;

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
