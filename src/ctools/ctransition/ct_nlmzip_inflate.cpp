/* $Id$ */
/*****************************************************************************

    Name: Nlmzip_inflate.c                                       ur/compr/Nlmzip_inflate.c

    Description: Utility functions for uncompress data

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

       int
       Nlmzip_inflate (void)

    Modification History:
           11 Aug 1995 - grisha   -  original written

    Bugs and restriction on use:

    Notes:
      1. Distance pointers never point before the beginning of the output
         stream.

      2. Distance pointers can point back across blocks, up to 32k away.

      3. There is an implied maximum of 7 bits for the bit length table and
         15 bits for the actual data.
      4. If only one code exists, then it is encoded using one bit.  (Zero
         would be more efficient, but perhaps a little confusing.)  If two
         codes exist, they are coded using one bit each (0 and 1).

      5. There is no way of sending zero distance codes--a dummy must be
         sent if there are none.  (History: a pre 2.0 version of PKZIP would
         store blocks with no distance codes, but this was discovered to be
         too harsh a criterion.)  Valid only for 1.93a.  2.04c does allow
         zero distance codes, which is sent as one code of zero bits in
        length.

     6. There are up to 286 literal/length codes.  Code 256 represents the
        end-of-block.  Note however that the static length tree defines
        288 codes just to fill out the Huffman codes.  Codes 286 and 287
        cannot be used though, since there is no length base or extra bits
        defined for them.  Similarly, there are up to 30 distance codes.
        However, static trees define 32 codes (all 5 bits) to fill out the
        Huffman codes, but the last two had better not show up in the data.

     7. Unzip can check dynamic Huffman blocks for complete code sets.
        The exception is that a single code would not be complete (see #4).

     8. The five bits following the block type is really the number of
        literal codes sent minus 257.

     9. Length codes 8,16,16 are interpreted as 13 length codes of 8 bits
        (1+6+6).  Therefore, to output three times the length, you output
        three codes (1+1+1), whereas to output four times the same length,
        you only need two codes (1+3).  Hmm.

    10. In the tree reconstruction algorithm, Code = Code + Increment
        only if BitLength(i) is not zero.  (Pretty obvious.)

    11. Correction: 4 Bits: # of Bit Length codes - 4     (4 - 19)

    12. Note: length code 284 can represent 227-258, but length code 285
        really is 258.  The last length deserves its own, short code
        since it gets used a lot in very redundant files.  The length
        258 is special since 258 - 3 (the min match length) is 255.

    13. The literal/length and distance code bit lengths are read as a
        single stream of lengths.  It is possible (and advantageous) for
        a repeat code (16, 17, or 18) to go across the boundary between
        the two sets of lengths.

*****************************************************************************/

#include <ncbi_pch.hpp>
#include "ct_nlmzip_i.h"

BEGIN_CTRANSITION_SCOPE


/****************************************************************************/
/* DEFINES */
/****************************************************************************/
/* If BMAX needs to be larger than 16, then h and x[] should be ulg. */
#define BMAX 16         /* maximum bit length of any code (16 for explode) */
#define N_MAX 288       /* maximum number of codes in any set */

/* The Nlmzip_inflate algorithm uses a sliding 32K byte Nlmzip_window on the uncompressed
   stream to find repeated byte strings.  This is implemented here as a
   circular buffer.  The index is updated simply by incrementing and then
   and'ing with 0x7fff (32K-1). It is left to other modules to supply
   the 32K area.  It is assumed to be usable as if it were declared
   "uch Nlmzip_window[32768];" or as just "uch *Nlmzip_window;" and then malloc'ed
   in the latter case.  The definition must be in unzip.h, included above.
*/
#define flush_output(w) (Nlmzip_outcnt=(w),Nlmzip_flush_window())

