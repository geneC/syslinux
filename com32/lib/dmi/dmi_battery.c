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
const char *dmi_battery_chemistry(uint8_t code)
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

void dmi_battery_capacity(uint16_t code, uint8_t multiplier,char *capacity)
{
        if (code == 0)
                sprintf(capacity,"%s","Unknown");
        else
                sprintf(capacity,"%u mWh", code * multiplier);
}

void dmi_battery_voltage(uint16_t code, char *voltage)
{
        if (code == 0)
                sprintf(voltage,"%s","Unknown");
        else
                sprintf(voltage,"%u mV", code);
}

void dmi_battery_maximum_error(uint8_t code, char *error)
{
        if (code == 0xFF)
                sprintf(error,"%s","Unknown");
        else
                sprintf(error,"%u%%", code);
}

