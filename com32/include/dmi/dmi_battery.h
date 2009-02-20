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

#ifndef DMI_BATTERY_H
#define DMI_BATTERY_H

#define BATTERY_LOCATION_SIZE		255
#define BATTERY_MANUFACTURER_SIZE	255
#define BATTERY_MANUFACTURE_DATE_SIZE	255
#define BATTERY_SERIAL_SIZE		255
#define BATTERY_DEVICE_NAME_SIZE	255
#define BATTERY_CHEMISTRY_SIZE		32
#define BATTERY_CAPACITY_SIZE		16
#define BATTERY_DESIGN_VOLTAGE_SIZE	16
#define BATTERY_SBDS_SIZE		255
#define BATTERY_MAXIMUM_ERROR_SIZE	32
#define BATTERY_SBDS_SERIAL_SIZE	32
#define BATTERY_SBDS_MANUFACTURE_DATE_SIZE	255
#define BATTERY_SBDS_CHEMISTRY_SIZE	16
#define BATTERY_OEM_INFO_SIZE		255

typedef struct {
char location[BATTERY_LOCATION_SIZE];
char manufacturer[BATTERY_MANUFACTURER_SIZE];
char manufacture_date[BATTERY_MANUFACTURE_DATE_SIZE];
char serial[BATTERY_SERIAL_SIZE];
char name[BATTERY_DEVICE_NAME_SIZE];
char chemistry[BATTERY_CHEMISTRY_SIZE];
char design_capacity[BATTERY_CAPACITY_SIZE];
char design_voltage[BATTERY_DESIGN_VOLTAGE_SIZE];
char sbds[BATTERY_SBDS_SIZE];
char sbds_serial[BATTERY_SBDS_SERIAL_SIZE];
char maximum_error[BATTERY_MAXIMUM_ERROR_SIZE];
char sbds_manufacture_date[BATTERY_SBDS_MANUFACTURE_DATE_SIZE];
char sbds_chemistry[BATTERY_SBDS_CHEMISTRY_SIZE];
char oem_info[BATTERY_OEM_INFO_SIZE];
/* The filled field have to be set to true when the dmitable implement that item */
bool filled;
} s_battery;

static const char *dmi_battery_chemistry(u8 code)
{
        /* 3.3.23.1 */
        static const char *chemistry[] = {
                "Other", /* 0x01 */
                "Unknown",
                "Lead Acid",
                "Nickel Cadmium",
                "Nickel Metal Hydride",
                "Lithium Ion",
                "Zinc Air",
                "Lithium Polymer" /* 0x08 */
        };

        if (code >= 0x01 && code <= 0x08)
                return chemistry[code - 0x01];
        return out_of_spec;
}

static void dmi_battery_capacity(u16 code, u8 multiplier,char *capacity)
{
        if (code == 0)
                sprintf(capacity,"%s","Unknown");
        else
                sprintf(capacity,"%u mWh", code * multiplier);
}

static void dmi_battery_voltage(u16 code, char *voltage)
{
        if (code == 0)
                sprintf(voltage,"%s","Unknown");
        else
                sprintf(voltage,"%u mV", code);
}

static void dmi_battery_maximum_error(u8 code, char *error)
{
        if (code == 0xFF)
                sprintf(error,"%s","Unknown");
        else
                sprintf(error,"%u%%", code);
}

#endif
