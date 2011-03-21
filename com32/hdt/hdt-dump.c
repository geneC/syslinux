/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2011 Erwan Velu - All Rights Reserved
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
#include "hdt-dump.h"

struct print_buf p_buf;

void compute_filename(struct s_hardware *hardware, char *filename, int size) {

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

void print_and_flush(ZZJSON_CONFIG *config, ZZJSON **item) {
	zzjson_print(config, *item);
        zzjson_free(config, *item);
}

int dumpprintf(FILE *p, const char *format, ...) {
   va_list ap;
   int rv;

  (void) p;  
   va_start(ap, format);
   rv = vbufprintf(&p_buf,format, ap);
   va_end(ap);
   return rv;
}

/**
 * dump - dump info
 **/
void dump(struct s_hardware *hardware)
{
    ZZJSON *json = NULL;
    ZZJSON_CONFIG config = { ZZJSON_VERY_STRICT, NULL,
		(int(*)(void*)) fgetc,
		NULL,
		malloc, calloc, free, realloc,
		stderr, NULL, stdout,
		(int(*)(void *,const char*,...)) dumpprintf,
		(int(*)(int,void*)) fputc 
    };

    detect_hardware(hardware);
    dump_cpu(hardware, &config, &json);

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

    cpio_writefile(upload,"cpu",p_buf.buf,p_buf.len);

    /* We close & flush the file to send */
    cpio_close(upload);
    flush_data(upload);
    if (p_buf.buf) { 
	    free(p_buf.buf); 
    }
}
