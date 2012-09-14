# _bin.c files required by both BTARGET and ITARGET installers
BINFILES = bootsect_bin.c ldlinux_bin.c \
	   mbr_bin.c gptmbr_bin.c ldlinuxc32_bin.c

PERL	 = perl

all: $(BINFILES)

bootsect_bin.c: ../core/ldlinux.bss bin2c.pl
	$(PERL) bin2c.pl syslinux_bootsect < $< > $@

ldlinux_bin.c: ../core/ldlinux.sys bin2c.pl
	$(PERL) bin2c.pl syslinux_ldlinux 512 < $< > $@

mbr_bin.c: ../mbr/mbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_mbr < $< > $@

gptmbr_bin.c: ../mbr/gptmbr.bin bin2c.pl
	$(PERL) bin2c.pl syslinux_gptmbr < $< > $@

ldlinuxc32_bin.c: ../com32/elflink/ldlinux/ldlinux.c32 bin2c.pl
	$(PERL) bin2c.pl syslinux_ldlinuxc32 < $< > $@

tidy:
	rm -f $(BINFILES)

clean: tidy

dist: tidy

spotless: clean

strip:
