/* ----------------------------------------------------------------------- *
 *
 *   Copyright 20011 Erwan Velu - All Rights Reserved
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
 * -----------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "hdt-common.h"

static void compute_filename(struct s_hardware *hardware, char *filename, int size) {

   snprintf(filename,size,"%s/","hdt");

    if (hardware->is_pxe_valid) {
	    strncat(filename, hardware->pxe.mac_addr, sizeof(hardware->pxe.mac_addr));
	    strncat(filename, "+", 1);
    } 
    
    if (hardware->is_dmi_valid) {
	    strncat(filename, remove_spaces(hardware->dmi.system.product_name), sizeof(hardware->dmi.system.manufacturer));
	    strncat(filename, "+", 1);
	    strncat(filename, remove_spaces(hardware->dmi.system.manufacturer), sizeof(hardware->dmi.system.product_name));
    }

    /* We replace the ":" in the filename by some "-"
     * This will avoid Microsoft FS turning crazy */
    chrreplace(filename,':','-');

    /* Avoid space to make filename easier to manipulate */
    chrreplace(filename,' ','_');

}

/**
 * dump - dump info
 **/
void dump(struct s_hardware *hardware)
{
    detect_hardware(hardware);

    /* By now, we only support TFTP reporting */
    upload=&upload_tftp;
    upload->name="tftp";

    /* The following defines the behavior of the reporting */
    char *arg[64];
    char filename[512]={0};
    compute_filename(hardware, filename, sizeof(filename));

    /* The filename */
    arg[0] = filename;
    /* More to come */
    arg[1] = NULL;

    /* We initiate the cpio to send */
    cpio_init(upload,(const char **)arg);

    cpio_writefile(upload,"test","test1",4);

    /* We close & flush the file to send */
    cpio_close(upload);
    flush_data(upload);
}
