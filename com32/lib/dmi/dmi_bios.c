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

const char *bios_charac_strings[]={
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

const char *bios_charac_x1_strings[]={
     "ACPI is supported", /* 0 */
     "USB legacy is supported",
     "AGP is supported",
     "I2O boot is supported",
     "LS-120 boot is supported",
     "ATAPI Zip drive boot is supported",
     "IEEE 1394 boot is supported",
     "Smart battery is supported" /* 7 */
};

const char *bios_charac_x2_strings[]={
    "BIOS boot specification is supported", /* 0 */
    "Function key-initiated network boot is supported",
    "Targeted content distribution is supported" /* 2 */
};

