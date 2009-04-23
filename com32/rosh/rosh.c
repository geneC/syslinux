/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008 Gene Cumm - All Rights Reserved
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * rosh.c
 *
 * Read-Only shell; Simple shell system designed for SYSLINUX-derivitives.
 * Provides minimal commands utilizing the console via stdout/stderr as the
 * sole output devices.  Designed to compile for Linux for testing/debugging.
 */

/*
 * ToDos:
 * Change functions to use pwdstr
 * In rosh_run() Reparse cmdstr relative to pwdstr
 */

// #define DO_DEBUG 1
	/* Uncomment the above line for debugging output; Comment to remove */
// #define DO_DEBUG2 1
	/* Uncomment the above line for super-debugging output; Must have regular debugging enabled; Comment to remove */

#include "rosh.h"

#define APP_LONGNAME	"Read-Only Shell"
#define APP_NAME	"rosh"
#define APP_AUTHOR	"Gene Cumm"
#define APP_YEAR	"2008"
#define APP_VER		"beta-b032"

void rosh_version()
{
	printf("%s v %s; (c) %s %s.\n", APP_LONGNAME, APP_VER, APP_YEAR, \
		APP_AUTHOR);
}

void rosh_help(int type)
{
	rosh_version();
	switch (type) {
	case 2:	puts(rosh_help_str2);
		break;
	case 1:	default:
		puts(rosh_help_str1);
	}
}

/* Determine if a character is whitespace
 *	inc	input character
 *	returns	0 if not whitespace
 */
int rosh_issp(char inc)
{
	int rv;
	switch (inc){
	case ' ': case '\t':
		rv = 1;	break;
	default: rv = 0;
	}
	return rv;
}	/* ros_issp */

/* Search a string for first non-space (' ') character, starting at ipos
 *	istr	input string to parse
 *	ipos	input position to start at
 */
int rosh_search_nonsp(const char *istr, const int ipos)
{
	int curpos;
	char c;

	curpos = ipos;
	c = istr[curpos];
	while (rosh_issp(c) && c != 0)
		c = istr[++curpos];
	return curpos;
}

/* Search a string for space (' '), returning the position of the next space
 * or the '\0' at end of string
 *	istr	input string to parse
 *	ipos	input position to start at
 */
int rosh_search_sp(const char *istr, const int ipos)
{
	int curpos;
	char c;

	curpos = ipos;
	c = istr[curpos];
	while (!(rosh_issp(c)) && c != 0)
		c = istr[++curpos];
	return curpos;
}

/* Parse a string for the first non-space string, returning the end position
 * from src
 *	dest	string to contain the first non-space string
 *	src	string to parse
 *	ipos	Position to start in src
 */
int rosh_parse_sp_1(char *dest, const char *src, const int ipos)
{
	int bpos, epos;	/* beginning and ending position of source string
		to copy to destination string */

	bpos = 0;
	epos = 0;
/* //HERE-error condition checking */
	bpos = rosh_search_nonsp(src, ipos);
	epos = rosh_search_sp(src, bpos);
	if (epos > bpos) {
		memcpy(dest, src + bpos, epos-bpos);
		if (dest[epos - bpos] != 0)
			dest[epos - bpos] = 0;
	} else {
		epos = strlen(src);
		dest[0] = 0;
	}
	return epos;
}

/* Handle most/all errors
 *	ierrno	Input Error number
 *	cmdstr	Command being executed to cause error
 *	filestr	File/parameter causing error
 */
void rosh_error(const int ierrno, const char *cmdstr, const char *filestr)
{
	printf("--ERROR: %s '%s': ", cmdstr, filestr);
	switch (ierrno) {
	case EACCES:	printf("Access DENIED\n");
		break;
	case ENOENT:	printf("not found\n");
		/* SYSLinux-3.72 COM32 API returns this for a
				directory or empty file */
		ROSH_COM32("  (COM32) could be a directory or empty file\n");
		break;
	case ENOTDIR:	printf("not a directory\n");
		ROSH_COM32("  (COM32) could be directory\n");
		break;
	case ENOSYS:	printf("not implemented");
		break;
	default:	printf("returns error; errno=%d\n", ierrno);
	}
}

