PERL		= perl
CPSRC		= $(wildcard *.txt)
GENFILES	= $(patsubst %.txt,%.bin,$(CPSRC))

.SUFFIXES: .txt .bin

all: $(GENFILES)

%.bin: %.txt cptable.pl UnicodeData
	$(PERL) cptable.pl UnicodeData $< $@

tidy:
	rm -f $(GENFILES)

clean: tidy

dist: tidy

spotless: clean
