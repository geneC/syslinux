/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#ifndef _ERRNO_DISK_H
#define _ERRNO_DISK_H

extern int errno_disk;

#define	EINV	0x01	/* Invalid function in AH or invalid parameter */
#define	EADDR	0x02	/* Address mark not found */
#define	ERO	0x03	/* Disk write-protected */
#define	ENOFND	0x04	/* Sector not found/read error */
#define	ERFAIL	0x05	/* Reset failed (hard disk) */
#define	ECHANG	0x06	/* Disk changed (floppy) */
#define	EDFAIL	0x07	/* Drive parameter activity failed (hard disk) */
#define	EDMA	0x08	/* DMA overrun */
#define	EBOUND	0x09	/* Data boundary error (attempted DMA across 64K boundary or >80h sectors) */
#define	EBADS	0x0A	/* Bad sector detected (hard disk) */
#define	EBADT	0x0B	/* Bad track detected (hard disk) */
#define	EINVM	0x0C	/* Unsupported track or invalid media */
#define	EINVS	0x0D	/* Invalid number of sectors on format (PS/2 hard disk) */
#define	EADDRM	0x0E	/* Control data address mark detected (hard disk) */
#define	EDMARG	0x0F	/* DMA arbitration level out of range (hard disk) */
#define	ECRCF	0x10	/* Uncorrectable CRC or ECC error on read */
#define	ECRCV	0x11	/* Data ECC corrected (hard disk) */
#define	ECTRL	0x20	/* Controller failure */
#define	EMEDIA	0x31	/* No media in drive (IBM/MS INT 13 extensions) */
#define	ECMOS	0x32	/* Incorrect drive type stored in CMOS (Compaq) */
#define	ESEEKF	0x40	/* Seek failed */
#define	ETIME	0x80	/* Timeout (not ready) */
#define	EREADY	0xAA	/* Drive not ready (hard disk) */
#define	ENLOCK	0xB0	/* Volume not locked in drive (INT 13 extensions) */
#define	ELOCK	0xB1	/* Volume locked in drive (INT 13 extensions) */
#define	EREMOV	0xB2	/* Volume not removable (INT 13 extensions) */
#define	EUSED	0xB3	/* Volume in use (INT 13 extensions) */
#define	ECOUNT	0xB4	/* Lock count exceeded (INT 13 extensions) */
#define	EEJF	0xB5	/* Valid eject request failed (INT 13 extensions) */
#define	EUNKOWN	0xBB	/* Undefined error (hard disk) */
#define	EWF	0xCC	/* Write fault (hard disk) */
#define	ERF	0xE0	/* Status register error (hard disk) */
#define	ESF	0xFF	/* Sense operation failed (hard disk) */

#endif /* _ERRNO_DISK_H */