/* Concatenate command line arguments into one string
 *	cmdstr	Output command string
 *	argc	Argument Count
 *	argv	Argument Values
 *	barg	Beginning Argument
 */
int rosh_argcat(char *cmdstr, const int argc, char *argv[], const int barg)
{
	int i, arglen, curpos;	/* index, argument length, current position
		in cmdstr */
	curpos = 0;
	cmdstr[0] = '\0';	/* Nullify string just to be sure */
	for (i = barg; i < argc; i++) {
		arglen = strlen(argv[i]);
		/* Theoretically, this should never be met in SYSLINUX */
		if ((curpos + arglen) > (ROSH_CMD_SZ - 1))
			arglen = (ROSH_CMD_SZ - 1) - curpos;
		memcpy(cmdstr + curpos, argv[i], arglen);
		curpos += arglen;
		if (curpos >= (ROSH_CMD_SZ - 1)) {
			/* Hopefully, curpos should not be greater than
				(ROSH_CMD_SZ - 1) */
			/* Still need a '\0' at the last character */
			cmdstr[(ROSH_CMD_SZ - 1)] = 0;
			break;	/* Escape out of the for() loop;
				We can no longer process anything more */
		} else {
			cmdstr[curpos] = ' ';
			curpos += 1;
			cmdstr[curpos] = 0;
		}
	}
	/* If there's a ' ' at the end, remove it.  This is normal unless
		the maximum length is met/exceeded. */
	if (cmdstr[curpos - 1] == ' ')
		cmdstr[--curpos] = 0;
	return curpos;
}	/* rosh_argcat */

/*
 * Prints a lot of the data in a struct termios
 */
/*
void rosh_print_tc(struct termios *tio)
{
	printf("  -- termios: ");
	printf(".c_iflag=%04X ", tio->c_iflag);
	printf(".c_oflag=%04X ", tio->c_oflag);
	printf(".c_cflag=%04X ", tio->c_cflag);
	printf(".c_lflag=%04X ", tio->c_lflag);
	printf(".c_cc[VTIME]='%d' ", tio->c_cc[VTIME]);
	printf(".c_cc[VMIN]='%d'", tio->c_cc[VMIN]);
	printf("\n");
}
*/

/*
 * Switches console over to raw input mode.  Allows get_key to get just
 * 1 key sequence (without delay or display)
 */
void rosh_console_raw()
{
//	struct termios itio, ntio;
//	tcgetattr(0, &itio);
//	rosh_print_tc(&itio);
/*	ntio = itio;
	ntio.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(0, TCSAFLUSH, &ntio);*/
	console_ansi_raw();	/* Allows get_key to get just 1 key sequence
		 (w/o delay or display */
//	tcgetattr(0, &ntio);
//	rosh_print_tc(&ntio);
}

/*
 * Switches back to standard getline mode.
 */
void rosh_console_std()
{
//	struct termios itio, ntio;
	console_ansi_std();
//	tcsetattr(0, TCSANOW, &itio);
}

/*
 * Attempts to get a single key from the console
 *	returns	key pressed
 */
int rosh_getkey()
{
	int inc;

	inc = KEY_NONE;
//	rosh_console_raw();
	while (inc == KEY_NONE){
		inc = get_key(stdin, 6000);
	}
//	rosh_console_std();
	return inc;
}	/* rosh_getkey */

/* Template for command functions
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial PWD
 */
void rosh_1(const char *cmdstr, const char *pwdstr, const char *ipwdstr)
{
	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\npwd: '%s'\n", cmdstr, pwdstr, \
		ipwdstr);
}	/* rosh_1 */

/* Concatenate multiple files to stdout
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 */
void rosh_cat(const char *cmdstr, const char *pwdstr)
{
	FILE *f;
	char filestr[ROSH_PATH_SZ + 1];
	char buf[ROSH_BUF_SZ];
	int numrd;
	int cmdpos;

	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	/* Initialization */
	filestr[0] = 0;
	cmdpos = 0;
	/* skip the first word */
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	while (strlen(filestr) > 0) {
		printf("--File = '%s'\n", filestr);
		f = fopen(filestr, "r");
		if (f != NULL) {
			numrd = fread(buf, 1, ROSH_BUF_SZ, f);
			while (numrd > 0) {
				fwrite(buf, 1, numrd, stdout);
				numrd = fread(buf, 1, ROSH_BUF_SZ, f);
			}
			fclose(f);
		} else {
			rosh_error(errno, "cat", filestr);
		}
		cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	}
}	/* rosh_cat */

