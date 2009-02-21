/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef DMI_MEMORY_H
#define DMI_MEMORY_H

#define MEMORY_MANUFACTURER_SIZE	32
#define MEMORY_ERROR_SIZE		16
#define MEMORY_TOTAL_WIDTH_SIZE		16
#define MEMORY_DATA_WIDTH_SIZE		16
#define MEMORY_SIZE_SIZE		32
#define MEMORY_FORM_FACTOR_SIZE		32
#define MEMORY_DEVICE_SET_SIZE		32
#define MEMORY_DEVICE_LOCATOR_SIZE	32
#define MEMORY_BANK_LOCATOR_SIZE	32
#define MEMORY_TYPE_SIZE		32
#define MEMORY_TYPE_DETAIL_SIZE		16
#define MEMORY_SPEED_SIZE		16
#define MEMORY_SERIAL_SIZE		16
#define MEMORY_ASSET_TAG_SIZE		16
#define MEMORY_PART_NUMBER_SIZE		16

typedef struct {
char manufacturer[MEMORY_MANUFACTURER_SIZE];
char error[MEMORY_ERROR_SIZE];
char total_width[MEMORY_TOTAL_WIDTH_SIZE];
char data_width[MEMORY_DATA_WIDTH_SIZE];
char size[MEMORY_SIZE_SIZE];
char form_factor[MEMORY_FORM_FACTOR_SIZE];
char device_set[MEMORY_DEVICE_SET_SIZE];
char device_locator[MEMORY_DEVICE_LOCATOR_SIZE];
char bank_locator[MEMORY_BANK_LOCATOR_SIZE];
char type[MEMORY_TYPE_SIZE];
char type_detail[MEMORY_TYPE_DETAIL_SIZE];
char speed[MEMORY_SPEED_SIZE];
char serial[MEMORY_SERIAL_SIZE];
char asset_tag[MEMORY_ASSET_TAG_SIZE];
char part_number[MEMORY_PART_NUMBER_SIZE];
/* The filled field have to be set to true when the dmitable implement that item */
bool filled;
} s_memory;

static void dmi_memory_array_error_handle(u16 code,char *array)
{
 if (code == 0xFFFE)
     sprintf(array,"%s","Not Provided");
 else if (code == 0xFFFF)
     sprintf(array,"%s","No Error");
 else
     sprintf(array,"0x%04X", code);
}


static void dmi_memory_device_width(u16 code, char *width)
{
 /*
 * 3.3.18 Memory Device (Type 17)
 * If no memory module is present, width may be 0
 */
 if (code == 0xFFFF || code == 0)
	 sprintf(width,"%s","Unknown");
 else
	 sprintf(width,"%u bits", code);
}

static void dmi_memory_device_size(u16 code, char *size)
{
 if (code == 0)
     sprintf(size,"%s","No Module Installed");
 else if (code == 0xFFFF)
     sprintf(size,"%s","Unknown");
 else {
     if (code & 0x8000)
            sprintf(size, "%u kB", code & 0x7FFF);
     else
            sprintf(size,"%u MB", code);
 }
}

static const char *dmi_memory_device_form_factor(u8 code)
{
        /* 3.3.18.1 */
        static const char *form_factor[] = {
                "Other", /* 0x01 */
                "Unknown",
                "SIMM",
                "SIP",
                "Chip",
                "DIP",
                "ZIP",
                "Proprietary Card",
                "DIMM",
                "TSOP",
                "Row Of Chips",
                "RIMM",
                "SODIMM",
                "SRIMM",
                "FB-DIMM" /* 0x0F */
        };

        if (code >= 0x01 && code <= 0x0F)
                return form_factor[code - 0x01];
        return out_of_spec;
}

static void dmi_memory_device_set(u8 code, char *set)
{
 if (code == 0)
     sprintf(set,"%s","None");
 else if (code == 0xFF)
           sprintf(set,"%s","Unknown");
      else
           sprintf(set,"%u", code);
}

static const char *dmi_memory_device_type(u8 code)
{
        /* 3.3.18.2 */
        static const char *type[] = {
                "Other", /* 0x01 */
                "Unknown",
                "DRAM",
                "EDRAM",
                "VRAM",
                "SRAM",
                "RAM",
                "ROM",
                "Flash",
                "EEPROM",
                "FEPROM",
                "EPROM",
                "CDRAM",
                "3DRAM",
                "SDRAM",
                "SGRAM",
                "RDRAM",
                "DDR",
                "DDR2",
                "DDR2 FB-DIMM" /* 0x14 */
        };

        if (code >= 0x01 && code <= 0x14)
                return type[code - 0x01];
        return out_of_spec;
}


static void dmi_memory_device_type_detail(u16 code,char *type_detail)
{
        /* 3.3.18.3 */
        static const char *detail[] = {
                "Other", /* 1 */
                "Unknown",
                "Fast-paged",
                "Static Column",
                "Pseudo-static",
                "RAMBus",
                "Synchronous",
                "CMOS",
                "EDO",
                "Window DRAM",
                "Cache DRAM",
                "Non-Volatile" /* 12 */
        };

        if ((code & 0x1FFE) == 0)
                sprintf(type_detail,"%s","None");
        else
        {
                int i;

                for (i = 1; i <= 12; i++)
                        if (code & (1 << i))
                                sprintf(type_detail,"%s", detail[i - 1]);
        }
}

static void dmi_memory_device_speed(u16 code, char *speed)
{
        if (code == 0)
                sprintf(speed,"%s","Unknown");
        else
                sprintf(speed,"%u MHz", code);
}


#endif
