
#ifndef _SYSLINUX_H_
#define _SYSLINUX_H_

char issyslinux(); // Check if syslinux is running

void runcommand(char *cmd); // Run specified command

void gototxtmode(); // Change mode to text mode

#endif
