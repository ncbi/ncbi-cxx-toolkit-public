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
* ===========================================================================*/

/*****************************************************************************

File name: blast_filter.h

Author: Ilya Dondoshansky

Contents: SEG filtering functions.

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_SEG__
#define __BLAST_SEG__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_def.h>

#define AA20    2

#define LN20    2.9957322735539909

#define CHAR_SET 128

/*--------------------------------------------------------------(structs)---*/

typedef struct Seg
  {
   int begin;
   int end;
   struct Seg *next;
  } Seg,* SegPtr;

typedef struct Alpha
  {
   Int4 alphabet;
   Int4 alphasize;
   FloatHi lnalphasize;
   Int4Ptr alphaindex;
   unsigned char* alphaflag;
   CharPtr alphachar;
  } Alpha,* AlphaPtr;

typedef struct SegParameters
  {
   Int4 window;
   FloatHi locut;
   FloatHi hicut;
   Int4 period;
   Int4 hilenmin;
   Boolean overlaps;	/* merge overlapping pieces if TRUE. */
   Int4 maxtrim;
   Int4 maxbogus;
   AlphaPtr palpha;
  } SegParameters,* SegParametersPtr;

typedef struct Sequence
  {
   struct Sequence* parent;
   CharPtr seq;
   AlphaPtr palpha;
   Int4 start;
   Int4 length;
   Int4 bogus;
   Boolean punctuation;
   Int4* composition;
   Int4* state;
   FloatHi entropy;
  } Sequence,* SequencePtr;

SegParametersPtr SegParametersNewAa (void);
void SegParametersFree(SegParametersPtr sparamsp);

Int2 SeqBufferSeg (Uint1Ptr sequence, Int4 length, Int4 offset,
                   SegParametersPtr sparamsp, BlastSeqLocPtr* seg_locs);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
