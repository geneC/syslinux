## License would go here

########
# Tools

CC = gcc

RM = rm -f

################
# Build options

CFLAGS = -Wall -DELF_USERSPACE_TEST

LDFLAGS = 

##################
# Generated files

# Test executable name
TESTPROG = elftest

# Test module name
TESTMODULE = hello.so

###############
# Make targets
.PHONY: all test-prog test-module clean

all: test-prog test-module 


# The testing user-space application
test-prog: $(TESTPROG)

$(TESTPROG): elftest.o elf_module.o elf_utils.o
	$(CC) -o $@ $^

	
# The shared module to test on
test-module: $(TESTMODULE)

$(TESTMODULE): hello.o
	$(CC) -shared -o $@ $^


# Cleanup target
clean:
	-$(RM) *.o
	-$(RM) $(TESTPROG)
	-$(RM) $(TESTMODULE)
	
