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

void dump_memory_size(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	APPEND_ARRAY
		add_ai("dmi.memory_size (KB)",hardware->detected_memory_size)
		add_ai("dmi.memory_size (MB)",(hardware->detected_memory_size + (1 << 9)) >> 10)
	END_OF_APPEND;
}

void dump_memory_modules(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	if (hardware->dmi.memory_module_count == 0) {
			APPEND_ARRAY
				add_as("dmi.warning","No memory module structure found")
			END_OF_APPEND;
			return;
	}

	for (int module=0; module<hardware->dmi.memory_module_count;module++) {
		if (hardware->dmi.memory_module[module].filled == false) {
			char msg[64]={0};
			snprintf(msg,sizeof(msg),"Module %d doesn't contain any information", module);

			APPEND_ARRAY
				add_as("dmi.warning",msg)
			END_OF_APPEND;
			continue;
		}

		APPEND_ARRAY
		add_ai("Memory module", module)
		add_as("dmi.memory_module.socket_designation", hardware->dmi.memory_module[module].socket_designation)
		add_as("dmi.memory_module.bank_connections", hardware->dmi.memory_module[module].bank_connections)
		add_as("dmi.memory_module.speed", hardware->dmi.memory_module[module].speed)
		add_as("dmi.memory_module.type", hardware->dmi.memory_module[module].type)
		add_as("dmi.memory_module.installed_size", hardware->dmi.memory_module[module].installed_size)
		add_as("dmi.memory_module.enabled_size", hardware->dmi.memory_module[module].enabled_size)
		add_as("dmi.memory_module.error_status", hardware->dmi.memory_module[module].error_status)
		END_OF_APPEND;
	}
}
	
void dump_cache(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	if (hardware->dmi.cache_count == 0) {
			APPEND_ARRAY
				add_as("dmi.warning","No cache structure found")
			END_OF_APPEND;
			return;
	}

	for (int cache=0; cache<hardware->dmi.cache_count;cache++) {
		APPEND_ARRAY
		add_ai("Cache", cache)
		add_as("dmi.cache.socket_designation", hardware->dmi.cache[cache].socket_designation)
		add_as("dmi.cache.configuration", hardware->dmi.cache[cache].configuration)
		add_as("dmi.cache.mode", hardware->dmi.cache[cache].mode)
		add_as("dmi.cache.location", hardware->dmi.cache[cache].location)
		add_ai("dmi.cache.installed_size (KB)", hardware->dmi.cache[cache].installed_size)
		add_ai("dmi.cache.max_size (KB)", hardware->dmi.cache[cache].max_size)
		add_as("dmi.cache.supported_sram_types", hardware->dmi.cache[cache].supported_sram_types)
		add_as("dmi.cache.installed_sram_types", hardware->dmi.cache[cache].installed_sram_types)
		add_ai("dmi.cache.speed (ns)", hardware->dmi.cache[cache].speed)
		add_as("dmi.cache.error_correction_type", hardware->dmi.cache[cache].error_correction_type)
		add_as("dmi.cache.system_type", hardware->dmi.cache[cache].system_type)
		add_as("dmi.cache.associativity", hardware->dmi.cache[cache].associativity)
		END_OF_APPEND;
	}
}
void dump_memory_banks(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	if (hardware->dmi.memory_count == 0) {
			APPEND_ARRAY
				add_as("dmi.warning","No memory bank structure found")
			END_OF_APPEND;
			return;
	}

	for (int bank=0; bank<hardware->dmi.memory_count;bank++) {

		if (hardware->dmi.memory[bank].filled == false) {
			char msg[64]={0};
			snprintf(msg,sizeof(msg),"Bank %d doesn't contain any information", bank);

			APPEND_ARRAY
				add_as("dmi.warning",msg)
			END_OF_APPEND;
			continue;
		}

		APPEND_ARRAY
		add_ai("Memory Bank", bank)
		add_as("dmi.memory.form_factor", hardware->dmi.memory[bank].form_factor)
		add_as("dmi.memory.type", hardware->dmi.memory[bank].type)
		add_as("dmi.memory.type_detail", hardware->dmi.memory[bank].type_detail)
		add_as("dmi.memory.speed", hardware->dmi.memory[bank].speed)
		add_as("dmi.memory.size", hardware->dmi.memory[bank].size)
		add_as("dmi.memory.device_set", hardware->dmi.memory[bank].device_set)
		add_as("dmi.memory.device_locator", hardware->dmi.memory[bank].device_locator)
		add_as("dmi.memory.bank_locator", hardware->dmi.memory[bank].bank_locator)
		add_as("dmi.memory.total_width", hardware->dmi.memory[bank].total_width)
		add_as("dmi.memory.data_width", hardware->dmi.memory[bank].data_width)
		add_as("dmi.memory.error", hardware->dmi.memory[bank].error)
		add_as("dmi.memory.vendor", hardware->dmi.memory[bank].manufacturer)
		add_as("dmi.memory.serial", hardware->dmi.memory[bank].serial)
		add_as("dmi.memory.asset_tag", hardware->dmi.memory[bank].asset_tag)
		add_as("dmi.memory.part_number", hardware->dmi.memory[bank].part_number)
		END_OF_APPEND;
	}
}