/****************************************************************************/
/* STRUCT DEFINITIONS */
/****************************************************************************/
/* Huffman code lookup table entry--this entry is four bytes for machines
   that have 16-bit pointers (e.g. PC's in the small or medium model).
   Valid extra bits are 0..13.  e == 15 is EOB (end of block), e == 16
   means that v is a literal, 16 < e < 32 means that v is a pointer to
   the next table, which codes e - 16 bits, and lastly e == 99 indicates
   an unused code.  If a code with e == 99 is looked up, this implies an
   error in the data.
*/
struct huft {
    uch e;                /* number of extra bits or operation */
    uch b;                /* number of bits in this code or subcode */
    union {
        ush n;            /* literal, length base, or distance base */
        struct huft *t;   /* pointer to next Nlmzip_level of table */
    } v;
};

/****************************************************************************/
/* LOCAL FUNCTIONS PROTOTYPES */
/****************************************************************************/

static int inflate_codes   _((struct huft*,struct huft*,int,int));
static int huft_build      _((
               Uint4*,Uint4,Uint4,ush*,ush*,struct huft**,int*
           ));
static int huft_free       _((struct huft*));
static int inflate_stored  _((void));
static int inflate_fixed   _((void));
static int inflate_dynamic _((void));
static int inflate_block   _((int*));

/****************************************************************************/
/* LOCAL VARIABLES */
/****************************************************************************/
/* Tables for Nlmzip_deflate */
static Uint4 border[] = {        /* Order of the bit length code lengths */
          16, 17, 18, 0, 8, 7, 9, 6,
          10, 5, 11, 4, 12, 3, 13, 2,
          14, 1, 15
       };

static unsigned short cplens[] = {  /* Copy lengths for literal 257..285 */
         3,   4,   5,   6,   7,   8,   9,  10,
        11,  13,  15,  17,  19,  23,  27,  31,
        35,  43,  51,  59,  67,  83,  99, 115,
        131, 163, 195, 227, 258,  0,   0
       };

/* note: see note #13 above about the 258 in this list. */
static ush cplext[] = {            /* Extra bits for literal codes 257..285 */
        0, 0, 0, 0, 0, 0, 0, 0,
        1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4,
        5, 5, 5, 5, 0, 99, 99
       }; /* 99==invalid */

static ush cpdist[] = {         /* Copy offsets for distance codes 0..29 */
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577
       };
static ush cpdext[] = {         /* Extra bits for distance codes */
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13
       };

/* Macros for Nlmzip_inflate() bit peeking and grabbing.
   The usage is:
   
        NEEDBITS(j)
        x = b & mask_bits[j];
        DUMPBITS(j)

   where NEEDBITS makes sure that b has at least j bits in it, and
   DUMPBITS removes the bits from b.  The macros use the variable k
   for the number of bits in b.  Normally, b and k are register
   variables for speed, and are initialized at the beginning of a
   routine that uses these macros from a global bit buffer and count.

   If we assume that EOB will be the longest code, then we will never
   ask for bits with NEEDBITS that are beyond the end of the stream.
   So, NEEDBITS should not read any more bytes than are needed to
   meet the request.  Then no bytes need to be "returned" to the buffer
   at the end of the last block.

   However, this assumption is not true for fixed blocks--the EOB code
   is 7 bits, but the other literal/length codes can be 8 or 9 bits.
   (The EOB code is shorter than other codes because fixed blocks are
   generally short.  So, while a block always has an EOB, many other
   literal/length codes have a significantly lower probability of
   showing up at all.)  However, by making the first table have a
   lookup of seven bits, the EOB code will be found in that first
   lookup, and so will not require that too many bits be pulled from
   the stream.
*/
static ulg bb;        /* bit buffer */
static Uint4 bk;             /* bits in bit buffer */
static unsigned short mask_bits[] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#define NEEDBITS(n) { \
             while ( k < (n) ) { \
                b |= ((ulg)Nlmzip_ReadByte()) << k; \
                k+=8; \
             } \
        }
#define DUMPBITS(n) { \
             b >>= (n); \
             k -= (n); \
        }