/* Change PWD (Present Working Directory)
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial PWD
 */
void rosh_cd(const char *cmdstr, char *pwdstr, const char *ipwdstr)
{
	int rv;
	char filestr[ROSH_PATH_SZ + 1];
	int cmdpos;
	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	/* Initialization */
	filestr[0] = 0;
	cmdpos = 0;
	rv = 0;
	/* skip the first word */
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	ROSH_COM32(" -- cd (Change Directory) not implemented for use with run and exit.\n");
	if (strlen(filestr) != 0)
		rv = chdir(filestr);
	else
		rv = chdir(ipwdstr);
	if (rv != 0) {
		rosh_error(errno, "cd", filestr);
	} else {
		getcwd(pwdstr, ROSH_PATH_SZ + 1);
		printf("  %s\n", pwdstr);
	}
}	/* rosh_cd */

/* Print the syslinux config file name
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 */
void rosh_cfg(const char *cmdstr, const char *pwdstr)
{
	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	printf("CFG:     '%s'\n", syslinux_config_file());
}	/* rosh_cfg */

/* Simple directory listing for one argument (file/directory) based on
 * filestr and pwdstr
 *	ifilstr	input filename/directory name to list
 *	pwdstr	Present Working Directory string
 */
void rosh_dir_arg(const char *ifilstr, const char *pwdstr)
{
	struct stat fdstat;
	int status;
	int fd;
	char filestr[ROSH_PATH_SZ + 1];
	int filepos;
	DIR *d;
	struct dirent *de;
#ifdef DO_DEBUG
	char filestr2[ROSH_PATH_SZ + 1];
	int fd2, file2pos;
#ifdef __COM32__
//	int inchar;
	char ty;
#endif	/* __COM32__ */
#endif	/* DO_DEBUG */

	/* Initialization; make filestr based on leading character of ifilstr
		and pwdstr */
	if (ifilstr[0] == SEP) {
		strcpy(filestr, ifilstr);
	} else {
		strcpy(filestr, pwdstr);
		filepos = strlen(pwdstr);
		if (filestr[filepos-1] != SEP)
			filestr[filepos++] = SEP;
		strcpy(filestr + filepos, ifilstr);
ROSH_DEBUG("--'%s'\n", filestr);
	}
	fd = open(filestr, O_RDONLY);
	if (fd != -1) {
		status = fstat(fd, &fdstat);
		if (S_ISDIR(fdstat.st_mode)) {
			ROSH_DEBUG("PATH '%s' is a directory\n", ifilstr);
			d = fdopendir(fd);
			de = readdir(d);
			while (de != NULL) {
#ifdef DO_DEBUG
				filestr2[0] = 0;
				file2pos = strlen(filestr);
				memcpy(filestr2, filestr, file2pos);
				filestr2[file2pos] = '/';
				strcpy(filestr2+file2pos+1, de->d_name);
				fd2 = open(filestr2, O_RDONLY);
				status = fstat(fd2, &fdstat);
				printf("@%8d:%8d:", (int)de->d_ino, (int)fdstat.st_size);
				fd2 = close(fd2);
#endif	/* DO_DEBUG */
				printf("%s\n", de->d_name);
#ifdef DO_DEBUG
// inchar = fgetc(stdin);
#endif	/* DO_DEBUG */
				de = readdir(d);
			}
			closedir(d);
		} else if (S_ISREG(fdstat.st_mode)) {
			ROSH_DEBUG("PATH '%s' is a regular file\n", ifilstr);
			printf("%8d:%s\n", (int)fdstat.st_size, ifilstr);
		} else {
			ROSH_DEBUG("PATH '%s' is some other file\n", ifilstr);
			printf("        :%s\n", ifilstr);
		}
	} else {
#ifdef __COM32__
		if (filestr[strlen(filestr)-1] == SEP) {
			/* Directory */
			filepos = 0;
			d = opendir(filestr);
			if (d != NULL) {
printf("DIR:'%s'    %8d %8d\n", d->dd_name, d->dd_fd, d->dd_sect);
				de = readdir(d);
				while (de != NULL) {
					filepos++;
#ifdef DO_DEBUG
// if (strlen(de->d_name) > 25) de->d_name[25] = 0;
					switch (de->d_mode) {
					case 16 : ty = 'D';	break;
					case 32 : ty = 'F';	break;
					default : ty = '*';
					}
					printf("@%8d:%8d:%4d ", (int)de->d_ino, (int)de->d_size, de->d_mode);
#endif	/* DO_DEBUG */
//					printf("%s\n", de->d_name);
printf("'%s'\n", de->d_name);
#ifdef DO_DEBUG
// inchar = fgetc(stdin);
// fgets(instr, ROSH_CMD_SZ, stdin);
#endif	/* DO_DEBUG */
					free(de);
					de = readdir(d);
// if(filepos>15){	de = NULL;	printf("Force Break\n");}
				}
printf("Dir.dd_fd: '%8d'\n", d->dd_fd);
				closedir(d);
			} else {
				rosh_error(0, "dir:NULL", filestr);
			}
		} else {
			rosh_error(errno, "dir_c32", filestr);
		}
#else
		rosh_error(errno, "dir", filestr);
#endif	/* __COM32__ */
	}
}	/* rosh_dir_arg */