void dump_processor(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	if (hardware->dmi.processor.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no processor structure found")
		END_OF_APPEND;
		return;
	}

	char voltage[16]={0};
	snprintf(voltage,sizeof(voltage),"%d.%02d",
		hardware->dmi.processor.voltage_mv / 1000,
		hardware->dmi.processor.voltage_mv - ((hardware->dmi.processor.voltage_mv / 1000) * 1000));

	CREATE_TEMP_OBJECT;
	add_ts("dmi.item","processor");
	add_ths(dmi.processor.socket_designation);
	add_ths(dmi.processor.type);
	add_ths(dmi.processor.family);
	add_ths(dmi.processor.manufacturer);
	add_ths(dmi.processor.version);
	add_thi(dmi.processor.external_clock);
	add_thi(dmi.processor.max_speed);
	add_thi(dmi.processor.current_speed);
	add_thi(dmi.processor.signature.type);
	add_thi(dmi.processor.signature.family);
	add_thi(dmi.processor.signature.model);
	add_thi(dmi.processor.signature.stepping);
	add_thi(dmi.processor.signature.minor_stepping);
	add_ts("dmi.processor.voltage",voltage);
	add_ths(dmi.processor.status);
	add_ths(dmi.processor.upgrade);
	add_ths(dmi.processor.cache1);
	add_ths(dmi.processor.cache2);
	add_ths(dmi.processor.cache3);
	add_ths(dmi.processor.serial);
	add_ths(dmi.processor.part_number);
	add_thi(dmi.processor.core_count);
	add_thi(dmi.processor.core_enabled);
	add_thi(dmi.processor.thread_count);
	add_ths(dmi.processor.id);
	for (int i = 0; i < PROCESSOR_FLAGS_ELEMENTS; i++) {
	        if (((bool *) (&hardware->dmi.processor.cpu_flags))[i] == true) {
	            add_ts("dmi.processor.flag",(char *)cpu_flags_strings[i]);
		}
	}
	APPEND_TEMP_OBJECT_ARRAY;
}

void dump_battery(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	if (hardware->dmi.battery.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no battery structure found")
		END_OF_APPEND;
		return;
	}

	APPEND_ARRAY
	add_as("dmi.item","battery")
	add_ahs(dmi.battery.manufacturer)
	add_ahs(dmi.battery.manufacture_date)
	add_ahs(dmi.battery.serial)
	add_ahs(dmi.battery.name)
	add_ahs(dmi.battery.chemistry)
	add_ahs(dmi.battery.design_capacity)
	add_ahs(dmi.battery.design_voltage)
	add_ahs(dmi.battery.sbds)
	add_ahs(dmi.battery.sbds_manufacture_date)
	add_ahs(dmi.battery.sbds_chemistry)
	add_ahs(dmi.battery.maximum_error)
	add_ahs(dmi.battery.oem_info)
	END_OF_APPEND;

}

