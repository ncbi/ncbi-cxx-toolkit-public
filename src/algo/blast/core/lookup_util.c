/* $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's offical duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 */

/** @file lookup_util.c
 *  Utility functions for lookup table generation.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <algo/blast/core/lookup_util.h>
#include <algo/blast/core/blast_options.h>

void __sfree(void **x)
{
  free(*x);
  *x=NULL;
  return;
}

static void fkm_output(Int4* a, Int4 n, Int4 p, Uint1* output, Int4* cursor, Uint1* alphabet);
static void fkm(Int4* a, Int4 n, Int4 k, Uint1* output, Int4* cursor, Uint1* alphabet);

Int4 iexp(Int4 x, Int4 n)
{
Int4 r,y;

r=1;
y=x;

 if(n==0) return 1;
 if(x==0) return 0;
  
while (n > 1)
  { 
    if ( (n%2)==1) r *= y;
    n = n >> 1;
    y = y*y;
  }
 r = r*y;
 return r;
}

Int4 ilog2(Int4 x)
{
  Int4 lg=0;

  if (x==0) return 0;

  while ( ( x = x >> 1) )
    lg++;

  return lg;
}

Int4 makemask(Int4 x)
{
Int4 mask=1;

if (x==0) return 0;

mask = mask << (x-1);

mask |= (mask >> 1);
mask |= (mask >> 2);
mask |= (mask >> 4);
mask |= (mask >> 8);
mask |= (mask >> 16);

return mask;
}


/** Output a Lyndon word as part of a de Bruijn sequence.
 *
 *if the length of a lyndon word is divisible by n, print it. 
 * @param a the shift register
 * @param p
 * @param n
 * @param output the output sequence
 * @param cursor current location in the output sequence
 * @param alphabet optional translation alphabet
 */

static void fkm_output(Int4* a, Int4 n, Int4 p, Uint1* output, Int4* cursor, Uint1* alphabet)
{
  Int4 i;

  if (n % p == 0)
    for(i=1;i<=p;i++)
      {
	if (alphabet != NULL)
	  output[*cursor] = alphabet[ a[i] ];
	else
	  output[*cursor] = a[i];
	*cursor = *cursor + 1;
      }
}

/**
 * iterative fredricksen-kessler-maiorana algorithm
 * to generate de bruijn sequences.
 *
 * generates all lyndon words, in lexicographic order.
 * the concatenation of all lyndon words whose length is 
 * divisible by n yields a de bruijn sequence.
 *
 * further, the sequence generated is of shortest lexicographic length.
 *
 * references: 
 * http://mathworld.wolfram.com/deBruijnSequence.html
 * http://www.theory.csc.uvic.ca/~cos/inf/neck/NecklaceInfo.html
 * http://www.cs.usyd.edu.au/~algo4301/ , chapter 7.2
 * http://citeseer.nj.nec.com/ruskey92generating.html
 *
 * @param a the shift register
 * @param n the number of letters in each word
 * @param k the size of the alphabet
 * @param output the output sequence
 * @param cursor the current location in the output sequence
 * @param alphabet optional translation alphabet
 */

static void fkm(Int4* a, Int4 n, Int4 k, Uint1* output, Int4* cursor, Uint1* alphabet)
{
  Int4 i,j;

  fkm_output(a,n,1,output,cursor,alphabet);

  i=n;

do
  {
    a[i] = a[i] + 1;

    for(j=1;j<=n-i;j++)
      a[j+i] = a[j];

    fkm_output(a,n,i,output,cursor,alphabet);

    i=n;

    while (a[i] == k-1)
      i--;
  }
 while (i>0);
}

void debruijn(Int4 n, Int4 k, Uint1* output, Uint1* alphabet)
{
  Int4* a;
  Int4 cursor=0;

  /* n+1 because the array is indexed from one, not zero */
  a = (Int4*) calloc( (n+1), sizeof(Int4));

  /* compute the (n,k) de Bruijn sequence and store it in output */
 
  fkm(a,n,k,output,&cursor,alphabet);
  
  sfree(a);
  return;
}

Int4 CalculateBestStride(Int4 word_size, Boolean var_words, Int4 lut_type)
{
   Int4 lut_width;
   Int4 extra = 1;
   Uint1 remainder;
   Int4 stride;

   if (lut_type == MB_LOOKUP_TABLE)
      lut_width = 12;
   else if (word_size >= 8)
      lut_width = 8;
   else
      lut_width = 4;

   remainder = word_size % COMPRESSION_RATIO;

   if (var_words && (remainder == 0) )
      extra = COMPRESSION_RATIO;

   stride = word_size - lut_width + extra;

   remainder = stride % 4;

   /*
    The resulting stride is rounded to a number divisible by 4 
    for all values except 6 and 7. This is done because scanning database 
    with a stride divisible by 4 does not require splitting bytes of 
    compressed sequences. For values 6 and 7 however the advantage of a 
    larger stride outweighs the disadvantage of splitting the bytes.
    */
   if (stride > 8 || (stride > 4 && remainder == 1) )
      stride -= remainder;
   return stride;
}
