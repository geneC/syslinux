#ifndef _H_SYSLXOPT_
#define _H_SYSLXOPT_

/* These are the options we can set and their values */
struct sys_options {
    unsigned int sectors;
    unsigned int heads;
    int raid_mode;
    int stupid_mode;
    int reset_adv;
    const char *set_once;
    int update_only;
    const char *directory;
    const char *device;
    unsigned int offset;
    const char *menu_save;
};

enum long_only_opt {
    OPT_NONE,
    OPT_RESET_ADV,
};

void __attribute__ ((noreturn)) usage(int rv, int mode);
void parse_options(int argc, char *argv[], int mode);

extern struct sys_options opt;
extern const struct option long_options[];
extern const char short_options[];

#endif
