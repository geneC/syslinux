// A simple Hello World ELF module

// TODO: Define some macros that would put the initialization and termination
// functions in a separate section (suggestion: .init and .fini)

// Undefined symbol
extern int undef_symbol;

int exported_symbol;

// Undefined function
extern void undef_func(int param);

int test_func(void) {
	return undef_symbol++;
}

static int hello_init(void) {
	undef_symbol++;
	
	return 0;
}

static void hello_exit(void) {
	undef_func(undef_symbol);
}