/* Simple directory listing based on cmdstr and pwdstr
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 */
void rosh_dir(const char *cmdstr, const char *pwdstr)
{
	char filestr[ROSH_PATH_SZ + 1];
	int cmdpos;	/* Position within cmdstr */

	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	/* Initialization */
	filestr[0] = 0;
	cmdpos = 0;
	/* skip the first word */
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	/* If there are no real arguments, substitute PWD */
	if (strlen(filestr) == 0)
		strcpy(filestr, pwdstr);
	while (strlen(filestr) > 0) {
		rosh_dir_arg(filestr, pwdstr);
		cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	}
}	/* rosh_dir */

/* List Directory; Calls rosh_dir() for now.
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 */
void rosh_ls(const char *cmdstr, const char *pwdstr)
{
	printf("  ls implemented as dir (for now)\n");
	rosh_dir(cmdstr, pwdstr);
}	/* rosh_ls */

/* Page through a buffer string
 *	buf	Buffer to page through
 */
void rosh_more_buf(char *buf, int buflen, int rows, int cols)
{
	char *bufp, *bufeol;	/* Pointer to current and next end-of-line
		position in buffer */
	int bufpos, bufcnt;	/* current position, count characters */
	char scrbuf[ROSH_SBUF_SZ];
	int inc;
	int i, numln;	/* Index, Number of lines */

	bufpos = 0;
	bufp = buf + bufpos;
	bufeol = bufp;
	numln = rows - 1;
printf("--(%d)\n", buflen);
// printf("--termIOS CONSTS: ");
// printf("ISIG=%08X ", ISIG);
// printf("ICANON=%08X ", ICANON);
// printf("ECHO=%08X ", ECHO);
// printf("=%08X", );
// printf("\n");
	while (bufpos < buflen) {
		for (i=0; i<numln; i++){
			bufeol = strchr(bufeol, '\n');
			if (bufeol == NULL) {
				bufeol = buf + buflen;
				i = numln;
			} else {
				bufeol++;
			}
// printf("--readln\n");
		}
		bufcnt = bufeol - bufp;
printf("--(%d/%d @%d)\n", bufcnt, buflen, bufpos);
		memcpy(scrbuf, bufp, bufcnt);
		scrbuf[bufcnt] = 0;
		printf("%s", scrbuf);
		bufp = bufeol;
		bufpos += bufcnt;
		if (bufpos == buflen)	break;
		inc = rosh_getkey();
		numln = 1;
		switch (inc){
		case KEY_CTRL('c'): case 'q': case 'Q':
			bufpos = buflen;	break;
		case ' ':
			numln = rows - 1;
//		default:
		}
	}
/*tcgetattr(0, &tio);
rosh_print_tc(&tio);
printf("\n--END\n");*/
}	/* rosh_more_buf */

/* Page through a single file using the open file stream
 *	fd	File Descriptor
 */
