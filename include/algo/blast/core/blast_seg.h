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
 *
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_seg.h
 * SEG filtering functions. @todo FIXME: should this be combined with
 * blast_filter/dust? Needs doxygen documentation and comments
 */

#ifndef __BLAST_SEG__
#define __BLAST_SEG__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_def.h>

#define AA20    2

#define LN20    2.9957322735539909

#define CHAR_SET 128

/*--------------------------------------------------------------(structs)---*/

typedef struct Seg
  {
   int begin;
   int end;
   struct Seg *next;
  } Seg;

typedef struct Alpha
  {
   Int4 alphabet;
   Int4 alphasize;
   double lnalphasize;
   Int4* alphaindex;
   unsigned char* alphaflag;
   char* alphachar;
  } Alpha;

typedef struct SegParameters
  {
   Int4 window;
   double locut;
   double hicut;
   Int4 period;
   Int4 hilenmin;
   Boolean overlaps;	/* merge overlapping pieces if TRUE. */
   Int4 maxtrim;
   Int4 maxbogus;
   Alpha* palpha;
  } SegParameters;

typedef struct Sequence
  {
   struct Sequence* parent;
   char* seq;
   Alpha* palpha;
   Int4 start;
   Int4 length;
   Int4 bogus;
   Boolean punctuation;
   Int4* composition;
   Int4* state;
   double entropy;
  } Sequence;

SegParameters* SegParametersNewAa (void);
void SegParametersFree(SegParameters* sparamsp);

Int2 SeqBufferSeg (Uint1* sequence, Int4 length, Int4 offset,
                   SegParameters* sparamsp, BlastSeqLoc** seg_locs);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
