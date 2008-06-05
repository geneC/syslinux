PERL		= perl
CPSRC		= $(wildcard *.txt)
GENFILES	= $(patsubst %.txt,%.bin,$(CPSRC))

.SUFFIXES: .txt .bin

all: $(GENFILES)

tidy:

clean:
	rm -f $(GENFILES)

dist: tidy

%.bin: %.txt cptable.pl UnicodeData
	$(PERL) cptable.pl UnicodeData $< $@
