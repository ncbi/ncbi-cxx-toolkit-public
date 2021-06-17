#ifndef __NLMZIPint__
#define __NLMZIPint__

/* $Id$ */
/*****************************************************************************

    Name: nlmzip_i.h                                 internal/compr/nlmzip_i.h

    Description: Internal definitions for compress/uncompress sybsystem.

    Author: Grisha Starchenko
            InforMax, Inc.
            Gaithersburg, USA.

   ***************************************************************************

                          PUBLIC DOMAIN NOTICE
              National Center for Biotechnology Information

    This software/database is a "United States Government Work" under the
    terms of the United States Copyright Act.  It was written as part of    
    the author's official duties as a United States Government employee
    and thus cannot be copyrighted.  This software/database is freely
    available to the public for use. The National Library of Medicine and
    the U.S. Government have not placed any restriction on its use or
    reproduction.

    Although all reasonable efforts have been taken to ensure the accuracy
    and reliability of the software and data, the NLM and the U.S.
    Government do not and cannot warrant the performance or results that
    may be obtained by using this software or data. The NLM and the U.S.
    Government disclaim all warranties, express or implied, including
    warranties of performance, merchantability or fitness for any
    particular purpose.

    Please cite the author in any work or product based on this material.

   ***************************************************************************

   Modification History:
           $Log: nlmzip_i.h,v $
           Revision 1.3  2001/05/09 00:57:42  kimelman
           cosmetics

           Revision 1.2  1998/05/18 16:31:01  kimelman
           osf/1 porting

           Revision 1.1  1998/05/15 19:05:18  kimelman
           all old 'gzip' names changed to be suitable for library names.
           prefix Nlmzip_ is now used for all of this local stuff.
           interface headers changed their names, moving from artdb/ur to nlmzip

           05 Aug 1995 - grisha   -  original written

    Bugs and restriction on use:

    Notes:
      1. To save memory for 16 bit systems, some arrays are
         overlaid between the various modules:
          Nlmzip_deflate: Nlmzip_prev+head   Nlmzip_window      Nlmzip_d_buf  l_buf  Nlmzip_outbuf
          Nlmzip_inflate:                    Nlmzip_window             Nlmzip_inbuf
         For compression, input is done in Nlmzip_window[]. For decompression,
         output is done in Nlmzip_window.

*****************************************************************************/

/****************************************************************************/
/* INCLUDES */
/****************************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctools/ctransition/nlmzip.hpp>

BEGIN_CTRANSITION_SCOPE


#ifndef _
#define _(proto) proto
#endif

/****************************************************************************/
/* TYPEDEFS */
/****************************************************************************/
typedef unsigned char  uch;
typedef unsigned short ush;
typedef          Uint4 ulg;

/****************************************************************************/
/* EXTERNS */
/****************************************************************************/
extern unsigned char Nlmzip_inbuf[];  /* input buffer */
extern unsigned char Nlmzip_outbuf[]; /* output buffer */
extern unsigned short Nlmzip_d_buf[]; /* buffer for distances, see trees.c */
extern unsigned char Nlmzip_window[]; /* Sliding Nlmzip_window and suffix table */
extern unsigned short Nlmzip_prev[];

/* Compressions parameters */
extern int Nlmzip_method;      /* compression Nlmzip_method */
extern int Nlmzip_level;       /* compression Nlmzip_level */

extern Uint4 Nlmzip_insize; /* valid bytes in Nlmzip_inbuf */
extern Uint4 Nlmzip_outcnt; /* bytes in output buffer */

/* Nlmzip_window position at the beginning of the current output block.
   Gets negative when the Nlmzip_window is moved backwards.
*/
extern Int4  Nlmzip_block_start;
extern Uint4 Nlmzip_strstart;                 /* start of string to insert */

/****************************************************************************/
/* DEFINES */
/****************************************************************************/
#define local static
#define OF(args)  args

/* Return codes from gzip */
#define NLMZIP_OK      0
#define NLMZIP_ERROR   1
#define NLMZIP_WARNING 2

/* Compression methods (see algorithm.doc) */
#define NLMZIP_STORED      0
#define NLMZIP_COMPRESSED  1
#define NLMZIP_PACKED      2
#define NLMZIP_LZHED       3
#define NLMZIP_DEFLATED    8
#define NLMZIP_MAX_METHODS 9

/* Main work buffers size */
#define INBUFSIZ     0x8000  /* input buffer size */
#define DIST_BUFSIZE 0x8000  /* buffer for distances, see trees.c */
#define WSIZE        0x8000  /* Nlmzip_window size--must be a power of two, and */
                             /*  at least 32K for zip's Nlmzip_deflate Nlmzip_method */
#define OUTBUFSIZ    16384   /* output buffer size */
#define BITS         16

/* Hash buffer will be in Nlmzip_prev */
#define head (Nlmzip_prev+WSIZE)    /* hash head (see Nlmzip_deflate.c) */

/* Special codes to be compatible with gzip */
#define	MAGIC_KEY      "\037\213" /* Magic key for check header, 1F 8B */
#define OS_CODE        0x03       /* assume Unix */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define CONTINUATION 0x02 /* bit 1 set: continuation of multi-part gzip file */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define ENCRYPTED    0x20 /* bit 5 set: file is encrypted */
#define RESERVED     0xC0 /* bit 6,7:   reserved */

/* internal file attribute */
#define UNKNOWN 0xffff
#define BINARY  0
#define ASCII   1

/* The minimum and maximum match lengths */
#define MIN_MATCH  3
#define MAX_MATCH  258

/* Minimum amount of lookahead, except at the end of the input file.
   See Nlmzip_deflate.c for comments about the MIN_MATCH+1.
*/
#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)

/* In order to simplify the code, particularly on 16 bit machines, match
   distances are limited to MAX_DIST instead of WSIZE.
 */
/*
#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)
*/
#define MAX_DIST  1024

/* Macros for getting two-byte and four-byte header values */
#define SH(p) ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8))
#define LG(p) ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16))

/* Macros for error reporting */
#define URCOMPRERR(x)     Nlmzip_Err(__FILE__,__LINE__,x)

/****************************************************************************/
/* FUNCTION PROTOTYPES */
/****************************************************************************/

void          Nlmzip_lm_init           _((int,unsigned short*));
ulg           Nlmzip_deflate           _((void));
void          Nlmzip_ct_init           _((unsigned short*,int*));
int           Nlmzip_ct_tally          _((int,int));
ulg           Nlmzip_flush_block       _((char*,ulg,int));
void          Nlmzip_bi_init           _((void));
void          Nlmzip_send_bits         _((int,int));
Uint4         Nlmzip_bi_reverse        _((Uint4,int));
void          Nlmzip_bi_windup         _((void));
void          Nlmzip_copy_block        _((char*,Uint4,int));
ulg           Nlmzip_updcrc            _((unsigned char*, Uint4));
void          Nlmzip_clear_bufs        _((void));
void          Nlmzip_flush_window      _((void));
int           Nlmzip_inflate           _((void));
void          Nlmzip_Err               _((const char*,int,const char*));
unsigned char Nlmzip_ReadByte          _((void));
int           Nlmzip_ReadData          _((unsigned char*,int));
void          Nlmzip_ReadUndo          _((void));
void          Nlmzip_WriteByte         _((unsigned char));
void          Nlmzip_WriteShort        _((unsigned short));
void          Nlmzip_WriteLong         _((ulg));
void          Nlmzip_WriteData         _((unsigned char*,int));


END_CTRANSITION_SCOPE

#endif /* __NLMZIPint__ */
