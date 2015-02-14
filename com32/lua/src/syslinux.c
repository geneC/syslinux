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

#include <getkey.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslinux/boot.h>

#define lnetlib_c		/* Define the library */

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "syslinux/boot.h"
#include "syslinux/loadfile.h"
#include "syslinux/linux.h"
#include "syslinux/config.h"
#include "syslinux/reboot.h"

int __parse_argv(char ***argv, const char *str);

static const char SYSLINUX_FILE[] = "syslinux_file";
static const char SYSLINUX_INITRAMFS[] = "syslinux_initramfs";

typedef struct syslinux_file {
    void *data;
    char *name;
    size_t size;
} syslinux_file;

/*
 * Most code taken from:
 *	com32/modules/linux.c
 */

/* Find the last instance of a particular command line argument
   (which should include the final =; do not use for boolean arguments) */
static char *find_argument(char **argv, const char *argument)
{
    int la = strlen(argument);
    char **arg;
    char *ptr = NULL;

    for (arg = argv; *arg; arg++) {
	if (!memcmp(*arg, argument, la))
	    ptr = *arg + la;
    }

    return ptr;
}

/* Stitch together the command line from a set of argv's */
static char *make_cmdline(char **argv)
{
    char **arg;
    size_t bytes;
    char *cmdline, *p;

    bytes = 1;			/* Just in case we have a zero-entry cmdline */
    for (arg = argv; *arg; arg++) {
	bytes += strlen(*arg) + 1;
    }

    p = cmdline = malloc(bytes);
    if (!cmdline)
	return NULL;

    for (arg = argv; *arg; arg++) {
	int len = strlen(*arg);
	memcpy(p, *arg, len);
	p[len] = ' ';
	p += len + 1;
    }

    if (p > cmdline)
	p--;			/* Remove the last space */
    *p = '\0';

    return cmdline;
}

static int sl_run_command(lua_State * L)
{
    const char *cmd = luaL_checkstring(L, 1);	/* Reads the string parameter */
    syslinux_run_command(cmd);
    return 0;
}

/* do default boot */
static int sl_run_default(lua_State * L)
{
    /* Preventing GCC to complain against unused L */
    L=L;
    syslinux_run_default();
    return 0;
}

/* do local boot */
static int sl_local_boot(lua_State * L)
{
    uint16_t flags = luaL_checkint(L, 1);
    syslinux_local_boot(flags);
    return 0;
}

static int sl_final_cleanup(lua_State * L)
{
    syslinux_final_cleanup (luaL_checkint (L, 1));
    return 0;
}

/* boot linux kernel and initrd */
static int sl_boot_linux(lua_State * L)
{
    const char *kernel = luaL_checkstring(L, 1);
    const char *cmdline = luaL_optstring(L, 2, "");
    char *initrd;
    void *kernel_data;
    size_t kernel_len;
    struct initramfs *initramfs;
    char *newcmdline;
    int ret;
    char **argv, **argp, *arg, *p;

    ret = __parse_argv(&argv, cmdline);

    newcmdline = malloc(strlen(kernel) + 12);
    if (!newcmdline)
	printf("Mem alloc failed: cmdline\n");

    strcpy(newcmdline, "BOOT_IMAGE=");
    strcpy(newcmdline + strlen(newcmdline), kernel);
    argv[0] = newcmdline;
    argp = argv;

    /* DEBUG
       for (i=0; i<ret; i++)
       printf("%d: %s\n", i, argv[i]);
     */

    newcmdline = make_cmdline(argp);
    if (!newcmdline)
	printf("Creating command line failed!\n");

    /* DEBUG
       printf("Command line: %s\n", newcmdline);
       msleep(1000);
     */

    printf("Loading kernel %s... ", kernel);
    if (loadfile(kernel, &kernel_data, &kernel_len))
	printf("failed!\n");
    else
	printf("ok\n");

    initramfs = initramfs_init();
    if (!initramfs)
	printf("Initializing initrd failed!\n");

    if ((arg = find_argument(argp, "initrd="))) {
	do {
	    p = strchr(arg, ',');
	    if (p)
		*p = '\0';

	    initrd = arg;
	    printf("Loading initrd %s... ", initrd);
	    if (initramfs_load_archive(initramfs, initrd))
		printf("failed!\n");
	    else
		printf("ok\n");

	    if (p)
		*p++ = ',';
	} while ((arg = p));
    }

    ret = syslinux_boot_linux(kernel_data, kernel_len, initramfs, NULL, newcmdline);

    printf("syslinux_boot_linux returned %d\n", ret);

    return 0;
}

/* sleep for sec seconds */
static int sl_sleep(lua_State * L)
{
    unsigned int sec = luaL_checkint(L, 1);
    sleep(sec);
    return 0;
}

/* sleep for msec milliseconds */
static int sl_msleep(lua_State * L)
{
    unsigned int msec = luaL_checkint(L, 1);
    msleep(msec);
    return 0;
}

