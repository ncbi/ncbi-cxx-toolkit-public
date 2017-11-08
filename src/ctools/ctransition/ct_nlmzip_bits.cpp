/* $Id$ */
/*****************************************************************************

    Name: bits.c                                             ur/compr/bits.c

    Description: Utility functions for compress/uncompress data

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

    Entry Points:

       void
       Nlmzip_bi_init (void)

       void
       Nlmzip_send_bits (value,length)
         int value;             [ value to send (I) ]
         int length;            [ number of bits (I) ]

       Uint4
       Nlmzip_bi_reverse (code,len)
         Uint4 code;         [ the value to invert (I) ]
         int len;               [ its bit length (I) ]

       void
       Nlmzip_bi_windup (void)

       void
       Nlmzip_copy_block (buf,len,header)
         char     *buf;         [ the input data (I) ]
         Uint4 len;          [ its length (I) ]
         int      header;       [ true if block header must be written (I) ]

    Modification History:
           05 Aug 1995 - grisha   -  original written

    Bugs and restriction on use:

    Notes:

*****************************************************************************/

#include <ncbi_pch.hpp>
#include "ct_nlmzip_i.h"

BEGIN_CTRANSITION_SCOPE


/****************************************************************************/
/* DEFINES */
/****************************************************************************/

/* Number of bits used within bi_buf. (bi_buf might be implemented on
   more than 16 bits on some systems.)
*/
#define Buf_size (8 * 2 * sizeof(char))

/****************************************************************************/
/* LOCAL VARIABLES */
/****************************************************************************/

/* Output buffer. bits are inserted starting at the bottom
   (least significant bits).
*/
static unsigned short bi_buf;

/* Number of valid bits in bi_buf.  All bits above the last valid
   bit are always zero.
*/
static int bi_valid;

/****************************************************************************/
/* GLOBAL FUNCTIONS */
/****************************************************************************/

/****************************************************************************/
/*.doc Nlmzip_bi_init (external) */
/*+
   Initialize the bit string routines.
-*/
/****************************************************************************/
void
Nlmzip_bi_init (void) /*FCN*/
{
    bi_buf = 0;
    bi_valid = 0;
}                                    /* Nlmzip_bi_init() */

/****************************************************************************/
/*.doc Nlmzip_send_bits (external) */
/*+
   Send a value on a given number of bits.
   IN assertion: length <= 16 and value fits in length bits.
-*/
/****************************************************************************/
void
Nlmzip_send_bits ( /*FCN*/
  int value,             /* value to send (I) */
  int length             /* number of bits (I) */
){
    /* If not enough room in bi_buf, use (valid) bits from bi_buf and
       (16 - bi_valid) bits from value, leaving (width - (16-bi_valid))
       unused bits in value.
    */
    if ( bi_valid > (int)Buf_size - length ) {
        bi_buf |= (value << bi_valid);
        Nlmzip_WriteShort (bi_buf);
        bi_buf = (ush)value >> (Buf_size - bi_valid);
        bi_valid += length - Buf_size;
    } else {
        bi_buf |= value << bi_valid;
        bi_valid += length;
    }
}                                   /* Nlmzip_send_bits() */

/****************************************************************************/
/*.doc Nlmzip_bi_reverse (external) */
/*+
   Reverse the first len bits of a code, using straightforward code (a faster
   Nlmzip_method would use a table)
   IN assertion: 1 <= len <= 15
-*/
/****************************************************************************/
Uint4
Nlmzip_bi_reverse ( /*FCN*/
  Uint4 code,                   /* the value to invert (I) */
  int len                          /* its bit length (I) */
){
    register Uint4 res = 0;

    do {
        res |= code & 1;
        code >>= 1;
        res <<= 1;
    } while ( --len > 0 );

    return res >> 1;
}

/****************************************************************************/
/*.doc Nlmzip_bi_windup (external) */
/*+
   Write out any remaining bits in an incomplete byte.
-*/
/****************************************************************************/
void
Nlmzip_bi_windup (void) /*FCN*/
{
    if ( bi_valid > 8 ) {
        Nlmzip_WriteShort (bi_buf);
    } else if ( bi_valid > 0 ) {
        Nlmzip_WriteByte ((unsigned char)bi_buf);
    }
    bi_buf = 0;
    bi_valid = 0;
}

/****************************************************************************/
/*.doc Nlmzip_copy_block (external) */
/*+
   Copy a stored block to the zip file, storing first the length and
   its one's complement if requested.
-*/
/****************************************************************************/
void
Nlmzip_copy_block ( /*FCN*/
  char     *buf,              /* the input data (I) */
  Uint4 len,               /* its length (I) */
  int      header             /* true if block header must be written (I) */
){
    Nlmzip_bi_windup();              /* align on byte boundary */

    if ( header ) {           /* write header information */
        Nlmzip_WriteShort ((ush)len);   
        Nlmzip_WriteShort ((ush)~len);
    }
    while ( len-- ) {         /* write data */
	Nlmzip_WriteByte (*buf++);
    }
}


END_CTRANSITION_SCOPE
