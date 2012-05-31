#include <stdlib.h>
#include <syslinux/linux.h>
#include <syslinux/loadfile.h>

struct fdt *fdt_init(void)
{
	struct fdt *fdt;

	fdt = calloc(1, sizeof(*fdt));
	if (!fdt)
		return NULL;

	return fdt;
}

int fdt_load(struct fdt *fdt, const char *filename)
{
	void *data;
	size_t len;

	if (loadfile(filename, &data, &len))
		return -1;

	fdt->data = data;
	fdt->len = len;

	return 0;
}
