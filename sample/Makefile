#ident "$Id$"
## -----------------------------------------------------------------------
##   
##   Copyright 2001 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
##   USA; either version 2 of the License, or (at your option) any later
##   version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## samples for syslinux users
##

PPMTOLSS16 = 	../ppmtolss16

all: syslogo.lss

syslogo.lss:	syslogo.png $(PPMTOLSS16)
	pngtopnm syslogo.png | \
		$(PPMTOLSS16) \#000000=0 \#d0d0d0=7 \#f6f6f6=15	\
		> syslogo.lss

clean:
	rm -f *.lss
