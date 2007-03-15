/*
 * Small jpeg decoder library
 *
 * Copyright (c) 2006, Luc Saillard <luc@saillard.org>
 * All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 * - Neither the name of the author nor the names of its contributors may be
 *  used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * yuv420p.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "tinyjpeg.h"
#include "tinyjpeg-internal.h"

/**
 *  YCrCb -> YUV420P (1x1)
 *  .---.
 *  | 1 |
 *  `---'
 */
static void YCrCB_to_YUV420P_1x1(struct jdec_private *priv)
{
  const unsigned char *s, *y;
  unsigned char *p;
  int i,j;

  p = priv->plane[0];
  y = priv->Y;
  for (i=0; i<8; i++)
   {
     memcpy(p, y, 8);
     p += priv->bytes_per_row[0];
     y += 8;
   }

  p = priv->plane[1];
  s = priv->Cb;
  for (i=0; i<8; i+=2)
   {
     for (j=0; j<8; j+=2, s+=2)
       *p++ = *s;
     s += 8; /* Skip one line */
     p += priv->bytes_per_row[1] - 4;
   }

  p = priv->plane[2];
  s = priv->Cr;
  for (i=0; i<8; i+=2)
   {
     for (j=0; j<8; j+=2, s+=2)
       *p++ = *s;
     s += 8; /* Skip one line */
     p += priv->bytes_per_row[2] - 4;
   }
}

/**
 *  YCrCb -> YUV420P (2x1)
 *  .-------.
 *  | 1 | 2 |
 *  `-------'
 */
static void YCrCB_to_YUV420P_2x1(struct jdec_private *priv)
{
  unsigned char *p;
  const unsigned char *s, *y1;
  int i,j;

  p = priv->plane[0];
  y1 = priv->Y;
  for (i=0; i<8; i++)
   {
     memcpy(p, y1, 16);
     p += priv->bytes_per_row[0];
     y1 += 16;
   }

  p = priv->plane[1];
  s = priv->Cb;
  for (i=0; i<8; i+=2)
   {
     for (j=0; j<8; j+=1, s+=1)
       *p++ = *s;
     s += 8; /* Skip one line */
     p += priv->bytes_per_row[1] - 8;
   }

  p = priv->plane[2];
  s = priv->Cr;
  for (i=0; i<8; i+=2)
   {
     for (j=0; j<8; j+=1, s+=1)
       *p++ = *s;
     s += 8; /* Skip one line */
     p += priv->bytes_per_row[2] - 8;
   }
}


/**
 *  YCrCb -> YUV420P (1x2)
 *  .---.
 *  | 1 |
 *  |---|
 *  | 2 |
 *  `---'
 */
static void YCrCB_to_YUV420P_1x2(struct jdec_private *priv)
{
  const unsigned char *s, *y;
  unsigned char *p;
  int i,j;

  p = priv->plane[0];
  y = priv->Y;
  for (i=0; i<16; i++)
   {
     memcpy(p, y, 8);
     p+=priv->bytes_per_row[0];
     y+=8;
   }

  p = priv->plane[1];
  s = priv->Cb;
  for (i=0; i<8; i++)
   {
     for (j=0; j<8; j+=2, s+=2)
       *p++ = *s;
     p += priv->bytes_per_row[1] - 4;
   }

  p = priv->plane[2];
  s = priv->Cr;
  for (i=0; i<8; i++)
   {
     for (j=0; j<8; j+=2, s+=2)
       *p++ = *s;
     p += priv->bytes_per_row[2] - 4;
   }
}

/**
 *  YCrCb -> YUV420P (2x2)
 *  .-------.
 *  | 1 | 2 |
 *  |---+---|
 *  | 3 | 4 |
 *  `-------'
 */
static void YCrCB_to_YUV420P_2x2(struct jdec_private *priv)
{
  unsigned char *p;
  const unsigned char *s, *y1;
  int i;

  p = priv->plane[0];
  y1 = priv->Y;
  for (i=0; i<16; i++)
   {
     memcpy(p, y1, 16);
     p += priv->bytes_per_row[0];
     y1 += 16;
   }

  p = priv->plane[1];
  s = priv->Cb;
  for (i=0; i<8; i++)
   {
     memcpy(p, s, 8);
     s += 8;
     p += priv->bytes_per_row[1];
   }

  p = priv->plane[2];
  s = priv->Cr;
  for (i=0; i<8; i++)
   {
     memcpy(p, s, 8);
     s += 8;
     p += priv->bytes_per_row[2];
   }
}

static int initialize_yuv420p(struct jdec_private *priv,
			      unsigned int *bytes_per_blocklines,
			      unsigned int *bytes_per_mcu)
{
  if (priv->components[0] == NULL)
    priv->components[0] = (uint8_t *)malloc(priv->width * priv->height);
  if (priv->components[1] == NULL)
    priv->components[1] = (uint8_t *)malloc(priv->width * priv->height/4);
  if (priv->components[2] == NULL)
    priv->components[2] = (uint8_t *)malloc(priv->width * priv->height/4);
  if (!priv->bytes_per_row[0])
    priv->bytes_per_row[0] = priv->width;
  if (!priv->bytes_per_row[1])
    priv->bytes_per_row[1] = priv->width/2;
  if (!priv->bytes_per_row[2])
    priv->bytes_per_row[2] = priv->width/2;

  bytes_per_blocklines[0] = priv->bytes_per_row[0];
  bytes_per_blocklines[1] = priv->bytes_per_row[1]/2;
  bytes_per_blocklines[2] = priv->bytes_per_row[2]/2;
  bytes_per_mcu[0] = 8;
  bytes_per_mcu[1] = 4;
  bytes_per_mcu[2] = 4;

  /* Return nonzero on failure */
  return !priv->components[0] || !priv->components[1] || !priv->components[2];
}

static const struct tinyjpeg_colorspace format_yuv420p =
  {
    {
      YCrCB_to_YUV420P_1x1,
      YCrCB_to_YUV420P_1x2,
      YCrCB_to_YUV420P_2x1,
      YCrCB_to_YUV420P_2x2,
    },
    tinyjpeg_decode_mcu_3comp_table,
    initialize_yuv420p
  };

const tinyjpeg_colorspace_t TINYJPEG_FMT_YUV420P = &format_yuv420p;
