/* -----------------------------------------------------------------------
 *
 *   Copyright 1994-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009-2010 Intel Corporation; author: H. Peter Anvin
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#include <inttypes.h>
#include <com32.h>
#include <syslinux/pmapi.h>
#include "core.h"
#include "fs.h"

const struct com32_pmapi pm_api_vector =
{
    .read_file	= pmapi_read_file,
};
