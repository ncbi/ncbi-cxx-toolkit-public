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

File name: blast_util.h

Author: Ilya Dondoshansky

Contents: Various auxiliary BLAST utility functions

Detailed Contents: 

******************************************************************************
 * $Revision$
 * */
#ifndef __BLAST_UTIL__
#define __BLAST_UTIL__

#ifdef __cplusplus
extern "C" {
#endif

#include <blast_def.h>

/** Different types of sequence encodings for sequence retrieval from the 
 * BLAST database 
 */
#define BLASTP_ENCODING 0
#define BLASTNA_ENCODING 1
#define NCBI4NA_ENCODING 2
#define NCBI2NA_ENCODING 3
#define ERROR_ENCODING 255
#ifndef IS_residue
#define IS_residue(x) (x <= 250)
#endif
#if 0
/** Retrieve a sequence from the BLAST database
 * @param db BLAST database [in]
 * @param seq_ptr Pointer to sequence buffer [out]
 * @param oid Ordinal id of the sequence to be retrieved [in]
 * @param encoding In what encoding should the sequence be retrieved? [in]
 */ 
void
MakeBlastSequenceBlk(ReadDBFILEPtr db, BLAST_SequenceBlkPtr PNTR seq_ptr,
                     Int4 oid, Uint1 encoding);
#endif

/** Deallocate memory only for the sequence in the sequence block */
Int2 BlastSequenceBlkClean(BLAST_SequenceBlkPtr seq_blk);
   
/** Deallocate memory for a sequence block */
BLAST_SequenceBlkPtr BlastSequenceBlkFree(BLAST_SequenceBlkPtr seq_blk);

/** Set number for a given program type.  Return is zero on success.
 * @param program string name of program [in]
 * @param number number of program [out]
*/
Int2 BlastProgram2Number(const Char *program, Uint1 *number);

/** Return string name for program given a number.  Return is zero on success.
 * @param number number of program [in]
 * @param program string name of program (memory should be deallocated by called) [out]
*/
Int2 BlastNumber2Program(Uint1 number, CharPtr *program);

/** Allocates memory for *sequence_blk and then populates it.
 * @param buffer start of sequence [in]
 * @param length query sequence length [in]
 * @param context context number [in]
 * @param seq_blk SequenceBlk to be allocated and filled in [out]
 * @param buffer_allocated Is the buffer allocated? If yes, 'sequence_start' is
 *        the start of the sequence, otherwise it is 'sequence'. [in]
*/
Int2
BlastSetUp_SeqBlkNew (const Uint1Ptr buffer, Int4 length, Int2 context,
	BLAST_SequenceBlkPtr *seq_blk, Boolean buffer_allocated);

/** Allocate and initialize the query information structure.
 * @param program_number Type of BLAST program [in]
 * @param num_queries Number of query sequences [in]
 * @param query_info_ptr The initialized structure [out]
 */
Int2 BLAST_QueryInfoInit(Uint1 program_number, 
        Int4 num_queries, BlastQueryInfoPtr *query_info_ptr);

/** GetTranslation to get the translation of the nucl. sequence in the
 * appropriate frame and with the appropriate GeneticCode.
 * The function return an allocated CharPtr, the caller must delete this.
 * The first and last spaces of this CharPtr contain NULLB's.
 * @param query_seq Forward strand of the nucleotide sequence [in]
 * @param query_seq_rev Reverse strand of the nucleotide sequence [in]
 * @param nt_length Length of the nucleotide sequence [in]
 * @param frame What frame to translate into? [in]
 * @param buffer Preallocated buffer for the translated sequence [in][out]
 * @param genetic_code Genetic code to use for translation, 
 *                     in ncbistdaa encoding [in]
 * @return Length of the traslated protein sequence.
*/
Int4 LIBCALL
BLAST_GetTranslation(Uint1Ptr query_seq, Uint1Ptr query_seq_rev, 
   Int4 nt_length, Int2 frame, Uint1Ptr buffer, const Uint1Ptr genetic_code);

/** Translate a nucleotide sequence without ambiguity codes.
 * This is used for the first-pass translation of the database.
 * The genetic code to be used is determined by the translation_table
 * This function translates a packed (ncbi2na) nucl. alphabet.  It
 * views a basepair as being in one of four sets of 2-bits:
 * |0|1|2|3||0|1|2|3||0|1|2|3||...
 *
 * 1st byte | 2 byte | 3rd byte...
 *
 * A codon that starts at the beginning of the above sequence starts in
 * state "0" and includes basepairs 0, 1, and 2.  The next codon, in the
 * same frame, after that starts in state "3" and includes 3, 0, and 1.
 *
 *** Optimization:
 * changed the single main loop to 
 * - advance to state 0, 
 * - optimized inner loop does two (3 byte->4 codon) translation per iteration
 *   (loads are moved earlier so they can be done in advance.)
 * - do remainder
 *
 * @param translation The translation table [in]
 * @param length Length of the nucleotide sequence [in]
 * @param nt_seq The original nucleotide sequence [in]
 * @param frame What frame to translate to? [in]
 * @param prot_seq Preallocated buffer for the (translated) protein sequence, 
 *                 with NULLB sentinels on either end. [out]
*/
Int4 LIBCALL
BLAST_TranslateCompressedSequence(Uint1Ptr translation, Int4 length, 
   Uint1Ptr nt_seq, Int2 frame, Uint1Ptr prot_seq);

/** Reverse a nucleotide sequence in the blastna encoding, adding sentinel 
 * bytes on both ends.
 * @param sequence Forward strand of the sequence [in]
 * @param length Length of the sequence plus 1 for the sentinel byte [in]
 * @param rev_sequence_ptr Reverse strand of the sequence [out]
 */
Int2 GetReverseNuclSequence(Uint1Ptr sequence, Int4 length, 
                            Uint1Ptr PNTR rev_sequence_ptr);

#if 0
/** CC: Moved to blast_engine.c as static functions? */
/** Initialize the thread information structure
 * @param last_oid2search Last ordinal id to examine in a database search.
 */
BlastThrInfoPtr BLAST_ThrInfoNew(Int4 last_oid2search);

/** Deallocate the thread information structure */
void BLAST_ThrInfoFree(BlastThrInfoPtr thr_info);

/** Gets the next set of sequences from the database to search.
 * @param rdfp The BLAST database(s) being searched [in]
 * @param start The first ordinal id in the chunk [out]
 * @param stop The last ordinal id in the chunk, plus 1 [out]
 * @param id_list List of ordinal ids to search in case of masked 
 *                database(s) [out]
 * @param id_list_number Number of sequences in id_list [out]
 * @param thr_info Keeps track of what sequences of the databases have already
 *                 been searched [in] [out]
 * @return Is this the last chunk of the database? 
*/
Boolean 
BLAST_GetDbChunk(ReadDBFILEPtr rdfp, Int4Ptr start, Int4Ptr stop, 
                Int4Ptr id_list, Int4Ptr id_list_number, BlastThrInfoPtr thr_info);
#endif

/** This function translates the context number of a context into the frame of 
 * the sequence.
 * @param prog_number Integer corresponding to the BLAST program
 * @param context_number Context number 
 * @return Sequence frame (+-1 for nucleotides, -3..3 for translations)
*/
Int2 BLAST_ContextToFrame(Uint1 prog_number, Int2 context_number);

/** Find the length of an individual query within a concatenated set of 
 * queries.
 * @param query_info Queries information structure containing offsets into
 *                   the concatenated sequence [in]
 * @param context Index of the query/strand/frame within the concatenated 
 *                set [in]
 * @return Length of the individual sequence/strand/frame.
 */
Int4 BLAST_GetQueryLength(BlastQueryInfoPtr query_info, Int4 context);

/** Deallocate memory for query information structure */
BlastQueryInfoPtr BlastQueryInfoFree(BlastQueryInfoPtr query_info);

Int2 BLAST_PackDNA(Uint1Ptr buffer, Int4 length, Uint1 encoding, 
                   Uint1Ptr PNTR packed_seq);

/** Initialize the mixed-frame sequence for out-of-frame gapped extension.
 * @param query_blk Sequence block containing the concatenated frames of the 
 *                  query. The mixed-frame sequence is saved here. [in] [out]
 * @param query_info Query information structure containing offsets into the* 
 *                   concatenated sequence. [in]
 */
Int2 BLAST_InitDNAPSequence(BLAST_SequenceBlkPtr query_blk, 
                       BlastQueryInfoPtr query_info);

/** Translate nucleotide into 6 frames. All frames are put into a 
 * translation buffer, with sentinel NULLB bytes in between.
 * Array of offsets into the translation buffer is also returned.
 * For out-of-frame gapping option, a mixed frame sequence is created.
 * @param nucl_seq The nucleotide sequence [in] 
 * @param encoding Sequence encoding: ncbi2na or ncbi4na [in]
 * @param nucl_length Length of the nucleotide sequence [in]
 * @param genetic_code The genetic code to be used for translations,
 *                     in ncbistdaa encoding [in]
 * @param translation_buffer_ptr Buffer to hold all translated frames [out]
 * @param frame_offsets_ptr Offsets into the translation buffer for each 
 *                          frame [out]
 * @param mixed_seq_ptr If not NULL, will hold a mixed frame sequence for 
 *                      out-of-frame gapping [out]
 */
Int2 BLAST_GetAllTranslations(const Uint1Ptr nucl_seq, Uint1 encoding,
        Int4 nucl_length, Uint1Ptr genetic_code, 
        Uint1Ptr PNTR translation_buffer_ptr, Int4Ptr PNTR frame_offsets_ptr,
        Uint1Ptr PNTR mixed_seq_ptr);

/** Convert translation frame into a context for the concatenated translation
 * buffer.
 */
Int2 FrameToContext(Int2 frame);


#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_UTIL__ */
