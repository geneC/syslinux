/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2003 Lars Munch Christensen - All Rights Reserved
 *   Copyright 1998-2008 H. Peter Anvin - All Rights Reserved
 *
 *   Based on the Linux installer program for SYSLINUX by H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * syslinux-mingw.c - Win2k/WinXP installer program for SYSLINUX
 */

#include <windows.h>
#include <stdio.h>
#include <ctype.h>

#include "syslinux.h"
#include "libfat.h"

#ifdef __GNUC__
# define noreturn void __attribute__((noreturn))
#else
# define noreturn void
#endif

void error(char* msg);

/* Begin stuff for MBR code */

#include <winioctl.h>

#define SECTOR_SIZE 512
#define PART_TABLE  0x1be
#define PART_SIZE   0x10
#define PART_COUNT  4
#define PART_ACTIVE 0x80

// The following struct should be in the ntddstor.h file, but I didn't have it.
// TODO: Make this a conditional compilation
typedef struct _STORAGE_DEVICE_NUMBER {
  DEVICE_TYPE  DeviceType;
  ULONG  DeviceNumber;
  ULONG  PartitionNumber;
} STORAGE_DEVICE_NUMBER, *PSTORAGE_DEVICE_NUMBER;

BOOL GetStorageDeviceNumberByHandle( HANDLE handle, const STORAGE_DEVICE_NUMBER *sdn ) {
  BOOL result = FALSE;
  DWORD count;

  if ( DeviceIoControl( handle, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL,
			0, (LPVOID)sdn, sizeof( *sdn ), &count, NULL ) ) {
    result = TRUE;
  }
  else {
    error("GetDriveNumber: DeviceIoControl failed");
  }

  return( result );
}

int GetBytesPerSector( HANDLE drive ) {
  int result = 0;
  DISK_GEOMETRY g;
  DWORD count;

  if ( DeviceIoControl( drive, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
			&g, sizeof( g ), &count, NULL ) ) {
    result = g.BytesPerSector;
  }

  return( result );
}

BOOL FixMBR(int driveNum, int partitionNum, int write_mbr, int set_active) {
  BOOL result = TRUE;
  HANDLE drive;

  char driveName[128];

  sprintf( driveName, "\\\\.\\PHYSICALDRIVE%d", driveNum );

  drive = CreateFile( driveName,
		      GENERIC_READ | GENERIC_WRITE,
		      FILE_SHARE_WRITE | FILE_SHARE_READ,
		      NULL,
		      OPEN_EXISTING,
		      0,
		      NULL );

  if( drive == INVALID_HANDLE_VALUE ) {
    error("Accessing physical drive");
    result = FALSE;
  }

  if( result ) {
    unsigned char sector[SECTOR_SIZE];
    DWORD howMany;

    if( GetBytesPerSector( drive ) != SECTOR_SIZE ) {
      fprintf(stderr, "Error: Sector size of this drive is %d; must be %d\n",
	      GetBytesPerSector( drive ), SECTOR_SIZE );
      result = FALSE;
    }

    if ( result ) {
      if ( ReadFile( drive, sector, sizeof( sector ), &howMany, NULL ) == 0 ) {
	error("Reading raw drive");
	result = FALSE;
      } else if ( howMany != sizeof( sector ) ) {
	fprintf(stderr, "Error: ReadFile on drive only got %d of %d bytes\n",
		(int)howMany, sizeof( sector ) );
	result = FALSE;
      }
    }

    // Copy over the MBR code if specified (-m)
    if ( write_mbr ) {
      if ( result ) {
	if ( syslinux_mbr_len >= PART_TABLE ) {
	  fprintf(stderr, "Error: MBR will not fit; not writing\n" );
	  result = FALSE;
	} else {
	  memcpy( sector, syslinux_mbr, syslinux_mbr_len );
	}
      }
    }

    // Check that our partition is active if specified (-a)
    if ( set_active ) {
      if ( sector[ PART_TABLE + ( PART_SIZE * ( partitionNum - 1 ) ) ] != 0x80 ) {
	int p;
	for ( p = 0; p < PART_COUNT; p++ )
	  sector[ PART_TABLE + ( PART_SIZE * p ) ] = ( p == partitionNum - 1 ? 0x80 : 0 );
      }
    }

    if ( result ) {
      SetFilePointer( drive, 0, NULL, FILE_BEGIN );

      if ( WriteFile( drive, sector, sizeof( sector ), &howMany, NULL ) == 0 ) {
	error("Writing MBR");
	result = FALSE;
      } else if ( howMany != sizeof( sector ) ) {
	fprintf(stderr, "Error: WriteFile on drive only wrote %d of %d bytes\n",
		(int)howMany, sizeof( sector ) );
	result = FALSE;
      }
    }

    if( !CloseHandle( drive ) ) {
      error("CloseFile on drive");
      result = FALSE;
    }
  }

  return( result );
}

