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

/** @file blast_seqsrc_impl.h
 * Definitions needed for implementing the BlastSeqSrc interface and low level 
 * details of the implementation of the BlastSeqSrc framework
 */

#ifndef ALGO_BLAST_CORE__BLAST_SEQSRC_IMPL__H
#define ALGO_BLAST_CORE__BLAST_SEQSRC_IMPL__H

#include <algo/blast/core/ncbi_std.h>
#include <algo/blast/core/blast_message.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Function pointer typedef to create a new BlastSeqSrc structure.
 * BlastSeqSrcNew uses this function pointer and the ctor_argument (both
 * obtained from the BlastSeqSrcNewInfo structure) after allocating the
 * BlastSeqSrc structure.
 * Client implementations MUST return a non-NULL BlastSeqSrc (the one that is
 * actually passed in) even if initialization of the BlastSeqSrc 
 * implementation fails, case in which only the functionality to retrieve an 
 * initialization error message and to deallocate the BlastSeqSrc structure 
 * must be defined (C++ implementations must NOT throw exceptions!).
 * If initialization of the BlastSeqSrc implementation succeeds, then this 
 * function should initialize all the function pointers and appropriate data 
 * fields for the BlastSeqSrc using the _BlastSeqSrcImpl_* functions
 * defined by the macros at the end of this file.
 */
typedef BlastSeqSrc* (*BlastSeqSrcConstructor) 
    (BlastSeqSrc* seqsrc, /**< pointer to an already allocated structure to 
                            be populated with implementation's function
                            pointers and data structures */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
     );

/** Complete type definition of the structure used to create a new 
 * BlastSeqSrc */
struct BlastSeqSrcNewInfo {
    BlastSeqSrcConstructor constructor; /**< User-defined function to initialize
                                          a BlastSeqSrc structure */
    void* ctor_argument;                /**< Argument to the above function */
};

/** Function pointer typedef to deallocate a BlastSeqSrc structure, always 
 * returns NULL. This function's implementation should free resources allocated
 * in the BlastSeqSrcConstructor, the BlastSeqSrc structure itself is free'd by
 * BlastSeqSrcFree */
typedef BlastSeqSrc* (*BlastSeqSrcDestructor) 
    (BlastSeqSrc* seqrc /**< BlastSeqSrc structure to free */
     );

/** Function pointer typedef to modify the contents of a BlastSeqSrc
 * structure, copied by BlastSeqSrcCopy, to achieve multi-thread safety.
 * Argument is a pointer to the BlastSeqSrc structure to be modified.
 * Returns the same structure, with modified contents. 
 */
typedef BlastSeqSrc* (*BlastSeqSrcCopier) (BlastSeqSrc*);

/** Function pointer typedef to set a 4-byte integer. */
typedef void (*SetInt4FnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     int arg);

/** Function pointer typedef to return a 4-byte integer. */
typedef Int4 (*GetInt4FnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
     );

/** Function pointer typedef to return a 8-byte integer. */
typedef Int8 (*GetInt8FnPtr) 
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
    );

/** Function pointer typedef to return a null terminated string, used to return
 * the name of a BlastSeqSrc implementation (e.g.: BLAST database name).
 */
typedef const char* (*GetStrFnPtr) 
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
    );

/** Function pointer typedef to return a boolean value, used to return whether
 * a given BlastSeqSrc implementation contains protein or nucleotide sequences
 * (e.g.: BlastSeqSrcGetIsProt).
 */
typedef Boolean (*GetBoolFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
    );

/** Function pointer typedef to set partial fetching range */
typedef void (*SetSeqRangeFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     BlastSeqSrcSetRangesArg* arg /**< arguments to set partial fetching ranges */
    );

/** Function pointer typedef to retrieve sequences from data structure embedded
 * in the BlastSeqSrc structure. Return value is one of the BLAST_SEQSRC_* 
 * defines @sa BlastSeqSrcGetSeqArg */
typedef Int2 (*GetSeqBlkFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     BlastSeqSrcGetSeqArg* arg /**< arguments to fetch sequence data from a 
                 client-defined BlastSeqSrc implementation */
    );

/** Function pointer typedef to release sequences obtained from the data 
 * structure embedded in the BlastSeqSrc structure.
 * @sa BlastSeqSrcGetSeqArg */
typedef void (*ReleaseSeqBlkFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     BlastSeqSrcGetSeqArg* arg /**< arguments to fetch sequence data from a 
                 client-defined BlastSeqSrc implementation */
    );

#ifdef KAPPA_PRINT_DIAGNOSTICS
/** Function pointer typedef to retrieve gis for a given ordinal id */
typedef Blast_GiList* (*GetGisFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     void* arg /**< place holder argument to pass arguments to the
                 client-defined BlastSeqSrc implementation */
    );
