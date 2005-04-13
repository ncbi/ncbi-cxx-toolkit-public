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
 *  Implementations of this interface should provide functions for all
 *  the functions listed above as well as life-cycle functions (constructor,
 *  destructor, and copy function). Furthermore, all these functions must have 
 *  C linkage.
 *
 *  Currently available client implementations of the BlastSeqSrc API include:
 *  - ReaddbBlastSeqSrcInit (C toolkit)
 *  - SeqDbBlastSeqSrcInit (C++ toolkit)
 *  - MultiSeqBlastSeqSrcInit (C/C++ toolkit)
 *  .
 *  Please note that for consistency, C++ implementations should not throw 
 *  exceptions but rather use the error reporting mechanism which allows all
 *  implementations to uniformly retrieve initialization errors via
 *  BlastSeqSrcGetInitError.
 *
 *  For ease of maintenance, please follow the following conventions:
 *  - Client implementations' initialization function should be called 
 *    \c XBlastSeqSrcInit, where X is the name of the implementation
 *  - Client implementations should reside in a file named \c seqsrc_X.[hc] or
 *    \c seqsrc_X.[ch]pp, where X is the name of the implementation.
 */
typedef struct BlastSeqSrc BlastSeqSrc;

/** Function pointer typedef to create a new BlastSeqSrc structure.
 * First argument is a pointer to the structure to be populated (allocated for
 * client implementations), second argument should be typecast'd to the 
 * correct type by user-defined constructor function. Client implementations
 * MUST return a non-NULL BlastSeqSrc even if initialization of the BlastSeqSrc
 * implementation failed, case in which only the functionality to retrieve an
 * initialization error message and to deallocate the BlastSeqSrc structure 
 * must be defined. */
typedef BlastSeqSrc* (*BlastSeqSrcConstructor) (BlastSeqSrc*, void*);

/** Function pointer typedef to deallocate a BlastSeqSrc structure.
 * Argument is the BlastSeqSrc structure to free, always returns NULL. */
typedef BlastSeqSrc* (*BlastSeqSrcDestructor) (BlastSeqSrc*);

/** Function pointer typedef to modify whatever is necessary in a copy of a 
 * BlastSeqSrc structure to achieve multi-thread safety.
 * Argument is the already copied BlastSeqSrc structure; 
 * returns the modified structure. */
typedef BlastSeqSrc* (*BlastSeqSrcCopier) (BlastSeqSrc*);

/** Function pointer typedef to return a 4-byte integer.
 * First argument is the BlastSeqSrc structure used, second argument is
 * passed to user-defined implementation */
typedef Int4 (*GetInt4FnPtr) (void*, void*);

/** Function pointer typedef to return a 8-byte integer.
 * First argument is the BlastSeqSrc structure used, second argument is
 * passed to user-defined implementation */
typedef Int8 (*GetInt8FnPtr) (void*, void*);

/** Function pointer typedef to return a null terminated string. 
 * First argument is the BlastSeqSrc structure used, second
 * argument is passed to user-defined implementation. */
typedef const char* (*GetStrFnPtr) (void*, void*);

/** Function pointer typedef to return a boolean value. 
 * First argument is the BlastSeqSrc structure used, second
 * argument is passed to user-defined implementation. */
typedef Boolean (*GetBoolFnPtr) (void*, void*);

/** Function pointer typedef to retrieve sequences from data structure embedded
 * in the BlastSeqSrc structure.
 * First argument is the BlastSeqSrc structure used, second argument is
 * passed to user-defined implementation.
 * (currently, the GetSeqArg structure defined below is used in the engine) */
typedef Int2 (*GetSeqBlkFnPtr) (void*, void*); 

/** Second argument to GetSeqBlkFnPtr */
typedef struct GetSeqArg {
    /** Oid in BLAST database, index in an array of sequences, etc [in] */
    Int4 oid;
    /** Encoding of sequence, ie: BLASTP_ENCODING, NCBI4NA_ENCODING, etc [in] */
    Uint1 encoding;
    /** Sequence to return, if NULL, it should allocated by GetSeqBlkFnPtr, else
     * its contents are freed and the structure is reused [out]*/
    BLAST_SequenceBlk* seq;
} GetSeqArg;

/* Return values from GetSeqBlkFnPtr */

#define BLAST_SEQSRC_ERROR      -2  /**< Error while retrieving sequence */
#define BLAST_SEQSRC_EOF        -1  /**< No more sequences available */
#define BLAST_SEQSRC_SUCCESS     0  /**< Successful sequence retrieval */

