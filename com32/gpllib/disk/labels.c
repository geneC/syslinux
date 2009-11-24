/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer
 *
 *   This file is part of Syslinux, and is made available under
 *   the terms of the GNU General Public License version 2.
 *
 * ----------------------------------------------------------------------- */

#include <stdlib.h>
#include <string.h>

void get_label(int label, char **buffer_label)
{
    int buffer_size = (80 * sizeof(char));
    char *buffer = malloc(buffer_size);
    *buffer_label = buffer;

    switch (label) {
    case 0x01:
	strncpy(buffer, "DOS 12-bit fat", buffer_size);
	break;
    case 0x02:
	strncpy(buffer, "XENIX root", buffer_size);
	break;
    case 0x03:
	strncpy(buffer, "XENIX /usr", buffer_size);
	break;
    case 0x04:
	strncpy(buffer, "DOS 3.0+ 16-bit FAT (up to 32M)", buffer_size);
	break;
    case 0x05:
	strncpy(buffer, "DOS 3.3+ Extended Partition", buffer_size);
	break;
    case 0x06:
	strncpy(buffer, "DOS 3.31+ 16-bit FAT (over 32M)", buffer_size);
	break;
    case 0x07:
	strncpy(buffer, "OS/2 IFS (e.g., HPFS)", buffer_size);
	break;
	//case 0x07: strncpy(buffer, "Advanced Unix", buffer_size); break;
	//case 0x07: strncpy(buffer, "Windows NT NTFS", buffer_size); break;
	//case 0x07: strncpy(buffer, "QNX2.x (pre-1988)", buffer_size); break;
    case 0x08:
	strncpy(buffer, "OS/2 (v1.0-1.3 only)", buffer_size);
	break;
	//case 0x08: strncpy(buffer, "AIX boot partition", buffer_size); break;
	//case 0x08: strncpy(buffer, "SplitDrive", buffer_size); break;
	//case 0x08: strncpy(buffer, "DELL partition spanning multiple drives", buffer_size); break;
	//case 0x08: strncpy(buffer, "Commodore DOS", buffer_size); break;
	//case 0x08: strncpy(buffer, "QNX 1.x and 2.x ("qny")", buffer_size); break;
    case 0x09:
	strncpy(buffer, "AIX data partition", buffer_size);
	break;
	//case 0x09: strncpy(buffer, "Coherent filesystem", buffer_size); break;
	//case 0x09: strncpy(buffer, "QNX 1.x and 2.x ("qnz")", buffer_size); break;
    case 0x0a:
	strncpy(buffer, "OS/2 Boot Manager", buffer_size);
	break;
	//case 0x0a: strncpy(buffer, "Coherent swap partition", buffer_size); break;
	//case 0x0a: strncpy(buffer, "OPUS", buffer_size); break;
    case 0x0b:
	strncpy(buffer, "WIN95 OSR2 32-bit FAT", buffer_size);
	break;
    case 0x0c:
	strncpy(buffer, "WIN95 OSR2 32-bit FAT, LBA-mapped", buffer_size);
	break;
    case 0x0e:
	strncpy(buffer, "WIN95: DOS 16-bit FAT, LBA-mapped", buffer_size);
	break;
    case 0x0f:
	strncpy(buffer, "WIN95: Extended partition, LBA-mapped", buffer_size);
	break;
    case 0x10:
	strncpy(buffer, "OPUS (?)", buffer_size);
	break;
    case 0x11:
	strncpy(buffer, "Hidden DOS 12-bit FAT", buffer_size);
	break;
    case 0x12:
	strncpy(buffer, "Compaq config partition", buffer_size);
	break;
    case 0x14:
	strncpy(buffer, "Hidden DOS 16-bit FAT <32M", buffer_size);
	break;
    case 0x16:
	strncpy(buffer, "Hidden DOS 16-bit FAT >=32M", buffer_size);
	break;
    case 0x17:
	strncpy(buffer, "Hidden IFS (e.g., HPFS)", buffer_size);
	break;
    case 0x18:
	strncpy(buffer, "AST SmartSleep Partition", buffer_size);
	break;
    case 0x19:
	strncpy(buffer, "Unused (Claimed for Willowtech Photon COS)",
		buffer_size);
	break;
    case 0x1b:
	strncpy(buffer, "Hidden WIN95 OSR2 32-bit FAT", buffer_size);
	break;
    case 0x1c:
	strncpy(buffer, "Hidden WIN95 OSR2 32-bit FAT, LBA-mapped",
		buffer_size);
	break;
    case 0x1e:
	strncpy(buffer, "Hidden WIN95 16-bit FAT, LBA-mapped", buffer_size);
	break;
    case 0x20:
	strncpy(buffer, "Unused", buffer_size);
	break;
    case 0x21:
	strncpy(buffer, "Reserved", buffer_size);
	break;
	//case 0x21: strncpy(buffer, "Unused", buffer_size); break;
    case 0x22:
	strncpy(buffer, "Unused", buffer_size);
	break;
    case 0x23:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x24:
	strncpy(buffer, "NEC DOS 3.x", buffer_size);
	break;
    case 0x26:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x31:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x32:
	strncpy(buffer, "NOS", buffer_size);
	break;
    case 0x33:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x34:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x35:
	strncpy(buffer, "JFS on OS/2 or eCS", buffer_size);
	break;
    case 0x36:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x38:
	strncpy(buffer, "THEOS ver 3.2 2gb partition", buffer_size);
	break;
    case 0x39:
	strncpy(buffer, "Plan 9 partition", buffer_size);
	break;
	//case 0x39: strncpy(buffer, "THEOS ver 4 spanned partition", buffer_size); break;
    case 0x3a:
	strncpy(buffer, "THEOS ver 4 4gb partition", buffer_size);
	break;
    case 0x3b:
	strncpy(buffer, "THEOS ver 4 extended partition", buffer_size);
	break;
    case 0x3c:
	strncpy(buffer, "PartitionMagic recovery partition", buffer_size);
	break;
    case 0x3d:
	strncpy(buffer, "Hidden NetWare", buffer_size);
	break;
    case 0x40:
	strncpy(buffer, "Venix 80286", buffer_size);
	break;
    case 0x41:
	strncpy(buffer, "Linux/MINIX (sharing disk with DRDOS)", buffer_size);
	break;
	//case 0x41: strncpy(buffer, "Personal RISC Boot", buffer_size); break;
	//case 0x41: strncpy(buffer, "PPC PReP (Power PC Reference Platform) Boot", buffer_size); break;
    case 0x42:
	strncpy(buffer, "Linux swap (sharing disk with DRDOS)", buffer_size);
	break;
	//case 0x42: strncpy(buffer, "SFS (Secure Filesystem)", buffer_size); break;
	//case 0x42: strncpy(buffer, "Windows 2000 marker", buffer_size); break;
    case 0x43:
	strncpy(buffer, "Linux native (sharing disk with DRDOS)", buffer_size);
	break;
    case 0x44:
	strncpy(buffer, "GoBack partition", buffer_size);
	break;
    case 0x45:
	strncpy(buffer, "Boot-US boot manager", buffer_size);
	break;
	//case 0x45: strncpy(buffer, "Priam", buffer_size); break;
	//case 0x45: strncpy(buffer, "EUMEL/Elan", buffer_size); break;
    case 0x46:
	strncpy(buffer, "EUMEL/Elan", buffer_size);
	break;
    case 0x47:
	strncpy(buffer, "EUMEL/Elan", buffer_size);
	break;
    case 0x48:
	strncpy(buffer, "EUMEL/Elan", buffer_size);
	break;
    case 0x4a:
	strncpy(buffer, "AdaOS Aquila (Default)", buffer_size);
	break;
	//case 0x4a: strncpy(buffer, "ALFS/THIN lightweight filesystem for DOS", buffer_size); break;
    case 0x4c:
	strncpy(buffer, "Oberon partition", buffer_size);
	break;
    case 0x4d:
	strncpy(buffer, "QNX4.x", buffer_size);
	break;
    case 0x4e:
	strncpy(buffer, "QNX4.x 2nd part", buffer_size);
	break;
    case 0x4f:
	strncpy(buffer, "QNX4.x 3rd part", buffer_size);
	break;
	//case 0x4f: strncpy(buffer, "Oberon partition", buffer_size); break;
    case 0x50:
	strncpy(buffer, "OnTrack Disk Manager (older versions) RO",
		buffer_size);
	break;
	//case 0x50: strncpy(buffer, "Lynx RTOS", buffer_size); break;
	//case 0x50: strncpy(buffer, "Native Oberon (alt)", buffer_size); break;
    case 0x51:
	strncpy(buffer, "OnTrack Disk Manager RW (DM6 Aux1)", buffer_size);
	break;
	//case 0x51: strncpy(buffer, "Novell", buffer_size); break;
    case 0x52:
	strncpy(buffer, "CP/M", buffer_size);
	break;
	//case 0x52: strncpy(buffer, "Microport SysV/AT", buffer_size); break;
    case 0x53:
	strncpy(buffer, "Disk Manager 6.0 Aux3", buffer_size);
	break;
    case 0x54:
	strncpy(buffer, "Disk Manager 6.0 Dynamic Drive Overlay", buffer_size);
	break;
    case 0x55:
	strncpy(buffer, "EZ-Drive", buffer_size);
	break;
    case 0x56:
	strncpy(buffer, "Golden Bow VFeature Partitioned Volume.", buffer_size);
	break;
	//case 0x56: strncpy(buffer, "DM converted to EZ-BIOS", buffer_size); break;
    case 0x57:
	strncpy(buffer, "DrivePro", buffer_size);
	break;
	//case 0x57: strncpy(buffer, "VNDI Partition", buffer_size); break;
    case 0x5c:
	strncpy(buffer, "Priam EDisk", buffer_size);
	break;
    case 0x61:
	strncpy(buffer, "SpeedStor", buffer_size);
	break;
    case 0x63:
	strncpy(buffer,
		"Unix System V (SCO, ISC Unix, UnixWare, ...), Mach, GNU Hurd",
		buffer_size);
	break;
    case 0x64:
	strncpy(buffer, "PC-ARMOUR protected partition", buffer_size);
	break;
	//case 0x64: strncpy(buffer, "Novell Netware 286, 2.xx", buffer_size); break;
    case 0x65:
	strncpy(buffer, "Novell Netware 386, 3.xx or 4.xx", buffer_size);
	break;
    case 0x66:
	strncpy(buffer, "Novell Netware SMS Partition", buffer_size);
	break;
    case 0x67:
	strncpy(buffer, "Novell", buffer_size);
	break;
    case 0x68:
	strncpy(buffer, "Novell", buffer_size);
	break;
    case 0x69:
	strncpy(buffer, "Novell Netware 5+, Novell Netware NSS Partition",
		buffer_size);
	break;
    case 0x70:
	strncpy(buffer, "DiskSecure Multi-Boot", buffer_size);
	break;
    case 0x71:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x73:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x74:
	strncpy(buffer, "Reserved", buffer_size);
	break;
	//case 0x74: strncpy(buffer, "Scramdisk partition", buffer_size); break;
    case 0x75:
	strncpy(buffer, "IBM PC/IX", buffer_size);
	break;
    case 0x76:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0x77:
	strncpy(buffer, "M2FS/M2CS partition", buffer_size);
	break;
	//case 0x77: strncpy(buffer, "VNDI Partition", buffer_size); break;
    case 0x78:
	strncpy(buffer, "XOSL FS", buffer_size);
	break;
    case 0x7E:
	strncpy(buffer, " ", buffer_size);
	break;
    case 0x80:
	strncpy(buffer, "MINIX until 1.4a", buffer_size);
	break;
    case 0x81:
	strncpy(buffer, "MINIX since 1.4b, early Linux", buffer_size);
	break;
	//case 0x81: strncpy(buffer, "Mitac disk manager", buffer_size); break;
	//case 0x82: strncpy(buffer, "Prime", buffer_size); break;
	//case 0x82: strncpy(buffer, "Solaris x86", buffer_size); break;
    case 0x82:
	strncpy(buffer, "Linux swap", buffer_size);
	break;
    case 0x83:
	strncpy(buffer, "Linux native (usually ext2fs)", buffer_size);
	break;
    case 0x84:
	strncpy(buffer, "OS/2 hidden C: drive", buffer_size);
	break;
	//case 0x84: strncpy(buffer, "Hibernation partition", buffer_size); break;
    case 0x85:
	strncpy(buffer, "Linux extended partition", buffer_size);
	break;
	//case 0x86: strncpy(buffer, "Old Linux RAID partition superblock", buffer_size); break;
    case 0x86:
	strncpy(buffer, "NTFS volume set", buffer_size);
	break;
    case 0x87:
	strncpy(buffer, "NTFS volume set", buffer_size);
	break;
    case 0x8a:
	strncpy(buffer, "Linux Kernel Partition (used by AiR-BOOT)",
		buffer_size);
	break;
    case 0x8b:
	strncpy(buffer, "Legacy Fault Tolerant FAT32 volume", buffer_size);
	break;
    case 0x8c:
	strncpy(buffer,
		"Legacy Fault Tolerant FAT32 volume using BIOS extd INT 13h",
		buffer_size);
	break;
    case 0x8d:
	strncpy(buffer, "Free FDISK hidden Primary DOS FAT12 partitition",
		buffer_size);
	break;
    case 0x8e:
	strncpy(buffer, "Linux Logical Volume Manager partition", buffer_size);
	break;
    case 0x90:
	strncpy(buffer, "Free FDISK hidden Primary DOS FAT16 partitition",
		buffer_size);
	break;
    case 0x91:
	strncpy(buffer, "Free FDISK hidden DOS extended partitition",
		buffer_size);
	break;
    case 0x92:
	strncpy(buffer, "Free FDISK hidden Primary DOS large FAT16 partitition",
		buffer_size);
	break;
    case 0x93:
	strncpy(buffer, "Hidden Linux native partition", buffer_size);
	break;
	//case 0x93: strncpy(buffer, "Amoeba", buffer_size); break;
    case 0x94:
	strncpy(buffer, "Amoeba bad block table", buffer_size);
	break;
    case 0x95:
	strncpy(buffer, "MIT EXOPC native partitions", buffer_size);
	break;
    case 0x97:
	strncpy(buffer, "Free FDISK hidden Primary DOS FAT32 partitition",
		buffer_size);
	break;
    case 0x98:
	strncpy(buffer, "Free FDISK hidden Primary DOS FAT32 partitition (LBA)",
		buffer_size);
	break;
    case 0x99:
	strncpy(buffer, "DCE376 logical drive", buffer_size);
	break;
    case 0x9a:
	strncpy(buffer, "Free FDISK hidden Primary DOS FAT16 partitition (LBA)",
		buffer_size);
	break;
    case 0x9b:
	strncpy(buffer, "Free FDISK hidden DOS extended partitition (LBA)",
		buffer_size);
	break;
    case 0x9f:
	strncpy(buffer, "BSD/OS", buffer_size);
	break;
    case 0xa0:
	strncpy(buffer, "Laptop hibernation partition", buffer_size);
	break;
    case 0xa1:
	strncpy(buffer, "Laptop hibernation partition", buffer_size);
	break;
	//case 0xa1: strncpy(buffer, "HP Volume Expansion (SpeedStor variant)", buffer_size); break;
    case 0xa3:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xa4:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xa5:
	strncpy(buffer, "BSD/386, 386BSD, NetBSD, FreeBSD", buffer_size);
	break;
    case 0xa6:
	strncpy(buffer, "OpenBSD", buffer_size);
	break;
    case 0xa7:
	strncpy(buffer, "NEXTSTEP", buffer_size);
	break;
    case 0xa8:
	strncpy(buffer, "Mac OS-X", buffer_size);
	break;
    case 0xa9:
	strncpy(buffer, "NetBSD", buffer_size);
	break;
    case 0xaa:
	strncpy(buffer, "Olivetti Fat 12 1.44Mb Service Partition",
		buffer_size);
	break;
    case 0xab:
	strncpy(buffer, "Mac OS-X Boot partition", buffer_size);
	break;
	//case 0xab: strncpy(buffer, "GO! partition", buffer_size); break;
    case 0xae:
	strncpy(buffer, "ShagOS filesystem", buffer_size);
	break;
    case 0xaf:
	strncpy(buffer, "ShagOS swap partition", buffer_size);
	break;
    case 0xb0:
	strncpy(buffer, "BootStar Dummy", buffer_size);
	break;
    case 0xb1:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xb3:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xb4:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xb6:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xb7:
	strncpy(buffer, "BSDI BSD/386 filesystem", buffer_size);
	break;
    case 0xb8:
	strncpy(buffer, "BSDI BSD/386 swap partition", buffer_size);
	break;
    case 0xbb:
	strncpy(buffer, "Boot Wizard hidden", buffer_size);
	break;
    case 0xbe:
	strncpy(buffer, "Solaris 8 boot partition", buffer_size);
	break;
    case 0xc0:
	strncpy(buffer, "CTOS", buffer_size);
	break;
	//case 0xc0: strncpy(buffer, "REAL/32 secure small partition", buffer_size); break;
	//case 0xc0: strncpy(buffer, "NTFT Partition", buffer_size); break;
    case 0xc1:
	strncpy(buffer, "DRDOS/secured (FAT-12)", buffer_size);
	break;
    case 0xc2:
	strncpy(buffer, "Reserved for DR-DOS 7+", buffer_size);
	break;
	//case 0xc2: strncpy(buffer, "Hidden Linux", buffer_size); break;
    case 0xc3:
	strncpy(buffer, "Hidden Linux swap", buffer_size);
	break;
    case 0xc4:
	strncpy(buffer, "DRDOS/secured (FAT-16, < 32M)", buffer_size);
	break;
    case 0xc5:
	strncpy(buffer, "DRDOS/secured (extended)", buffer_size);
	break;
    case 0xc6:
	strncpy(buffer, "DRDOS/secured (FAT-16, >= 32M)", buffer_size);
	break;
	//case 0xc6: strncpy(buffer, "Windows NT corrupted FAT16 volume/stripe set", buffer_size); break;
    case 0xc7:
	strncpy(buffer, "Windows NT corrupted NTFS volume/stripe set",
		buffer_size);
	break;
	//case 0xc7: strncpy(buffer, "Syrinx boot", buffer_size); break;
    case 0xc8:
	strncpy(buffer, "(See also ID c2.)", buffer_size);
	break;
    case 0xc9:
	strncpy(buffer, "(See also ID c2.)", buffer_size);
	break;
    case 0xca:
	strncpy(buffer, "(See also ID c2.)", buffer_size);
	break;
    case 0xcb:
	strncpy(buffer, "reserved for DRDOS/secured (FAT32)", buffer_size);
	break;
    case 0xcc:
	strncpy(buffer, "reserved for DRDOS/secured (FAT32, LBA)", buffer_size);
	break;
    case 0xcd:
	strncpy(buffer, "CTOS Memdump?", buffer_size);
	break;
    case 0xce:
	strncpy(buffer, "reserved for DRDOS/secured (FAT16, LBA)", buffer_size);
	break;
    case 0xd0:
	strncpy(buffer, "REAL/32 secure big partition", buffer_size);
	break;
    case 0xd1:
	strncpy(buffer, "Old Multiuser DOS secured FAT12", buffer_size);
	break;
    case 0xd4:
	strncpy(buffer, "Old Multiuser DOS secured FAT16 <32M", buffer_size);
	break;
    case 0xd5:
	strncpy(buffer, "Old Multiuser DOS secured extended partition",
		buffer_size);
	break;
    case 0xd6:
	strncpy(buffer, "Old Multiuser DOS secured FAT16 >=32M", buffer_size);
	break;
    case 0xd8:
	strncpy(buffer, "CP/M-86", buffer_size);
	break;
    case 0xda:
	strncpy(buffer, "Non-FS Data", buffer_size);
	break;
    case 0xdb:
	strncpy(buffer,
		"Digital Research CP/M, Concurrent CP/M, Concurrent DOS",
		buffer_size);
	break;
	//case 0xdb: strncpy(buffer, "CTOS (Convergent Technologies OS -Unisys)", buffer_size); break;
	//case 0xdb: strncpy(buffer, "KDG Telemetry SCPU boot", buffer_size); break;
    case 0xdd:
	strncpy(buffer, "Hidden CTOS Memdump?", buffer_size);
	break;
    case 0xde:
	strncpy(buffer, "Dell PowerEdge Server utilities (FAT fs)",
		buffer_size);
	break;
    case 0xdf:
	strncpy(buffer, "DG/UX virtual disk manager partition", buffer_size);
	break;
	//case 0xdf: strncpy(buffer, "BootIt EMBRM", buffer_size); break;
    case 0xe0:
	strncpy(buffer,
		"Reserved by STMicroelectronics for a filesystem called ST AVFS.",
		buffer_size);
	break;
    case 0xe1:
	strncpy(buffer, "DOS access or SpeedStor 12-bit FAT extended partition",
		buffer_size);
	break;
    case 0xe3:
	strncpy(buffer, "DOS R/O or SpeedStor", buffer_size);
	break;
    case 0xe4:
	strncpy(buffer, "SpeedStor 16-bit FAT extended partition < 1024 cyl.",
		buffer_size);
	break;
    case 0xe5:
	strncpy(buffer,
		"Tandy DOS with logical sectored FAT (According to Powerquest.)",
		buffer_size);
	break;
	//case 0xe5: strncpy(buffer, "Reserved", buffer_size); break;
    case 0xe6:
	strncpy(buffer, "Reserved", buffer_size);
	break;
    case 0xeb:
	strncpy(buffer, "BFS (aka BeFS)", buffer_size);
	break;
    case 0xed:
	strncpy(buffer, "Reserved for Matthias Paul's Sprytix", buffer_size);
	break;
    case 0xee:
	strncpy(buffer,
		"Indication that this legacy MBR is followed by an EFI header",
		buffer_size);
	break;
    case 0xef:
	strncpy(buffer, "Partition that contains an EFI file system",
		buffer_size);
	break;
    case 0xf0:
	strncpy(buffer, "Linux/PA-RISC boot loader", buffer_size);
	break;
    case 0xf1:
	strncpy(buffer, "SpeedStor", buffer_size);
	break;
    case 0xf2:
	strncpy(buffer,
		"DOS 3.3+ secondary partition (Powerquest writes: Unisys DOS with logical sectored FAT.)",
		buffer_size);
	break;
    case 0xf3:
	strncpy(buffer,
		"Reserved (Powerquest writes: Storage Dimensions SpeedStor.)",
		buffer_size);
	break;
    case 0xf4:
	strncpy(buffer, "SpeedStor large partition", buffer_size);
	break;
	//case 0xf4: strncpy(buffer, "Prologue single-volume partition", buffer_size); break;
    case 0xf5:
	strncpy(buffer, "Prologue multi-volume partition", buffer_size);
	break;
    case 0xf6:
	strncpy(buffer,
		"Reserved (Powerquest writes: Storage Dimensions SpeedStor. )",
		buffer_size);
	break;
    case 0xfa:
	strncpy(buffer, "Bochs", buffer_size);
	break;
    case 0xfb:
	strncpy(buffer, "VMware File System partition", buffer_size);
	break;
    case 0xfc:
	strncpy(buffer, "VMware Swap partition", buffer_size);
	break;
    case 0xfd:
	strncpy(buffer,
		"Linux raid partition with autodetect using persistent superblock (Powerquest writes: Reserved for FreeDOS. )",
		buffer_size);
	break;
    case 0xfe:
	strncpy(buffer, "SpeedStor > 1024 cyl.", buffer_size);
	break;
	//case 0xfe: strncpy(buffer, "LANstep", buffer_size); break;
	//case 0xfe: strncpy(buffer, "IBM PS/2 IML (Initial Microcode Load) partition, located at the end of the disk.", buffer_size); break;
	//case 0xfe: strncpy(buffer, "Windows NT Disk Administrator hidden partition", buffer_size); break;
	//case 0xfe: strncpy(buffer, "Linux Logical Volume Manager partition (old)", buffer_size); break;
    case 0xff:
	strncpy(buffer, "Xenix Bad Block Table ", buffer_size);
	break;
    default:
	strncpy(buffer, "Unknown", buffer_size);
	break;
    }
}
