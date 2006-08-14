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

#ifndef DMI_CHASSIS_H
#define DMI_CHASSIS_H

#define CHASSIS_MANUFACTURER_SIZE      	32
#define CHASSIS_TYPE_SIZE		16	
#define CHASSIS_LOCK_SIZE		16	
#define CHASSIS_VERSION_SIZE   		16
#define CHASSIS_SERIAL_SIZE  		32
#define CHASSIS_ASSET_TAG_SIZE  	32
#define CHASSIS_BOOT_UP_STATE_SIZE  	32
#define CHASSIS_POWER_SUPPLY_STATE_SIZE  	32
#define CHASSIS_THERMAL_STATE_SIZE  	32
#define CHASSIS_SECURITY_STATUS_SIZE  	32
#define CHASSIS_OEM_INFORMATION_SIZE  	32

typedef struct {
char manufacturer[CHASSIS_MANUFACTURER_SIZE];	
char type[CHASSIS_TYPE_SIZE];	
char lock[CHASSIS_LOCK_SIZE];	
char version[CHASSIS_VERSION_SIZE];	
char serial[CHASSIS_SERIAL_SIZE];	
char asset_tag[CHASSIS_ASSET_TAG_SIZE];	
char boot_up_state[CHASSIS_BOOT_UP_STATE_SIZE];	
char power_supply_state[CHASSIS_POWER_SUPPLY_STATE_SIZE];	
char thermal_state[CHASSIS_THERMAL_STATE_SIZE];	
char security_status[CHASSIS_SECURITY_STATUS_SIZE];	
char oem_information[CHASSIS_OEM_INFORMATION_SIZE];	
u16  height;	
u16  nb_power_cords;	
} s_chassis;

static const char *dmi_chassis_type(u8 code)
{
        /* 3.3.4.1 */
        static const char *type[]={
                "Other", /* 0x01 */
                "Unknown",
                "Desktop",
                "Low Profile Desktop",
                "Pizza Box",
                "Mini Tower",
                "Tower",
                "Portable",
                "Laptop",
                "Notebook",
                "Hand Held",
                "Docking Station",
                "All In One",
                "Sub Notebook",
                "Space-saving",
                "Lunch Box",
                "Main Server Chassis", /* master.mif says System */
                "Expansion Chassis",
                "Sub Chassis",
                "Bus Expansion Chassis",
                "Peripheral Chassis",
                "RAID Chassis",
                "Rack Mount Chassis",
                "Sealed-case PC",
                "Multi-system" /* 0x19 */
        };

        if(code>=0x01 && code<=0x19)
                return type[code-0x01];
        return out_of_spec;
}

static const char *dmi_chassis_lock(u8 code)
{
        static const char *lock[]={
                "Not Present", /* 0x00 */
                "Present" /* 0x01 */
        };

        return lock[code];
}

static const char *dmi_chassis_state(u8 code)
{
        /* 3.3.4.2 */
        static const char *state[]={
                "Other", /* 0x01 */
                "Unknown",
                "Safe", /* master.mif says OK */
                "Warning",
                "Critical",
                "Non-recoverable" /* 0x06 */
        };

        if(code>=0x01 && code<=0x06)
                return(state[code-0x01]);
        return out_of_spec;
}

static const char *dmi_chassis_security_status(u8 code)
{
        /* 3.3.4.3 */
        static const char *status[]={
                "Other", /* 0x01 */
                "Unknown",
                "None",
                "External Interface Locked Out",
                "External Interface Enabled" /* 0x05 */
        };

        if(code>=0x01 && code<=0x05)
                return(status[code-0x01]);
        return out_of_spec;
}

#endif
