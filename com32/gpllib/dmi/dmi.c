/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2006 Erwan Velu - All Rights Reserved
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
#include "dmi/dmi.h"

const char *out_of_spec = "<OUT OF SPEC>";
const char *bad_index = "<BAD INDEX>";

void dmi_bios_runtime_size(uint32_t code, s_dmi *dmi)
{
        if(code&0x000003FF) {
                dmi->bios.runtime_size=code;
                strcpy(dmi->bios.runtime_size_unit,"bytes");
        } else {
                dmi->bios.runtime_size=code >>10;
                strcpy(dmi->bios.runtime_size_unit,"KB");

        }
}

void dmi_bios_characteristics(uint64_t code, s_dmi *dmi)
{
        int i;
        /*
         * This isn't very clear what this bit is supposed to mean
         */
        //if(code.l&(1<<3))
        if(code&&(1<<3))
        {
                ((bool *)(& dmi->bios.characteristics))[0]=true;
                return;
        }

        for(i=4; i<=31; i++)
                //if(code.l&(1<<i))
                if(code&(1<<i))
                        ((bool *)(& dmi->bios.characteristics))[i-3]=true;
}

void dmi_bios_characteristics_x1(uint8_t code, s_dmi *dmi)
{
        int i;

        for(i=0; i<=7; i++)
                if(code&(1<<i))
                        ((bool *)(& dmi->bios.characteristics_x1))[i]=true;
}

void dmi_bios_characteristics_x2(uint8_t code, s_dmi *dmi)
{
        int i;

        for(i=0; i<=2; i++)
                if(code&(1<<i))
                        ((bool *)(& dmi->bios.characteristics_x2))[i]=true;
}


void dmi_system_uuid(uint8_t *p, s_dmi *dmi)
{
        int only0xFF=1, only0x00=1;
        int i;

        for(i=0; i<16 && (only0x00 || only0xFF); i++)
        {
                if(p[i]!=0x00) only0x00=0;
                if(p[i]!=0xFF) only0xFF=0;
        }

        if(only0xFF)
        {
                sprintf(dmi->system.uuid,"Not Present");
                return;
        }
        if(only0x00)
        {
                sprintf(dmi->system.uuid,"Not Settable");
                return;
        }

        sprintf(dmi->system.uuid,"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
                p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);
}

void dmi_system_wake_up_type(uint8_t code, s_dmi *dmi)
{
        /* 3.3.2.1 */
        static const char *type[]={
                "Reserved", /* 0x00 */
                "Other",
                "Unknown",
                "APM Timer",
                "Modem Ring",
                "LAN Remote",
                "Power Switch",
                "PCI PME#",
                "AC Power Restored" /* 0x08 */
        };

        if(code<=0x08) {
                strcpy(dmi->system.wakeup_type,type[code]);
        } else {
                strcpy(dmi->system.wakeup_type,out_of_spec);
        }
return;
}

static void dmi_base_board_features(uint8_t code, s_dmi *dmi)
{
        if((code&0x1F)!=0)
        {
                int i;

                for(i=0; i<=4; i++)
                        if(code&(1<<i))
                        ((bool *)(& dmi->base_board.features))[i]=true;
        }
}

static void dmi_processor_voltage(uint8_t code, s_dmi *dmi)
{
        /* 3.3.5.4 */
        static const float voltage[]={
                5.0,
                3.3,
                2.9
        };
        int i;

        if(code&0x80)
                dmi->processor.voltage=((float)(code&0x7f)/10);
        else
        {
                for(i=0; i<=2; i++)
                        if(code&(1<<i))
                                dmi->processor.voltage=voltage[i];
        }
}

