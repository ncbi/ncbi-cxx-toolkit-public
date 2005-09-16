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
#include <algo/blast/core/blast_encoding.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef IS_residue
/** Does character encode a residue? */
#define IS_residue(x) (x <= 250)
#endif

/** Bit mask for obtaining a single base from a byte in ncbi2na format */
#define NCBI2NA_MASK 0x03

/** Macro to extract base N from a byte x (N >= 0, N < 4) */
#define NCBI2NA_UNPACK_BASE(x, N) (((x)>>(2*(N))) & NCBI2NA_MASK)


/** Deallocate memory only for the sequence in the sequence block */
NCBI_XBLAST_EXPORT
void BlastSequenceBlkClean(BLAST_SequenceBlk* seq_blk);
   
/** Deallocate memory for a sequence block */
NCBI_XBLAST_EXPORT
BLAST_SequenceBlk* BlastSequenceBlkFree(BLAST_SequenceBlk* seq_blk);

/** Copies contents of the source sequence block without copying sequence 
 * buffers; sets all "field_allocated" booleans to FALSE, to make sure 
 * fields are not freed on the call to BlastSequenceBlkFree.
 * @param copy New sequence block [out]
 * @param src Input sequence block [in]
 */
NCBI_XBLAST_EXPORT
void BlastSequenceBlkCopy(BLAST_SequenceBlk** copy, 
                          BLAST_SequenceBlk* src);

/** Set number for a given program type.  Return is zero on success.
 * @param program string name of program [in]
 * @param number Enumerated value of program [out]
*/
Int2 BlastProgram2Number(const char *program, EBlastProgramType *number);

/** Return string name for program given a number.  Return is zero on success.
 * @param number Enumerated value of program [in]
 * @param program string name of program (memory should be deallocated by called) [out]
*/
NCBI_XBLAST_EXPORT
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
NCBI_XBLAST_EXPORT
Int2
BlastSetUp_SeqBlkNew (const Uint1* buffer, Int4 length, Int4 context,
    BLAST_SequenceBlk* *seq_blk, Boolean buffer_allocated);

/** Allocates a new sequence block structure. 
 * @param retval Pointer to where the sequence block structure will be
 * allocated [out]
 */
NCBI_XBLAST_EXPORT
Int2 BlastSeqBlkNew(BLAST_SequenceBlk** retval);

/** Stores the sequence in the sequence block structure.
 * @param seq_blk The sequence block structure to modify [in/out]
 * @param sequence Actual sequence buffer. The first byte must be a sentinel
 * byte [in]
 * @param seqlen Length of the sequence buffer above [in]
 */
NCBI_XBLAST_EXPORT
Int2 BlastSeqBlkSetSequence(BLAST_SequenceBlk* seq_blk, 
                            const Uint1* sequence,
                            Int4 seqlen);

/** Stores the compressed nucleotide sequence in the sequence block structure
 * for the subject sequence when BLASTing 2 sequences. This sequence should be
 * encoded in eBlastEncodingNcbi2na and NOT have sentinel bytes (as this 
 * encoding doesn't allow them).
 * @param seq_blk The sequence block structure to modify [in/out]
 * @param sequence Actual sequence buffer. [in]
 */
NCBI_XBLAST_EXPORT
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
NCBI_XBLAST_EXPORT
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
NCBI_XBLAST_EXPORT
Int4 BLAST_TranslateCompressedSequence(Uint1* translation, Int4 length, 
        const Uint1* nt_seq, Int2 frame, Uint1* prot_seq);

/** Reverse a nucleotide sequence in the blastna encoding, adding sentinel 
 * bytes on both ends.
 * @param sequence Forward strand of the sequence [in]
 * @param length Length of the sequence plus 1 for the sentinel byte [in]
 * @param rev_sequence_ptr Reverse strand of the sequence [out]
 */
NCBI_XBLAST_EXPORT
Int2 GetReverseNuclSequence(const Uint1* sequence, Int4 length, 
                            Uint1** rev_sequence_ptr);