#endif /* KAPPA_PRINT_DIAGNOSTICS */

/******************** BlastSeqSrcIterator API *******************************/

/** Defines the type of data contained in the BlastSeqSrcIterator structure */
typedef enum BlastSeqSrcItrType {
    eOidList,   /**< Data is a list of discontiguous ordinal ids (indices) */
    eOidRange   /**< Data is a range of contiguous ordinal ids (indices) */
} BlastSeqSrcItrType;

/** Complete type definition of Blast Sequence Source Iterator */
struct BlastSeqSrcIterator {
    /** Indicates which member to access: oid_list or oid_range */
    BlastSeqSrcItrType  itr_type;

    /** Array of ordinal ids used when itr_type is eOidList */
    int* oid_list;
    /** This is a half-closed interval [a,b) */
    int  oid_range[2];

    /** Keep track of this iterator's current position, implementations use
     * UINT4_MAX to indicate this is uninitialized/invalid  */
    unsigned int  current_pos;
    /** Size of the chunks to advance over the BlastSeqSrc, also size of 
      * oid_list member, this is provided to reduce mutex contention when
      * implementing MT-safe iteration */
    unsigned int  chunk_sz;
};

/** Function pointer typedef to obtain the next ordinal id to fetch from the
 * BlastSeqSrc structure. 
 * Return value is the next ordinal id, or BLAST_SEQSRC_EOF if no more
 * sequences are available. This is to be used in the oid field of the
 * BlastSeqSrcGetSeqArg structure to indicate an index into the BlastSeqSrc
 * from which the next sequence should be retrieved using
 * BlastSeqSrcGetSequence
 */
typedef Int4 (*AdvanceIteratorFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     BlastSeqSrcIterator* itr /**< iterator which contains the state of the
                                iteration being performed */
     );

/** Function pointer typedef to obtain the next chunk of ordinal ids for the
 * BLAST engine to search. By calling this function with a give chunk size
 * (stored in the iterator structure), one reduces the number of calls which
 * have to be guarded by a mutex in a multi-threaded environment by examining
 * the BlastSeqSrc structure infrequently, i.e.: not every implementation of
 * the BlastSeqSrc needs to provide this if this does not help in satisfying
 * the MT-safe iteration requirement of the BlastSeqSrc interface.
 * Return value is one of the BLAST_SEQSRC_* defines
 */
typedef Int2 (*GetNextChunkFnPtr)
    (void* seqsrc_impl, /**< BlastSeqSrc implementation's data structure */
     BlastSeqSrcIterator* itr /**< iterator which contains the state of the
                                iteration being performed */
     );

/** Function pointer typedef to reset the internal "bookmark" of the last chunk
 * provided for iteration by the data structure embedded in the BlastSeqSrc 
 * structure.
 */
typedef void (*ResetChunkIteratorFnPtr)
    (void* seqsrc_impl); /**< BlastSeqSrc implementation's data structure */

/*****************************************************************************/

#ifndef SKIP_DOXYGEN_PROCESSING

/* The following macros provide access to the BlastSeqSrc structure's data 
   This is provided to allow some basic error checking (no NULL pointer
   dereferencing or assignment).
   These "member functions" of the BlastSeqSrc should be called by
   implementations of the interface to set the appropriate function pointers
   and data structures.
 */

#define DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(member_type, member) \
DECLARE_BLAST_SEQ_SRC_ACCESSOR(member_type, member); \
DECLARE_BLAST_SEQ_SRC_MUTATOR(member_type, member)

#define DECLARE_BLAST_SEQ_SRC_ACCESSOR(member_type, member) \
NCBI_XBLAST_EXPORT \
member_type _BlastSeqSrcImpl_Get##member(const BlastSeqSrc* var)

#define DECLARE_BLAST_SEQ_SRC_MUTATOR(member_type, member) \
NCBI_XBLAST_EXPORT \
void _BlastSeqSrcImpl_Set##member(BlastSeqSrc* var, member_type arg) \


DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcCopier, CopyFnPtr);

DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(SetInt4FnPtr, SetNumberOfThreads);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqsStats);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetAvgSeqLen);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLenStats);

DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetStrFnPtr, GetName);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetBoolFnPtr, GetIsProt);

DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetBoolFnPtr, GetSupportsPartialFetching);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(SetSeqRangeFnPtr, SetSeqRange);

DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetSeqLen);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(ReleaseSeqBlkFnPtr, ReleaseSequence);

DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(AdvanceIteratorFnPtr, IterNext);
#ifdef KAPPA_PRINT_DIAGNOSTICS
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetGisFnPtr, GetGis);
#endif /* KAPPA_PRINT_DIAGNOSTICS */
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(ResetChunkIteratorFnPtr,
                                       ResetChunkIterator);

/* Not really a member functions, but fields */
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(void*, DataStructure);
DECLARE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(char*, InitErrorStr);
#endif

/**
 * @page _impl_blast_seqsrc_howto Implementing the BlastSeqSrc interface
 *
 *  Implementations of this interface should provide life-cycle functions as
 *  well as functions which satisfy the BlastSeqSrc interface. These functions
 *  must have C linkage, as these functions are invoked by the BlastSeqSrc
 *  framework.
 *  An initialization function must also be provided, this is intended to be
 *  invoked by client code which wants to use a specific BlastSeqSrc
 *  implementation through the BlastSeqSrc interface.
 *  For example, MyDatabaseFormat implementation would define the following
 *  functions:
 *  
 *  - Initialization function
 *  @code
 *  // Calls BlastSeqSrcNew in behalf of client code, client should free using
 *  // BlastSeqSrcFree
 *  BlastSeqSrc* MyDatabaseFormatBlastSeqSrcInit(...);
 *  @endcode
 *
 *  - Life-cycle functions
 *  @code
 *  extern "C" {
 *  // required signature: BlastSeqSrcConstructor
 *  BlastSeqSrc* MyDatabaseFormatNew(BlastSeqSrc*, void*);
 *  // required signature: BlastSeqSrcDestructor
 *  BlastSeqSrc* MyDatabaseFormatFree(BlastSeqSrc*);
 *  // required signature: BlastSeqSrcCopier
 *  BlastSeqSrc* MyDatabaseFormatCopy(BlastSeqSrc*);
 *  }
 *  @endcode
 *  
 *  - BlastSeqSrc interface
 *  @code
 *  extern "C" {
 *  // required signature: SetInt4FnPtr
 *  void MyDatabaseFormatSetNumberOfThreads(int);
 *  // required signature: GetInt4FnPtr
 *  Int4 MyDatabaseFormatGetNumSeqs(void*, void*);
 *  // required signature: GetInt4FnPtr
 *  Int4 MyDatabaseFormatGetNumSeqsStats(void*, void*);
 *  // required signature: GetInt4FnPtr
 *  Int4 MyDatabaseFormatGetMaxSeqLen(void*, void*);
 *  // required signature: GetInt4FnPtr
 *  Int4 MyDatabaseFormatGetAvgSeqLen(void*, void*);
 *  // required signature: GetInt8FnPtr
 *  Int8 MyDatabaseFormatGetTotLen(void*, void*);
 *  // required signature: GetInt8FnPtr
 *  Int8 MyDatabaseFormatGetTotLenStats(void*, void*);
 *  // required signature: GetStrFnPtr
 *  const char* MyDatabaseFormatGetName(void*, void*);
 *  // required signature: GetBoolFnPtr
 *  Boolean MyDatabaseFormatGetIsProt(void*, void*);
 *  // required signature: GetSeqBlkFnPtr
 *  Int2 MyDatabaseFormatGetSequence(void*, BlastSeqSrcGetSeqArg*);
 *  // required signature: GetInt4FnPtr
 *  Int4 MyDatabaseFormatGetSeqLen(void*, void*);
 *  // required signature: ReleaseSeqBlkFnPtr
 *  void MyDatabaseFormatReleaseSequence(void*, BlastSeqSrcGetSeqArg*);
 *  // required signature: AdvanceIteratorFnPtr
 *  Int4 MyDatabaseFormatItrNext(void*, BlastSeqSrcIterator* itr);
 *  // required signature: ResetChunkIteratorFnPtr
 *  Int4 MyDatabaseFormatResetChunkIterator(void*);
 *  }
 *  @endcode
 *  
 *  Since the life-cycle and BlastSeqSrc interface functions above are
 *  called by the BlastSeqSrc framework (BlastSeqSrc* functions declared in
 *  blast_seqsrc.h), no exceptions should be thrown in C++ implementations.
 *  When not obvious, please see the required signature's documentation for
 *  determining what to implement (see blast_seqsrc_impl.h).
 *   
 *  For ease of maintenance, please follow the following conventions:
 *  - Client implementations' initialization function should be called 
 *    \c XBlastSeqSrcInit, where \c X is the name of the implementation
 *  - Client implementations should reside in a file named \c seqsrc_X.[hc] or
 *    \c seqsrc_X.[ch]pp, where \c X is the name of the implementation.
 */

#ifdef __cplusplus
}
#endif

#endif /* !ALGO_BLAST_CORE__BLAST_SEQSRC__H */