/* Huffman code decoding is performed using a multi-Nlmzip_level table lookup.
   The fastest way to decode is to simply build a lookup table whose
   size is determined by the longest code.  However, the time it takes
   to build this table can also be a factor if the data being decoded
   is not very long.  The most common codes are necessarily the
   shortest codes, so those codes dominate the decoding time, and hence
   the speed.  The idea is you can have a shorter table that decodes the
   shorter, more probable codes, and then point to subsidiary tables for
   the longer codes.  The time it costs to decode the longer codes is
   then traded against the time it takes to make longer tables.

   This results of this trade are in the variables lbits and dbits
   below.  lbits is the number of bits the first Nlmzip_level table for literal/
   length codes can decode in one step, and dbits is the same thing for
   the distance codes.  Subsequent tables are also less than or equal to
   those sizes.  These values may be adjusted either when all of the
   codes are shorter than that, in which case the longest code length in
   bits is used, or when the shortest code is *longer* than the requested
   table size, in which case the length of the shortest code in bits is
   used.

   There are two different values for the two tables, since they code a
   different number of possibilities each.  The literal/length table
   codes 286 possible values, or in a flat code, a little over eight
   bits.  The distance table codes 30 possible values, or a little less
   than five bits, flat.  The optimum values for speed end up being
   about one bit more than those, so lbits is 8+1 and dbits is 5+1.
   The optimum values may differ though from machine to machine, and
   possibly even between compilers.  Your mileage may vary.
*/
static int lbits = 9;   /* bits in base literal/length lookup table */
static int dbits = 6;   /* bits in base distance lookup table */

/* track memory usage */
static Uint4 hufts;

/****************************************************************************/
/* LOCAL FUNCTIONS */
/****************************************************************************/

