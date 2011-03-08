#ifndef _COM32_CHAIN_MANGLE_H
#define _COM32_CHAIN_MANGLE_H

#include "chain.h"
#include "partiter.h"

/* file's manglers */
int manglef_isolinux(struct data_area *data);
int manglef_grub(const struct part_iter *iter, struct data_area *data);
int manglef_bpb(const struct part_iter *iter, struct data_area *data);
/* int manglef_drmk(struct data_area *data);*/

/* sector's manglers */
int mangles_bpb(const struct part_iter *iter, struct data_area *data);
int mangles_save(const struct part_iter *iter, const struct data_area *data, void *org);
int mangles_cmldr(struct data_area *data);

/* sector + file's manglers */
int manglesf_bss(struct data_area *sec, struct data_area *fil);

/* registers' manglers */
int mangler_init(const struct part_iter *iter);
int mangler_handover(const struct part_iter *iter, const struct data_area *data);
int mangler_grldr(const struct part_iter *iter);

/* partition layout's manglers */
int manglepe_fixchs(struct part_iter *miter);
int manglepe_hide(struct part_iter *miter);

#endif

/* vim: set ts=8 sts=4 sw=4 noet: */
