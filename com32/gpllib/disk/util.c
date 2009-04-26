#include <com32.h>
#include <stdlib.h>
#include <string.h>
#include <disk/geom.h>

#define MAX_NB_RETRIES 6

/**
 * int13_retry - int13h with error handling
 * @inreg:	int13h function parameters
 * @outreg:	output registers
 *
 * Call int 13h, but with retry on failure.  Especially floppies need this.
 **/
int int13_retry(const com32sys_t *inreg, com32sys_t *outreg)
{
	int retry = MAX_NB_RETRIES;		/* Number of retries */
	com32sys_t tmpregs;

	if ( !outreg ) outreg = &tmpregs;

	while ( retry-- ) {
		__intcall(0x13, inreg, outreg);
		if ( !(outreg->eflags.l & EFLAGS_CF) )
			return 0;			/* CF=0 => OK */
	}

	/* If we get here: error */
	return -1;
}
