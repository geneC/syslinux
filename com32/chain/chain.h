#ifndef _COM32_CHAIN_CHAIN_H
#define _COM32_CHAIN_CHAIN_H

#include <syslinux/movebits.h>

struct data_area {
    void *data;
    addr_t base;
    addr_t size;
};

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
