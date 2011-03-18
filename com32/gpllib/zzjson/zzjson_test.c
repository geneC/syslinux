/* crude test program, used for testing during development
 * ZZJSON - Copyright (C) 2008 by Ivo van Poorten
 * License: GNU General Public License version 2 or later
 */

#include "zzjson.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static void myerror(void *ehandle, const char *format, ...) {
    va_list ap;
    fprintf(ehandle, "error: ");
    va_start(ap, format);
    vfprintf(ehandle, format, ap);
    va_end(ap);
    fputc('\n', ehandle);
}

int main(int argc, char **argv) {
    ZZJSON *zzjson, *tmp;
    ZZJSON_CONFIG config = { ZZJSON_VERY_STRICT, NULL,
                             (int(*)(void*)) fgetc,
                             (int(*)(int,void*)) ungetc,
                             malloc, calloc, free, realloc,
                             stderr, myerror, stdout,
                             (int(*)(void*,const char*,...)) fprintf,
                             (int(*)(int,void*)) fputc };
    FILE *fp;

    if (argc != 2) {
        fprintf(stderr, "%s: usage: %s <json-file>\n", argv[0], argv[0]);
        return 1;
    }

    if (!(fp = fopen(argv[1], "rb"))) {
        fprintf(stderr, "%s: unable to open %s\n", argv[0], argv[1]);
        return 1;
    }
    config.ihandle = fp;

    zzjson = zzjson_parse(&config);

    if (!zzjson) {
        fprintf(stderr, "%s: unable to parse json file\n", argv[0]);
        fprintf(stderr, "%s: filepos: %lli\n", argv[0], (long long) ftell(fp));
        fclose(fp);
        return 1;
    }

    fclose(fp);

    zzjson_print(&config, zzjson);

    {
        ZZJSON *res;
        res = zzjson_object_find_labels(zzjson, "one", "two", "three", NULL);

        if (!res) fprintf(stderr, "snafu not found\n");
        else {
            fprintf(stderr, "snafu found: %s\n", res->value.string.string);
        }
    }

    fprintf(stderr, "object count: %u\n", zzjson_object_count(zzjson));
    fprintf(stderr, "array count: %u\n", zzjson_array_count(zzjson));

    fprintf(stderr, "%s\n", ZZJSON_IDENT);
    fprintf(stderr, "%i\n", ZZJSON_VERSION_INT);

    do {
        ZZJSON *tmp2;

        tmp = zzjson_create_array(&config,
                zzjson_create_number_d(&config, 3.14),
                zzjson_create_number_i(&config, 1234LL),
                zzjson_create_number_i(&config, -4321LL),
                zzjson_create_true(&config),
                zzjson_create_false(&config),
                zzjson_create_null(&config),
                zzjson_create_string(&config, "hello, world"),
                zzjson_create_object(&config,
                    "picard", zzjson_create_string(&config, "jean-luc"),
                    "riker",  zzjson_create_string(&config, "william t."),
                    NULL),
                zzjson_create_object(&config, NULL),
                zzjson_create_array(&config, NULL),
                NULL );

        if (!tmp) {
            fprintf(stderr, "error during creation of json tree\n");
            break;
        }

        tmp2 = zzjson_array_prepend(&config, tmp,
                    zzjson_create_string(&config, "prepended string"));

        if (!tmp2) {
            fprintf(stderr, "error during prepend\n");
            break;
        }
        tmp = tmp2;

        tmp2 = zzjson_array_append(&config, tmp,
                    zzjson_create_string(&config, "appended string (slow)"));

        if (!tmp2) {
            fprintf(stderr, "error during append\n");
            break;
        }
        tmp = tmp2;

        zzjson_print(&config, tmp);
    } while(0);
    if (tmp) zzjson_free(&config, tmp);

    {
        tmp = zzjson_create_array(&config, NULL); /* empty array */
        tmp = zzjson_array_prepend(&config, tmp, zzjson_create_true(&config));
        zzjson_print(&config, tmp);
        zzjson_free(&config, tmp);
    }

    {
        tmp = zzjson_create_object(&config, NULL); /* empty object */
        tmp = zzjson_object_prepend(&config, tmp, "hello",
                                zzjson_create_string(&config, "world"));
        tmp = zzjson_object_append(&config, tmp, "goodbye",
                                zzjson_create_string(&config, "cruel world"));
        zzjson_print(&config, tmp);
        zzjson_free(&config, tmp);
    }

    zzjson_free(&config, zzjson);

    return 0;
}
