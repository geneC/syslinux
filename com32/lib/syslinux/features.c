/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2007-2008 H. Peter Anvin - All Rights Reserved
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
 * ----------------------------------------------------------------------- */

/*
 * syslinux/features.c
 *
 * SYSLINUX feature flag query
 */

#include <klibc/compiler.h>
#include <syslinux/features.h>
#include <string.h>
#include <com32.h>

struct __syslinux_feature_flags __syslinux_feature_flags;

void __constructor __syslinux_detect_features(void)
{
    static com32sys_t reg;

    memset(&reg, 0, sizeof reg);
    reg.eax.w[0] = 0x0015;
    __intcall(0x22, &reg, &reg);

    __syslinux_feature_flags.len = reg.ecx.w[0];
    __syslinux_feature_flags.ptr = MK_PTR(reg.es, reg.ebx.w[0]);
}
