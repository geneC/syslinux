#include <stdio.h>
#include <stdlib.h>
#include <console.h>

int main(int argc, char *argv[])
{
    FILE *f;
    int ch;
    int i;

    openconsole(&dev_stdcon_r, &dev_stdcon_w);

    printf("argv = %p\n", argv);
    for (i = 0; i <= argc; i++)
	printf("argv[%d] = %p = \"%s\"\n", i, argv[i], argv[i]);

    if (argc < 2) {
	fprintf(stderr, "Usage: cat.c32 filename\n");
	return 1;
    }

    printf("File = %s\n", argv[1]);

    f = fopen(argv[1], "r");
    if (!f) {
	fprintf(stderr, "File name \"%s\" does not exist.\n", argv[1]);
	return 1;
    }

    while ((ch = getc(f)) != EOF)
	putchar(ch);

    fclose(f);

    return 0;
}
