#ifndef _COM32_CHAIN_OPTIONS_H
#define _COM32_CHAIN_OPTIONS_H

struct options {
    unsigned int fseg;
    unsigned int foff;
    unsigned int fip;
    unsigned int sseg;
    unsigned int soff;
    unsigned int sip;
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
    bool sect;
    bool save;
    bool bss;
    bool setbpb;
    bool filebpb;
    bool mbrchs;
    bool warn;
    bool chain;
    uint16_t keeppxe;
    struct syslinux_rm_regs regs;
};

int soi_s2n(char *ptr, unsigned int *seg, unsigned int *off,
	unsigned int *ip, unsigned int def);
void usage(void);
int parse_args(int argc, char *argv[]);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