void dump_ipmi(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	if (hardware->dmi.ipmi.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no IPMI structure found")
		END_OF_APPEND;
		return;
	}

	char spec_ver[16]={0};
	char i2c[16]={0};
	char base[16]={0};
	snprintf(spec_ver,sizeof(spec_ver),"%u.%u",
			hardware->dmi.ipmi.major_specification_version,
			hardware->dmi.ipmi.minor_specification_version);

	snprintf(i2c,sizeof(i2c),"0x%02x", hardware->dmi.ipmi.I2C_slave_address);
	snprintf(base,sizeof(base),"%08X%08X",
			(uint32_t)(hardware->dmi.ipmi.base_address >> 32),
			(uint32_t)((hardware->dmi.ipmi.base_address & 0xFFFF) & ~1));

	APPEND_ARRAY
	add_as("dmi.item","ipmi")
	add_ahs(dmi.ipmi.interface_type)
	add_as("dmi.ipmi.spec_version",spec_ver)
	add_ahi(dmi.ipmi.I2C_slave_address)
	add_ahi(dmi.ipmi.nv_address)
	add_as("dmi.ipmi.base_address",base)
	add_ahi(dmi.ipmi.irq)
	END_OF_APPEND;
}

void dump_chassis(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	if (hardware->dmi.chassis.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no chassis structure found")
		END_OF_APPEND;
		return;
	}

	APPEND_ARRAY
	add_as("dmi.item","bios")
	add_ahs(dmi.chassis.manufacturer)
	add_ahs(dmi.chassis.type)
	add_ahs(dmi.chassis.lock)
	add_ahs(dmi.chassis.version)
	add_ahs(dmi.chassis.serial)
	add_as("dmi.chassis.asset_tag",del_multi_spaces(hardware->dmi.chassis.asset_tag))
	add_ahs(dmi.chassis.boot_up_state)
	add_ahs(dmi.chassis.power_supply_state)
	add_ahs(dmi.chassis.thermal_state)
	add_ahs(dmi.chassis.security_status)
	add_ahs(dmi.chassis.oem_information)
	add_ahi(dmi.chassis.height)
	add_ahi(dmi.chassis.nb_power_cords)
	END_OF_APPEND;

}

