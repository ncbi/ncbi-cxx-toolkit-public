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

File name: greedy_align.h

Author: Ilya Dondoshansky

Contents: Copy of mbalign.h from ncbitools library

******************************************************************************
 * $Revision$
 * */
#ifndef _GREEDY_H_
#define _GREEDY_H_

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef Uint4 edit_op_t; /* 32 bits */
typedef struct MBGapEditScript {
    edit_op_t *op;                  /* array of edit operations */
    Uint4 size, num;         /* size of allocation, number in use */
    edit_op_t last;                 /* most recent operation added */
} MBGapEditScript;

MBGapEditScript *MBGapEditScriptFree(MBGapEditScript *es);
MBGapEditScript *MBGapEditScriptNew(void);
MBGapEditScript *MBGapEditScriptAppend(MBGapEditScript *es, MBGapEditScript *et);

/* ----- pool allocator ----- */
typedef struct SThreeVal {
    Int4 I, C, D;
} SThreeVal;

typedef struct SMBSpace {
    SThreeVal* space_array;
    Int4 used, size;
    struct SMBSpace *next;
} SMBSpace;

SMBSpace* MBSpaceNew(void);
void MBSpaceFree(SMBSpace* sp);

typedef struct SGreedyAlignMem {
   Int4** flast_d;
   Int4* max_row_free;
   SThreeVal** flast_d_affine;
   Int4* uplow_free;
   SMBSpace* space;
} SGreedyAlignMem;

Int4 
BLAST_GreedyAlign (const Uint1* s1, Int4 len1,
			     const Uint1* s2, Int4 len2,
			     Boolean reverse, Int4 xdrop_threshold, 
			     Int4 match_cost, Int4 mismatch_cost,
			     Int4* e1, Int4* e2, SGreedyAlignMem* abmp, 
			     MBGapEditScript *S, Uint1 rem);
Int4 
BLAST_AffineGreedyAlign (const Uint1* s1, Int4 len1,
				  const Uint1* s2, Int4 len2,
				  Boolean reverse, Int4 xdrop_threshold, 
				  Int4 match_cost, Int4 mismatch_cost,
				  Int4 gap_open, Int4 gap_extend,
				  Int4* e1, Int4* e2, 
				  SGreedyAlignMem* abmp, 
				  MBGapEditScript *S, Uint1 rem);

#ifdef __cplusplus
}
#endif
#endif /* _GREEDY_H_ */
