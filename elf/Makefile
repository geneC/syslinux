## License would go here

# Tools

CC = gcc

RM = rm -f


# Build options

CFLAGS = -Wall

LDFLAGS = 


# Generated files

# Test executable name
TESTPROG = elftest


# Make targets
.PHONY: all test clean

all: test

test: $(TESTPROG)

$(TESTPROG): elftest.o
	$(CC) -o $@ $^


clean:
	-$(RM) *.o
	-$(RM) $(TESTPROG)
	
