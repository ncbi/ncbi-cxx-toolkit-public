/* $Id$
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
 * Author:  Ilya Dondoshansky
 *
 */

/** @file blast_filter.h
 * BLAST filtering functions. @todo FIXME: contains more than filtering 
 * functions, combine with blast_dust.h?
 */

#ifndef __BLAST_FILTER__
#define __BLAST_FILTER__

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_message.h>

#ifdef __cplusplus
extern "C" {
#endif

/** BLASTNA element used to mask bases in BLAST */
extern const Uint1 kNuclMask;
/** NCBISTDAA element used to mask residues in BLAST */
extern const Uint1 kProtMask;

/** Repeats filtering default options. */
#define REPEATS_SEARCH_EVALUE 0.1       /**< Default e-value threshold */
#define REPEATS_SEARCH_PENALTY -1       /**< Default mismatch penalty */
#define REPEATS_SEARCH_GAP_OPEN 2       /**< Default gap opening cost */
#define REPEATS_SEARCH_GAP_EXTEND 1     /**< Default gap extension cost */
#define REPEATS_SEARCH_WORD_SIZE 11     /**< Default word size */
#define REPEATS_SEARCH_XDROP_UNGAPPED 40/**< Default X-dropoff for ungapped 
                                           extension */
#define REPEATS_SEARCH_XDROP_FINAL 90   /**< Default X-dropoff for gapped 
                                           extension with traceback */
#define REPEATS_SEARCH_FILTER_STRING "F"/**< Default filter string - 
                                           no filtering */

/** Largest gap allowed to be filled between repeat mask intervals */
#define REPEAT_MASK_LINK_VALUE 5

/** Create and initialize a new sequence interval.
 * @param head existing BlastSeqLoc to add onto, if *head
 *   is NULL then it will be set to new BlastSeqLoc, may be NULL [in|out]
 * @param from Start of the interval [in]
 * @param to End of the interval [in]
 * @return Pointer to the allocated BlastSeqLoc structure.
 */
NCBI_XBLAST_EXPORT
BlastSeqLoc* BlastSeqLocNew(BlastSeqLoc** head, Int4 from, Int4 to);

/** Deallocate all BlastSeqLoc objects in a chain.
 * @param loc object to be freed [in]
 * @return NULL pointer returned.
 */
NCBI_XBLAST_EXPORT
BlastSeqLoc* BlastSeqLocFree(BlastSeqLoc* loc);

/** Deallocate memory for a BlastMaskLoc structure
 * as well as the BlastSeqLoc's pointed to.
 * @param mask_loc the object to be deleted [in]
 * @return NULL pointer
 */
NCBI_XBLAST_EXPORT
BlastMaskLoc* BlastMaskLocFree(BlastMaskLoc* mask_loc);

/** Allocate memory for a BlastMaskLoc, also allocates array for BlastSeqLoc* of length total.
 * @param total which context (i.e., strand) [in]
 * @return Pointer to the allocated BlastMaskLoc structure.
*/
NCBI_XBLAST_EXPORT
BlastMaskLoc* BlastMaskLocNew(Int4 total);

/** Given a BlastMaskLoc with an array of lists of DNA mask locations, 
 * substitutes that array by a new array of per-protein-frame mask location 
 * lists.
 * @param mask_loc Mask locations structure [in|out]
 * @param query_info Query information structure, containing contexts data [in]
 */
Int2 BlastMaskLocDNAToProtein(BlastMaskLoc* mask_loc, 
                              const BlastQueryInfo* query_info);

/** Given a BlastMaskLoc with an array of lists of mask locations per protein
 * frame, recalculates all mask offsets in terms of the DNA sequence. 
 * @param mask_loc Mask locations structure [in|out]
 * @param query_info Query information structure, containing contexts data [in]
 */
Int2 BlastMaskLocProteinToDNA(BlastMaskLoc* mask_loc, 
                              const BlastQueryInfo* query_info);

/** Go through all mask locations in one sequence, 
 * combine any that overlap. Deallocate the memory for the locations that 
 * were on the list, produce a new (merged) list of locations. 
 * @param mask_loc The list of masks to be merged [in] 
 * @param mask_loc_out The new (merged) list of masks. [out]
 * @param link_value Largest gap size between locations fow which they
 *                   should be linked together [in] 
*/
NCBI_XBLAST_EXPORT
Int2
CombineMaskLocations(BlastSeqLoc* mask_loc, BlastSeqLoc* *mask_loc_out,
                     Int4 link_value);

/** This function takes the list of mask locations (i.e., regions that 
 * should not be searched or not added to lookup table) and makes up a set 
 * of SSeqRange*'s in the concatenated sequence built from a set of queries,
 * that should be searched (that is, takes the complement). 
 * If all sequences in the query set are completely filtered, then an
 * SSeqRange is created and both of its elements (left and right) are set to 
 * -1 to indicate this. 
 * If any of the mask_loc's is NULL, an SSeqRange for the full span of the 
 * respective query sequence is created.
 * @param program_number Type of BLAST program [in]
 * @param query_info The query information structure [in]
 * @param mask_loc All mask locations [in]
 * @param complement_mask Linked list of SSeqRange*s in the concatenated 
 *                        sequence to be indexed in the lookup table . [out]
 */
NCBI_XBLAST_EXPORT
Int2 
BLAST_ComplementMaskLocations(EBlastProgramType program_number, 
   BlastQueryInfo* query_info, BlastMaskLoc* mask_loc, 
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
NCBI_XBLAST_EXPORT
Int2
BlastSetUp_Filter(EBlastProgramType program_number, 
    Uint1* sequence, 
    Int4 length, 
    Int4 offset, 
    const char* instructions, 
    Boolean *mask_at_hash, 
    BlastSeqLoc* *seqloc_retval);


/** Does preparation for filtering and then calls BlastSetUp_Filter
 * @param query_blk sequence to be filtered [in]
 * @param query_info info on sequence to be filtered [in]
 * @param program_number one of blastn,blastp,blastx,etc. [in]
 * @param filter_string instructions for filtering [in]
 * @param filter_out resulting locations for filtered region. [out]
 * @param mask_at_hash If TRUE masking is done while making the lookup table
 *                     only. [out] 
 * @param blast_message message that needs to be sent back to user.
*/
NCBI_XBLAST_EXPORT
Int2
BlastSetUp_GetFilteringLocations(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info,
    EBlastProgramType program_number, const char* filter_string, BlastMaskLoc** filter_out, Boolean* mask_at_hash,
    Blast_Message* *blast_message);

/** Masks the letters in buffer.
 * This is a low-level routine and takes a raw buffer which it assumes
 * to be in ncbistdaa (protein) or blastna (nucleotide).
 * @param buffer the sequence to be masked (will be modified). [out]
 * @param length length of the sequence to be masked . [in]
 * @param is_na nucleotide if TRUE [in]
 * @param mask_loc the BlastSeqLoc to use for masking [in] 
 * @param reverse minus strand if TRUE [in]
 * @param offset how far along sequence is 1st residuse in buffer [in]
 *
*/
NCBI_XBLAST_EXPORT
Int2
Blast_MaskTheResidues(Uint1 * buffer, Int4 length, Boolean is_na, 
    BlastSeqLoc* mask_loc, Boolean reverse, Int4 offset);

/** Masks the sequence given a BlastMaskLoc
 * @param query_blk sequence to be filtered [in]
 * @param query_info info on sequence to be filtered [in]
 * @param filter_maskloc Locations to filter [in]
 * @param program_number one of blastn,blastp,blastx,etc. [in]
*/
NCBI_XBLAST_EXPORT
Int2
BlastSetUp_MaskQuery(BLAST_SequenceBlk* query_blk, BlastQueryInfo* query_info,
    BlastMaskLoc *filter_maskloc, EBlastProgramType program_number);


/** Parses out thre repeats filtering option from a filter string option. 
 * @param filter_string Full filtering string, e.g. "m L;R -d rodents.lib" [in]
 * @return Part of filtering string corresponding to repeats filtering.
 */
char* Blast_GetRepeatsFilterOption(const char* filter_string);

/** Extracts repeats database name from the repeat filtering options string.
 * @param repeat_options Repeat filtering string, e.g. "R -d rodents.lib" [in]
 * @param dbname Database name. Human repeats database "humrep" is returned if 
 *               database is not specified in the option. [out]
 */
void 
Blast_ParseRepeatOptions(const char* repeat_options, char** dbname);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_FILTER__ */
