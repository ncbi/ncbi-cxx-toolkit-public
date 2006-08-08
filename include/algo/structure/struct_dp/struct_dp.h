/*  $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
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
* Authors:  Paul Thiessen
*
* File Description:
*      Dynamic programming-based alignment algorithms (C interface)
*
* ===========================================================================
*/

#ifndef STRUCT_DP__H
#define STRUCT_DP__H

#include <corelib/ncbi_limits.h>

#ifdef __cplusplus
extern "C" {
#endif

/* function result codes */
#define STRUCT_DP_FOUND_ALIGNMENT  1  /* found an alignment */
#define STRUCT_DP_NO_ALIGNMENT     2  /* algorithm successful, but no significant alignment found */
#define STRUCT_DP_PARAMETER_ERROR  3  /* error/inconsistency in function parameters */
#define STRUCT_DP_ALGORITHM_ERROR  4  /* error encountered during algorithm execution */
#define STRUCT_DP_OKAY             5  /* generic ok status */

/* lowest possible score */
static const int DP_NEGATIVE_INFINITY = kMin_Int;

/* highest possible loop penalty */
static const unsigned int DP_POSITIVE_INFINITY = kMax_UInt;

/*
 * Block alignment structures and functions
 */

/* use for block that is not frozen */
static const unsigned int DP_UNFROZEN_BLOCK = kMax_UInt;

/* info on block structure */
typedef struct {
    unsigned int nBlocks;           /* number of blocks */
    unsigned int *blockPositions;   /* first residue of each block on subject (zero-numbered) */
    unsigned int *blockSizes;       /* length of each block */
    unsigned int *maxLoops;         /* nBlocks-1 max loop sizes */
    unsigned int *freezeBlocks;     /* first residue of each block on query (zero-numbered) if frozen;
                                        DP_UNFROZEN_BLOCK otherwise (default); global alignment only! */
} DP_BlockInfo;

/* convenience functions for allocating/destroying BlockInfo structures;
   do not use free (MemFree), because these are C++-allocated! */
extern DP_BlockInfo * DP_CreateBlockInfo(unsigned int nBlocks);
extern void DP_DestroyBlockInfo(DP_BlockInfo *blocks);

/* standard function for calculating max loop length, given a list of the sizes of all the
   corresponding loops in each sequence/row of an existing multiple alignment:

        if percentile < 1.0:
            max = (len such that <percentile>% of loops are <= len) + extension
        else if percentile >= 1.0:
            max = (<percentile> * max loop len)(rounded) + extension

        if cutoff > 0, max will be truncated to be <= cutoff
*/
extern unsigned int DP_CalculateMaxLoopLength(
    unsigned int nLoops, const unsigned int *loopLengths,   /* based on existing alignment */
    double percentile, unsigned int extension, unsigned int cutoff);

/* callback function to get the score for a block at a specified position; should
   return DP_NEGATIVE_INFINITY if the block can't be aligned at this position. For
   local alignments, the average expected score for a random match must be <= zero. */
typedef int (*DP_BlockScoreFunction)(unsigned int block, unsigned int queryPos);

/* returned alignment structure */
typedef struct {
    int score;                      /* alignment score */
    unsigned int nBlocks;           /* number of blocks in this alignment */
    unsigned int firstBlock;        /* first block aligned (w.r.t. subject blocks) */
    unsigned int *blockPositions;   /* positions of blocks on query (zero-numbered) */
} DP_AlignmentResult;

/* for destroying alignment result; do not use free (MemFree) */
extern void DP_DestroyAlignmentResult(DP_AlignmentResult *alignment);

/* global alignment routine */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_GlobalBlockAlign(
    const DP_BlockInfo *blocks,         /* blocks on subject */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_AlignmentResult **alignment      /* alignment, if one found; caller should destroy */
);

/* local alignment routine */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_LocalBlockAlign(
    const DP_BlockInfo *blocks,         /* blocks on subject; NOTE: block freezing ignored! */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_AlignmentResult **alignment      /* alignment, if one found; caller should destroy */
);

/* returned alignment container for multiple tracebacks */
typedef struct {
    unsigned int nAlignments;           /* number of alignments returned */
    DP_AlignmentResult *alignments;     /* individual alignments, highest-scoring first */
} DP_MultipleAlignmentResults;

/* for destroying multiple alignment results; do not use free (MemFree) */
extern void DP_DestroyMultipleAlignmentResults(DP_MultipleAlignmentResults *alignments);

/* local alignment routine returning sorted list of highest-scoring alignments */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_MultipleLocalBlockAlign(
    const DP_BlockInfo *blocks,         /* blocks on subject; NOTE: block freezing ignored! */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_MultipleAlignmentResults **alignments,   /* alignments, if any found; caller should destroy */
    unsigned int maxAlignments          /* max # alignments to return, 0 == unlimited (all) */
);

/* callback function to get the score (a positive or zero penalty) for a given loop number and length */
typedef unsigned int (*DP_LoopPenaltyFunction)(unsigned int loopNumber, unsigned int loopLength);

/* global alignment routine for generic loop scoring function */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_GlobalBlockAlignGeneric(
    const DP_BlockInfo *blocks,         /* blocks on subject; note that maxLoops are ignored! */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    DP_LoopPenaltyFunction LoopScore,   /* loop scoring function */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_AlignmentResult **alignment      /* alignment, if one found; caller should destroy */
);

/* local alignment routine for generic loop scoring */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_LocalBlockAlignGeneric(
    const DP_BlockInfo *blocks,         /* blocks on subject; NOTE: block freezing ignored! */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    DP_LoopPenaltyFunction LoopScore,   /* loop scoring function */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_AlignmentResult **alignment      /* alignment, if one found; caller should destroy */
);

/* local generic alignment routine returning sorted list of highest-scoring alignments */
extern int                              /* returns an above STRUCT_DP_ status code */
DP_MultipleLocalBlockAlignGeneric(
    const DP_BlockInfo *blocks,         /* blocks on subject; NOTE: block freezing ignored! */
    DP_BlockScoreFunction BlockScore,   /* scoring function for blocks on query */
    DP_LoopPenaltyFunction LoopScore,   /* loop scoring function */
    unsigned int queryFrom,             /* range of query to search */
    unsigned int queryTo,
    DP_MultipleAlignmentResults **alignments,   /* alignments, if any found; caller should destroy */
    unsigned int maxAlignments          /* max # alignments to return, 0 == unlimited (all) */
);

#ifdef __cplusplus
}
#endif