/** This function translates the context number of a context into the frame of 
 * the sequence.
 * @param prog_number Integer corresponding to the BLAST program
 * @param context_number Context number 
 * @return Sequence frame: -1,1 for nucleotides, -3,-2,-1,1,2,3 for 
 * translations, 0 for proteins and INT1_MAX in case of unsupported program
*/
NCBI_XBLAST_EXPORT
Int1 BLAST_ContextToFrame(EBlastProgramType prog_number, Uint4 context_number);

/** Given a context from BLAST engine core, return the query index.
 * @param context Context saved in a BlastHSP structure [in]
 * @param program Type of BLAST program [in]
 * @return Query index in a set of queries or -1 on error
 */
NCBI_XBLAST_EXPORT
Int4 Blast_GetQueryIndexFromContext(Int4 context, EBlastProgramType program);

/** Deallocate memory for query information structure */
NCBI_XBLAST_EXPORT
BlastQueryInfo* BlastQueryInfoFree(BlastQueryInfo* query_info);

/** Duplicates the query information structure */
NCBI_XBLAST_EXPORT
BlastQueryInfo* BlastQueryInfoDup(BlastQueryInfo* query_info);

/** Obtains the sequence length for a given query in the query, without taking
 * into consideration any applicable translations 
 * @param qinfo BlastQueryInfo structure [in]
 * @param program CORE program type [in]
 * @param query_index number of the query 
 * (query_index < BlastQueryInfo::num_queries) [in]
 * @return the length of the query sequence requested
 */
Int4 BlastQueryInfoGetQueryLength(const BlastQueryInfo* qinfo,
                                  EBlastProgramType program,
                                  Int4 query_index);

/** Create auxiliary query structures with all data corresponding
 * to a single query sequence within a concatenated set. Allocates the 
 * structures if the pointers are NULL on input; otherwise only changes the
 * contents.
 * @param one_query_info_ptr Pointer to the query information structure for a 
 *                           single query. Allocated and filled here, so the
 *                           caller of this function will be responsible for
 *                           freeing it. [out]
 * @param one_query_ptr Pointer to the query sequence block structure; allocated
 *                      here, but the contents are not allocated; it is still 
 *                      safe to free by the caller after use. [out]
 * @param query_info Query information structure containing information about a 
 *                   concatenated set. [in]
 * @param query Query sequence block corresponding to a concatenated set of 
 *              queries. [in]
 * @param query_index Which query index to create the auxiliary structures 
 *                    for? [in]
 * @return -1 if memory allocation failed; 0 on success
 */
NCBI_XBLAST_EXPORT
Int2 Blast_GetOneQueryStructs(BlastQueryInfo** one_query_info_ptr, 
                              BLAST_SequenceBlk** one_query_ptr,
                              const BlastQueryInfo* query_info, 
                              BLAST_SequenceBlk* query, Int4 query_index);


/** Convert a sequence in ncbi4na or blastna encoding into a packed sequence
 * in ncbi2na encoding. Needed for 2 sequences BLASTn comparison.
 * @param buffer original sequence data (one base per byte) [in]
 * @param length length of the sequence data above [in]
 * @param encoding source encoding of the sequence data above [in]
 * @param packed_seq output buffer containing compressed sequence. Its length
 * will be (length/COMPRESSION_RATIO + 1), caller is responsible for
 * deallocating it [out]
 * @return 0 in case of success, -1 in case of memory allocation failure
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_PackDNA(const Uint1* buffer, Int4 length, 
                   EBlastEncoding encoding, Uint1** packed_seq);

/** Initialize the mixed-frame sequence for out-of-frame gapped extension.
 * @param query_blk Sequence block containing the concatenated frames of the 
 *                  query. The mixed-frame sequence is saved here. [in] [out]
 * @param query_info Query information structure containing offsets into the* 
 *                   concatenated sequence. [in]
 */
