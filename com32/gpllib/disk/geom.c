#include <com32.h>
#include <string.h>
#include <stdio.h>
#include <disk/geom.h>

#include <stdio.h>

/**
 * lba_to_chs - split given lba into cylinders/heads/sectors
 **/
void lba_to_chs(const struct driveinfo* drive_info, const int lba,
		unsigned int* cylinder, unsigned int* head,
		unsigned int* sector)
{
	unsigned int track;

	*cylinder = (lba % drive_info->sectors_per_track) + 1;
	track = lba / drive_info->sectors_per_track;
	*head = track % drive_info->heads;
	*sector = track / drive_info->heads;
}

/**
 * detect_extensions - detect if we can use extensions
 *
 * INT 13 - IBM/MS INT 13 Extensions - INSTALLATION CHECK
 *    AH = 41h
 *    BX = 55AAh
 *    DL = drive (80h-FFh)
 *
 * Return: CF set on error (extensions not supported)
 *    AH = 01h (invalid function)
 *    CF clear if successful
 *    BX = AA55h if installed
 *    AH = major version of extensions
 *        01h = 1.x
 *        20h = 2.0 / EDD-1.0
 *        21h = 2.1 / EDD-1.1
 *        30h = EDD-3.0
 *    AL = internal use
 *    CX = API subset support bitmap (see #00271)
 *    DH = extension version (v2.0+ ??? -- not present in 1.x)
 *
 * Note: the Phoenix Enhanced Disk Drive Specification v1.0 uses version 2.0 of
 *       the INT 13 Extensions API
 *
 * Bitfields for IBM/MS INT 13 Extensions API support bitmap:
 * Bit(s)    Description    (Table 00271)
 *     0 extended disk access functions (AH=42h-44h,47h,48h) supported
 *     1 removable drive controller functions (AH=45h,46h,48h,49h,INT 15/AH=52h)
 *       supported
 *     2 enhanced disk drive (EDD) functions (AH=48h,AH=4Eh) supported
 *       extended drive parameter table is valid (see #00273,#00278)
 *     3-15    reserved (0)
 **/
static void detect_extensions(struct driveinfo* drive_info)
{
	com32sys_t getebios, ebios;

	memset(&getebios, 0, sizeof(com32sys_t));
	memset(&ebios, 0, sizeof(com32sys_t));

	getebios.eax.w[0] = 0x4100;
	getebios.ebx.w[0] = 0x55aa;
	getebios.edx.b[0] = drive_info->disk;
	getebios.eflags.b[0] = 0x3;	/* CF set */

	__intcall(0x13, &getebios, &ebios);

	if ( !(ebios.eflags.l & EFLAGS_CF) &&
	     ebios.ebx.w[0] == 0xaa55 ) {
		drive_info->ebios = 1;
		drive_info->edd_version = ebios.eax.b[1];
		drive_info->edd_functionality_subset = ebios.ecx.w[0];
	}
}

/**
 * get_drive_parameters_with_extensions - retrieve disk parameters via AH=48h
 *
 * INT 13 - IBM/MS INT 13 Extensions - GET DRIVE PARAMETERS
 *     AH = 48h
 *     DL = drive (80h-FFh)
 *     DS:SI -> buffer for drive parameters
 * Return: CF clear if successful
 *     AH = 00h
 *     DS:SI buffer filled
 *     CF set on error
 *     AH = error code (see #00234)
 * BUG: several different Compaq BIOSes incorrectly report high-numbered
 *     drives (such as 90h, B0h, D0h, and F0h) as present, giving them the
 *     same geometry as drive 80h; as a workaround, scan through disk
 *     numbers, stopping as soon as the number of valid drives encountered
 *     equals the value in 0040h:0075h
 **/
static int get_drive_parameters_with_extensions(struct driveinfo* drive_info)
{
	com32sys_t inreg, outreg;
	struct device_parameter dp;

	memset(&inreg, 0, sizeof(com32sys_t));
	memset(&outreg, 0, sizeof(com32sys_t));
	memset(&dp, 0, sizeof(struct device_parameter));

	inreg.esi.w[0] = OFFS(__com32.cs_bounce);
	inreg.ds = SEG(__com32.cs_bounce);
	inreg.eax.w[0] = 0x4800;
	inreg.edx.b[0] = drive_info->disk;

	__intcall(0x13, &inreg, &outreg);

	/* Saving bounce buffer before anything corrupts it */
	memcpy(&dp, __com32.cs_bounce, sizeof(struct device_parameter));

	/* CF set on error */
	if ( outreg.eflags.l & EFLAGS_CF )
		return outreg.eax.b[1];

	/* Override values found without extensions */
	drive_info->cylinder = dp.cylinders;
	drive_info->heads = dp.heads;
	drive_info->sectors = dp.sectors;
	drive_info->bytes_per_sector = dp.bytes_per_sector;

	/* The rest of the functions is EDD v3.0+ only */
	if (drive_info->edd_version < 0x30)
		return 0;

	/* "ISA" or "PCI" */
	strncpy(drive_info->host_bus_type, (char *) dp.host_bus_type,
		sizeof drive_info->host_bus_type);

	strncpy(drive_info->interface_type, (char *) dp.interface_type,
		sizeof drive_info->interface_type);

	if ( drive_info->sectors > 0 )
		drive_info->cbios = 1;	/* Valid geometry */

	return 0;
}

