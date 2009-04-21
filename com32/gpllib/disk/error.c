#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * get_error - decode a disk error status
 * @status:	Error code
 * @buffer_ptr:	Pointer to set to the error message
 *
 * A buffer will be allocated to contain the error message.
 * @buffer_ptr will point to it. The caller will need to free it.
 **/
void get_error(int status, char** buffer_ptr)
{
	int buffer_size = (80 * sizeof(char));
	char* buffer = malloc(buffer_size);
	*buffer_ptr = buffer;

	switch (status) {
	case 0x0:
	strncpy(buffer, "successful completion", buffer_size);
	break;
	case 0x01:
	strncpy(buffer, "invalid function in AH or invalid parameter", buffer_size);
	break;
	case 0x02:
	strncpy(buffer, "address mark not found", buffer_size);
	break;
	case 0x03:
	strncpy(buffer, "disk write-protected", buffer_size);
	break;
	case 0x04:
	strncpy(buffer, "sector not found/read error", buffer_size);
	break;
	case 0x05:
	strncpy(buffer, "reset failed (hard disk)", buffer_size);
	//strncpy(buffer, "data did not verify correctly (TI Professional PC)", buffer_size);
	break;
	case 0x06:
	strncpy(buffer, "disk changed (floppy)", buffer_size);
	break;
	case 0x07:
	strncpy(buffer, "drive parameter activity failed (hard disk)", buffer_size);
	break;
	case 0x08:
	strncpy(buffer, "DMA overrun", buffer_size);
	break;
	case 0x09:
	strncpy(buffer, "data boundary error (attempted DMA across 64K boundary or >80h sectors)", buffer_size);
	break;
	case 0x0A:
	strncpy(buffer, "bad sector detected (hard disk)", buffer_size);
	break;
	case 0x0B:
	strncpy(buffer, "bad track detected (hard disk)", buffer_size);
	break;
	case 0x0C:
	strncpy(buffer, "unsupported track or invalid media", buffer_size);
	break;
	case 0x0D:
	strncpy(buffer, "invalid number of sectors on format (PS/2 hard disk)", buffer_size);
	break;
	case 0x0E:
	strncpy(buffer, "control data address mark detected (hard disk)", buffer_size);
	break;
	case 0x0F:
	strncpy(buffer, "DMA arbitration level out of range (hard disk)", buffer_size);
	break;
	case 0x10:
	strncpy(buffer, "uncorrectable CRC or ECC error on read", buffer_size);
	break;
	case 0x11:
	strncpy(buffer, "data ECC corrected (hard disk)", buffer_size);
	break;
	case 0x20:
	strncpy(buffer, "controller failure", buffer_size);
	break;
	case 0x31:
	strncpy(buffer, "no media in drive (IBM/MS INT 13 extensions)", buffer_size);
	break;
	case 0x32:
	strncpy(buffer, "incorrect drive type stored in CMOS (Compaq)", buffer_size);
	break;
	case 0x40:
	strncpy(buffer, "seek failed", buffer_size);
	break;
	case 0x80:
	strncpy(buffer, "timeout (not ready)", buffer_size);
	break;
	case 0xAA:
	strncpy(buffer, "drive not ready (hard disk)", buffer_size);
	break;
	case 0xB0:
	strncpy(buffer, "volume not locked in drive (INT 13 extensions)", buffer_size);
	break;
	case 0xB1:
	strncpy(buffer, "volume locked in drive (INT 13 extensions)", buffer_size);
	break;
	case 0xB2:
	strncpy(buffer, "volume not removable (INT 13 extensions)", buffer_size);
	break;
	case 0xB3:
	strncpy(buffer, "volume in use (INT 13 extensions)", buffer_size);
	break;
	case 0xB4:
	strncpy(buffer, "lock count exceeded (INT 13 extensions)", buffer_size);
	break;
	case 0xB5:
	strncpy(buffer, "valid eject request failed (INT 13 extensions)", buffer_size);
	break;
	case 0xBB:
	strncpy(buffer, "undefined error (hard disk)", buffer_size);
	break;
	case 0xCC:
	strncpy(buffer, "write fault (hard disk)", buffer_size);
	break;
	case 0xE0:
	strncpy(buffer, "status register error (hard disk)", buffer_size);
	break;
	case 0xFF:
	strncpy(buffer, "sense operation failed (hard disk)", buffer_size);
	break;
	default:
	snprintf(buffer, buffer_size, "unknown error 0x%X, buggy bios?", status);
	break;
	}
}
