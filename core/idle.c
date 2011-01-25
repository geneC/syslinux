/* -*- fundamental -*- ---------------------------------------------------
 *
 *   Copyright 2008 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 *   Boston MA 02110-1301, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

/*
 * idle.c:
 *
 * This function provided protected-mode access to the idle handling.
 * It needs to be carefully coordinated with idle.inc, which provides
 * idle services to real-mode code.
 */

#include "core.h"
#include <sys/cpu.h>

#define TICKS_TO_IDLE	4	/* Also in idle.inc */

extern uint32_t _IdleTimer;
extern uint16_t NoHalt;

int (*idle_hook_func)(void);

void reset_idle(void)
{
    _IdleTimer = jiffies();
}

void __idle(void)
{
    if (jiffies() - _IdleTimer < TICKS_TO_IDLE)
	return;

    if (idle_hook_func && idle_hook_func())
	return;			/* Nonzero return = do not idle */

    if (NoHalt)
	cpu_relax();
    else
	hlt();
}
