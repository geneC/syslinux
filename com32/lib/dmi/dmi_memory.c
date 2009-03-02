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

void dmi_memory_array_error_handle(uint16_t code,char *array)
{
 if (code == 0xFFFE)
      sprintf(array,"%s","Not Provided");
 else if (code == 0xFFFF)
      sprintf(array,"%s","No Error");
 else
      sprintf(array,"0x%04X", code);
}

void dmi_memory_device_width(uint16_t code, char *width)
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

void dmi_memory_device_size(uint16_t code, char *size)
{
 if (code == 0)
     sprintf(size,"%s","Free");
 else if (code == 0xFFFF)
     sprintf(size,"%s","Unknown");
 else {
     if (code & 0x8000)
            sprintf(size, "%u kB", code & 0x7FFF);
     else
            sprintf(size,"%u MB", code);
 }
}

const char *dmi_memory_device_form_factor(uint8_t code)
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

void dmi_memory_device_set(uint8_t code, char *set)
{
 if (code == 0)
     sprintf(set,"%s","None");
 else if (code == 0xFF)
           sprintf(set,"%s","Unknown");
      else
           sprintf(set,"%u", code);
}

const char *dmi_memory_device_type(uint8_t code)
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

void dmi_memory_device_type_detail(uint16_t code,char *type_detail)
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

void dmi_memory_device_speed(uint16_t code, char *speed)
{
 if (code == 0)
      sprintf(speed,"%s","Unknown");
 else
      sprintf(speed,"%u MHz", code);
}

