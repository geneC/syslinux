/*
 * unzip.c
 * 
 * This is a collection of several routines from gzip-1.0.3 
 * adapted for Linux.
 *
 * malloc by Hannu Savolainen 1993 and Matthias Urlichs 1994
 * puts by Nick Holloway 1993, better puts by Martin Mares 1995
 * High loaded stuff by Hans Lermen & Werner Almesberger, Feb. 1996
 *
 * Adapted for MEMDISK by H. Peter Anvin, April 2003
 */

#include <stdint.h>
#include "memdisk.h"
#include "conio.h"

/*
 * gzip declarations
 */

#define OF(args)  args
#define STATIC static

#define memzero(s, n)     memset ((s), 0, (n))

typedef uint8_t  uch;
typedef uint16_t ush;
typedef uint32_t ulg;

#define WSIZE 0x8000	        /* Window size must be at least 32k, */
				/* and a power of two */

static uch *inbuf;		/* input pointer */
static uch window[WSIZE];	/* sliding output window buffer */

static unsigned insize;		/* total input bytes read */
static unsigned inbytes;	/* valid bytes in inbuf */
static unsigned outcnt;		/* bytes in output buffer */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ASCII text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/* Diagnostic functions */
#ifdef DEBUG
#  define Assert(cond,msg) {if(!(cond)) error(msg);}
#  define Trace(x) fprintf x
#  define Tracev(x) {if (verbose) fprintf x ;}
#  define Tracevv(x) {if (verbose>1) fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) fprintf x ;}
#else
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

static int  fill_inbuf(void);
static void flush_window(void);
static void error(char *m);
static void gzip_mark(void **);
static void gzip_release(void **);

extern ulg crc_32_tab[256];

/* Get byte from input buffer */
static inline uch get_byte(void)
{
  if ( inbytes ) {
    uch b = *inbuf++;
    inbytes--;
    return b;
  } else {
    return fill_inbuf();	/* Input buffer underrun */
  }
}

/* Unget byte from input buffer */
static inline void unget_byte(void)
{
  inbytes++;
  inbuf--;
}

static ulg bytes_out = 0;	/* Number of bytes output */
static uch *output_data;	/* Output data pointer */
static ulg output_size;		/* Number of output bytes expected */

static void *malloc(int size);
static void free(void *where);

static ulg free_mem_ptr, free_mem_end_ptr;

#include "inflate.c"

static void *malloc(int size)
{
  void *p;
  
  if (size < 0) error("malloc error");
  
  free_mem_ptr = (free_mem_ptr + 3) & ~3;	/* Align */
  
  p = (void *)free_mem_ptr;
  free_mem_ptr += size;
  
  if (free_mem_ptr >= free_mem_end_ptr)
    error("out of memory");
  
  return p;
}

static void free(void *where)
{
  /* Don't care */
}

static void gzip_mark(void **ptr)
{
  *ptr = (void *) free_mem_ptr;
}

static void gzip_release(void **ptr)
{
  free_mem_ptr = (long) *ptr;
}
 
/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty
 * and at least one byte is really needed.
 */
static int fill_inbuf(void)
{
  /* This should never happen.  We have already pointed the algorithm
     to all the data we have. */
  printf("failed\nDecompression error: ran out of input data\n");
  die();
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
static void flush_window(void)
{
    ulg c = crc;         /* temporary variable */
    unsigned n;
    uch *in, *out, ch;

    if ( bytes_out+outcnt > output_size )
      error("output buffer overrun");
    
    in = window;
    out = output_data;
    for (n = 0; n < outcnt; n++) {
	    ch = *out++ = *in++;
	    c = crc_32_tab[(c ^ ch) & 0xff] ^ (c >> 8);
    }
    crc = c;
    output_data = out;
    bytes_out += (ulg)outcnt;
    outcnt = 0;
}

static void error(char *x)
{
  printf("failed\nDecompression error: %s\n", x);
  die();
}

/*
 * Decompress the image, trying to flush the end of it as close
 * to end_mem as possible.  Return a pointer to the data block,
 * and change datalen.
 */
extern void _end;

void *unzip(void *indata, unsigned long zbytes, void *target)
{
  /* The uncompressed length of a gzip file is the last four bytes */
  unsigned long dbytes = *(uint32_t *)((char *)indata + zbytes - 4);

  /* Set up the heap; it's the 64K after the bounce buffer */
  free_mem_ptr = (ulg)sys_bounce + 0x10000;
  free_mem_end_ptr = free_mem_ptr + 0x10000;

  /* Set up input buffer */
  inbuf  = indata;
  insize = inbytes = zbytes;

  /* Set up output buffer */
  outcnt = 0;
  output_data = target;
  output_size = dbytes;
  bytes_out = 0;

  makecrc();
  gunzip();

  if ( bytes_out != dbytes )
    error("length error");

  puts("ok\n");

  return target;
}
