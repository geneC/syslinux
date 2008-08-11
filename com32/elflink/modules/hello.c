/*
 * hello.c - A simple ELF module that sorts a couple of numbers
 *
 *  Created on: Aug 11, 2008
 *      Author: Stefan Bucur <stefanb@zytor.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/module.h>

#include "sort.h"


#define NUM_COUNT		10
#define MAX_NUM			100


static int hello_init(void) {
	int *nums = NULL;
	int i;

	printf("Hello, world, from 0x%08X!\n", (unsigned int)&hello_init);

	nums = malloc(NUM_COUNT*sizeof(int));

	for (i = 0; i < NUM_COUNT; i++) {
		nums[i] = rand() % MAX_NUM;
	}

	printf("Numbers before sort: ");
	for (i = 0; i < NUM_COUNT; i++) {
		printf("%d ", nums[i]);
	}
	printf("\n");

	quick_sort(nums, NUM_COUNT);

	printf("Numbers after sort: ");
	for (i = 0; i < NUM_COUNT; i++) {
		printf("%d ", nums[i]);
	}
	printf("\n");

	free(nums);

	return 0;
}

static void hello_exit(void) {
	printf("Good bye, cruel world!\n");
}

MODULE_INIT(hello_init);
MODULE_EXIT(hello_exit);
