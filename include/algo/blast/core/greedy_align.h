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
 * Author: Ilya Dondoshansky
 *
 */

/** @file greedy_align.h
 * Copy of mbalign.h from ncbitools library 
 * @todo FIXME need better file description
 */

#ifndef _GREEDY_H_
#define _GREEDY_H_

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Type of editing operation: deletion, insertion, substitution */
typedef Uint4 MBEditOpType; /* 32 bits */

/** Edit script structure for saving traceback information for greedy gapped 
 * alignment. */
typedef struct MBGapEditScript {
    MBEditOpType *op;                  /* array of edit operations */
    Uint4 size, num;         /* size of allocation, number in use */
    MBEditOpType last;                 /* most recent operation added */
} MBGapEditScript;

/** Frees the edit script structure */
MBGapEditScript *MBGapEditScriptFree(MBGapEditScript *es);
/** Allocates an edit script structure */
MBGapEditScript *MBGapEditScriptNew(void);
/** Appends a new edit script to an existing one. */
MBGapEditScript *MBGapEditScriptAppend(MBGapEditScript *es, MBGapEditScript *et);

/* ----- pool allocator ----- */

/** @todo FIXME Need to determine what the members of this structure mean.
 * Can these be combined with the BlastGapDP structure? @sa BlastGapDP
 */
typedef struct SThreeVal {
    Int4 I, C, D;
} SThreeVal;

/** Space structure for greedy alignment algorithm */
typedef struct SMBSpace {
    SThreeVal* space_array;
    Int4 used, size;
    struct SMBSpace *next;
} SMBSpace;

/** Allocate a space structure for greedy alignment */
SMBSpace* MBSpaceNew(void);
/** Free the space structure */
void MBSpaceFree(SMBSpace* sp);

/** All auxiliary memory needed for the greedy extension algorithm. */
typedef struct SGreedyAlignMem {
   Int4** flast_d;
   Int4* max_row_free;
   SThreeVal** flast_d_affine;
   Int4* uplow_free;
   SMBSpace* space;
} SGreedyAlignMem;

/** Perform the greedy extension algorithm with non-affine gap penalties.
 * @param s1 First sequence [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param s2 Second sequence [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param e1 Length of extension to the left [out]
 * @param e2 Length of extension to the right [out]
 * @param abmp Structure containing all preallocated memory [in]
 * @param S Edit script structure for saving traceback. Traceback is not saved
 *          if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed subject sequence. 
 *            Set to 4 if sequence is uncompressed. [in]
 */
Int4 
BLAST_GreedyAlign (const Uint1* s1, Int4 len1,
			     const Uint1* s2, Int4 len2,
			     Boolean reverse, Int4 xdrop_threshold, 
			     Int4 match_cost, Int4 mismatch_cost,
			     Int4* e1, Int4* e2, SGreedyAlignMem* abmp, 
			     MBGapEditScript *S, Uint1 rem);

/** Perform the greedy extension algorithm with affine gap penalties.
 * @param s1 First sequence [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param s2 Second sequence [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param e1 Length of extension to the left [out]
 * @param e2 Length of extension to the right [out]
 * @param abmp Structure containing all preallocated memory [in]
 * @param S Edit script structure for saving traceback. Traceback is not saved
 *          if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed subject sequence.
 *            Set to 4 if sequence is uncompressed. [in]
 */
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
