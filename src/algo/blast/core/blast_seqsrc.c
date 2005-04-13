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
 *
 */

/** @file blast_seqsrc.c
 * Definition of ADT to retrieve sequences for the BLAST engine
 */

#ifndef SKIP_DOXYGEN_PROCESSING
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
#endif

#include <algo/blast/core/blast_seqsrc.h>

/** Complete type definition of Blast Sequence Source ADT */
struct BlastSeqSrc {

    BlastSeqSrcConstructor NewFnPtr;       /**< Constructor */
    BlastSeqSrcDestructor  DeleteFnPtr;    /**< Destructor */
    BlastSeqSrcCopier      CopyFnPtr;      /**< Copier */

   /* Functions to get information about database as a whole */
    GetInt4FnPtr      GetNumSeqs;     /**< Get number of sequences in set */
    GetInt4FnPtr      GetMaxSeqLen;   /**< Get length of longest seq in set */
    GetInt4FnPtr      GetAvgSeqLen;   /**< Get average length of sequences in 
                                         the set */
    GetInt8FnPtr      GetTotLen;      /**< Get tot length of all seqs in set */
    GetStrFnPtr       GetName;        /**< Get the name of the database */
    GetBoolFnPtr      GetIsProt;      /**< Find if database is a protein or 
                                         nucleotide */

   /* Functions to get information about individual sequences */
    GetSeqBlkFnPtr    GetSequence;    /**< Retrieve individual sequence */
    GetInt4FnPtr      GetSeqLen;      /**< Retrieve given sequence length */
    ReleaseSeqBlkFnPtr ReleaseSequence; /**< Deallocate individual sequence 
                                         (if applicable) */

   /* Functions to iterate over sequences in the database */
    GetNextChunkFnPtr GetNextChunk;   /**< Get next chunk of seq indices */
    AdvanceIteratorFnPtr IterNext;    /**< Gets next oid from the iterator */
   
    void*             DataStructure;  /**< ADT holding the sequence data */

    char*             InitErrorStr;   /**< initialization error string */
};

BlastSeqSrc* BlastSeqSrcNew(const BlastSeqSrcNewInfo* bssn_info)
{
    BlastSeqSrc* retval = NULL;

    if (!bssn_info) {
        return (BlastSeqSrc*) NULL;
    }

    if ( !(retval = (BlastSeqSrc*) calloc(1, sizeof(BlastSeqSrc)))) {
        return (BlastSeqSrc*) NULL;
    }

    /* Save the constructor and invoke it */
    if ((retval->NewFnPtr = bssn_info->constructor)) {
        retval = (*retval->NewFnPtr)(retval, bssn_info->ctor_argument);
    } else {
        sfree(retval);
    }

    return retval;
}

BlastSeqSrc* BlastSeqSrcFree(BlastSeqSrc* seq_src)
{
    BlastSeqSrcDestructor destructor_fnptr = NULL;

    if (!seq_src) {
        return (BlastSeqSrc*) NULL;
    }

    if (seq_src->InitErrorStr) {
        sfree(seq_src->InitErrorStr);
    }

    /* This could leave a memory leak if destructor function pointer is not
     * initialized! It is the implementation's resposibility to provide this */
    if ( !(destructor_fnptr = (*seq_src->DeleteFnPtr))) {
        sfree(seq_src);
        return (BlastSeqSrc*) NULL;
    }

    return (BlastSeqSrc*) (*destructor_fnptr)(seq_src);
}

BlastSeqSrc* BlastSeqSrcCopy(const BlastSeqSrc* seq_src)
{
    BlastSeqSrcCopier copy_fnptr = NULL;
    BlastSeqSrc* retval = NULL;

    if (!seq_src) {
        return (BlastSeqSrc*) NULL;
    }

    if ( !(retval = (BlastSeqSrc*) BlastMemDup(seq_src, sizeof(BlastSeqSrc)))) {
        return (BlastSeqSrc*) NULL;
    }

    /* If copy function is not provided, just return a copy of the structure */
    if ( !(copy_fnptr = (*seq_src->CopyFnPtr))) {
        return retval;
    }

    return (BlastSeqSrc*) (*copy_fnptr)(retval);
}

char* BlastSeqSrcGetInitError(const BlastSeqSrc* seq_src)
{
    if ( !seq_src || !seq_src->InitErrorStr ) {
        return NULL;
    }

    return strdup(seq_src->InitErrorStr);
}

/** How many database sequences to process in one database chunk. */
const unsigned int kBlastSeqSrcDefaultChunkSize = 1024;

/******************** BlastSeqSrcIterator API *******************************/

BlastSeqSrcIterator* BlastSeqSrcIteratorNew(unsigned int chunk_sz)
{
    BlastSeqSrcIterator* itr = NULL;

    if (chunk_sz == 0)
       chunk_sz = kBlastSeqSrcDefaultChunkSize;

    itr = (BlastSeqSrcIterator*) calloc(1, sizeof(BlastSeqSrcIterator));
    if (!itr) {
        return NULL;
    }

    /* Should employ lazy initialization? */
    itr->oid_list = (int*)malloc(chunk_sz * sizeof(unsigned int));
    if (!itr->oid_list) {
        sfree(itr);
        return NULL;
    }

    itr->chunk_sz = chunk_sz;
    itr->current_pos = UINT4_MAX;   /* mark iterator as uninitialized */

    return itr;
}

BlastSeqSrcIterator* BlastSeqSrcIteratorFree(BlastSeqSrcIterator* itr)
{
    if (!itr) {
        return NULL;
    }
    if (itr->oid_list) {
        sfree(itr->oid_list);
    }

    sfree(itr);
    return NULL;
}

Int4 BlastSeqSrcIteratorNext(const BlastSeqSrc* seq_src, BlastSeqSrcIterator* itr)
{
    ASSERT(seq_src);
    ASSERT(itr);
    ASSERT(seq_src->IterNext);

    return (*seq_src->IterNext)((void*) seq_src, itr);
}

/*****************************************************************************/

#ifndef SKIP_DOXYGEN_PROCESSING

#define DEFINE_MEMBER_FUNCTIONS(member_type, member, data_structure_type) \
DEFINE_ACCESSOR(member_type, member, data_structure_type) \
DEFINE_MUTATOR(member_type, member, data_structure_type)

#define DEFINE_ACCESSOR(member_type, member, data_structure_type) \
member_type Get##member(const data_structure_type var) \
{ \
    if (var) \
        return var->member; \
    else \
        return (member_type) NULL; \
}

#define DEFINE_MUTATOR(member_type, member, data_structure_type) \
void Set##member(data_structure_type var, member_type arg) \
{ if (var) var->member = arg; }

#endif

/* Note there's no ; after these macros! */
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcCopier, CopyFnPtr, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(void*, DataStructure, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetAvgSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(GetStrFnPtr, GetName, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetBoolFnPtr, GetIsProt, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(ReleaseSeqBlkFnPtr, ReleaseSequence, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(GetNextChunkFnPtr, GetNextChunk, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(AdvanceIteratorFnPtr, IterNext, BlastSeqSrc*)

DEFINE_ACCESSOR(char*, InitErrorStr, BlastSeqSrc*)
DEFINE_MUTATOR(char*, InitErrorStr, BlastSeqSrc*)