static int sl_run_kernel_image(lua_State * L)
{
    const char *filename = luaL_checkstring(L, 1);
    const char *cmdline = luaL_checkstring(L, 2);
    uint32_t ipappend_flags = luaL_checkint(L, 3);
    uint32_t type = luaL_checkint(L, 4);

    syslinux_run_kernel_image(filename, cmdline, ipappend_flags, type);
    return 0;
}

static int sl_loadfile(lua_State * L)
{
    size_t name_len;
    const char *filename = luaL_checklstring (L, 1, &name_len);
    syslinux_file *file = lua_newuserdata (L, sizeof (syslinux_file));

    file->name = malloc (name_len+1);
    if (!file->name) return luaL_error (L, "Out of memory");
    memcpy (file->name, filename, name_len+1);
    if (loadfile (file->name, &file->data, &file->size)) {
        free (file->name);
        return luaL_error (L, "Could not load file");
    }
    luaL_setmetatable (L, SYSLINUX_FILE);
    return 1;
}

static int sl_unloadfile (lua_State *L)
{
    syslinux_file *file = luaL_checkudata (L, 1, SYSLINUX_FILE);

    free (file->name);
    free (file->data);
    /* the __gc method may also be (repeatedly) called before garbage collection, so: */
    file->name = file->data = NULL;
    return 0;
}

static int sl_filesize(lua_State * L)
{
    const syslinux_file *file = luaL_checkudata(L, 1, SYSLINUX_FILE);

    lua_pushinteger(L, file->size);

    return 1;
}

static int sl_filename(lua_State * L)
{
    const syslinux_file *file = luaL_checkudata(L, 1, SYSLINUX_FILE);

    lua_pushstring(L, file->name);

    return 1;
}

static int sl_initramfs_init(lua_State * L)
{
    struct initramfs *ir = lua_newuserdata (L, sizeof (*ir));

    memset (ir, 0, sizeof (*ir)); /* adapted from initramfs_init() */
    ir->prev = ir->next = ir;
    luaL_setmetatable (L, SYSLINUX_INITRAMFS);
    return 1;
}

static int sl_initramfs_load_archive(lua_State * L)
{
    const char *filename = luaL_checkstring(L, 2);

    if (initramfs_load_archive (luaL_checkudata(L, 1, SYSLINUX_INITRAMFS), filename))
        return luaL_error (L, "Loading initramfs %s failed", filename);
    lua_settop (L, 1);
    return 1;
}

static int sl_initramfs_add_file(lua_State * L)
{
    const char *filename = luaL_checkstring(L, 2);
    size_t file_len;
    const char *file_data = luaL_optlstring (L, 3, NULL, &file_len);
    void *data = NULL;

    if (file_len) {
        data = malloc (file_len);
        if (!data) return luaL_error (L, "Out of memory");
        memcpy (data, file_data, file_len);
    }
    if (initramfs_add_file(luaL_checkudata(L, 1, SYSLINUX_INITRAMFS),
                           data, file_len, file_len, filename,
                           luaL_optint (L, 4, 0), luaL_optint (L, 5, 0755)))
        return luaL_error (L, "Adding file %s to initramfs failed", filename);
    lua_settop (L, 1);
    return 1;
}

static int sl_initramfs_size (lua_State *L)
{
    lua_pushinteger (L, initramfs_size (luaL_checkudata(L, 1, SYSLINUX_INITRAMFS)));
    return 1;
}

static int sl_initramfs_purge (lua_State *L)
{
    struct initramfs *ir = luaL_checkudata(L, 1, SYSLINUX_INITRAMFS);

    ir = ir->next;
    while (ir->len) {
        free ((void *)ir->data);
        ir = ir->next;
        free (ir->prev);
    }
    /* the __gc method may also be (repeatedly) called before garbage collection, so: */
    ir->next = ir->prev = ir;
    return 0;
}

static int sl_boot_it(lua_State * L)
{
    const syslinux_file *kernel = luaL_checkudata(L, 1, SYSLINUX_FILE);
    struct initramfs *ir = luaL_testudata(L, 2, SYSLINUX_INITRAMFS);
    size_t len;
    const char *cmdline_param = luaL_optlstring(L, 3, "", &len);
    char *cmdline = malloc (len+1); /* syslinux_boot_linux needs non-const cmdline */
    int err;

    if (!cmdline) return luaL_error (L, "Out of memory");
    memcpy (cmdline, cmdline_param, len+1);
    err = syslinux_boot_linux (kernel->data, kernel->size, ir, NULL, cmdline);
    free (cmdline);
    if (err) return luaL_error (L, "Booting failed");
    return 0; /* unexpected */
}

static int sl_config_file(lua_State * L)
{
    const char *config_file = syslinux_config_file();
    lua_pushstring(L, config_file);
    return 1;
}

static int sl_reboot(lua_State * L)
{
    int warm_boot = luaL_optint(L, 1, 0);
    /* explicitly convert it to 1 or 0 */
    warm_boot = warm_boot? 1 : 0;
    syslinux_reboot(warm_boot);
    return 0;
}

