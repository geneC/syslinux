#include <syslinux/config.h>
#include <com32.h>
#include <fs.h>

extern uint8_t DriveNumber;
extern far_ptr_t PartInfo;
extern far_ptr_t OrigESDI;
extern uint64_t Hidden;

void get_derivative_info(union syslinux_derivative_info *di)
{
	di->disk.filesystem = SYSLINUX_FS_SYSLINUX;
	di->disk.sector_shift = SectorShift;
	di->disk.drive_number = DriveNumber;

	di->disk.ptab_ptr = GET_PTR(PartInfo);
	di->disk.esdi_ptr = GET_PTR(OrigESDI);
	di->disk.partoffset = Hidden;
}