NCBI_XBLAST_EXPORT
Int2 BLAST_InitDNAPSequence(BLAST_SequenceBlk* query_blk, 
                            const BlastQueryInfo* query_info);

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
NCBI_XBLAST_EXPORT
Int2 BLAST_GetAllTranslations(const Uint1* nucl_seq, EBlastEncoding encoding,
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
NCBI_XBLAST_EXPORT
int Blast_GetPartialTranslation(const Uint1* nucl_seq,
        Int4 nucl_length, Int2 frame, const Uint1* genetic_code,
        Uint1** translation_buffer_ptr, Int4* protein_length, 
        Uint1** mixed_seq_ptr);


/** Convert translation frame into a context for the concatenated translation
 * buffer.
 */
NCBI_XBLAST_EXPORT
Int4 FrameToContext(Int2 frame);


/** The following binary search routine assumes that array A is filled. */
NCBI_XBLAST_EXPORT
Int4 BSearchInt4(Int4 n, Int4* A, Int4 size);


/** Search BlastContextInfo structures for the specified offset */
NCBI_XBLAST_EXPORT
Int4 BSearchContextInfo(Int4 n, BlastQueryInfo * A);


/** Get the number of bytes required for the concatenated sequence
 * buffer, given a query info structure.  The context data should
 * already be assigned.
 * @param qinfo  Query info structure. [in/out]
 * @return Number of bytes for all queries and inter-query marks.
 */
Uint4
QueryInfo_GetSeqBufLen(const BlastQueryInfo* qinfo);


/** Copy the context query offsets to an allocated array of Int4.
 * @param info Describes the concatenated query.
 * @return Allocated array.
 */
NCBI_XBLAST_EXPORT
Int4 * ContextOffsetsToOffsetArray(BlastQueryInfo* info);


/** Copy the context query offsets from an array of Int4, allocating
 * the context array if needed.
 * @param info Destination for the values.
 * @param new_offsets Array of values to copy from.
 * @param prog        The blast program type.
 */
NCBI_XBLAST_EXPORT
void OffsetArrayToContextOffsets(BlastQueryInfo    * info,
                                 Int4              * new_offsets,
                                 EBlastProgramType   prog);


/** Get the standard amino acid probabilities. This is basically a wrapper for 
 * BlastScoreBlkNew() and Blast_ResFreqStdComp() from blast_stat.c with a more
 * intention-revealing name :)
 * Caller is responsible for deallocating return value via sfree().
 * @return NULL if there is not enough memory otherwise an array of length
 * BLASTAA_SIZE, where each index corresponds to an amino acid as specified in
 * the NCBIstdaa encoding.
 */
NCBI_XBLAST_EXPORT
double* 
BLAST_GetStandardAaProbabilities(void);

/** Returns a copy of the input string with all its characters turned to
 * uppercase. Useful for saving score matrix names. Caller is responsible for
 * deallocating return value.
 * @param string string to copy [in]
 * @return newly allocated string in upper case or NULL if string is NULL or
 * out of memory
 */
char*
BLAST_StrToUpper(const char* string);

/** Maximal unpacked subject sequence length for which full translation is 
 * performed up front. 
 */
#define MAX_FULL_TRANSLATION 2100

/** Translate the subject sequence into 6 frames, and create a mixed-frame 
 * sequence, if out-of-frame gapping will be used.
 * @param subject_blk Subject sequence structure [in]
 * @param gen_code_string Genetic code to use for translation [in]
 * @param translation_buffer Pointer to buffer to hold the translated 
 *                           sequence(s) [out]
 * @param frame_offsets Pointer to an array to hold offsets into the
 *                      translation buffer for each frame. Mixed-frame 
 *                      sequence is to be returned, if NULL. [in] [out]
 * @param partial_translation Should partial translations be performed later
 *                            for each HSP instead of a full translation? [out]
 */
NCBI_XBLAST_EXPORT
Int2
Blast_SetUpSubjectTranslation(BLAST_SequenceBlk* subject_blk, 
                              const Uint1* gen_code_string,
                              Uint1** translation_buffer, 
                              Int4** frame_offsets,
                              Boolean* partial_translation);

/** Get the number of contexts for a given program. This corresponds to the
 * number of translation frames or strands whenever applicable. 
 * @return 0 on unsupported program, non-zero otherwise
 */
NCBI_XBLAST_EXPORT
unsigned int BLAST_GetNumberOfContexts(EBlastProgramType program);

#ifdef __cplusplus
}
#endif
#endif /* !__BLAST_UTIL__ */