#endif /* STRUCT_DP__H */

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2006/08/08 16:30:42  thiessen
* extern -> static for constants
*
* Revision 1.1  2004/02/19 01:46:04  thiessen
* move to include/algo/structure/struct_dp
*
* Revision 1.12  2003/12/19 14:37:50  thiessen
* add local generic loop function alignment routines
*
* Revision 1.11  2003/12/08 16:33:59  thiessen
* fix signed/unsigned mix
*
* Revision 1.10  2003/12/08 16:21:36  thiessen
* add generic loop scoring function interface
*
* Revision 1.9  2003/09/07 00:06:19  thiessen
* add multiple tracebacks for local alignments
*
* Revision 1.8  2003/08/22 14:28:49  thiessen
* add standard loop calculating function
*
* Revision 1.7  2003/07/11 15:27:48  thiessen
* add DP_ prefix to globals
*
* Revision 1.6  2003/06/22 12:11:37  thiessen
* add local alignment
*
* Revision 1.5  2003/06/19 13:48:23  thiessen
* cosmetic/typo fixes
*
* Revision 1.4  2003/06/18 21:46:09  thiessen
* add traceback, alignment return structure
*
* Revision 1.3  2003/06/18 19:17:17  thiessen
* try once more to fix lf issue
*
* Revision 1.2  2003/06/18 19:10:17  thiessen
* fix lf issues
*
* Revision 1.1  2003/06/18 16:54:12  thiessen
* initial checkin; working global block aligner and demo app
*
*/