/******************** BlastSeqSrcIterator API *******************************/

/** Defines the type of data contained in the BlastSeqSrcIterator structure */
typedef enum BlastSeqSrcItrType {
    eOidList,   /**< Data is a list of discontiguous ordinal ids (indices) */
    eOidRange   /**< Data is a range of contiguous ordinal ids (indices) */
} BlastSeqSrcItrType;

/** Definition of Blast Sequence Source Iterator */
typedef struct BlastSeqSrcIterator {
    /** Indicates which member to access: oid_list or oid_range */
    BlastSeqSrcItrType  itr_type;

    /** Array of ordinal ids used when itr_type is eOidList */
    int* oid_list;
    /** This is a half-closed interval [a,b) */
    int  oid_range[2];

    /** Keep track of this iterator's current position */
    unsigned int  current_pos;
    /** Size of the chunks to advance over the BlastSeqSrc, also size of 
      * oid_list member */
    unsigned int  chunk_sz;
} BlastSeqSrcIterator;

/** How many database sequences to process in one database chunk. */
extern const unsigned int kBlastSeqSrcDefaultChunkSize;

/** Allocated and initialized an iterator over a BlastSeqSrc. 
 * @param chunk_sz sets the chunk size of the iterator, if zero 
 *    use kBlastSeqSrcDefaultChunkSize (above). [in]
 * @return pointer to initialized iterator for BlastSeqSrc
 */
BlastSeqSrcIterator* BlastSeqSrcIteratorNew(unsigned int chunk_sz);

/** Frees the BlastSeqSrcIterator structure. 
 * @param itr BlastSeqSrcIterator to free [in]
 * @return NULL
 */
BlastSeqSrcIterator* BlastSeqSrcIteratorFree(BlastSeqSrcIterator* itr);

/** Increments the BlastSeqSrcIterator.
 * @param itr the BlastSeqSrcIterator to increment.
 * @param seq_src the underlying BlastSeqSrc
 */
Int4 BlastSeqSrcIteratorNext(const BlastSeqSrc* seq_src, BlastSeqSrcIterator* itr);

/** Function pointer typedef to obtain the next ordinal id to fetch from the
 * BlastSeqSrc structure. First argument is the BlastSeqSrc structure used,
 * second argument is the iterator structure. This structure should allow
 * multi-threaded iteration over sequence sources such as a BLAST database.
 * Return value is the next ordinal id, or BLAST_SEQSRC_EOF if no more
 * sequences are available.
 */
typedef Int4 (*AdvanceIteratorFnPtr) (void*, BlastSeqSrcIterator*);

/** Function pointer typedef to obtain the next chunk of ordinal ids for the
 * BLAST engine to search. By calling this function with a give chunk size
 * (stored in the iterator structure), one reduces the number of calls which
 * have to be guarded by a mutex in a multi-threaded environment by examining
 * the BlastSeqSrc structure infrequently.
 * First argument is the BlastSeqSrc structure used, second argument is the 
 * iterator structure which is updated with the next chunk of ordinal ids to
 * retrieve.
 * Return value is one of the BLAST_SEQSRC_* defines
 */
typedef Int2 (*GetNextChunkFnPtr) (void*, BlastSeqSrcIterator*);

/*****************************************************************************/

/** Structure that contains the information needed for BlastSeqSrcNew to fully
 * populate the BlastSeqSrc structure it returns */
typedef struct BlastSeqSrcNewInfo {
    BlastSeqSrcConstructor constructor; /**< User-defined function to initialize
                                          a BlastSeqSrc structure */
    void* ctor_argument;                /**< Argument to the above function */
} BlastSeqSrcNewInfo;

/** Allocates memory for a BlastSeqSrc structure and then invokes the
 * constructor function defined in its first argument, passing the 
 * ctor_argument member of that same structure. If the constructor function
 * pointer is not set or there is a memory allocation failure, NULL is
 * returned.
 * @param bssn_info Structure defining constructor and its argument to be
 *        invoked from this function [in]
 * @return a properly initialized BlastSeqSrc structure or NULL.
 */
BlastSeqSrc* BlastSeqSrcNew(const BlastSeqSrcNewInfo* bssn_info);

