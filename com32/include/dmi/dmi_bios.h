/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef DMI_BIOS_H
#define DMI_BIOS_H

#include "stdbool.h"
#define BIOS_VENDOR_SIZE		32
#define BIOS_VERSION_SIZE		32
#define BIOS_RELEASE_SIZE		16
#define BIOS_RUNTIME_SIZE_UNIT_SIZE	16
#define BIOS_ROM_UNIT_SIZE		16
#define BIOS_BIOS_REVISION_SIZE		16
#define BIOS_FIRMWARE_REVISION_SIZE	16

#define BIOS_CHAR_NB_ELEMENTS		28
#define BIOS_CHAR_X1_NB_ELEMENTS	8
#define BIOS_CHAR_X2_NB_ELEMENTS	3

static const char *bios_charac_strings[]={
   "BIOS characteristics not supported", /* 3 */
   "ISA is supported",
   "MCA is supported",
   "EISA is supported",
   "PCI is supported",
   "PC Card (PCMCIA) is supported",
   "PNP is supported",
   "APM is supported",
   "BIOS is upgradeable",
   "BIOS shadowing is allowed",
   "VLB is supported",
   "ESCD support is available",
   "Boot from CD is supported",
   "Selectable boot is supported",
   "BIOS ROM is socketed",
   "Boot from PC Card (PCMCIA) is supported",
   "EDD is supported",
   "Japanese floppy for NEC 9800 1.2 MB is supported (int 13h)",
   "Japanese floppy for Toshiba 1.2 MB is supported (int 13h)",
   "5.25\"/360 KB floppy services are supported (int 13h)",
   "5.25\"/1.2 MB floppy services are supported (int 13h)",
   "3.5\"/720 KB floppy services are supported (int 13h)",
   "3.5\"/2.88 MB floppy services are supported (int 13h)",
   "Print screen service is supported (int 5h)",
   "8042 keyboard services are supported (int 9h)",
   "Serial services are supported (int 14h)",
   "Printer services are supported (int 17h)",
   "CGA/mono video services are supported (int 10h)",
   "NEC PC-98" /* 31 */
};

/* this struct has BIOS_CHAR_NB_ELEMENTS */
/* each bool is associated with the relevant message above */
typedef struct {
bool bios_characteristics_not_supported;
bool isa;
bool mca;
bool eisa;
bool pci;
bool pc_card;
bool pnp;
bool apm;
bool bios_upgreadable;
bool bios_shadowing;
bool vlb;
bool escd;
bool boot_from_cd;
bool selectable_boot;
bool bios_rom_socketed;
bool edd;
bool japanese_floppy_nec_9800_1_2MB;
bool japanese_floppy_toshiba_1_2MB;
bool floppy_5_25_360KB;
bool floppy_5_25_1_2MB;
bool floppy_3_5_720KB;
bool floppy_3_5_2_88MB;
bool print_screen;
bool keyboard_8042_support;
bool serial_support;
bool printer_support;
bool cga_mono_support;
bool nec_pc_98;
}  __attribute__((__packed__)) s_characteristics;

static const char *bios_charac_x1_strings[]={
     "ACPI is supported", /* 0 */
     "USB legacy is supported",
     "AGP is supported",
     "I2O boot is supported",
     "LS-120 boot is supported",
     "ATAPI Zip drive boot is supported",
     "IEEE 1394 boot is supported",
     "Smart battery is supported" /* 7 */
};

/* this struct has BIOS_CHAR_X1_NB_ELEMENTS */
/* each bool is associated with the relevant message above */
typedef struct {
bool acpi;
bool usb_legacy;
bool agp;
bool i2o_boot;
bool ls_120_boot;
bool zip_drive_boot;
bool ieee_1394_boot;
bool smart_battery;
} __attribute__((__packed__)) s_characteristics_x1;

static const char *bios_charac_x2_strings[]={
    "BIOS boot specification is supported", /* 0 */
    "Function key-initiated network boot is supported",
    "Targeted content distribution is supported" /* 2 */
};

/* this struct has BIOS_CHAR_X2_NB_ELEMENTS */
/* each bool is associated with the relevant message above */
typedef struct {
bool bios_boot_specification;
bool bios_network_boot_by_keypress;
bool target_content_distribution;
} __attribute__((__packed__)) s_characteristics_x2;

typedef struct {
char vendor[BIOS_VENDOR_SIZE];
char version[BIOS_VERSION_SIZE];
char release_date[BIOS_RELEASE_SIZE];
u16  address;
u16  runtime_size;
char runtime_size_unit[BIOS_RUNTIME_SIZE_UNIT_SIZE];
u16  rom_size;
char rom_size_unit[BIOS_ROM_UNIT_SIZE];
s_characteristics characteristics;
s_characteristics_x1 characteristics_x1;
s_characteristics_x2 characteristics_x2;
char bios_revision [BIOS_BIOS_REVISION_SIZE];
char firmware_revision [BIOS_FIRMWARE_REVISION_SIZE];
/* The filled field have to be set to true when the dmitable implement that item */
bool filled;
} s_bios;

#endif