static int sl_ipappend_strs(lua_State * L)
{
    int i;
    const struct syslinux_ipappend_strings *ip_strs = syslinux_ipappend_strings();
    lua_newtable(L);
    for (i = 0; i < ip_strs->count; i++) {
        lua_pushinteger(L, i + 1);
        lua_pushstring(L, ip_strs->ptr[i]);
        lua_settable(L,-3);
    }
    return 1;
}

static int sl_derivative(lua_State * L)
{
    const struct syslinux_version *sv;

    sv = syslinux_version();

    switch (sv->filesystem) {
    case SYSLINUX_FS_SYSLINUX:
	lua_pushstring(L, "SYSLINUX");
	break;
    case SYSLINUX_FS_PXELINUX:
	lua_pushstring(L, "PXELINUX");
	break;
    case SYSLINUX_FS_ISOLINUX:
	lua_pushstring(L, "ISOLINUX");
	break;
    case SYSLINUX_FS_UNKNOWN:
    default:
	lua_pushstring(L, "Unknown Syslinux derivative");
	break;
    }

    return 1;
}

static int sl_version(lua_State * L)
{
    const struct syslinux_version *sv;

    sv = syslinux_version();
    lua_pushstring(L, sv->version_string);

    return 1;
}

static int sl_get_key (lua_State * L)
{
    int timeout = luaL_checkint (L, 1);
    lua_pushinteger (L, get_key (stdin, timeout));
    return 1;
}

static int sl_KEY_CTRL (lua_State * L)
{
    lua_pushinteger (L, KEY_CTRL (luaL_checkint (L, 1)));
    return 1;
}

static const luaL_Reg syslinuxlib[] = {
    {"run_command", sl_run_command},
    {"run_default", sl_run_default},
    {"local_boot", sl_local_boot},
    {"final_cleanup", sl_final_cleanup},
    {"boot_linux", sl_boot_linux},
    {"run_kernel_image", sl_run_kernel_image},
    {"sleep", sl_sleep},
    {"msleep", sl_msleep},
    {"loadfile", sl_loadfile},
    {"initramfs", sl_initramfs_init},
    {"boot_it", sl_boot_it},
    {"config_file", sl_config_file},
    {"ipappend_strs", sl_ipappend_strs},
    {"reboot", sl_reboot},
    {"derivative", sl_derivative},
    {"version", sl_version},
    {"get_key", sl_get_key},
    {"KEY_CTRL", sl_KEY_CTRL},
    {NULL, NULL}
};

static const luaL_Reg file_methods[] = {
    {"__gc", sl_unloadfile},
    {"name", sl_filename},
    {"size", sl_filesize},
    {NULL, NULL}
};

static const luaL_Reg initramfs_methods[] = {
    {"__gc", sl_initramfs_purge},
    {"load", sl_initramfs_load_archive},
    {"add_file", sl_initramfs_add_file},
    {"size", sl_initramfs_size},
    {NULL, NULL}
};

/* This defines a function that opens up your library. */

LUALIB_API int luaopen_syslinux(lua_State * L)
{

    luaL_newmetatable(L, SYSLINUX_FILE);
    lua_pushstring (L, "__index");
    lua_pushvalue (L, -2);
    lua_settable (L, -3);
    luaL_setfuncs (L, file_methods, 0);

    luaL_newmetatable (L, SYSLINUX_INITRAMFS);
    lua_pushstring (L, "__index");
    lua_pushvalue (L, -2);
    lua_settable (L, -3);
    luaL_setfuncs (L, initramfs_methods, 0);

    luaL_newlib(L, syslinuxlib);

#define export(c,x) lua_pushinteger (L,c##_##x); lua_setfield (L, -2, #x);

    lua_newtable (L);
    export (KEY, NONE);
    export (KEY, BACKSPACE);
    export (KEY, TAB);
    export (KEY, ENTER);
    export (KEY, ESC);
    export (KEY, DEL);
    export (KEY, F1);
    export (KEY, F2);
    export (KEY, F3);
    export (KEY, F4);
    export (KEY, F5);
    export (KEY, F6);
    export (KEY, F7);
    export (KEY, F8);
    export (KEY, F9);
    export (KEY, F10);
    export (KEY, F11);
    export (KEY, F12);
    export (KEY, UP);
    export (KEY, DOWN);
    export (KEY, LEFT);
    export (KEY, RIGHT);
    export (KEY, PGUP);
    export (KEY, PGDN);
    export (KEY, HOME);
    export (KEY, END);
    export (KEY, INSERT);
    export (KEY, DELETE);
    lua_setfield (L, -2, "KEY");

    lua_newtable (L);
    export (IMAGE_TYPE, KERNEL);
    export (IMAGE_TYPE, LINUX);
    export (IMAGE_TYPE, BOOT);
    export (IMAGE_TYPE, BSS);
    export (IMAGE_TYPE, PXE);
    export (IMAGE_TYPE, FDIMAGE);
    export (IMAGE_TYPE, COM32);
    export (IMAGE_TYPE, CONFIG);
    export (IMAGE_TYPE, LOCALBOOT);
    lua_setfield (L, -2, "IMAGE_TYPE");

    return 1;
}
