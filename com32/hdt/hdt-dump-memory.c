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

#include <memory.h>
#include "hdt-common.h"
#include "hdt-dump.h"

void dump_88(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	(void) hardware;
	int mem_size = 0;
	if (detect_memory_88(&mem_size)) {
		APPEND_ARRAY
			add_as("memory.error","8800h memory configuration is invalid")
		END_OF_APPEND;
		return;
	}

	APPEND_ARRAY
	add_as("dmi.item","memory via 88")
	add_ai("memory.size (KiB)", mem_size)
	add_ai("memory.size (MiB)", mem_size >> 10)
	END_OF_APPEND;

}

void dump_e801(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	(void) hardware;
	int mem_low, mem_high = 0;
	if (detect_memory_e801(&mem_low,&mem_high)) {
		APPEND_ARRAY
			add_as("memory.error","e801 memory configuration is invalid")
		END_OF_APPEND;
		return;
	}

	APPEND_ARRAY
	add_as("dmi.item","memory via e801")
	add_ai("memory.total.size (KiB)", mem_low + (mem_high << 6))
	add_ai("memory.total.size (MiB)", (mem_low >> 10) + (mem_high >> 4))
	add_ai("memory.low.size (KiB)", mem_low )
	add_ai("memory.low.size (MiB)", mem_low >> 10)
	add_ai("memory.high.size (KiB)", mem_high << 6)
	add_ai("memory.high.size (MiB)", mem_high >> 4)
	END_OF_APPEND;

}
void dump_e820(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
    
	(void) hardware;
	struct e820entry map[E820MAX];
	struct e820entry nm[E820MAX];
	unsigned long memsize = 0;
	int count = 0;
	char type[14] = {0};
	
	detect_memory_e820(map, E820MAX, &count);
 	memsize = memsize_e820(map, count);

	APPEND_ARRAY
		add_as("dmi.item","memory via e820")
		add_ai("memory.total.size (KiB)", memsize)
		add_ai("memory.total.size (MiB)", (memsize + (1 << 9)) >> 10)
	END_OF_APPEND;

	for (int i = 0; i < count; i++) {
		get_type(map[i].type, type, sizeof(type));
		char begin[24]={0};
		char size[24]={0};
		char end[24]={0};
		snprintf(begin,sizeof(begin),"0x%016llx",map[i].addr);
		snprintf(size,sizeof(size),"0x%016llx",map[i].size);
		snprintf(end,sizeof(end),"0x%016llx",map[i].addr+map[i].size);
		CREATE_TEMP_OBJECT
			add_ts("memory.segment.start",begin);
			add_ts("memory.segment.size ",size);
			add_ts("memory.segment.end  ",end);
			add_ts("memory.segment.type ",remove_spaces(type));
		APPEND_TEMP_OBJECT_ARRAY;
	}

	int nr = sanitize_e820_map(map, nm, count);
	for (int i = 0; i < nr; i++) {
		get_type(nm[i].type, type, sizeof(type));
		char begin[24]={0};
		char size[24]={0};
		char end[24]={0};
		snprintf(begin,sizeof(begin),"0x%016llx",nm[i].addr);
		snprintf(size,sizeof(size),"0x%016llx",nm[i].size);
		snprintf(end,sizeof(end),"0x%016llx",nm[i].addr+nm[i].size);
		CREATE_TEMP_OBJECT
			add_ts("sanitized_memory.segment.start",begin);
			add_ts("sanitized_memory.segment.size ",size);
			add_ts("sanitized_memory.segment.end  ",end);
			add_ts("sanitized_memory.segment.type ",remove_spaces(type));
		APPEND_TEMP_OBJECT_ARRAY;
	}
}

void dump_memory(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	CREATE_ARRAY
		add_as("Memory configuration","true")
	END_OF_ARRAY;	

	dump_88(hardware,config,item);
	dump_e801(hardware,config,item);
	dump_e820(hardware,config,item);
	flush("memory",config,item);
}
