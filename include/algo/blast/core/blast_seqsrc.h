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
* File Description:
*   Definition of ADT to retrieve sequences for the BLAST engine
*
*/

#ifndef BLAST_SEQSRC_H
#define BLAST_SEQSRC_H
#include <blast_def.h>

#ifdef __cplusplus
extern "C" {
#endif

/** This incomplete type is the only handle to the Blast Sequence Source ADT */
typedef struct BlastSeqSrc BlastSeqSrc;

/** Function pointer typedef to create a new BlastSeqSrc structure.
 * First argument is a pointer to the structure to be populated (allocated for
 * client implementations), second argument should be typecast'd to the 
 * correct type by user-defined constructor function */
typedef BlastSeqSrc* (*BlastSeqSrcConstructor) (BlastSeqSrc*, void*);

/** Function pointer typedef to deallocate a BlastSeqSrc structure.
 * Argument is the BlastSeqSrc structure to free, always returns NULL. */
typedef BlastSeqSrc* (*BlastSeqSrcDestructor) (BlastSeqSrc*);

/** Function pointer typedef to return a 4-byte integer.
 * First argument is the BlastSeqSrc structure used, second argument is
 * passed to user-defined implementation */
typedef Int4 (*GetInt4FnPtr) (void*, void*);

/** Function pointer typedef to return a 8-byte integer.
 * First argument is the BlastSeqSrc structure used, second argument is
 * passed to user-defined implementation */
typedef Int8 (*GetInt8FnPtr) (void*, void*);

/** Function pointer typedef to return a null terminated string containing a
 * sequence identifier. First argument is the BlastSeqSrc structure used, second
 * argument is passed to user-defined implementation. */
typedef char* (*GetSeqIdFnPtr) (void*, void*);

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

#define BLAST_SEQSRC_ERROR      -1  /**< Error while retrieving sequence */
#define BLAST_SEQSRC_EOF        0   /**< No more sequences available */
#define BLAST_SEQSRC_SUCCESS    1   /**< Successful sequence retrieval */

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
 * pointer is not set, NULL is returned.
 * @param bssn_info Structure defining constructor and its argument to be
 *        invoked from this function [in]
 */
BlastSeqSrc* BlastSeqSrcNew(BlastSeqSrcNewInfo* bssn_info);

/** Frees the BlastSeqSrc structure by invoking the destructor function set by
 * the user-defined constructor function when the structure is initialized
 * (indirectly, by BlastSeqSrcNew). If the destructor function pointer is not
 * set, a memory leak could occur.
 * @param bssp BlastSeqSrc to free [in]
 * @return NULL
 */
BlastSeqSrc* BlastSeqSrcFree(BlastSeqSrc* bssp);

/** Convenience macros call function pointers (TODO: needs to be more robust)
 * Currently, this defines the API */
#define BLASTSeqSrcGetNumSeqs(bssp) \
    (*GetGetNumSeqs(bssp))(GetDataStructure(bssp), NULL)
#define BLASTSeqSrcGetMaxSeqLen(bssp) \
    (*GetGetMaxSeqLen(bssp))(GetDataStructure(bssp), NULL)
#define BLASTSeqSrcGetTotLen(bssp) \
    (*GetGetTotLen(bssp))(GetDataStructure(bssp), NULL)
#define BLASTSeqSrcGetSequence(bssp, arg) \
    (*GetGetSequence(bssp))(GetDataStructure(bssp), arg)
#define BLASTSeqSrcGetSeqIdStr(bssp, arg) \
    (*GetGetSeqIdStr(bssp))(GetDataStructure(bssp), arg)

#define DEFINE_MEMBER_FUNCTIONS(member_type, member, data_structure_type) \
DEFINE_ACCESSOR(member_type, member, data_structure_type); \
DEFINE_MUTATOR(member_type, member, data_structure_type)

#define DEFINE_ACCESSOR(member_type, member, data_structure_type) \
member_type Get##member(data_structure_type var)

#define DEFINE_MUTATOR(member_type, member, data_structure_type) \
void Set##member(data_structure_type var, member_type arg) \

DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr, BlastSeqSrc*);

DEFINE_MEMBER_FUNCTIONS(void*, DataStructure, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence, BlastSeqSrc*);
DEFINE_MEMBER_FUNCTIONS(GetSeqIdFnPtr, GetSeqIdStr, BlastSeqSrc*);

#ifdef __cplusplus
}
#endif

#endif /* BLAST_SEQSRC_H */
