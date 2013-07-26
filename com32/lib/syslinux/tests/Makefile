CFLAGS = -I$(topdir)/tests/unittest/include

tests = zonelist movebits memscan load_linux
.INTERMEDIATE: $(tests)

all: banner $(tests)
	for t in $(tests); \
		do printf "      [+] $$t passed\n" ; ./$$t ; done
banner:
	printf "    Running library unit tests...\n"

harness-files = test-harness.c

zonelist: zonelist.c ../zonelist.c $(harness-files)
movebits: movebits.c ../movebits.c $(harness-files)
memscan: memscan.c ../memscan.c
load_linux: load_linux.c

%: %.c
	$(CC) $(CFLAGS) -o $@ $<


