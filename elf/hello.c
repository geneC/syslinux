// A simple Hello World ELF module

// TODO: Define some macros that would put the initialization and termination
// functions in a separate section (suggestion: .init and .fini)

// Undefined symbol
extern int undef_symbol;

// Undefined function
extern void undef_func(int param);

int hello_init() {
	undef_symbol++;
	
	return 0;
}

void hello_exit() {
	undef_func(undef_symbol);
}
