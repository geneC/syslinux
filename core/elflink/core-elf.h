#ifndef _CORE_ELF_H
#define _coRE_ELF_H

extern char *append;
extern char *ippappend;
extern char *globaldefault;
extern short onerrorlen;

extern void parse_configs(char **argv);
extern int new_linux_kernel(char *okernel, char *ocmdline);

/* load_env32.c, should be moved out */
extern void enter_cmdline(void);

extern void start_ui(char *config_file);
#endif
