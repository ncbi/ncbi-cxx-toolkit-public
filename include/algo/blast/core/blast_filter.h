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

Contents: BLAST filtering functions.

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_FILTER__
#define __BLAST_FILTER__

#ifdef __cplusplus
extern "C" {
#endif

#include <algo/blast/core/blast_def.h>

/** Create and initialize a new sequence interval.
 * @param from Start of the interval [in]
 * @param to End of the interval [in]
 * @return Pointer to the allocated BlastSeqLoc structure.
 */
BlastSeqLoc* BlastSeqLocNew(Int4 from, Int4 to);

/** Deallocate a BlastSeqLoc structure */
BlastSeqLoc* BlastSeqLocFree(BlastSeqLoc* loc);

/** Deallocate memory for a list of BlastMask structures */
BlastMask* BlastMaskFree(BlastMask* mask_loc);

/** Go through all mask locations in one sequence, 
 * combine any that overlap. Deallocate the memory for the locations that 
 * were on the list, produce a new (merged) list of locations. 
 * @param mask_loc The list of masks to be merged [in] 
 * @param mask_loc_out The new (merged) list of masks. [out]
*/
Int2
CombineMaskLocations(BlastSeqLoc* mask_loc, BlastSeqLoc* *mask_loc_out);

/** This function takes the list of mask locations (i.e., regions that 
 * should not be searched or not added to lookup table) and makes up a set 
 * of DoubleInt*'s that should be searched (that is, takes the 
 * complement). If the entire sequence is filtered, then a DoubleInt is 
 * created and both of its elements (i1 and i2) are set to -1 to indicate 
 * this. 
 * If any of the mask_loc's is NULL, a DoubleInt for full span of the 
 * respective query sequence is created.
 * @param program_number Type of BLAST program [in]
 * @param query_info The query information structure [in]
 * @param mask_loc All mask locations [in]
 * @param complement_mask Linked list of DoubleInt*s with offsets. [out]
*/
Int2 
BLAST_ComplementMaskLocations(Uint1 program_number, 
   BlastQueryInfo* query_info, BlastMask* mask_loc, 
   BlastSeqLoc* *complement_mask);

/** Runs filtering functions, according to the string "instructions", on the
 * SeqLocPtr. Should combine all SeqLocs so they are non-redundant.
 * @param program_number Type of BLAST program [in]
 * @param sequence The sequence or part of the sequence to be filtered [in]
 * @param length Length of the (sub)sequence [in]
 * @param offset Offset into the full sequence [in]
 * @param instructions String of instructions to filtering functions. [in]
 * @param mask_at_hash If TRUE masking is done while making the lookup table
 *                     only. [out] 
 * @param seqloc_retval Resulting locations for filtered region. [out]
*/
Int2
BlastSetUp_Filter(Uint1 program_number, Uint1* sequence, Int4 length, 
   Int4 offset, char* instructions, Boolean *mask_at_hash, 
   BlastSeqLoc* *seqloc_retval);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
