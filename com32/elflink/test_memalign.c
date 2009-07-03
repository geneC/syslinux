#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <errno.h>

void perform_allocation(int align) {
	int res = 0;
	int size = 100;
	void *ptr;

	printf("Allocation aligned at 0x%05X bytes: ", align);
	res = posix_memalign(&ptr, align, size);

	switch (res) {
	case 0:
		printf("address 0x%08X\n", ptr);
		break;
	case EINVAL:
		printf("EINVAL\n");
		break;
	case ENOMEM:
		printf("ENOMEM\n");
		break;
	}
}

int main(int argc, char **argv) {
	int align = 0x10000;

	// Open a standard r/w console
	openconsole(&dev_stdcon_r, &dev_stdcon_w);

	while (align >= sizeof(void*)) {
		perform_allocation(align);
		align /= 2;
	}

	printf("\n");

	while (align <= 0x10000) {
		perform_allocation(align);
		align *= 2;
	}

	return 0;
}

