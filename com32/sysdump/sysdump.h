#ifndef SYSDUMP_H
#define SYSDUMP_H

struct backend;

void dump_memory(struct backend *);
void dump_dmi(struct backend *);
void dump_vesa_tables(struct backend *);

#endif /* SYSDUMP_H */
