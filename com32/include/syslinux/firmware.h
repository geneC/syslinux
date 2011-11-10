#ifndef _SYSLINUX_FIRMWARE_H
#define _SYSLINUX_FIRMWARE_H

struct firmware {
	void (*init)(void);
	int (*scan_memory)(scan_memory_callback_t, void *);
	void (*adjust_screen)(void);
	void (*cleanup)(void);
};

extern struct firmware *firmware;

extern void syslinux_register_bios(void);

#endif /* _SYSLINUX_FIRMWARE_H */
