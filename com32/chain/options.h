#ifndef _COM32_CHAIN_OPTIONS_H
#define _COM32_CHAIN_OPTIONS_H

int soi_s2n(char *ptr, unsigned int *seg, unsigned int *off, unsigned int *ip);
void usage(void);
int parse_args(int argc, char *argv[]);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