/** Frees the BlastSeqSrc structure by invoking the destructor function set by
 * the user-defined constructor function when the structure is initialized
 * (indirectly, by BlastSeqSrcNew). If the destructor function pointer is not
 * set, a memory leak could occur.
 * @param seq_src BlastSeqSrc to free [in]
 * @return NULL
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* BlastSeqSrcFree(BlastSeqSrc* seq_src);

/** Copy function: needed to guarantee thread safety.
 * @param seq_src BlastSeqSrc to copy [in]
 * @return a copy of the structure passed in or NULL.
 */
NCBI_XBLAST_EXPORT
BlastSeqSrc* BlastSeqSrcCopy(const BlastSeqSrc* seq_src);

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

/** Convenience macros call function pointers (TODO: needs to be more robust)
 * Currently, this defines the API */
#define BLASTSeqSrcGetNumSeqs(seq_src) \
    (*GetGetNumSeqs(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetMaxSeqLen(seq_src) \
    (*GetGetMaxSeqLen(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetAvgSeqLen(seq_src) \
    (*GetGetAvgSeqLen(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetTotLen(seq_src) \
    (*GetGetTotLen(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetName(seq_src) \
    (*GetGetName(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetIsProt(seq_src) \
    (*GetGetIsProt(seq_src))(GetDataStructure(seq_src), NULL)
#define BLASTSeqSrcGetSequence(seq_src, arg) \
    (*GetGetSequence(seq_src))(GetDataStructure(seq_src), arg)
#define BLASTSeqSrcGetSeqLen(seq_src, arg) \
    (*GetGetSeqLen(seq_src))(GetDataStructure(seq_src), arg)
#define BLASTSeqSrcGetNextChunk(seq_src, iterator) \
    (*GetGetNextChunk(seq_src))(GetDataStructure(seq_src), iterator)
#define BLASTSeqSrcRetSequence(seq_src, arg) \
    (*GetRetSequence(seq_src))(GetDataStructure(seq_src), arg)

#ifndef SKIP_DOXYGEN_PROCESSING

#define DECLARE_MEMBER_FUNCTIONS(member_type, member, data_structure_type) \
DECLARE_ACCESSOR(member_type, member, data_structure_type); \
DECLARE_MUTATOR(member_type, member, data_structure_type)

#define DECLARE_ACCESSOR(member_type, member, data_structure_type) \
NCBI_XBLAST_EXPORT member_type Get##member(const data_structure_type var)

#define DECLARE_MUTATOR(member_type, member, data_structure_type) \
NCBI_XBLAST_EXPORT void Set##member(data_structure_type var, member_type arg) \


DECLARE_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(BlastSeqSrcCopier, CopyFnPtr, BlastSeqSrc*);

DECLARE_MEMBER_FUNCTIONS(void*, DataStructure, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs, BlastSeqSrc*);

DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetAvgSeqLen, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetStrFnPtr, GetName, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetBoolFnPtr, GetIsProt, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetSeqLen, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetNextChunkFnPtr, GetNextChunk, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(AdvanceIteratorFnPtr, IterNext, BlastSeqSrc*);
DECLARE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, RetSequence, BlastSeqSrc*);

/* Not really a member function, but a field */
DECLARE_ACCESSOR(char*, InitErrorStr, BlastSeqSrc*);
DECLARE_MUTATOR(char*, InitErrorStr, BlastSeqSrc*);
#endif

/*!\fn BLASTSeqSrcGetMaxSeqLen(seq_src)
   \brief Get the length of the longest sequence in the sequence source.
*/

/*!\fn BLASTSeqSrcGetAvgSeqLen(seq_src)
   \brief Get the average length of all sequences in the sequence source.
*/

/*!\fn BLASTSeqSrcGetTotLen(seq_src)
   \brief Get the total length of all sequences in the sequence source.
*/

/*!\fn BLASTSeqSrcGetName(seq_src)
   \brief Get the database name.
*/

/*!\fn BLASTSeqSrcGetIsProt(seq_src)
   \brief Find if the database is protein or nucleotide.
*/

/*!\fn BLASTSeqSrcGetSequence(seq_src,arg)
   \brief Retrieve an individual sequence.
*/

/*!\fn BLASTSeqSrcGetSeqLen(seq_src,arg)
   \brief Retrieve sequence length.
*/

/*!\fn BLASTSeqSrcGetNextChunk(seq_src,iterator)
   \brief Get next chunk of sequence indices.
*/

/*!\fn BLASTSeqSrcRetSequence(seq_src,arg)
   \brief Deallocate individual sequence buffer if necessary.
*/

#ifdef __cplusplus
}
#endif

#endif /* BLAST_SEQSRC_H */
