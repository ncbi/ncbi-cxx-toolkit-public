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

/** @file gapinfo.h
 * Definitions of structures used for saving traceback information.
 */

#ifndef __GAPINFO__
#define __GAPINFO__

#include <algo/blast/core/blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Operation types within the edit script*/
typedef enum EGapAlignOpType { 
   eGapAlignDel = 0, /**< Deletion: a gap in query */
   eGapAlignDel2 = 1,/**< Frame shift deletion of two nucleotides */
   eGapAlignDel1 = 2,/**< Frame shift deletion of one nucleotide */
   eGapAlignSub = 3, /**< Substitution */
   eGapAlignIns1 = 4,/**< Frame shift insertion of one nucleotide */
   eGapAlignIns2 = 5,/**< Frame shift insertion of two nucleotides */
   eGapAlignIns = 6, /**< Insertion: a gap in subject */
   eGapAlignDecline = 7, /**< Non-aligned region */
   eGapAlignInvalid = 8 /**< Invalid operation */
} EGapAlignOpType;

/** Edit script: linked list of correspondencies between two sequences */
typedef struct GapEditScript {
   EGapAlignOpType op_type;    /**< Type of operation */
   Int4 num;                   /**< Number of operations */
   struct GapEditScript* next; /**< Pointer to next link */
} GapEditScript;

/** A version of GapEditScript used to store initial results
    from the gapped alignment routines */
typedef struct GapPrelimEditScript {
   EGapAlignOpType op_type;    /**< Type of operation */
   Int4 num;                   /**< Number of operations */
} GapPrelimEditScript;

/** Editing block structure containing all information returned from a 
 * single gapped extension. */
typedef struct GapEditBlock {
   Int4 start1;  /**< Start of alignment in query. */
   Int4 start2;  /**< Start of alignment in subject. */
   Int4 length1; /**< Length of alignment in query. */
   Int4 length2; /**< Length of alignment in subject. */
   Int4 original_length1; /**< Untranslated query length. */
   Int4 original_length2; /**< Untranslated subject length. */
   Int2 frame1;  /**< Query frame. */
   Int2 frame2;	 /**< Subject frame. */
   Boolean translate1; /**< Is query translated? */
   Boolean translate2; /**< Is subject translated? */
   Boolean reverse; /**< reverse sequence 1 and 2 when producing SeqALign? */
   Boolean is_ooframe; /**< Is this out_of_frame edit block? */
   Boolean discontinuous; /**< Is this OK to produce discontinuous SeqAlign? */
   GapEditScript* esp; /**< Editing script for the traceback. */
} GapEditBlock;

/** Preliminary version of GapEditBlock, used directly by the low-
 * level dynamic programming routines 
 */
typedef struct GapPrelimEditBlock {
    GapPrelimEditScript *edit_ops;  /**< array of edit operations */
    Int4 num_ops_allocated;        /**< size of allocated array */
    Int4 num_ops;                  /**< number of edit ops presently in use */
    EGapAlignOpType last_op;        /**< most recent operation added */
} GapPrelimEditBlock;

/** Structure to keep memory for state structure. */ 
typedef struct GapStateArrayStruct {
	Int4 	length,		/**< length of the state_array. */
		used;		/**< how much of length is used. */
	Uint1* state_array;	/**< array to be used. */
	struct GapStateArrayStruct* next; /**< Next link in the list. */
} GapStateArrayStruct;

/** Initialize the edit script structure. 
 *  @param old Pointer to existing edit script to which
 *         the new script will be appended
 *  @return Pointer to the new edit script
 */
GapEditScript* 
GapEditScriptNew (GapEditScript* old);

/** Free all links of an edit script structure. 
 *  @param esp Pointer to first link in the edit script [in]
 *  @return Always NULL
 */
GapEditScript* 
GapEditScriptDelete (GapEditScript* esp);

/** Initialize an edit block structure. 
 * @param start1 Offset to start alignment in first sequence. [in]
 * @param start2 Offset to start alignment in second sequence. [in]
 */
GapEditBlock* 
GapEditBlockNew (Int4 start1, Int4 start2);

/** Free an edit block structure. 
 *  @param edit_block The edit block to delete [in]
 *  @return Always NULL
 */
GapEditBlock* 
GapEditBlockDelete (GapEditBlock* edit_block);

/** Frees a preliminary edit block structure 
 *  @param edit_block The edit block to free [in]
 *  @return Always NULL
 */
GapPrelimEditBlock *
GapPrelimEditBlockFree(GapPrelimEditBlock *edit_block);

/** Allocates a preliminary edit block structure 
 *  @return Pointer to the allocated preliminary edit block
 */
GapPrelimEditBlock *
GapPrelimEditBlockNew(void);

/** Add a new operation to a preliminary edit block, possibly combining
 *  it with the last operation if the two operations are identical
 *
 *  @param edit_block The script to update [in/modified]
 *  @param op_type The operation type to add [in]
 *  @param num_ops The number of the specified type of operation to add [in]
 */
void
GapPrelimEditBlockAdd(GapPrelimEditBlock *edit_block, 
                 EGapAlignOpType op_type, Int4 num_ops);

/** Reset a preliminary edit block without freeing it 
 * @param edit_block The preliminary edit block to reset
 */
void
GapPrelimEditBlockReset(GapPrelimEditBlock *edit_block);

/** Free the gap state structure. 
 * @param state_struct The state structure to free
 * @return Always NULL
 */
GapStateArrayStruct* 
GapStateFree(GapStateArrayStruct* state_struct);

#ifdef __cplusplus
}
#endif
#endif /* !__GAPINFO__ */
