/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Erwan Velu - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
*/

#include <dmi/dmi.h>
#include <stdio.h>

const char *dmi_chassis_type(uint8_t code)
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

const char *dmi_chassis_lock(uint8_t code)
{
        static const char *lock[]={
                "Not Present", /* 0x00 */
                "Present" /* 0x01 */
        };

        return lock[code];
}

const char *dmi_chassis_state(uint8_t code)
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

const char *dmi_chassis_security_status(uint8_t code)
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
