#ifndef SYSDUMP_H
#define SYSDUMP_H

struct backend;

void dump_memory_map(struct backend *);
void snapshot_lowmem(void);
void dump_memory(struct backend *);
void dump_dmi(struct backend *);
void dump_acpi(struct backend *);
void dump_cpuid(struct backend *);
void dump_pci(struct backend *);
void dump_vesa_tables(struct backend *);

#endif /* SYSDUMP_H */
