#include <stdio.h>
#include <stdlib.h>
#include <console.h>

int main(int argc, char *argv[])
{
    FILE *f;
    int ch;

    openconsole(&dev_stdcon_r, &dev_stdcon_w);

    if (argc < 2) {
	fprintf(stderr, "Usage: cat.c32 filename\n");
	return 1;
    }

    f = fopen(argv[1], "r");
    if (!f) {
	fprintf(stderr, "File \"%s\" does not exist.\n", argv[1]);
	return 1;
    }

    while ((ch = getc(f)) != EOF)
	putchar(ch);

    fclose(f);

    return 0;
}
