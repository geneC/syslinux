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

A2X_OPTS	 = -k -v
XSLTPROC_OPTS	 = --nonet --stringparam callout.graphics 1 --stringparam navig.graphics 0 --stringparam admon.textlabel 0 --stringparam admon.graphics 1 --stringparam admon.graphics.path /etc/asciidoc/images/icons// --stringparam callout.graphics.path /etc/asciidoc/images/icons//callouts/ --stringparam navig.graphics.path /etc/asciidoc/images/icons//

DOCS1 		 = syslinux.txt
DOCS5		 = syslxcfg.txt
HTML_DOCS	:= $(patsubst %.txt,html/%.html,$(DOCS1) $(DOCS5)) 
XHTML_DOCS	:= $(patsubst %.txt,%.html,$(DOCS1) $(DOCS5))
MAN_DOCS	:= $(patsubst %.txt,%.1,$(DOCS1)) $(patsubst %.txt,%.5,$(DOCS5))
TEXT_DOCS	:= $(patsubst %.txt,%.text,$(DOCS1) $(DOCS5))


# all: syslinux.html xhtml/syslinux.html syslinux.1 text/syslinux.text
all: $(HTML_DOCS) $(XHTML_DOCS) $(MAN_DOCS) $(TEXT_DOCS)

# %.html:	%.txt
# 	asciidoc -D html $< 

html/%.html:	%.txt
	asciidoc -o $@ $< 

# As of AsciiDoc-8.5.2, altering the output filename does not appear possible
xhtml/%.html:	%.txt
	a2x $(A2X_OPTS) -f xhtml $<

%.xml:	%.txt
	asciidoc --backend docbook  --doctype article --verbose --out-file $@ $<
	xmllint --nonet --noout --valid $@

%.html:	%.xml
# 	a2x $(A2X_OPTS) -f xhtml $<
	xsltproc $(XSLTPROC_OPTS) --output $@ /etc/asciidoc/docbook-xsl/xhtml.xsl $<


# %.1: %.txt
# 	a2x $(A2X_OPTS) -f manpage $<
%.1:	%.xml
	xsltproc $(XSLTPROC_OPTS) /etc/asciidoc/docbook-xsl/manpage.xsl $<

# %.5: %.txt
# 	a2x $(A2X_OPTS) -f manpage $<
%.5:	%.xml
	xsltproc $(XSLTPROC_OPTS) /etc/asciidoc/docbook-xsl/manpage.xsl $<

text/%.text:	%.txt
	a2x $(A2X_OPTS) -f text $<

%.text:	%.xml
# 	a2x $(A2X_OPTS) -f text $<
	xsltproc $(XSLTPROC_OPTS) --output $@.html /etc/asciidoc/docbook-xsl/text.xsl $<
	w3m -cols 70 -dump -T text/html -no-graph $@.html > $@


tidy dist:
	rm -f *.xml *.text.html text/*.text.html

clean: tidy

spotless: clean
	rm -f *.1 *.5 *.html *.text html/*.html text/*.text xhtml/*.html *.css

-include .*.d
