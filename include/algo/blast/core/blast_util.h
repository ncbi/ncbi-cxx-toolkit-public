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

/** @file blast_util.h
 * Various auxiliary BLAST utility functions
 */

#ifndef __BLAST_UTIL__
#define __BLAST_UTIL__

#include <algo/blast/core/blast_def.h>

#ifdef NCBI_DLL_BUILD
#include <algo/blast/core/blast_export.h>
#endif
#ifndef NCBI_XBLAST_EXPORT
#define NCBI_XBLAST_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** Different types of sequence encodings for sequence retrieval from the 
 * BLAST database 
 */
#define BLASTP_ENCODING 0  /**< NCBIstdaa */
#define BLASTNA_ENCODING 1 /**< Special encoding for preliminary stage of 
                              BLAST: permutation of NCBI8na */
#define NCBI4NA_ENCODING 2 /**< NCBI8na */
#define NCBI2NA_ENCODING 3 /**< NCBI2na */
#define ERROR_ENCODING 255 /**< Error value for encoding */
/**< Does character encode a residue? */
#ifndef IS_residue
#define IS_residue(x) (x <= 250)
#endif

/** Bit mask for obtaining a single base from a byte in ncbi2na format */
#define NCBI2NA_MASK 0x03

/** Macro to extract base N from a byte x (N >= 0, N < 4) */
#define NCBI2NA_UNPACK_BASE(x, N) (((x)>>(2*(N))) & NCBI2NA_MASK)


/** Deallocate memory only for the sequence in the sequence block */
Int2 BlastSequenceBlkClean(BLAST_SequenceBlk* seq_blk);
   
/** Deallocate memory for a sequence block */
BLAST_SequenceBlk* BlastSequenceBlkFree(BLAST_SequenceBlk* seq_blk);

/** Copies contents of the source sequence block without copying sequence 
 * buffers; sets all "field_allocated" booleans to FALSE, to make sure 
 * fields are not freed on the call to BlastSequenceBlkFree.
 * @param copy New sequence block [out]
 * @param src Input sequence block [in]
 */
void BlastSequenceBlkCopy(BLAST_SequenceBlk** copy, 
                          BLAST_SequenceBlk* src);

/** Set number for a given program type.  Return is zero on success.
 * @param program string name of program [in]
 * @param number Enumerated value of program [out]
*/
NCBI_XBLAST_EXPORT
Int2 BlastProgram2Number(const char *program, EBlastProgramType *number);

/** Return string name for program given a number.  Return is zero on success.
 * @param number Enumerated value of program [in]
 * @param program string name of program (memory should be deallocated by called) [out]
*/
Int2 BlastNumber2Program(EBlastProgramType number, char* *program);

/** Allocates memory for *sequence_blk and then populates it.
 * @param buffer start of sequence [in]
 * @param length query sequence length [in]
 * @param context context number [in]
 * @param seq_blk SequenceBlk to be allocated and filled in [out]
 * @param buffer_allocated Is the buffer allocated? If yes, 'sequence_start' is
 *        the start of the sequence, otherwise it is 'sequence'. [in]
 * @deprecated Use BlastSeqBlkNew and BlastSeqBlkSet* functions instead
*/
Int2
BlastSetUp_SeqBlkNew (const Uint1* buffer, Int4 length, Int4 context,
	BLAST_SequenceBlk* *seq_blk, Boolean buffer_allocated);

/** Allocates a new sequence block structure. 
 * @param retval Pointer to where the sequence block structure will be
 * allocated [out]
 */
Int2 BlastSeqBlkNew(BLAST_SequenceBlk** retval);

/** Stores the sequence in the sequence block structure.
 * @param seq_blk The sequence block structure to modify [in/out]
 * @param sequence Actual sequence buffer. The first byte must be a sentinel
 * byte [in]
 * @param seqlen Length of the sequence buffer above [in]
 */
Int2 BlastSeqBlkSetSequence(BLAST_SequenceBlk* seq_blk, 
                            const Uint1* sequence,
                            Int4 seqlen);

/** Stores the compressed nucleotide sequence in the sequence block structure
 * for the subject sequence when BLASTing 2 sequences. This sequence should be
 * encoded in NCBI2NA_ENCODING and NOT have sentinel bytes.
 * @param seq_blk The sequence block structure to modify [in/out]
 * @param sequence Actual sequence buffer. [in]
 */
Int2 BlastSeqBlkSetCompressedSequence(BLAST_SequenceBlk* seq_blk,
                                      const Uint1* sequence);
                            

/** GetTranslation to get the translation of the nucl. sequence in the
 * appropriate frame and with the appropriate GeneticCode.
 * The function return an allocated char*, the caller must delete this.
 * The first and last spaces of this char* contain NULLB's.
 * @param query_seq Forward strand of the nucleotide sequence [in]
 * @param query_seq_rev Reverse strand of the nucleotide sequence [in]
 * @param nt_length Length of the nucleotide sequence [in]
 * @param frame What frame to translate into? [in]
 * @param buffer Preallocated buffer for the translated sequence [in][out]
 * @param genetic_code Genetic code to use for translation, 
 *                     in ncbistdaa encoding [in]
 * @return Length of the traslated protein sequence.
*/
Int4 BLAST_GetTranslation(const Uint1* query_seq, 
   const Uint1* query_seq_rev, Int4 nt_length, Int2 frame, Uint1* buffer, 
   const Uint1* genetic_code);



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
Int4 BLAST_TranslateCompressedSequence(Uint1* translation, Int4 length, 
        const Uint1* nt_seq, Int2 frame, Uint1* prot_seq);

