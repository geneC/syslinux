/* ----------------------------------------------------------------------- *
 *   
 *   Copyright 2003 Lars Munch Christensen - All Rights Reserved
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

char *program;			/* Name of program */
char *drive;			/* Drive to install to */

/*
 * Check Windows version.
 *
 * On Windows Me/98/95 you cannot open a directory, physical disk, or
 * volume using CreateFile.
 */
int checkver()
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

void usage(void)
{
  fprintf(stderr, "Usage: syslinux.exe [-sf] <drive>:\n");
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

  int force = 0;		/* -f (force) option */

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
	if ( *opt == 's' ) {
	  syslinux_make_stupid();	/* Use "safe, slow and stupid" code */
        } else if ( *opt == 'f' ) {
          force = 1;                    /* Force install */
	} else {
	  usage();
	}
	opt++;
      }
    } else {
      if ( drive )
	usage();
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

  if(d_handle == INVALID_HANDLE_VALUE) {
    error("Could not open drive");
    exit(1);
  }

  /*
   * Make sure we can read the boot sector
   */  
  ReadFile(d_handle, sectbuf, 512, &bytes_read, NULL);
  if(bytes_read != 512) {
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
  
  if(f_handle == INVALID_HANDLE_VALUE) {
    error("Unable to create ldlinux.sys");
    exit(1);
  }

  /* Write ldlinux.sys file */
  if (!WriteFile(f_handle, syslinux_ldlinux, syslinux_ldlinux_len, &bytes_written, NULL) ||
      bytes_written != syslinux_ldlinux_len ) {
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
  syslinux_patch(sectors, nsectors);

  /*
   * Rewrite the file
   */
  if ( SetFilePointer(f_handle, 0, NULL, FILE_BEGIN) != 0 ||
       !WriteFile(f_handle, syslinux_ldlinux, syslinux_ldlinux_len, &bytes_written, NULL) ||
       bytes_written != syslinux_ldlinux_len ) {
    error("Could not write ldlinux.sys");
    exit(1);
  }

  /* Close file */ 
  CloseHandle(f_handle);

  /* Make the syslinux boot sector */
  syslinux_make_bootsect(sectbuf);

  /* Write the syslinux boot sector into the boot sector */
  SetFilePointer(d_handle, 0, NULL, FILE_BEGIN);
  WriteFile( d_handle, sectbuf, 512, &bytes_written, NULL ) ;

  if(bytes_written != 512) {
    fprintf(stderr, "Could not write the whole boot sector\n");
    exit(1);
  }

  /* Close file */ 
  CloseHandle(d_handle);

  /* Done! */
  return 0;
}
