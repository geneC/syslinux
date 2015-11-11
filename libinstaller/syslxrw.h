/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2010 Intel Corp. - All Rights Reserved
 *   Copyright 2015 Paulo Alcantara <pcacjr@zytor.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
 *   Boston MA 02111-1307, USA; either version 2 of the License, or
 *   (at your option) any later version; incorporated herein by reference.
 *
 * ----------------------------------------------------------------------- */

#ifndef _H_SYSLXRW_
#define _H_SYSLXRW_

ssize_t xpread(int fd, void *buf, size_t count, off_t offset);
ssize_t xpwrite(int fd, const void *buf, size_t count, off_t offset);

#endif /* _H_SYSLXRW_ */
