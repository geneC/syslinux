
int undef_symbol;

void undef_func(int param) {
	undef_symbol = 100;
}

static int hello_init(void) {
	// Do nothing
	
	return 0;
}

static void hello_exit(void) {
	// Do nothing
}
