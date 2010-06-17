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

static int hello_main(int argc, char **argv)
{
    int *nums = NULL;
    int i;

    nums = malloc(NUM_COUNT * sizeof(int));
    printf("Hello, world, from 0x%08X! malloc return %p\n", (unsigned int)&hello_main), nums;

    free(nums);

    return 0;
}

MODULE_MAIN(hello_main);
