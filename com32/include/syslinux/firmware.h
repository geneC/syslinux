#ifndef _SYSLINUX_FIRMWARE_H
#define _SYSLINUX_FIRMWARE_H

#include <syslinux/memscan.h>

struct output_ops {
	void (*erase) (int, int, int, int, uint8_t);
	void (*write_char) (uint8_t, uint8_t);
	void (*showcursor) (uint16_t);
	void (*scroll_up) (uint8_t, uint8_t, uint8_t);
	void (*set_cursor) (int, int, bool);
	void (*beep) (void);
	void (*get_mode)(int *, int *);
	void (*set_mode)(uint16_t);
	void (*get_cursor)(int *, int *);
};

struct input_ops {
	char (*getchar)(void);
};

struct firmware {
	void (*init)(void);
	int (*scan_memory)(scan_memory_callback_t, void *);
	void (*adjust_screen)(void);
	void (*cleanup)(void);
	struct disk *(*disk_init)(struct disk_private *);
	struct output_ops *o_ops;
	struct input_ops *i_ops;
	char *(*get_config_file_name)(void);
	void (*get_serial_console_info)(uint16_t *, uint16_t *, uint16_t *);
	bool (*ipappend_strings)(char **, int *);
};

extern struct firmware *firmware;

extern void syslinux_register_bios(void);

#endif /* _SYSLINUX_FIRMWARE_H */