/* End stuff for MBR code */

const char *program;		/* Name of program */
const char *drive;		/* Drive to install to */

/*
 * Check Windows version.
 *
 * On Windows Me/98/95 you cannot open a directory, physical disk, or
 * volume using CreateFile.
 */
int checkver(void)
{
  OSVERSIONINFO osvi;

  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
  GetVersionEx(&osvi);

  return  (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT) &&
          ((osvi.dwMajorVersion > 4) ||
          ((osvi.dwMajorVersion == 4) && (osvi.dwMinorVersion == 0)));
}

/*
 * Windows error function
 */
void error(char* msg)
{
  LPVOID lpMsgBuf;

  /* Format the Windows error message */
  FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
		(LPTSTR) &lpMsgBuf, 0, NULL );

  /* Print it */
  fprintf(stderr, "%s: %s", msg, (char*) lpMsgBuf);

  /* Free the buffer */
  LocalFree(lpMsgBuf);
}

/*
 * Wrapper for ReadFile suitable for libfat
 */
int libfat_readfile(intptr_t pp, void *buf, size_t secsize, libfat_sector_t sector)
{
  uint64_t offset = (uint64_t)sector * secsize;
  LONG loword = (LONG)offset;
  LONG hiword  = (LONG)(offset >> 32);
  LONG hiwordx = hiword;
  DWORD bytes_read;

  if ( SetFilePointer((HANDLE)pp, loword, &hiwordx, FILE_BEGIN) != loword ||
       hiword != hiwordx ||
       !ReadFile((HANDLE)pp, buf, secsize, &bytes_read, NULL) ||
       bytes_read != secsize ) {
    fprintf(stderr, "Cannot read sector %u\n", sector);
    exit(1);
  }

  return secsize;
}

