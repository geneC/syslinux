/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2009 Pierre-Alexandre Meyer - All Rights Reserved
 *
 *   Permission is hereby granted, free of charge, to any person
 *   obtaining a copy of this software and associated documentation
 *   files (the "Software"), to deal in the Software without
 *   restriction, including without limitation the rights to use,
 *   copy, modify, merge, publish, distribute, sublicense, and/or
 *   sell copies of the Software, and to permit persons to whom
 *   the Software is furnished to do so, subject to the following
 *   conditions:
 *
 *   The above copyright notice and this permission notice shall
 *   be included in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *   OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *   NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *   HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *   FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 *   OTHER DEALINGS IN THE SOFTWARE.
 *
 * -----------------------------------------------------------------------
 */

#include <stdio.h>

void sectors_to_size(int sectors, char *buffer)
{
	int b = (sectors / 2);
	int mib = b >> 10;
	int gib = mib >> 10;
	int tib = gib >> 10;

	if (tib > 0)
		sprintf(buffer, "%3d TiB", tib);
	else if (gib > 0)
		sprintf(buffer, "%3d GiB", gib);
	else if (mib > 0)
		sprintf(buffer, "%3d MiB", mib);
	else
		sprintf(buffer, "%d b", b);
}

void sectors_to_size_dec(char *previous_unit, int *previous_size, char *unit, int *size, int sectors)
{
	*size = sectors / 2; // Converting to bytes
	strlcpy(unit, "KB", 2);
	strlcpy(previous_unit, unit, 2);
	*previous_size = *size;
	if (*size > 1000) {
		*size = *size / 1000;
		strlcpy(unit, "MB", 2);
		if (*size > 1000) {
			*previous_size = *size;
			*size = *size / 1000;
			strlcpy(previous_unit, unit, 2);
			strlcpy(unit, "GB", 2);
			if (*size > 1000) {
				*previous_size = *size;
				*size = *size / 1000;
				strlcpy(previous_unit, unit, 2);
				strlcpy(unit, "TB", 2);
			}
		}
	}
}