void rosh_more_fd(int fd, int rows, int cols)
{
	struct stat fdstat;
	int status;
	char *buf;
	int bufpos;
	int numrd;
	FILE *f;

	status = fstat(fd, &fdstat);
	if (S_ISREG(fdstat.st_mode)) {
		buf = malloc((int)fdstat.st_size);
		if (buf != NULL) {
			f = fdopen(fd, "r");
			bufpos = 0;
			numrd = fread(buf, 1, (int)fdstat.st_size, f);
			while (numrd > 0) {
				bufpos += numrd;
				numrd = fread(buf+bufpos, 1, \
					((int)fdstat.st_size - bufpos), f);
			}
			fclose(f);
			rosh_more_buf(buf, bufpos, rows, cols);
		}
	} else {
	}

}	/* rosh_more_fd */

/* Page through a file like the more command
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial PWD
 */
void rosh_more(const char *cmdstr, const char *pwdstr)
	/*, const char *ipwdstr)*/
{
	int fd;
	char filestr[ROSH_PATH_SZ + 1];
	int cmdpos;
	int rows, cols;

	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	/* Initialization */
	filestr[0] = 0;
	cmdpos = 0;
	if (getscreensize(1, &rows, &cols)) {
		ROSH_DEBUG("getscreensize() fail; fall back\n");
		ROSH_DEBUG("\tROWS='%d'\tCOLS='%d'\n", rows, cols);
		/* If either fail, go under normal size, just in case */
		if (!rows)
			rows = 20;
		if (!cols)
			cols = 75;
	}
	ROSH_DEBUG("\tROWS='%d'\tCOLS='%d'\n", rows, cols);

	/* skip the first word */
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
	if (strlen(filestr) > 0) {
		/* There is no need to mess up the console if we don't have a
			file */
		rosh_console_raw();
		while (strlen(filestr) > 0) {
			printf("--File = '%s'\n", filestr);
			fd = open(filestr, O_RDONLY);
			if (fd != -1) {
				rosh_more_fd(fd, rows, cols);
				close(fd);
			} else {
				rosh_error(errno, "more", filestr);
			}
			cmdpos = rosh_parse_sp_1(filestr, cmdstr, cmdpos);
		}
		rosh_console_std();
	}
}	/* rosh_more */

/* Page a file with rewind
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial PWD
 */
void rosh_less(const char *cmdstr, const char *pwdstr)
{
	printf("  less implemented as more (for now)\n");
	rosh_more(cmdstr, pwdstr);
}	/* rosh_less */

/* Show PWD
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 */
void rosh_pwd(const char *cmdstr, const char *pwdstr)
{
	int istr;
	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	printf("%s\n", pwdstr);
	istr = htonl(*(int*)pwdstr);
	ROSH_DEBUG("  --%08X\n", istr);
}	/* rosh_pwd */

/* Run a boot string, calling syslinux_run_command
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial PWD
 */
void rosh_run(const char *cmdstr, const char *pwdstr, const char *ipwdstr)
{
	int cmdpos;
	char *cmdptr;
	char istr[ROSH_CMD_SZ];	/* input command string */

	cmdpos = 0;
	ROSH_DEBUG("CMD: '%s'\npwd: '%s'\n", cmdstr, pwdstr);
	/* skip the first word */
	cmdpos = rosh_search_sp(cmdstr, cmdpos);
	/* skip spaces */
	cmdpos = rosh_search_nonsp(cmdstr, cmdpos);
	cmdptr = (char *)(cmdstr + cmdpos);
	printf("--run: '%s'\n", cmdptr);
	/* //HERE--Reparse if pwdstr != ipwdstr; seems a little daunting as
		detecting params vs filenames is difficult/impossible */
	if (strcmp(pwdstr, ipwdstr) != 0) {
		/* For now, just prompt for verification */
		printf("  from directory '%s'? (y/N):", pwdstr);
		fgets(istr, ROSH_CMD_SZ, stdin);
		if ((istr[0] != 'y') && (istr[0] != 'Y')) {
			printf("Aborting run\n");
			return;
		}
		printf("Run anyways\n");
	}
	syslinux_run_command(cmdptr);
}	/* rosh_run */

