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

/** edit mask operations */
enum EOpType {
    eEditOpError  = 0x0,         /**< operation not valid */
    eEditOpInsert  = 0x1,         /**< insert operation */
    eEditOpDelete  = 0x2,         /**< delete operation */
    eEditOpReplace = 0x3          /**< replace operation */
};

/** A collection of identical editing operations */
typedef struct MBEditOp {
    enum EOpType op_type : 2;     /**< the type of operation */
    Uint4 num_ops : 30;    /**< number of operations of this type to perform */
} MBEditOp;

/** Macro for a formula estimating maximal gap length in the greedy algorithm */
#define ICEIL(x,y) ((((x)-1)/(y))+1)

/** Fraction of the sequence length in calculation of maximal cost (= 2) */
#define GREEDY_MAX_COST_FRACTION 2

/** Edit script structure for saving traceback information for greedy gapped 
 * alignment. */
typedef struct MBGapEditScript {
    MBEditOp *edit_ops;        /**< array of edit operations */
    Uint4 num_ops_allocated;   /**< size of allocated array */
    Uint4 num_ops;             /**< number of edit ops presently in use */
    enum EOpType last_op;      /**< most recent operation added */
} MBGapEditScript;

/** Frees an edit script structure 
    @param script The edit script to free [in]
    @return Always NULL
*/
MBGapEditScript *
MBGapEditScriptFree(MBGapEditScript *script);

/** Allocates an edit script structure 
    @return Pointer to the allocated edit script
*/
MBGapEditScript *
MBGapEditScriptNew(void);

/** Add the list of edit operations in one edit script to
    the list in another edit script

    @param dest_script The edit script to be expanded [in/modified]
    @param src_script The edit script whose operations
                        will be appended [in]
    @return Pointer to the expanded edit script
*/
MBGapEditScript *
MBGapEditScriptAppend(MBGapEditScript *dest_script, 
                      MBGapEditScript *src_script);

/* ----- pool allocator ----- */

/** @todo FIXME Need to determine what the members of this structure mean.
 * Can these be combined with the BlastGapDP structure? @sa BlastGapDP
 */
typedef struct SThreeVal {
    Int4 I, C, D;
} SThreeVal;

/** Space structure for greedy alignment algorithm */
typedef struct SMBSpace {
    SThreeVal* space_array;     /**< array of bookkeeping structures */
    Int4 space_allocated;       /**< number of structures allocated */
    Int4 space_used;            /**< number of structures actually in use */
    struct SMBSpace *next;      /**< pointer to next structure in list */
} SMBSpace;

/** Allocate a space structure for greedy alignment
    @return Pointer to allocated structure, or NULL upon failure
*/
SMBSpace* MBSpaceNew(void);

/** Free the space structure 
    @param sp Linked list of structures to free
*/
void MBSpaceFree(SMBSpace* sp);

/** All auxiliary memory needed for the greedy extension algorithm. */
typedef struct SGreedyAlignMem {
   Int4** flast_d;              /**< 2-D array of distances */
   Int4* max_row_free;          /**< array of maximum distances */
   SThreeVal** flast_d_affine;  /**< Like flast_d but for affine searches */
   Int4* uplow_free;            /**< ? */
   SMBSpace* space;             /**< local memory pool for SThreeVal structs */
} SGreedyAlignMem;

/** Perform the greedy extension algorithm with non-affine gap penalties.
 * @param seq1 First sequence (may be compressed) [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param seq2 Second sequence (always uncompressed) [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param extent1 Length of extension on sequence 1 [out]
 * @param extent2 Length of extension on sequence 2 [out]
 * @param aux_data Structure containing all preallocated memory [in]
 * @param script Edit script structure for saving traceback. 
 *          Traceback is not saved if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed first sequence. 
 *          Set to 4 if sequence is uncompressed. [in]
 * @return The minimum distance between the two sequences, i.e.
 *          the number of mismatches plus gaps in the resulting alignment
 */
Int4 
BLAST_GreedyAlign (const Uint1* seq1, Int4 len1,
                   const Uint1* seq2, Int4 len2,
                   Boolean reverse, Int4 xdrop_threshold, 
                   Int4 match_cost, Int4 mismatch_cost,
                   Int4* extent1, Int4* extent2, 
                   SGreedyAlignMem* aux_data, 
                   MBGapEditScript *script, Uint1 rem);

/** Perform the greedy extension algorithm with affine gap penalties.
 * @param seq1 First sequence (may be compressed) [in]
 * @param len1 Maximal extension length in first sequence [in]
 * @param seq2 Second sequence (always uncompressed) [in]
 * @param len2 Maximal extension length in second sequence [in]
 * @param reverse Is extension performed in backwards direction? [in]
 * @param xdrop_threshold X-dropoff value to use in extension [in]
 * @param match_cost Match score to use in extension [in]
 * @param mismatch_cost Mismatch score to use in extension [in]
 * @param extent1 Length of extension on sequence 1 [out]
 * @param extent2 Length of extension on sequence 2 [out]
 * @param aux_data Structure containing all preallocated memory [in]
 * @param script Edit script structure for saving traceback. 
 *          Traceback is not saved if NULL is passed. [in] [out]
 * @param rem Offset within a byte of the compressed first sequence.
 *          Set to 4 if sequence is uncompressed. [in]
 * @return The score of the alignment
 */
Int4 
BLAST_AffineGreedyAlign (const Uint1* seq1, Int4 len1,
                         const Uint1* seq2, Int4 len2,
                         Boolean reverse, Int4 xdrop_threshold, 
                         Int4 match_cost, Int4 mismatch_cost,
                         Int4 gap_open, Int4 gap_extend,
                         Int4* extent1, Int4* extent2, 
                         SGreedyAlignMem* aux_data, 
                         MBGapEditScript *script, Uint1 rem);

#ifdef __cplusplus
}
#endif
#endif /* _GREEDY_H_ */