/**
 * get_drive_parameters_without_extensions - retrieve drive parameters via AH=08h
 *
 * INT 13 - DISK - GET DRIVE PARAMETERS (PC,XT286,CONV,PS,ESDI,SCSI)
 *     AH = 08h
 *     DL = drive (bit 7 set for hard disk)
 *
 * Return: CF set on error
 *     AH = status (07h) (see #00234)
 *     CF clear if successful
 *     AH = 00h
 *     AL = 00h on at least some BIOSes
 *     BL = drive type (AT/PS2 floppies only) (see #00242)
 *     CH = low eight bits of maximum cylinder number
 *     CL = maximum sector number (bits 5-0)
 *          high two bits of maximum cylinder number (bits 7-6)
 *     DH = maximum head number
 *     DL = number of drives
 *     ES:DI -> drive parameter table (floppies only)
 *
 * Notes:
 *   - may return successful even though specified drive is greater than the
 *     number of attached drives of that type (floppy/hard); check DL to
 *     ensure validity
 *   - for systems predating the IBM AT, this call is only valid for hard
 *     disks, as it is implemented by the hard disk BIOS rather than the
 *     ROM BIOS
 *   - Toshiba laptops with HardRAM return DL=02h when called with DL=80h,
 *     but fail on DL=81h. The BIOS data at 40h:75h correctly reports 01h.
 *     may indicate only two drives present even if more are attached; to
 *     ensure a correct count, one can use AH=15h to scan through possible
 *     drives
 *   - for BIOSes which reserve the last cylinder for testing purposes, the
 *     cylinder count is automatically decremented
 *     on PS/1s with IBM ROM DOS 4, nonexistent drives return CF clear,
 *     BX=CX=0000h, and ES:DI = 0000h:0000h
 *   - the PC-Tools PCFORMAT program requires that AL=00h before it will
 *     proceed with the formatting
 *
 * BUG: several different Compaq BIOSes incorrectly report high-numbered
 *      drives (such as 90h, B0h, D0h, and F0h) as present, giving them the
 *      same geometry as drive 80h; as a workaround, scan through disk
 *      numbers, stopping as soon as the number of valid drives encountered
 *      equals the value in 0040h:0075h
 *
 * SeeAlso: AH=06h"Adaptec",AH=13h"SyQuest",AH=48h,AH=15h,INT 1E
 * SeeAlso: INT 41"HARD DISK 0"
 **/
static int get_drive_parameters_without_extensions(struct driveinfo* drive_info)
{
	com32sys_t getparm, parm;

	memset(&getparm, 0, sizeof(com32sys_t));
	memset(&parm, 0, sizeof(com32sys_t));

	getparm.eax.b[1] = 0x08;
	getparm.edx.b[0] = drive_info->disk;

	__intcall(0x13, &getparm, &parm);

	/* CF set on error */
	if ( parm.eflags.l & EFLAGS_CF )
		return parm.eax.b[1];

	/* DL contains the maximum drive number but it starts at 0! */
	drive_info->drives = parm.edx.b[0] + 1;

	// XXX broken
	/* Drive specified greater than the bumber of attached drives */
	//if (drive_info->disk > drive_info->drives)
	//	return -1;

	drive_info->type = parm.ebx.b[0];

	/* DH contains the maximum head number but it starts at 0! */
	drive_info->heads = parm.edx.b[1] + 1;

	/* Maximum sector number (bits 5-0) per track */
	drive_info->sectors_per_track = parm.ecx.b[0] & 0x3f;

	/*
	 * Maximum cylinder number:
	 *     CH = low eight bits of maximum cylinder number
	 *     CL = high two bits of maximum cylinder number (bits 7-6)
	 */
	drive_info->cylinder = parm.ecx.b[1] +
			      ((parm.ecx.b[0] & 0x40) * 256 +
			       (parm.ecx.b[0] & 0x80) * 512);

	if ( drive_info->sectors_per_track > 0 )
		drive_info->cbios = 1;	/* Valid geometry */

	return 0;
}

/**
 * get_drive_parameters - retrieve drive parameters
 * @drive_info:		driveinfo structure to fill
 **/
int get_drive_parameters(struct driveinfo *drive_info)
{
	detect_extensions(drive_info);

	if (drive_info->ebios) {
		get_drive_parameters_without_extensions(drive_info);
		return get_drive_parameters_with_extensions(drive_info);
	} else
		return get_drive_parameters_without_extensions(drive_info);
}