/* Process a single command string and call handling function
 *	cmdstr	command string to process
 *	pwdstr	Present Working Directory string
 *	ipwdstr	Initial Present Working Directory string
 *	returns	Whether to exit prompt
 */
char rosh_command(const char *cmdstr, char *pwdstr, const char *ipwdstr)
{
	char do_exit;
	do_exit = false;
	ROSH_DEBUG("--cmd:'%s'\n", cmdstr);
	switch (cmdstr[0]) {
	case 'e': case 'E': case 'q': case 'Q':
		do_exit = true;		break;
	case 'c': case 'C':	/* run 'cd' 'cat' 'cfg' */
		switch (cmdstr[1]) {
		case 'a': case 'A':
			rosh_cat(cmdstr, pwdstr);	break;
		case 'd': case 'D':
			rosh_cd(cmdstr, pwdstr, ipwdstr);	break;
		case 'f': case 'F':
			rosh_cfg(cmdstr, pwdstr);	break;
		default:	rosh_help(1);
		}
		break;
	case 'd': case 'D':	/* run 'dir' */
		rosh_dir(cmdstr, pwdstr);	break;
	case 'h': case 'H': case '?': rosh_help(2);
		break;
	case 'l': case 'L':	/* run 'ls' 'less' */
		switch (cmdstr[1]) {
		case 0: case 's': case 'S':
			rosh_ls(cmdstr, pwdstr);	break;
		case 'e': case 'E':
			rosh_less(cmdstr, pwdstr);	break;
		default:	rosh_help(1);
		}
		break;
	case 'm': case 'M':
		switch (cmdstr[1]) {
		case 'a': case 'A':
			rosh_help(2);
			break;
		case 'o': case 'O':
			rosh_more(cmdstr, pwdstr);
			break;
		default:	rosh_help(1);
		}
		break;
	case 'p': case 'P':	/* run 'pwd' */
		rosh_pwd(cmdstr, pwdstr);	break;
	case 'r': case 'R':	/* run 'run' */
		rosh_run(cmdstr, pwdstr, ipwdstr);	break;
	case 'v': case 'V':
		rosh_version();	break;
	case 0: case '\n':	break;
	default : rosh_help(1);
	}	/* switch(cmdstr[0]) */
	return do_exit;
}	/* rosh_command */

/* Process the prompt for commands as read from stdin and call rosh_command
 * to process command line string
 *	icmdstr	Initial command line string
 *	returns	Exit status
 */
int rosh_prompt(const char *icmdstr)
{
	int rv;
	char cmdstr[ROSH_CMD_SZ];
	char pwdstr[ROSH_PATH_SZ + 1], ipwdstr[ROSH_PATH_SZ + 1];
/*	int numchar;
*/	char do_exit;
	char *c;

	rv = 0;
	do_exit = false;
	strcpy(pwdstr, "/");
	getcwd(pwdstr, ROSH_PATH_SZ + 1);
	strcpy(ipwdstr, pwdstr);	/* Retain the original PWD */
	if (icmdstr[0] != '\0')
		do_exit = rosh_command(icmdstr, pwdstr, ipwdstr);
	while (!(do_exit)) {
		console_ansi_std();
		printf("\nrosh: ");
		/* Read a line from console */
		fgets(cmdstr, ROSH_CMD_SZ, stdin);
		/* remove newline from input string */
		c = strchr(cmdstr, '\n');
		*c = 0;
		do_exit = rosh_command(cmdstr, pwdstr, ipwdstr);
	}
	if (strcmp(pwdstr, ipwdstr) != 0) {
		/* Change directory to the original directory */
		strcpy(cmdstr, "cd ");
		strcpy(cmdstr + 3, ipwdstr);
		rosh_cd(cmdstr, pwdstr, ipwdstr);
	}
	return rv;
}

int main(int argc, char *argv[])
{
	int rv;
	char cmdstr[ROSH_CMD_SZ];

	/* Initialization */
	rv = 0;
	console_ansi_std();
//	console_ansi_raw();
	if (argc != 1) {
		rv = rosh_argcat(cmdstr, argc, argv, 1);
	} else {
		rosh_version();
		cmdstr[0] = '\0';
	}
	rv = rosh_prompt(cmdstr);
	printf("--Exiting '%s'\n", APP_NAME);
	console_ansi_std();
	return rv;
}
