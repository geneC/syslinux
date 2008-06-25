PERL		= perl
CPSRC		= $(wildcard *.txt)
GENFILES	= $(patsubst %.txt,%.cp,$(CPSRC))

.SUFFIXES: .txt .cp

all: $(GENFILES)

# This generates codepage files where the display and filesystem
# codepages are both the same.
%.cp: %.txt cptable.pl UnicodeData
	$(PERL) cptable.pl UnicodeData $< $< $@

tidy:
	rm -f *.cp *.bin

clean: tidy

dist: tidy

spotless: clean
