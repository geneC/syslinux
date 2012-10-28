## -----------------------------------------------------------------------
##
##   Copyright 2012 Gene Cumm
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## AsciiDoc documentation for syslinux
##

topdir = ..
MAKEDIR = $(topdir)/mk
# include $(MAKEDIR)/embedded.mk

A2X_OPTS	 = -k
# A2X_OPTS	+= -v
A2X_MAN_OPTS	 = -D man

DOCS1 		 = syslinux.txt
DOCS5		 = syslxcfg.txt
HTML_DOCS	:= $(patsubst %.txt,html/%.html,$(DOCS1) $(DOCS5)) 
XHTML_DOCS	:= $(patsubst %.txt,%.html,$(DOCS1) $(DOCS5))
MAN_DOCS	:= $(patsubst %.txt,man/%.1,$(DOCS1)) $(patsubst %.txt,man/%.5,$(DOCS5))
TEXT_DOCS	:= $(patsubst %.txt,%.text,$(DOCS1) $(DOCS5))
PDF_DOCS	:= $(patsubst %.txt,%.pdf,$(DOCS1) $(DOCS5))


all:	$(HTML_DOCS) $(MAN_DOCS) $(XHTML_DOCS) $(TEXT_DOCS)

# During 'make all', *.xml is kept but deleted at the end; do we _really_
#	need the XML longer?
.PRECIOUS:	%.xml

# %.html:	%.txt
# 	asciidoc -D html $< 

html/%.html:	%.txt
	asciidoc -o $@ $< 

# As of AsciiDoc-8.5.2, altering the output filename for a2x does not appear possible
# xhtml/%.html:	%.txt
# 	a2x $(A2X_OPTS) -D xhtml -f xhtml $<
# text/%.text:	%.xml %.txt
# 	a2x $(A2X_OPTS) -D text -f text $<

%.xml:	%.txt
	a2x $(A2X_OPTS) -f docbook $<

%.html:	%.xml %.txt
	a2x $(A2X_OPTS) -f xhtml $<

man/%.1:	%.txt
	a2x $(A2X_MAN_OPTS) -f manpage $<

# %.5:	%.xml %.txt
# 	a2x $(A2X_OPTS) -f manpage $<

man/syslinux.cfg.5:	syslxcfg.txt
	a2x $(A2X_MAN_OPTS) -f manpage $<

man/syslxcfg.5:	man/syslinux.cfg.5

%.text:	%.xml %.txt
	a2x $(A2X_OPTS) -f text $<

%.pdf:	%.xml %.txt
	a2x $(A2X_OPTS) -f pdf $<

tidy dist:
	rm -f *~ *.xml *.text.html text/*.text.html text/*.xml xhtml/*.xml

clean: tidy

spotless: clean
	rm -f *.1 *.5 *.css *.html *.text html/*.html text/*.text xhtml/*.html xhtml/*.css

-include .*.d
