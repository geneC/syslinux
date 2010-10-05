#ifndef _COM32_CHAIN_MANGLE_H
#define _COM32_CHAIN_MANGLE_H

#include "chain.h"
#include "partiter.h"

int manglef_isolinux(struct data_area *data);
int manglef_grub(const struct part_iter *iter, struct data_area *data);
int manglef_bpb(const struct part_iter *iter, struct data_area *data);
/* int manglef_drmk(struct data_area *data);*/

int mangles_bpb(const struct part_iter *iter, struct data_area *data);
int mangles_save(const struct part_iter *iter, const struct data_area *data, void *org);
int mangles_cmldr(struct data_area *data);

int manglesf_bss(struct data_area *sec, struct data_area *fil);

int mangler_common(const struct part_iter *iter);
int mangler_handover(const struct part_iter *iter, const struct data_area *data);
int mangler_grldr(const struct part_iter *iter);

int manglepe_mbrchshide(struct part_iter *miter);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