/** Reverse a nucleotide sequence in the blastna encoding, adding sentinel 
 * bytes on both ends.
 * @param sequence Forward strand of the sequence [in]
 * @param length Length of the sequence plus 1 for the sentinel byte [in]
 * @param rev_sequence_ptr Reverse strand of the sequence [out]
 */
Int2 GetReverseNuclSequence(const Uint1* sequence, Int4 length, 
                            Uint1** rev_sequence_ptr);

/** This function translates the context number of a context into the frame of 
 * the sequence.
 * @param prog_number Integer corresponding to the BLAST program
 * @param context_number Context number 
 * @return Sequence frame (+-1 for nucleotides, -3..3 for translations)
*/
Int2 BLAST_ContextToFrame(EBlastProgramType prog_number, Int4 context_number);

/** Given a context from BLAST engine core, return the query index.
 * @param context Context saved in a BlastHSP structure [in]
 * @param program Type of BLAST program [in]
 * @return Query index in a set of queries.
 */
Int4 Blast_GetQueryIndexFromContext(Int4 context, EBlastProgramType program);

/** Find the length of an individual query within a concatenated set of 
 * queries.
 * @param query_info Queries information structure containing offsets into
 *                   the concatenated sequence [in]
 * @param context Index of the query/strand/frame within the concatenated 
 *                set [in]
 * @return Length of the individual sequence/strand/frame.
 */
Int4 BLAST_GetQueryLength(const BlastQueryInfo* query_info, Int4 context);

/** Deallocate memory for query information structure */
BlastQueryInfo* BlastQueryInfoFree(BlastQueryInfo* query_info);

/** Duplicates the query information structure */
BlastQueryInfo* BlastQueryInfoDup(BlastQueryInfo* query_info);

Int2 BLAST_PackDNA(Uint1* buffer, Int4 length, Uint1 encoding, 
                   Uint1** packed_seq);

/** Initialize the mixed-frame sequence for out-of-frame gapped extension.
 * @param query_blk Sequence block containing the concatenated frames of the 
 *                  query. The mixed-frame sequence is saved here. [in] [out]
 * @param query_info Query information structure containing offsets into the* 
 *                   concatenated sequence. [in]
 */
Int2 BLAST_InitDNAPSequence(BLAST_SequenceBlk* query_blk, 
                       BlastQueryInfo* query_info);

/** Translate nucleotide into 6 frames. All frames are put into a 
 * translation buffer, with sentinel NULLB bytes in between.
 * Array of offsets into the translation buffer is also returned.
 * For out-of-frame gapping option, a mixed frame sequence is created.
 * @param nucl_seq The nucleotide sequence [in] 
 * @param encoding Sequence encoding: ncbi2na or ncbi4na [in]
 * @param nucl_length Length of the nucleotide sequence [in]
 * @param genetic_code The genetic code to be used for translations,
 *                     in ncbistdaa encoding [in]
 * @param translation_buffer_ptr Buffer to hold the frames of the translated 
 *                               sequence. [out]
 * @param frame_offsets_ptr Offsets into the translation buffer for each 
 *                          frame. [out]
 * @param mixed_seq_ptr Pointer to buffer for the mixed frame sequence [out]
 */
Int2 BLAST_GetAllTranslations(const Uint1* nucl_seq, Uint1 encoding,
        Int4 nucl_length, const Uint1* genetic_code, 
        Uint1** translation_buffer_ptr, Int4** frame_offsets_ptr,
        Uint1** mixed_seq_ptr);

/** Get one frame translation - needed when only parts of subject sequences
 * are translated. 
 * @param nucl_seq Pointer to start of nucleotide sequence to be translated [in]
 * @param nucl_length Length of nucleotide sequence to be translated [in]
 * @param frame What frame to translate into [in]
 * @param genetic_code What genetic code to use? [in]
 * @param translation_buffer_ptr Pointer to buffer with translated 
 *                               sequence [out]
 * @param protein_length Length of the translation buffer [out] 
 * @param mixed_seq_ptr Pointer to buffer with mixed frame sequence, in case
 *                      of out-of-frame gapping; buffer filled only if argument
 *                      not NULL. [out]
 */
int GetPartialTranslation(const Uint1* nucl_seq,
        Int4 nucl_length, Int2 frame, const Uint1* genetic_code,
        Uint1** translation_buffer_ptr, Int4* protein_length, 
        Uint1** mixed_seq_ptr);


/** Convert translation frame into a context for the concatenated translation
 * buffer.
 */
Int4 FrameToContext(Int2 frame);


/** The following binary search routine assumes that array A is filled. */
Int4 BSearchInt4(Int4 n, Int4* A, Int4 size);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_UTIL__ */
