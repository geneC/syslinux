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

#include "hdt-common.h"
#include "hdt-dump.h"
#include "hdt-util.h"

void show_disk(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item, int drive) {
	int i = drive - 0x80;
	struct driveinfo *d = &hardware->disk_info[i];
	char mbr_name[50]={0};
	char disk_size[11]={0};

	get_mbr_string(hardware->mbr_ids[i], &mbr_name,sizeof(mbr_name));
	if ((int)d->edd_params.sectors > 0)
		sectors_to_size((int)d->edd_params.sectors, disk_size);

	char disk[5]={0};
	char edd_version[5]={0};
	snprintf(disk,sizeof(disk),"0x%X",d->disk);
	snprintf(edd_version,sizeof(edd_version),"%X",d->edd_version);
	zzjson_print(config, *item);
	zzjson_free(config, *item);

        *item = zzjson_create_object(config, NULL); /* empty object */
	add_s("disk->number", disk);
	add_i("disk->cylinders",d->legacy_max_cylinder + 1);
	add_i("disk->heads",d->legacy_max_head + 1);
	add_i("disk->sectors_per_track",d->legacy_sectors_per_track);
	add_s("disk->edd_version",edd_version);
	add_s("disk->size",disk_size);
	add_i("disk->bytes_per_sector",(int)d->edd_params.bytes_per_sector);
	add_i("disk->sectors_per_track",(int)d->edd_params.sectors_per_track);
	add_s("disk->host_bus",remove_spaces((char *)d->edd_params.host_bus_type));
	add_s("disk->interface_type",remove_spaces((char *)d->edd_params.interface_type));
	add_s("disk->mbr_name",mbr_name);
	add_i("disk->mbr_id",hardware->mbr_ids[i]);

}

void dump_disks(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	bool found=false;
	for (int drive = 0x80; drive < 0xff; drive++) {
	        if (hardware->disk_info[drive - 0x80].cbios) {
			if (found==false) {
        			*item = zzjson_create_object(config, NULL); /* empty object */
				add_b("disks->is_valid",true);
       				found=true;
			}
			show_disk(hardware, config, item, drive);
		}
	}

	if (found==false) {
        	*item = zzjson_create_object(config, NULL); /* empty object */
		add_b("disks->is_valid",false);
	}
	flush("disks",config,item);
}