noreturn usage(void)
{
  fprintf(stderr, "Usage: syslinux.exe [-sfmar][-d directory] <drive>: [bootsecfile]\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  HANDLE f_handle, d_handle;
  DWORD bytes_read;
  DWORD bytes_written;
  DWORD drives;
  UINT drive_type;

  static unsigned char sectbuf[512];
  char **argp, *opt;
  static char drive_name[] = "\\\\.\\?:";
  static char drive_root[] = "?:\\";
  static char ldlinux_name[] = "?:\\ldlinux.sys" ;
  const char *errmsg;
  struct libfat_filesystem *fs;
  libfat_sector_t s, *secp, sectors[65]; /* 65 is maximum possible */
  uint32_t ldlinux_cluster;
  int nsectors;
  const char *bootsecfile = NULL;
  const char *subdir = NULL;

  int force = 0;		/* -f (force) option */
  int mbr = 0;			/* -m (MBR) option */
  int setactive = 0;		/* -a (set partition active) */
  int stupid = 0;		/* -s (stupid) option */
  int raid_mode = 0;		/* -r (RAID) option */

  (void)argc;

  if (!checkver()) {
    fprintf(stderr, "You need to be running at least Windows NT; use syslinux.com instead.\n");
    exit(1);
  }

  program = argv[0];
  drive = NULL;

  for ( argp = argv+1 ; *argp ; argp++ ) {
    if ( **argp == '-' ) {
      opt = *argp + 1;
      if ( !*opt )
	usage();

      while ( *opt ) {
	switch ( *opt ) {
	case 's':		/* Use "safe, slow and stupid" code */
	  stupid = 1;
	  break;
	case 'r':		/* RAID mode */
	  raid_mode = 1;
	  break;
	case 'f':		/* Force install */
	  force = 1;
	  break;
	case 'm':		/* Install MBR */
	  mbr = 1;
	  break;
	case 'a':		/* Mark this partition active */
	  setactive = 1;
	  break;
	case 'd':
	  if ( argp[1] )
	    subdir = *++argp;
	  break;
	default:
	  usage();
	  break;
	}
	opt++;
      }
    } else {
      if ( bootsecfile )
	usage();
      else if ( drive )
	bootsecfile = *argp;
      else
	drive = *argp;
    }
  }

  if ( !drive || !isalpha(drive[0]) || drive[1] != ':' || drive[2] )
    usage();

  /* Test if drive exists */
  drives = GetLogicalDrives();
  if(!(drives & ( 1 << (tolower(drive[0]) - 'a')))) {
    fprintf(stderr, "No such drive %c:\n", drive[0]);
    exit(1);
  }

  /* Determines the drive type */
  drive_name[4]   = drive[0];
  ldlinux_name[0] = drive[0];
  drive_root[0]   = drive[0];
  drive_type = GetDriveType(drive_root);

  /* Test for removeable media */
  if ((drive_type == DRIVE_FIXED) && (force == 0)) {
    fprintf(stderr, "Not a removable drive (use -f to override) \n");
    exit(1);
  }

  /* Test for unsupported media */
  if ((drive_type != DRIVE_FIXED) && (drive_type != DRIVE_REMOVABLE)) {
    fprintf(stderr, "Unsupported media\n");
    exit(1);
  }

  /*
   * First open the drive
   */
  d_handle = CreateFile(drive_name, GENERIC_READ | GENERIC_WRITE,
			 FILE_SHARE_READ | FILE_SHARE_WRITE,
			 NULL, OPEN_EXISTING, 0, NULL );

  if (d_handle == INVALID_HANDLE_VALUE) {
    error("Could not open drive");
    exit(1);
  }

  /*
   * Make sure we can read the boot sector
   */
  if ( !ReadFile(d_handle, sectbuf, 512, &bytes_read, NULL) ) {
    error("Reading boot sector");
    exit(1);
  }
  if (bytes_read != 512) {
    fprintf(stderr, "Could not read the whole boot sector\n");
    exit(1);
  }

  /* Check to see that what we got was indeed an MS-DOS boot sector/superblock */
  if( (errmsg = syslinux_check_bootsect(sectbuf)) ) {
    fprintf(stderr, "%s\n", errmsg);
    exit(1);
  }

  /* Change to normal attributes to enable deletion */
  /* Just ignore error if the file do not exists */
  SetFileAttributes(ldlinux_name, FILE_ATTRIBUTE_NORMAL);

  /* Delete the file */
  /* Just ignore error if the file do not exists */
  DeleteFile(ldlinux_name);

  /* Create ldlinux.sys file */
  f_handle = CreateFile(ldlinux_name, GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM |
			FILE_ATTRIBUTE_HIDDEN,
			NULL );

  if (f_handle == INVALID_HANDLE_VALUE) {
    error("Unable to create ldlinux.sys");
    exit(1);
  }

  /* Write ldlinux.sys file */
  if (!WriteFile(f_handle, syslinux_ldlinux, syslinux_ldlinux_len, &bytes_written, NULL)) {
    error("Could not write ldlinux.sys");
    exit(1);
  }

  if (bytes_written != syslinux_ldlinux_len) {
    fprintf(stderr, "Could not write whole ldlinux.sys\n");
    exit(1);
  }

  /* Now flush the media */
  if(!FlushFileBuffers(f_handle)) {
    error("FlushFileBuffers failed");
    exit(1);
  }

  /* Map the file (is there a better way to do this?) */
  fs = libfat_open(libfat_readfile, (intptr_t)d_handle);
  ldlinux_cluster = libfat_searchdir(fs, 0, "LDLINUX SYS", NULL);
  secp = sectors;
  nsectors = 0;
  s = libfat_clustertosector(fs, ldlinux_cluster);
  while ( s && nsectors < 65 ) {
    *secp++ = s;
    nsectors++;
    s = libfat_nextsector(fs, s);
  }
  libfat_close(fs);

  /*
   * Patch ldlinux.sys and the boot sector
   */
  syslinux_patch(sectors, nsectors, stupid, raid_mode);

  /*
   * Rewrite the file
   */
  if ( SetFilePointer(f_handle, 0, NULL, FILE_BEGIN) != 0 ||
       !WriteFile(f_handle, syslinux_ldlinux, syslinux_ldlinux_len, &bytes_written, NULL) ||
       bytes_written != syslinux_ldlinux_len ) {
    error("Could not write ldlinux.sys");
    exit(1);
  }

  /* If desired, fix the MBR */
  if( mbr || setactive ) {
    STORAGE_DEVICE_NUMBER sdn;
    if( GetStorageDeviceNumberByHandle( d_handle, &sdn ) ) {
      if( !FixMBR(sdn.DeviceNumber, sdn.PartitionNumber, mbr, setactive) ) {
        fprintf(stderr, "Did not successfully update the MBR; continuing...\n");
      }
    } else {
      fprintf(stderr, "Could not find device number for updating MBR; continuing...\n");
    }
  }

  /* Close file */
  CloseHandle(f_handle);

  /* Move the file to the desired location */
  if (subdir) {
    char new_ldlinux_name[strlen(subdir)+16];
    char *cp = new_ldlinux_name+3;
    const char *sd;
    int slash = 1;

    new_ldlinux_name[0] = drive[0];
    new_ldlinux_name[1] = ':';
    new_ldlinux_name[2] = '\\';

    for (sd = subdir; *sd; sd++) {
      char c = *sd;

      if (c == '/' || c == '\\') {
	if (slash)
	  continue;
	c = '\\';
	slash = 1;
      } else {
	slash = 0;
      }

      *cp++ = c;
    }

    /* Skip if subdirectory == root */
    if (cp > new_ldlinux_name+3) {
      if (!slash)
	*cp++ = '\\';

      memcpy(cp, "ldlinux.sys", 12);

      /* Delete any previous file */
      SetFileAttributes(new_ldlinux_name, FILE_ATTRIBUTE_NORMAL);
      DeleteFile(new_ldlinux_name);
      if (!MoveFile(ldlinux_name, new_ldlinux_name))
	SetFileAttributes(ldlinux_name, FILE_ATTRIBUTE_READONLY |
			  FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
      else
	SetFileAttributes(new_ldlinux_name, FILE_ATTRIBUTE_READONLY |
			  FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN);
    }
  }



  /* Make the syslinux boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write the syslinux boot sector into the boot sector */
  if ( bootsecfile ) {
    f_handle = CreateFile(bootsecfile, GENERIC_READ | GENERIC_WRITE,
			  FILE_SHARE_READ | FILE_SHARE_WRITE,
			  NULL, CREATE_ALWAYS,
			  FILE_ATTRIBUTE_ARCHIVE,
			  NULL );
    if (f_handle == INVALID_HANDLE_VALUE) {
      error("Unable to create bootsector file");
      exit(1);
    }
    if (!WriteFile(f_handle, sectbuf, 512, &bytes_written, NULL)) {
      error("Could not write boot sector file");
      exit(1);
    }
    CloseHandle(f_handle);
  } else {
    SetFilePointer(d_handle, 0, NULL, FILE_BEGIN);
    WriteFile( d_handle, sectbuf, 512, &bytes_written, NULL ) ;
  }

  if(bytes_written != 512) {
    fprintf(stderr, "Could not write the whole boot sector\n");
    exit(1);
  }

  /* Close file */
  CloseHandle(d_handle);

  /* Done! */
  return 0;
}