/****************************************************************************/
/*.doc huft_build (internal) */
/*+
  Given a list of code lengths and a maximum table size, make a set of
  tables to decode that set of codes.  Return zero on success, one if
  the given code set is incomplete (the tables are still built in this
  case), two if the input is invalid (all zero length codes or an
  oversubscribed set of lengths), and three if not enough memory.
-*/
/****************************************************************************/
static int
huft_build ( /*FCN*/
  Uint4*       b,      /* code lengths in bits (all assumed <= BMAX) */
  Uint4        n,      /* number of codes (assumed <= N_MAX) */
  Uint4        s,      /* number of simple-valued codes (0..s-1) */
  unsigned short* d,      /* list of base values for non-simple codes */
  unsigned short* e,      /* list of extra bits for non-simple codes */
  struct huft**   t,      /* result: starting table */
  int*            m       /* maximum lookup bits, returns actual */
){
  Uint4 a;                   /* counter for codes of length k */
  Uint4 c[BMAX+1];           /* bit length count table */
  Uint4 f;                   /* i repeats in table every f entries */
  int g;                        /* maximum code length */
  int h;                        /* table Nlmzip_level */
  Uint4 i;          /* counter, current code */
  Uint4 j;          /* counter */
  int k;               /* number of bits in current code */
  int l;                        /* bits per table (returned in m) */
  Uint4 *p;         /* pointer into c[], b[], or v[] */
  struct huft *q;      /* points to current table */
  struct huft r;                /* table entry for structure assignment */
  struct huft *u[BMAX];         /* table stack */
  Uint4 v[N_MAX];            /* values in order of bit length */
  int w;               /* bits before this table == (l * h) */
  Uint4 x[BMAX+1];           /* bit offsets, then code stack */
  Uint4 *xp;                 /* pointer into x */
  int y;                        /* number of dummy codes added */
  Uint4 z;                   /* number of entries in current table */


    /* Generate counts for each bit length */
    memset(c, 0, sizeof(c));
    p = b;  i = n;
    do {
        c[*p]++;     /* assume all entries <= BMAX */
        p++;         /* Can't combine with above line (Solaris bug) */
    } while (--i);

    if ( c[0] == n ) {         /* null input--all zero length codes */
        *t = (struct huft *)NULL;
        *m = 0;
        return 0;
    }

    /* Find minimum and maximum length, bound *m by those */
    l = *m;
    for ( j = 1; j <= BMAX; j++ ) {
         if ( c[j] ) {
             break;
         }
    }                      
    if ( (Uint4)l < (Uint4)(k = j) ) {  /* minimum code length */
        l = j;
    }
    for ( i = BMAX; i; i-- ) {
         if ( c[i] ) {
             break;
         }
    }
    if ( (Uint4)l > (Uint4)(g = i) ) {  /* maximum code length */
        l = i;
    }
    *m = l;


  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= c[i]) < 0)
    return 2;
  c[i] += y;


  /* Generate starting offsets into the value table for each length */
  x[1] = j = 0;
  p = c + 1;  xp = x + 2;
  while (--i) {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }


  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do {
    if ((j = *p++) != 0)
      v[x[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  x[0] = i = 0;                 /* first Huffman code is zero */
  p = v;                        /* grab values in bit order */
  h = -1;                       /* no tables yet--Nlmzip_level -1 */
  w = -l;                       /* bits decoded == (l * h) */
  u[0] = (struct huft *)NULL;   /* just to keep compilers happy */
  q = (struct huft *)NULL;      /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = c[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required Nlmzip_level */
      while (k > w + l)
      {
        h++;
        w += l;                 /* previous table always l bits */

        /* compute minimum size table less than or equal to l bits */
        z = (z = g - w) > (Uint4)l ? l : z;  /* upper limit on table size */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = c + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        z = 1 << j;             /* table entries for j-bit table */

        /* allocate and link in new table */
        if ((q = (struct huft *)malloc((z + 1)*sizeof(struct huft))) ==
            (struct huft *)NULL)
        {
          if (h)
            huft_free(u[0]);
          return 3;             /* not enough memory */
        }
        hufts += z + 1;         /* track memory usage */
        *t = q + 1;             /* link to list for huft_free() */
        *(t = &(q->v.t)) = (struct huft *)NULL;
        u[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          x[h] = i;             /* save pattern for backing up */
          r.b = (uch)l;         /* bits to dump before this table */
          r.e = (uch)(16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = i >> (w - l);     /* (get around Turbo C bug) */
          u[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (uch)(k - w);
      if (p >= v + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (uch)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = (ush)(*p);             /* simple code is just the value */
	p++;                           /* one compiler does not like *p++ */
      }
      else
      {
        r.e = (uch)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != x[h])
      {
        h--;                    /* don't need to update q */
        w -= l;
      }
    }
  }

  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}                                /* huft_build() */

/****************************************************************************/
/*.doc huft_free (internal) */
/*+
  Free the malloc'ed tables built by huft_build(), which makes a linked
  list of the tables it made, with the links in a dummy first entry of
  each table.
-*/
/****************************************************************************/
static int
huft_free ( /*FCN*/
  struct huft* t                /* table to free */
){
  struct huft* p;
  struct huft* q;

  /* Go through linked list, freeing from the malloced (t[-1]) address. */
  p = t;
  while ( p != NULL ) {
     q = (--p)->v.t;
     free((char*)p);
     p = q;
  }
  return 0;
}

/****************************************************************************/
/*.doc inflate_codes (internal) */
/*+
  Inflate (decompress) the codes in a deflated (compressed) block.
  Return an error code or zero if it all goes ok.
-*/
/****************************************************************************/
static int
inflate_codes ( /*FCN*/
  struct huft *tl,        /* literal/length and distance decoder tables */
  struct huft *td,
  int          bl,        /* number of bits decoded by tl[] and td[] */
  int          bd
){
    Uint4 e;  /* table entry flag/number of extra bits */
    Uint4 n, d;        /* length and index for copy */
    Uint4 w;           /* current Nlmzip_window position */
    struct huft *t;       /* pointer to table entry */
    Uint4 ml, md;      /* masks for bl and bd bits */
    ulg b;       /* bit buffer */
    Uint4 k;  /* number of bits in bit buffer */


    /* make local copies of globals */
    b = bb;               /* initialize bit buffer */
    k = bk;
    w = Nlmzip_outcnt;               /* initialize Nlmzip_window position */

    /* Nlmzip_inflate the coded data */
    ml = mask_bits[bl];           /* precompute masks for speed */
    md = mask_bits[bd];
    for (;;) {                    /* do until end of block */
       NEEDBITS((Uint4)bl)
       if ( (e = (t = tl + ((Uint4)b & ml))->e) > 16 ) {
           do {
               if ( e == 99 ) {
                   return 1;
               }
               DUMPBITS(t->b)
               e -= 16;
               NEEDBITS(e)
           } while (
                (e = (t = t->v.t + ((Uint4)b & mask_bits[e]))->e) > 16
             );
       }
       DUMPBITS(t->b)
       if ( e == 16 ) {           /* then it's a literal */
           Nlmzip_window[w++] = (uch)t->v.n;
           if ( w == WSIZE ) {
               flush_output(w);
               w = 0;
           }
       } else {                   /* it's an EOB or a length */
           /* exit if end of block */
           if ( e == 15 ) {
               break;
           }

           /* get length of block to copy */
           NEEDBITS(e)
           n = t->v.n + ((Uint4)b & mask_bits[e]);
           DUMPBITS(e);

           /* decode distance of block to copy */
           NEEDBITS((Uint4)bd)
           if ( (e = (t = td + ((Uint4)b & md))->e) > 16 ) {
               do {
                   if ( e == 99 ) {
                       return 1;
                   }
                   DUMPBITS(t->b)
                   e -= 16;
                   NEEDBITS(e)
               } while (
                   (e = (t = t->v.t + ((Uint4)b & mask_bits[e]))->e) > 16
                 );
           }
           DUMPBITS(t->b)
           NEEDBITS(e)
           d = w - t->v.n - ((Uint4)b & mask_bits[e]);
           DUMPBITS(e)

           /* do the copy */
           do {
               n -= (e = (
                          e = WSIZE - ((d &= WSIZE-1) > w ? d : w)
                         ) > n ? n : e
                    );
               /* (this test assumes unsigned comparison) */
               if ( w - d >= e ) {
                   memcpy(Nlmzip_window + w, Nlmzip_window + d, e);
                   w += e;
                   d += e;
               } else {  /* do it slow to avoid memcpy() overlap */
                   do {
                       Nlmzip_window[w++] = Nlmzip_window[d++];
                   } while (--e);
               }
               if ( w == WSIZE ) {
                   flush_output(w);
                   w = 0;
               }
           } while (n);
       }
    }


    /* restore the globals from the locals */
    Nlmzip_outcnt = w;          /* restore global Nlmzip_window pointer */
    bb = b;          /* restore global bit buffer */
    bk = k;

    return 0;        /* return to caller */
}

/****************************************************************************/
/*.doc inflate_stored (internal) */
/*+
  "decompress" an inflated type 0 (stored) block.
-*/
/****************************************************************************/
static int
inflate_stored (void) /*FCN*/
{
    Uint4 n;               /* number of bytes in block */
    Uint4 w;               /* current Nlmzip_window position */
    ulg b; /* bit buffer */
    Uint4 k;      /* number of bits in bit buffer */


    /* make local copies of globals */
    b = bb;                   /* initialize bit buffer */
    k = bk;
    w = Nlmzip_outcnt;               /* initialize Nlmzip_window position */


    /* go to byte boundary */
    n = k & 7;
    DUMPBITS(n);

    /* get the length and its complement */
    NEEDBITS(16)
    n = ((Uint4)b & 0xffff);
    DUMPBITS(16)

    NEEDBITS(16)
    if ( n != (Uint4)((~b) & 0xffff) ) {
        URCOMPRERR("error in compressed data");
        /* never be here */
    }
    DUMPBITS(16)

    /* read and output the compressed data */
    while ( n-- ) {
        NEEDBITS(8)
        Nlmzip_window[w++] = (uch)b;
        if ( w == WSIZE ) {
            flush_output(w);
            w = 0;
        }
        DUMPBITS(8)
    }

    /* restore the globals from the locals */
    Nlmzip_outcnt = w;               /* restore global Nlmzip_window pointer */
    bb = b;                   /* restore global bit buffer */
    bk = k;

    return 0;
}

/****************************************************************************/
/*.doc inflate_fixed (internal) */
/*+
  Decompress an inflated type 1 (fixed Huffman codes) block.  We should
  either replace this with a custom decoder, or at least precompute the
  Huffman tables.
-*/
/****************************************************************************/
static int
inflate_fixed (void) /*FCN*/
{
   int i;       /* temporary variable */
   struct huft *tl;      /* literal/length code table */
   struct huft *td;      /* distance code table */
   int bl;               /* lookup bits for tl */
   int bd;               /* lookup bits for td */
   Uint4 l[288];      /* length list for huft_build */


    /* set up literal table */
    for ( i = 0; i < 144; i++ ) {
       l[i] = 8;
    }
    for (; i < 256; i++) {
       l[i] = 9;
    }
    for (; i < 280; i++) {
       l[i] = 7;
    }
    for (; i < 288; i++) {  /* make a complete, but wrong code set */
       l[i] = 8;
    }

    bl = 7;
    if ( (i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0 ) {
       return i;
    }

    /* set up distance table */
    for ( i = 0; i < 30; i++ ) { /* make an incomplete code set */
         l[i] = 5;
    }
    bd = 5;
    if ( (i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) > 1 ) {
        huft_free(tl);
        return i;
    }

    /* decompress until an end-of-block code */
    if ( inflate_codes(tl, td, bl, bd) ) {
        return 1;
    }

    /* free the decoding tables, return */
    huft_free(tl);
    huft_free(td);

    return 0;               /* all okay. Return to caller */
}

/****************************************************************************/
/*.doc inflate_dynamic (internal) */
/*+
  Decompress an inflated type 2 (dynamic Huffman codes) block.
-*/
/****************************************************************************/
static int
inflate_dynamic (void) /*FCN*/
{
    int i;                /* temporary variables */
    Uint4 j;
    Uint4 l;           /* last length */
    Uint4 m;           /* mask for bit lengths table */
    Uint4 n;           /* number of lengths to get */
    struct huft *tl;      /* literal/length code table */
    struct huft *td;      /* distance code table */
    int bl;               /* lookup bits for tl */
    int bd;               /* lookup bits for td */
    Uint4 nb;          /* number of bit length codes */
    Uint4 nl;          /* number of literal/length codes */
    Uint4 nd;          /* number of distance codes */
    Uint4 ll[286+30];  /* literal/length and distance code lengths */
    ulg b;       /* bit buffer */
    Uint4 k;  /* number of bits in bit buffer */

    /* make local bit buffer */
    b = bb;
    k = bk;

    /* read in table lengths */
    NEEDBITS(5)
    nl = 257 + ((Uint4)b & 0x1f); /* number of literal/length codes */
    DUMPBITS(5)

    NEEDBITS(5)
    nd = 1 + ((Uint4)b & 0x1f);   /* number of distance codes */
    DUMPBITS(5)

    NEEDBITS(4)
    nb = 4 + ((Uint4)b & 0xf);    /* number of bit length codes */
    DUMPBITS(4)

    if ( nl > 286 || nd > 30 ) {
        URCOMPRERR("Bad lengths");
        /* never be here */
    }

    /* read in bit-length-code lengths */
    for (j = 0; j < nb; j++) {
       NEEDBITS(3)
       ll[border[j]] = (Uint4)b & 7;
       DUMPBITS(3)
    }
    for (; j < 19; j++ ) {
       ll[border[j]] = 0;
    }

    /* build decoding table for trees--single Nlmzip_level, 7 bit lookup */
    bl = 7;
    if ( (i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0 ) {
        if ( i == 1 ) {
            huft_free(tl);
        }
        return i;                   /* incomplete code set */
    }

    /* read in literal and distance code lengths */
    n = nl + nd;
    m = mask_bits[bl];
    i = l = 0;
    while ( (Uint4)i < n ) {
       NEEDBITS((Uint4)bl)
       j = (td = tl + ((Uint4)b & m))->b;
       DUMPBITS(j)
       j = td->v.n;
       if ( j < 16 ) {           /* length of code in bits (0..15) */
           ll[i++] = l = j;      /* save last length in l */
       } else if (j == 16) {     /* repeat last length 3 to 6 times */
           NEEDBITS(2)
           j = 3 + ((Uint4)b & 3);
           DUMPBITS(2)
           if ( (Uint4)i + j > n ) {
               return 1;
           }
           while ( j-- ) {
               ll[i++] = l;
           }
       } else if (j == 17) {     /* 3 to 10 zero length codes */
           NEEDBITS(3)
           j = 3 + ((Uint4)b & 7);
           DUMPBITS(3)
           if ( (Uint4)i + j > n ) {
               return 1;
           }
           while ( j-- ) {
              ll[i++] = 0;
           }
           l = 0;
       } else {                  /* j == 18: 11 to 138 zero length codes */
           NEEDBITS(7)
           j = 11 + ((Uint4)b & 0x7f);
           DUMPBITS(7)
           if ( (Uint4)i + j > n ) {
               return 1;
           }
           while ( j-- ) {
              ll[i++] = 0;
           }
           l = 0;
       }
    }

    /* free decoding table for trees */
    huft_free(tl);

    /* restore the global bit buffer */
    bb = b;
    bk = k;

    /* build the decoding tables for literal/length and distance codes */
    bl = lbits;
    if ( (i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0 ) {
        if ( i == 1 ) {
            huft_free(tl);
            URCOMPRERR("Incomplete literal tree");
            /* never be here */
        }
        return i;                   /* incomplete code set */
    }

    bd = dbits;
    if ( (i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0 ) {
        if ( i == 1 ) {
            huft_free(td);
            huft_free(tl);
            URCOMPRERR("Incomplete distance tree");
            /* never be here */
        }
        huft_free(tl);
        return i;                   /* incomplete code set */
    }

    /* decompress until an end-of-block code */
    if ( inflate_codes(tl, td, bl, bd) ) {
        return 1;
    }

    /* free the decoding tables, return */
    huft_free(tl);
    huft_free(td);
    return 0;
}

/****************************************************************************/
/*.doc inflate_block (internal) */
/*+
  This function decompress an inflated block
-*/
/****************************************************************************/
static int
inflate_block ( /*FCN*/
  int *e                /* last block flag */
){
    Uint4 t;           /* block type */
    ulg b;       /* bit buffer */
    Uint4 k;  /* number of bits in bit buffer */

    b = bb;               /* make local bit buffer */
    k = bk;

    NEEDBITS(1)           /* read in last block bit */
    *e = (int)b & 1;
    DUMPBITS(1)

    NEEDBITS(2)           /* read in block type */
    t = (Uint4)b & 3;
    DUMPBITS(2)

    bb = b;               /* restore the global bit buffer */
    bk = k;

    /* Nlmzip_inflate that block type */
    if ( t == 2 ) {
        return inflate_dynamic();
    }
    if ( t == 0 ) {
        return inflate_stored();
    }
    if ( t == 1 ) {
        return inflate_fixed();
    }

    URCOMPRERR("Bad block type");
    return 2;             /* never be here */
}

/****************************************************************************/
/* GLOBAL FUNCTIONS */
/****************************************************************************/

/****************************************************************************/
/*.doc Nlmzip_inflate (internal) */
/*+
  Decompress an inflated entry
-*/
/****************************************************************************/
int
Nlmzip_inflate (void) /*FCN*/
{
  int e;                /* last block flag */
  int r;                /* result code */
  Uint4 h;           /* maximum struct huft's malloc'ed */

  /* initialize Nlmzip_window, bit buffer */
  Nlmzip_outcnt = 0;
  bk = 0;
  bb = 0;

  /* decompress until the last block */
  h = 0;
  do
    {
      hufts = 0;
      if ( (r = inflate_block(&e)) != 0 ) 
        return r;
      if ( hufts > h ) 
        h = hufts;
    }
  while ( !e );

  /*
   * Undo too much lookahead. The next read will be byte aligned
   * so we can discard unused bits in the last meaningful byte.
   */
  while ( bk >= 8 )
    {
      bk -= 8;
      Nlmzip_ReadUndo ();
    }

  flush_output(Nlmzip_outcnt);            /* flush out Nlmzip_window */
 
  return 0;                        /* return success */
}


END_CTRANSITION_SCOPE
