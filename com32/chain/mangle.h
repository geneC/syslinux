#ifndef _COM32_CHAIN_MANGLE_H
#define _COM32_CHAIN_MANGLE_H

#include "chain.h"
#include "partiter.h"

int manglef_isolinux(struct data_area *data);
int manglef_grldr(const struct part_iter *iter);
int manglef_grub(const struct part_iter *iter, struct data_area *data);
int manglef_bpb(const struct part_iter *iter, struct data_area *data);
int try_mangles_bpb(const struct part_iter *iter, struct data_area *data);
int mangles_cmldr(struct data_area *data);
#if 0
int manglef_drmk(struct data_area *data);
#endif

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