void dump_bios(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {
	if (hardware->dmi.bios.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no bios structure found")
		END_OF_APPEND;
		return;
	}
	char address[16]={0};
	char runtime[16]={0};
	char rom[16]={0};
	snprintf(address,sizeof(address),"0x%04X0",hardware->dmi.bios.address);
	snprintf(runtime,sizeof(runtime),"%u %s",hardware->dmi.bios.runtime_size, hardware->dmi.bios.runtime_size_unit);
	snprintf(rom,sizeof(rom),"%u %s",hardware->dmi.bios.rom_size, hardware->dmi.bios.rom_size_unit);

	CREATE_TEMP_OBJECT;
	add_ts("dmi.item","bios");
	add_ths(dmi.bios.vendor);
	add_ths(dmi.bios.version);
	add_ths(dmi.bios.release_date);
	add_ths(dmi.bios.bios_revision);
	add_ths(dmi.bios.firmware_revision);
	add_ts("dmi.bios.address",address);
	add_ts("dmi.bios.runtime_size",runtime);
	add_ts("dmi.bios.rom_size",rom);
	for (int i = 0; i < BIOS_CHAR_NB_ELEMENTS; i++) {
	        if (((bool *) (&hardware->dmi.bios.characteristics))[i] == true) {
			add_ts("dmi.bios.characteristics",(char *)bios_charac_strings[i]);
		}
	}
	
	for (int i = 0; i < BIOS_CHAR_X1_NB_ELEMENTS; i++) {
	        if (((bool *) (&hardware->dmi.bios.characteristics_x1))[i] == true) {
			add_ts("dmi.bios.characteristics",(char *)bios_charac_x1_strings[i]);
		}
	}

	for (int i = 0; i < BIOS_CHAR_X2_NB_ELEMENTS; i++) {
	        if (((bool *) (&hardware->dmi.bios.characteristics_x2))[i] == true) {
			add_ts("dmi.bios.characteristics",(char *)bios_charac_x2_strings[i]);
		}
	}

	APPEND_TEMP_OBJECT_ARRAY;
}

void dump_system(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	if (hardware->dmi.system.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no system structure found")
		END_OF_APPEND;
		return;
	}
	char system_reset_status[10]={0};
	char watchdog_timer[15]={0};
	snprintf(system_reset_status,sizeof(system_reset_status),"%s", (hardware->dmi.system.system_reset.status ? "Enabled" :"Disabled"));
	snprintf(watchdog_timer,sizeof(watchdog_timer),"%s", (hardware->dmi.system.system_reset.watchdog ? "Present" :"Not Present"));

	APPEND_ARRAY
	add_as("dmi.item","system")
	add_ahs(dmi.system.manufacturer)
	add_ahs(dmi.system.product_name)
	add_ahs(dmi.system.version)
	add_ahs(dmi.system.serial)
	add_ahs(dmi.system.uuid)
	add_ahs(dmi.system.wakeup_type)
	add_ahs(dmi.system.sku_number)
	add_ahs(dmi.system.family)
	add_ahs(dmi.system.configuration_options)
	add_as("dmi.system.system_reset.status",system_reset_status)
	add_as("dmi.system.system_reset.watchdog",watchdog_timer)
	add_ahs(dmi.system.system_reset.boot_option)
	add_ahs(dmi.system.system_reset.boot_option_on_limit)
	add_ahs(dmi.system.system_reset.reset_count)
	add_ahs(dmi.system.system_reset.reset_limit)
	add_ahs(dmi.system.system_reset.timer_interval)
	add_ahs(dmi.system.system_reset.timeout)
	add_ahs(dmi.system.system_boot_status)
	END_OF_APPEND;

}

void dump_base_board(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	if (hardware->dmi.base_board.filled == false) {
		APPEND_ARRAY
			add_as("dmi.warning","no base_board structure found")
		END_OF_APPEND;
		return;
	}

	CREATE_TEMP_OBJECT;
	add_ts("dmi.item","base_board");
	add_ths(dmi.base_board.manufacturer);
	add_ths(dmi.base_board.product_name);
	add_ths(dmi.base_board.version);
	add_ths(dmi.base_board.serial);
	add_ths(dmi.base_board.asset_tag);
	add_ths(dmi.base_board.location);
	add_ths(dmi.base_board.type);
	for (int i = 0; i < BASE_BOARD_NB_ELEMENTS; i++) {
		if (((bool *) (&hardware->dmi.base_board.features))[i] == true) {
			add_ts("dmi.base_board.features",(char *)base_board_features_strings[i]);
		}
	}

	for (unsigned int i = 0; i < sizeof hardware->dmi.base_board.devices_information /
		         sizeof *hardware->dmi.base_board.devices_information; i++) {
	        if (strlen(hardware->dmi.base_board.devices_information[i].type)) {
			add_ts("dmi.base_board.devices_information.type", hardware->dmi.base_board.devices_information[i].type);
			add_ti("dmi.base_board.devices_information.status", hardware->dmi.base_board.devices_information[i].status);
			add_ts("dmi.base_board.devices_information.description", hardware->dmi.base_board.devices_information[i].description);
		}
	}
	
	APPEND_TEMP_OBJECT_ARRAY;

}

void dump_dmi(struct s_hardware *hardware, ZZJSON_CONFIG *config, ZZJSON **item) {

	*item = zzjson_create_object(config, NULL); /* empty object */
	add_hb(is_dmi_valid);

	if (hardware->is_dmi_valid == false) {
		goto exit;
	} else {
		zzjson_print(config, *item);
		zzjson_free(config, *item);
	}

	char buffer[8]={0};
	snprintf(buffer,sizeof(buffer),"%d.%d",hardware->dmi.dmitable.major_version, hardware->dmi.dmitable.minor_version);
	CREATE_ARRAY
	add_as("dmi.version",buffer)
	END_OF_ARRAY;	

	dump_base_board(hardware,config,item);
	dump_system(hardware,config,item);
	dump_bios(hardware,config,item);
	dump_chassis(hardware,config,item);
	dump_ipmi(hardware,config,item);
	dump_battery(hardware,config,item);
	dump_processor(hardware,config,item);
	dump_cache(hardware,config,item);
	dump_memory_banks(hardware,config,item);
	dump_memory_modules(hardware,config,item);
	dump_memory_size(hardware,config,item);
exit:
	flush("dmi",config,item);
}
