/*
 * readdir.c
 */

#include <dirent.h>
#include <stdio.h>
#include <errno.h>

#include <com32.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

struct dirent *readdir(DIR *dir)
{
	struct dirent *newde;
	com32sys_t regs;

	newde = NULL;
	if ((dir != NULL) && (dir->dd_fd != 0) && (dir->dd_stat >= 0)) {
		memset(__com32.cs_bounce, 0, 32);
		memset(&regs, 0, sizeof(regs));

		regs.eax.w[0] = 0x0021;
		regs.esi.w[0] = dir->dd_fd;
		regs.edi.w[0] = OFFS(__com32.cs_bounce);
		regs.es = SEG(__com32.cs_bounce);

		__com32.cs_intcall(0x22, &regs, &regs);

		/* Don't do this as we won't be able to rewind.
		dir->dd_fd = regs.esi.w[0];	/* Shouldn't be needed? */
		if ((!(regs.eflags.l & EFLAGS_CF)) && (regs.esi.w[0] != 0)) {
			newde = calloc(1, sizeof(newde));
			if (newde != NULL) {
				strcpy(newde->d_name, __com32.cs_bounce);
				newde->d_mode = regs.edx.b[0];
				newde->d_size = regs.eax.l;
				newde->d_ino = regs.ebx.l;
				dir->dd_stat = 1;
			} else {
				dir->dd_stat = -2;
				errno = ENOMEM;
			}
		} else {
			dir->dd_stat = -1;
			errno = EIO;	/* Is this the right nmber? */
		}
	} else {
		errno = EBADF;
	}

	return newde;
}
