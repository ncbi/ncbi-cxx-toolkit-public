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
 * Author:  Christiam Camacho
 *
 */

/** @file blast_seqsrc.h
 * Declaration of ADT to retrieve sequences for the BLAST engine.
 */

#ifndef BLAST_SEQSRC_H
#define BLAST_SEQSRC_H

#include <algo/blast/core/blast_def.h>
#include <algo/blast/core/blast_message.h>

#ifdef __cplusplus
extern "C" {
#endif

/** The BlastSeqSrc ADT is an opaque data type that defines an interface which
 *  is used by the core BLAST code to retrieve sequences.
 *  The interface currently provides the following services:
 *  - Retrieving number of sequences in a set
 *  - Retrieving the total length (in bases/residues) of sequences in a set.
 *  - Retrieving the length of the longest sequence in a set
 *  - Retrieving an individual sequence in a user-specified encoding by ordinal
 *    id (index into a set)
 *  - Retrieving the length of a given sequence in a set by ordinal id
 *  - Allow MT-safe iteration over sequences in a set through the
 *    BlastSeqSrcIterator abstraction
 *  .
 *
 *  Currently available client implementations of the BlastSeqSrc API include:
 *  - ReaddbBlastSeqSrcInit (C toolkit)
 *  - SeqDbBlastSeqSrcInit (C++ toolkit)
 *  - MultiSeqBlastSeqSrcInit (C/C++ toolkit)
 *  .
 *  For more details, see also @ref _impl_blast_seqsrc_howto
 */
typedef struct BlastSeqSrc BlastSeqSrc;

/** Blast Sequence Source Iterator, designed to be used in conjunction with the
 * BlastSeqSrc to provide MT-safe iteration over the sequences in the
 * BlastSeqSrc 
 * @todo This is still coupled to the BLAST database implementations of the
 * BlastSeqSrc, could be made more generic if needed.
 */
typedef struct BlastSeqSrcIterator BlastSeqSrcIterator;

/** Structure that contains the information needed for BlastSeqSrcNew to fully
 * populate the BlastSeqSrc structure it returns */
typedef struct BlastSeqSrcNewInfo BlastSeqSrcNewInfo;

/** Allocates memory for a BlastSeqSrc structure and then invokes the
 * constructor function defined in its first argument, passing the 
 * ctor_argument member of that same structure. If the constructor function
 * pointer is not set or there is a memory allocation failure, NULL is
 * returned.
 * @note This function need not be called directly by client code as all the
 * implementations of the BlastSeqSrc interface provide a function(s) which
 * call this function (@ref MultiSeqBlastSeqSrcInit, @ref SeqDbBlastSeqSrcInit)
 * @param bssn_info Structure defining constructor and its argument to be
 *        invoked from this function [in]
 * @return a properly initialized BlastSeqSrc structure or NULL.
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* BlastSeqSrcNew(const BlastSeqSrcNewInfo* bssn_info);

/** Copy function: needed to guarantee thread safety.
 * @todo Is this function really needed? Shouldn't this description be a bit
 * more elaborate?
 * @param seq_src BlastSeqSrc to copy [in]
 * @return an MT-safe copy of the structure passed in, NULL in case of memory
 * allocation failure, or if no copy function was provided by the
 * implementation, a bitwise copy of the input parameter.
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* BlastSeqSrcCopy(const BlastSeqSrc* seq_src);

/** Frees the BlastSeqSrc structure by invoking the destructor function set by
 * the user-defined constructor function when the structure is initialized
 * (indirectly, by BlastSeqSrcNew). If the destructor function pointer is not
 * set, a memory leak could occur.
 * @param seq_src BlastSeqSrc to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* BlastSeqSrcFree(BlastSeqSrc* seq_src);

/** Get the number of sequences contained in the sequence source.
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
Int4
BlastSeqSrcGetNumSeqs(const BlastSeqSrc* seq_src);

/** Get the length of the longest sequence in the sequence source.
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
Int4
BlastSeqSrcGetMaxSeqLen(const BlastSeqSrc* seq_src);

/** Get the average length of all sequences in the sequence source.
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
Int4
BlastSeqSrcGetAvgSeqLen(const BlastSeqSrc* seq_src);

/** Get the total length of all sequences in the sequence source.
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
Int8
BlastSeqSrcGetTotLen(const BlastSeqSrc* seq_src);

/** Get the Blast Sequence source name (e.g.: BLAST database name).
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
const char*
BlastSeqSrcGetName(const BlastSeqSrc* seq_src);

/** Find if the Blast Sequence Source contains protein or nucleotide sequences.
 * @param seq_src the BLAST sequence source [in]
 */
NCBI_XBLAST_EXPORT
Boolean
BlastSeqSrcGetIsProt(const BlastSeqSrc* seq_src);

/** Structure used as the second argument to functions satisfying the 
  GetSeqBlkFnPtr signature, usually associated with index-based 
  implementations of the BlastSeqSrc interface. Index-based implementations
  include BLAST databases or an array/vector of sequences. */
typedef struct BlastSeqSrcGetSeqArg {
    /** Oid in BLAST database, index in an array of sequences, etc [in] */
    Int4 oid;
    /** Encoding of sequence, ie: BLASTP_ENCODING, NCBI4NA_ENCODING, etc [in] */
    Uint1 encoding;
    /** Sequence to return, if NULL, it should allocated by GetSeqBlkFnPtr, else
     * its contents are freed and the structure is reused [out]*/
    BLAST_SequenceBlk* seq;
} BlastSeqSrcGetSeqArg;

/* Return values from BlastSeqSrcGetSequence */

#define BLAST_SEQSRC_ERROR      -2  /**< Error while retrieving sequence */
#define BLAST_SEQSRC_EOF        -1  /**< No more sequences available */
#define BLAST_SEQSRC_SUCCESS     0  /**< Successful sequence retrieval */

/** Retrieve an individual sequence.
 * @param seq_src the BLAST sequence source [in]
 * @param sequence should be of type BlastSeqSrcGetSeqArg [in|out]
 * @return one of the BLAST_SEQSRC_* defined in blast_seqsrc.h
 */
NCBI_XBLAST_EXPORT
Int2
BlastSeqSrcGetSequence(const BlastSeqSrc* seq_src, 
                       void* sequence);

/** Retrieve sequence length.
 * @param seq_src the BLAST sequence source [in]
 * @param oid ordinal id of the sequence desired (should be Uint4) [in]
 */
NCBI_XBLAST_EXPORT
Int4
BlastSeqSrcGetSeqLen(const BlastSeqSrc* seq_src, void* oid);

/** Deallocate individual sequence.
 * @param seq_src the BLAST sequence source [in]
 * @param sequence should be of type BlastSeqSrcGetSeqArg [in|out]
 */
NCBI_XBLAST_EXPORT
void
BlastSeqSrcReleaseSequence(const BlastSeqSrc* seq_src,
                           void* sequence);

/** Function to retrieve NULL terminated string containing the description 
 * of an initialization error or NULL. This function MUST ALWAYS be called 
 * after calling one of the client implementation's Init functions. If the
 * return value is not NULL, invoking any other functionality from the
 * BlastSeqSrc will result in undefined behavior. Caller is responsible for 
 * deallocating the return value. 
 * @param seq_src BlastSeqSrc from which to get an error [in]
 * @return error message or NULL
 */
NCBI_XBLAST_EXPORT
char* BlastSeqSrcGetInitError(const BlastSeqSrc* seq_src);

/******************** BlastSeqSrcIterator API *******************************/

/** Allocate and initialize an iterator over a BlastSeqSrc with a default chunk
 * size for MT-safe iteration.
 * @return pointer to initialized iterator for BlastSeqSrc
 */
NCBI_XBLAST_EXPORT
BlastSeqSrcIterator* BlastSeqSrcIteratorNew();

/** How many database sequences to process in one database chunk. */
extern const unsigned int kBlastSeqSrcDefaultChunkSize;

/** Allocate and initialize an iterator over a BlastSeqSrc. 
 * @param chunk_sz sets the chunk size of the iterator, if zero 
 *    use kBlastSeqSrcDefaultChunkSize (above). [in]
 * @return pointer to initialized iterator for BlastSeqSrc
 */
NCBI_XBLAST_EXPORT
BlastSeqSrcIterator* BlastSeqSrcIteratorNewEx(unsigned int chunk_sz);

/** Frees the BlastSeqSrcIterator structure. 
 * @param itr BlastSeqSrcIterator to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastSeqSrcIterator* BlastSeqSrcIteratorFree(BlastSeqSrcIterator* itr);

/** Increments the BlastSeqSrcIterator.
 * @param itr the BlastSeqSrcIterator to increment.
 * @param seq_src the underlying BlastSeqSrc
 * @return one of the BLAST_SEQSRC_* defined in blast_seqsrc.h
 */
NCBI_XBLAST_EXPORT
Int4 BlastSeqSrcIteratorNext(const BlastSeqSrc* seq_src, 
                             BlastSeqSrcIterator* itr);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* BLAST_SEQSRC_H */
