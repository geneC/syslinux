CFLAGS = -g -I$(topdir)/tests/unittest/include

tests = meminit
.INTERMEDIATE: $(tests)

all: banner $(tests)
	for t in $(tests); \
		do printf "      [+] $$t passed\n" ; ./$$t ; done

banner:
	printf "    Running memory subsystem unit tests...\n"

meminit: meminit.c ../init.c

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

