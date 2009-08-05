/* ----------------------------------------------------------------------- *
 *
 *   Copyright 2008-2009 H. Peter Anvin - All Rights Reserved
 *   Copyright 2009 Intel Corporation; author: H. Peter Anvin
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

#include <syslinux/config.h>
#include <klibc/compiler.h>
#include <com32.h>

union syslinux_derivative_info __syslinux_derivative_info;

void __constructor __syslinux_get_derivative_info(void)
{
    com32sys_t *const r = &__syslinux_derivative_info.rr.r;

    r->eax.w[0] = 0x000A;
    __intcall(0x22, r, r);

    __syslinux_derivative_info.r.esbx = MK_PTR(r->es, r->ebx.w[0]);
    __syslinux_derivative_info.r.fssi = MK_PTR(r->fs, r->esi.w[0]);
    __syslinux_derivative_info.r.gsdi = MK_PTR(r->gs, r->edi.w[0]);
}