static void dmi_processor_id(uint8_t type, uint8_t *p, const char *version, s_dmi *dmi)
{
        /*
         * Extra flags are now returned in the ECX register when one calls
         * the CPUID instruction. Their meaning is explained in table 6, but
         * DMI doesn't support this yet.
         */
        uint32_t eax, edx;
        int sig=0;

        /*
         * This might help learn about new processors supporting the
         * CPUID instruction or another form of identification.
         */
        sprintf(dmi->processor.id,"ID: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

        if(type==0x05) /* 80386 */
        {
                uint16_t dx=WORD(p);
                /*
                 * 80386 have a different signature.
                 */
                dmi->processor.signature.type=(dx >>12);
                dmi->processor.signature.family=((dx>>8)&0xF);
                dmi->processor.signature.stepping=(dx>>4)&0xF;
                dmi->processor.signature.minor_stepping=(dx&0xF);
                return;
        }
        if(type==0x06) /* 80486 */
        {
                uint16_t dx=WORD(p);
                /*
                 * Not all 80486 CPU support the CPUID instruction, we have to find
                 * wether the one we have here does or not. Note that this trick
                 * works only because we know that 80486 must be little-endian.
                 */
                if((dx&0x0F00)==0x0400
                && ((dx&0x00F0)==0x0040 || (dx&0x00F0)>=0x0070)
                && ((dx&0x000F)>=0x0003))
                        sig=1;
                else
                {
                        dmi->processor.signature.type=((dx >>12)&0x3);
                        dmi->processor.signature.family=((dx>>8)&0xF);
                        dmi->processor.signature.model=((dx>>4)&0xF);
                        dmi->processor.signature.stepping=(dx&0xF);
                        return;
                }
        }
        else if((type>=0x0B && type<=0x13) /* Intel, Cyrix */
        || (type>=0xB0 && type<=0xB3) /* Intel */
        || type==0xB5 /* Intel */
        || type==0xB9) /* Intel */
                sig=1;
        else if((type>=0x18 && type<=0x1D) /* AMD */
        || type==0x1F /* AMD */
        || (type>=0xB6 && type<=0xB7) /* AMD */
        || (type>=0x83 && type<=0x85)) /* AMD */
                sig=2;
        else if(type==0x01 || type==0x02)
        {
                /*
                 * Some X86-class CPU have family "Other" or "Unknown". In this case,
                 * we use the version string to determine if they are known to
                 * support the CPUID instruction.
                 */
                if(strncmp(version, "Pentium III MMX", 15)==0)
                        sig=1;
                else if(strncmp(version, "AMD Athlon(TM)", 14)==0
                || strncmp(version, "AMD Opteron(tm)", 15)==0)
                        sig=2;
                else
                        return;
        }
        else /* not X86-class */
                return;

        eax=DWORD(p);
        edx=DWORD(p+4);
        switch(sig)
        {
                case 1: /* Intel */
                        dmi->processor.signature.type=((eax >>12)&0x3);
                        dmi->processor.signature.family=(((eax>>16)&0xFF0)+((eax>>8)&0x00F));
                        dmi->processor.signature.model=(((eax>>12)&0xF0)+((eax>>4)&0x0F));
                        dmi->processor.signature.stepping=(eax&0xF);
                        break;
                case 2: /* AMD */
                        dmi->processor.signature.family=(((eax>>8)&0xF)==0xF?(eax>>20)&0xFF:(eax>>8)&0xF);
                        dmi->processor.signature.model =(((eax>>4)&0xF)==0xF?(eax>>16)&0xF :(eax>>4)&0xF);
                        dmi->processor.signature.stepping=(eax&0xF);
                        break;
        }

        edx=DWORD(p+4);
        if((edx&0x3FF7FDFF)!=0)
        {
                int i;
                for(i=0; i<=31; i++)
                        if(cpu_flags_strings[i]!=NULL && edx&(1<<i))
                                ((bool *)(& dmi->processor.cpu_flags))[i]=true;
                                //printf("%s\t%s\n", prefix, flags[i]);
        }
}


void to_dmi_header(struct dmi_header *h, uint8_t *data)
{
        h->type=data[0];
        h->length=data[1];
        h->handle=WORD(data+2);
        h->data=data;
}

const char *dmi_string(struct dmi_header *dm, uint8_t s)
{
        char *bp=(char *)dm->data;
        size_t i, len;

        if(s==0)
                return "Not Specified";

        bp+=dm->length;
        while(s>1 && *bp)
        {
                bp+=strlen(bp);
                bp++;
                s--;
        }

        if(!*bp)
                return bad_index;

        /* ASCII filtering */
        len=strlen(bp);
        for(i=0; i<len; i++)
                if(bp[i]<32 || bp[i]==127)
                        bp[i]='.';

        return bp;
}

int dmi_checksum(uint8_t *buf)
{
        uint8_t sum=0;
        int a;

        for(a=0; a<15; a++)
                sum+=buf[a];
        return (sum==0);
}

int dmi_iterate(s_dmi *dmi) {
  uint8_t buf[16];
  char *p,*q;

  /* Cleaning structures */
  memset(&dmi->base_board,0,sizeof (s_base_board));
  memset(&dmi->battery,0,sizeof (s_battery));
  memset(&dmi->bios,0,sizeof (s_bios));
  memset(&dmi->chassis,0,sizeof (s_chassis));
  for (int i=0;i<MAX_DMI_MEMORY_ITEMS;i++)
	memset(&dmi->memory[i],0,sizeof (s_memory));
  memset(&dmi->processor,0,sizeof (s_processor));
  memset(&dmi->system,0,sizeof (s_system));

  /* Until we found this elements in the dmitable, we consider them as not filled */
  dmi->base_board.filled=false;
  dmi->battery.filled=false;
  dmi->bios.filled=false;
  dmi->chassis.filled=false;
  for (int i=0;i<MAX_DMI_MEMORY_ITEMS;i++)
	  dmi->memory[i].filled=false;
  dmi->processor.filled=false;
  dmi->system.filled=false;

  p=(char *)0xF0000; /* The start address to look at the dmi table */
  for (q = p; q < p + 0x10000; q += 16) {
        memcpy(buf, q, 15);
        if(memcmp(buf, "_DMI_", 5)==0 && dmi_checksum(buf)) {
             dmi->dmitable.num  = buf[13]<<8|buf[12];
             dmi->dmitable.len  = buf[7]<<8|buf[6];
             dmi->dmitable.base = buf[11]<<24|buf[10]<<16|buf[9]<<8|buf[8];
	     dmi->dmitable.ver  = (buf[0x06]<<8)+buf[0x07];

             /*
              * DMI version 0.0 means that the real version is taken from
              * the SMBIOS version, which we don't know at this point.
             */
             if(buf[14]!=0) {
		   dmi->dmitable.major_version=buf[14]>>4;
		   dmi->dmitable.minor_version=buf[14]&0x0F;
	     }
             else {
		   dmi->dmitable.major_version=0;
		   dmi->dmitable.minor_version=0;

	     }
/*              printf("DMI present (version %d.%d)\n", dmitable.major_version,dmitable.minor_version);
              printf("%d structures occupying %d bytes.\n",dmitable.num, dmitable.len);
              printf("DMI table at 0x%08X.\n",dmitable.base);*/
	      return DMI_TABLE_PRESENT;
         }
  }
  dmi->dmitable.base=0;
  dmi->dmitable.num=0;
  dmi->dmitable.ver=0;
  dmi->dmitable.len=0;
  return -ENODMITABLE;
}

void dmi_decode(struct dmi_header *h, uint16_t ver, s_dmi *dmi)
{
        uint8_t *data=h->data;

        /*
         * Note: DMI types 37, 38 and 39 are untested
         */
        switch(h->type)
        {
                case 0: /* 3.3.1 BIOS Information */
//                        printf("BIOS Information\n");
                        if(h->length<0x12) break;
			dmi->bios.filled=true;
			strcpy(dmi->bios.vendor,dmi_string(h,data[0x04]));
			strcpy(dmi->bios.version,dmi_string(h,data[0x05]));
			strcpy(dmi->bios.release_date,dmi_string(h,data[0x08]));
			dmi->bios.address=WORD(data+0x06);
                        dmi_bios_runtime_size((0x10000-WORD(data+0x06))<<4,dmi);
			dmi->bios.rom_size=(data[0x09]+1)<<6;
			strcpy(dmi->bios.rom_size_unit,"kB");
                        dmi_bios_characteristics(QWORD(data+0x0A),dmi);

			if(h->length<0x13) break;
                        dmi_bios_characteristics_x1(data[0x12], dmi);
                        if(h->length<0x14) break;
                        dmi_bios_characteristics_x2(data[0x13], dmi);
                        if(h->length<0x18) break;
                        if(data[0x14]!=0xFF && data[0x15]!=0xFF)
                                sprintf(dmi->bios.bios_revision,"%u.%u",
                                        data[0x14], data[0x15]);
                        if(data[0x16]!=0xFF && data[0x17]!=0xFF)
                                sprintf(dmi->bios.firmware_revision,"%u.%u",
                                        data[0x16], data[0x17]);
                        break;
                case 1: /* 3.3.2 System Information */
//                        printf("System Information\n");
                        if(h->length<0x08) break;
			dmi->system.filled=true;
			strcpy(dmi->system.manufacturer,dmi_string(h,data[0x04]));
			strcpy(dmi->system.product_name,dmi_string(h,data[0x05]));
			strcpy(dmi->system.version,dmi_string(h,data[0x06]));
			strcpy(dmi->system.serial,dmi_string(h,data[0x07]));
                        if(h->length<0x19) break;
                        dmi_system_uuid(data+0x08,dmi);
                        dmi_system_wake_up_type(data[0x18],dmi);
                        if(h->length<0x1B) break;
			strcpy(dmi->system.sku_number,dmi_string(h,data[0x19]));
			strcpy(dmi->system.family,dmi_string(h,data[0x1A]));
                        break;

                case 2: /* 3.3.3 Base Board Information */
//                        printf("Base Board Information\n");
                        if(h->length<0x08) break;
			dmi->base_board.filled=true;
			strcpy(dmi->base_board.manufacturer,dmi_string(h,data[0x04]));
			strcpy(dmi->base_board.product_name,dmi_string(h,data[0x05]));
			strcpy(dmi->base_board.version,dmi_string(h,data[0x06]));
			strcpy(dmi->base_board.serial,dmi_string(h,data[0x07]));
                        if(h->length<0x0F) break;
			strcpy(dmi->base_board.asset_tag,dmi_string(h,data[0x08]));
                        dmi_base_board_features(data[0x09], dmi);
			strcpy(dmi->base_board.location,dmi_string(h,data[0x0A]));
			strcpy(dmi->base_board.type,dmi_string(h,data[0x0D]));
                        if(h->length<0x0F+data[0x0E]*sizeof(uint16_t)) break;
                        break;
                case 3: /* 3.3.4 Chassis Information */
//                        printf("Chassis Information\n");
                        if(h->length<0x09) break;
			dmi->chassis.filled=true;
			strcpy(dmi->chassis.manufacturer,dmi_string(h,data[0x04]));
                        strcpy(dmi->chassis.type,dmi_chassis_type(data[0x05]&0x7F));
                        strcpy(dmi->chassis.lock,dmi_chassis_lock(data[0x05]>>7));
			strcpy(dmi->chassis.version,dmi_string(h,data[0x06]));
			strcpy(dmi->chassis.serial,dmi_string(h,data[0x07]));
			strcpy(dmi->chassis.asset_tag,dmi_string(h,data[0x08]));
                        if(h->length<0x0D) break;
                        strcpy(dmi->chassis.boot_up_state,dmi_chassis_state(data[0x09]));
                        strcpy(dmi->chassis.power_supply_state,dmi_chassis_state(data[0x0A]));
                        strcpy(dmi->chassis.thermal_state,dmi_chassis_state(data[0x0B]));
                        strcpy(dmi->chassis.security_status,dmi_chassis_security_status(data[0x0C]));
                        if(h->length<0x11) break;
                        sprintf(dmi->chassis.oem_information,"0x%08X\n",DWORD(data+0x0D));
                        if(h->length<0x15) break;
                        dmi->chassis.height=data[0x11];
                        dmi->chassis.nb_power_cords=data[0x12];
                        break;

			case 4: /* 3.3.5 Processor Information */
//                        printf("Processor Information\n");
                        if(h->length<0x1A) break;
			dmi->processor.filled=true;
			strcpy(dmi->processor.socket_designation,dmi_string(h, data[0x04]));
                        strcpy(dmi->processor.type,dmi_processor_type(data[0x05]));
			strcpy(dmi->processor.manufacturer,dmi_string(h, data[0x07]));
                        strcpy(dmi->processor.family,dmi_processor_family(data[0x06],dmi->processor.manufacturer));
                        dmi_processor_id(data[0x06], data+8, dmi_string(h, data[0x10]), dmi);
                        strcpy(dmi->processor.version,dmi_string(h, data[0x10]));
                        dmi_processor_voltage(data[0x11],dmi);
                        dmi->processor.external_clock=WORD(data+0x12);
                        dmi->processor.max_speed=WORD(data+0x14);
                        dmi->processor.current_speed=WORD(data+0x16);
                        if(data[0x18]&(1<<6))
                                strcpy(dmi->processor.status,dmi_processor_status(data[0x18]&0x07));
                        else
                                sprintf(dmi->processor.status,"Unpopulated");
                        sprintf(dmi->processor.upgrade,dmi_processor_upgrade(data[0x19]));
                        if(h->length<0x20) break;
                        dmi_processor_cache(WORD(data+0x1A), "L1", ver,dmi->processor.cache1);
                        dmi_processor_cache(WORD(data+0x1C), "L2", ver,dmi->processor.cache2);
                        dmi_processor_cache(WORD(data+0x1E), "L3", ver,dmi->processor.cache3);
                        if(h->length<0x23) break;
                        strcpy(dmi->processor.serial,dmi_string(h, data[0x20]));
                        strcpy(dmi->processor.asset_tag,dmi_string(h, data[0x21]));
                        strcpy(dmi->processor.part_number,dmi_string(h, data[0x22]));
                        break;
                case 17: /* 3.3.18 Memory Device */
                        if (h->length < 0x15) break;
			dmi->memory_count++;
			s_memory *mem = &dmi->memory[dmi->memory_count-1];
			dmi->memory[dmi->memory_count-1].filled=true;
                        dmi_memory_array_error_handle(WORD(data + 0x06),mem->error);
                        dmi_memory_device_width(WORD(data + 0x08),mem->total_width);
                        dmi_memory_device_width(WORD(data + 0x0A),mem->data_width);
                        dmi_memory_device_size(WORD(data + 0x0C),mem->size);
                        strcpy(mem->form_factor,dmi_memory_device_form_factor(data[0x0E]));
                        dmi_memory_device_set(data[0x0F],mem->device_set);
                        strcpy(mem->device_locator,dmi_string(h, data[0x10]));
                        strcpy(mem->bank_locator,dmi_string(h, data[0x11]));
                        strcpy(mem->type,dmi_memory_device_type(data[0x12]));
                        dmi_memory_device_type_detail(WORD(data + 0x13),mem->type_detail);
                        if (h->length < 0x17) break;
                        dmi_memory_device_speed(WORD(data + 0x15),mem->speed);
		        if (h->length < 0x1B) break;
                        strcpy(mem->manufacturer, dmi_string(h, data[0x17]));
                        strcpy(mem->serial,dmi_string(h, data[0x18]));
                        strcpy(mem->asset_tag,dmi_string(h, data[0x19]));
                        strcpy(mem->part_number,dmi_string(h, data[0x1A]));
                        break;
		case 22: /* 3.3.23 Portable Battery */
                        if (h->length < 0x10) break;
			dmi->battery.filled=true;
                        strcpy(dmi->battery.location,dmi_string(h, data[0x04]));
                        strcpy(dmi->battery.manufacturer,dmi_string(h, data[0x05]));

			if (data[0x06] || h->length < 0x1A)
				strcpy(dmi->battery.manufacture_date, dmi_string(h, data[0x06]));

			if (data[0x07] || h->length < 0x1A)
				strcpy(dmi->battery.serial, dmi_string(h, data[0x07]));

			strcpy(dmi->battery.name,dmi_string(h, data[0x08]));

			if (data[0x09] != 0x02 || h->length < 0x1A)
                        strcpy(dmi->battery.chemistry,dmi_battery_chemistry(data[0x09]));

			if (h->length < 0x1A)
                                dmi_battery_capacity(WORD(data + 0x0A), 1,dmi->battery.design_capacity);
                        else
                                dmi_battery_capacity(WORD(data + 0x0A), data[0x15],dmi->battery.design_capacity);
                        dmi_battery_voltage(WORD(data + 0x0C),dmi->battery.design_voltage);
                        strcpy(dmi->battery.sbds,dmi_string(h, data[0x0E]));
                        dmi_battery_maximum_error(data[0x0F],dmi->battery.maximum_error);
                        if (h->length < 0x1A) break;
                        if (data[0x07] == 0)
			   sprintf(dmi->battery.sbds_serial,"%04X", WORD(data + 0x10));

			if (data[0x06] == 0)
				sprintf(dmi->battery.sbds_manufacture_date,"%u-%02u-%02u",
						1980 + (WORD(data + 0x12) >> 9),
						(WORD(data + 0x12) >> 5) & 0x0F,
						WORD(data + 0x12) & 0x1F);
                        if (data[0x09] == 0x02)
				strcpy(dmi->battery.sbds_chemistry, dmi_string(h, data[0x14]));

		//	sprintf(dmi->battery.oem_info,"0x%08X",DWORD(h, data+0x16));
                        break;
	      case 38: /* 3.3.39 IPMI Device Information */
                        if (h->length < 0x10) break;
			dmi->ipmi.filled=true;
			snprintf(dmi->ipmi.interface_type,sizeof(dmi->ipmi.interface_type),
				"%s", dmi_ipmi_interface_type(data[0x04]));
			dmi->ipmi.major_specification_version=data[0x05] >> 4;
			dmi->ipmi.minor_specification_version=data[0x05] & 0x0F;
			dmi->ipmi.I2C_slave_address=data[0x06] >> 1;
                        if (data[0x07] != 0xFF)
				dmi->ipmi.nv_address=data[0x07];
                        else
				dmi->ipmi.nv_address=0; /* Not Present */
                        dmi_ipmi_base_address(data[0x04], data + 0x08,
                                &dmi->ipmi);
                        if (h->length < 0x12) break;
		        if (data[0x11] != 0x00)
                        {
                                dmi->ipmi.irq=data[0x11];
                        }
                        break;
        }
}

void parse_dmitable(s_dmi *dmi) {
  int i=0;
  uint8_t *data = NULL;
  uint8_t buf[dmi->dmitable.len];

  memcpy(buf,(int *)dmi->dmitable.base,sizeof(uint8_t) * dmi->dmitable.len);
  data=buf;
  dmi->memory_count=0;
  while(i<dmi->dmitable.num && data+4<=buf+dmi->dmitable.len) /* 4 is the length of an SMBIOS structure header */ {
        uint8_t *next;
        struct dmi_header h;
        to_dmi_header(&h, data);

        /*
        * If a short entry is found (less than 4 bytes), not only it
        * is invalid, but we cannot reliably locate the next entry.
        * Better stop at this point, and let the user know his/her
        * table is broken.
        */
        if(h.length<4)
          {
            printf("Invalid entry length (%u). DMI table is broken! Stop.\n\n", (unsigned int)h.length);
            break;
          }

//        printf("Handle 0x%04X, DMI type %d, %d bytes\n", h.handle, h.type, h.length);

        /* loo for the next handle */
        next=data+h.length;
        while(next-buf+1<dmi->dmitable.len && (next[0]!=0 || next[1]!=0))
               next++;
        next+=2;
        if(next-buf<=dmi->dmitable.len)
           {
             dmi_decode(&h, dmi->dmitable.ver,dmi);
           }
        data=next;
        i++;
  }
}
