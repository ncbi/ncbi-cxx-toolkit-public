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

static char const rcsid[] = 
    "$Id$";

#include <algo/blast/core/blast_seqsrc.h>

/** Complete type definition of Blast Sequence Source ADT */
struct BlastSeqSrc {

    BlastSeqSrcConstructor NewFnPtr;       /**< Constructor */
    BlastSeqSrcDestructor  DeleteFnPtr;    /**< Destructor */

   /* Functions to get information about database as a whole */
    GetInt4FnPtr      GetNumSeqs;     /**< Get number of sequences in set */
    GetInt4FnPtr      GetMaxSeqLen;   /**< Get length of longest seq in set */
    GetInt4FnPtr      GetAvgSeqLen;   /**< Get average length of sequences in 
                                         the set */
    GetInt8FnPtr      GetTotLen;      /**< Get tot length of all seqs in set */
    GetStrFnPtr       GetName;        /**< Get the name of the database */
    GetStrFnPtr       GetDefinition;  /**< Get the database definition */
    GetStrFnPtr       GetDate;        /**< Get the date of the database */
    GetBoolFnPtr      GetIsProt;      /**< Find if database is a protein or 
                                         nucleotide */

   /* Functions to get information about individual sequences */
    GetSeqBlkFnPtr    GetSequence;    /**< Retrieve individual sequence */
    GetStrFnPtr       GetSeqIdStr;    /**< Retrieve sequence identifier 
                                         string */
    GetGenDataFnPtr   GetSeqId;       /**< Retrieve sequence identifier */
    GetGenDataFnPtr   GetSeqLoc;      /**< Retrieve sequence identifier */
    GetInt4FnPtr      GetSeqLen;      /**< Retrieve given sequence length */

   /* Functions to iterate over sequences in the database */
    GetNextChunkFnPtr GetNextChunk;   /**< Get next chunk of seq indices */
    AdvanceIteratorFnPtr IterNext;    /**< Gets next oid from the iterator */
    GetGenDataFnPtr   GetError;       /**< Gets a saved error message, if
                                         supported. */
    GetSeqBlkFnPtr    RetSequence;    /**< Deallocate individual sequence 
                                         buffer if necessary. */
   
    void*             DataStructure;  /**< ADT holding the sequence data */
};

BlastSeqSrc* BlastSeqSrcNew(const BlastSeqSrcNewInfo* bssn_info)
{
    BlastSeqSrc* retval = NULL;

    if (!bssn_info) {
        return NULL;
    }

    if ( !(retval = (BlastSeqSrc*) calloc(1, sizeof(BlastSeqSrc)))) {
        return NULL;
    }

    /* Save the constructor and invoke it */
    if ((retval->NewFnPtr = bssn_info->constructor)) {
        retval = (*retval->NewFnPtr)(retval, bssn_info->ctor_argument);
    } else {
      sfree(retval);
    }

    return retval;
}

BlastSeqSrc* BlastSeqSrcFree(BlastSeqSrc* bssp)
{
    BlastSeqSrcDestructor destructor_fnptr = NULL;

    if (!bssp) {
        return (BlastSeqSrc*) NULL;
    }

    /* This could leave a memory leak if destructor function pointer is not
     * initialized! It is the implementation's resposibility to provide this */
    if ( !(destructor_fnptr = (*bssp->DeleteFnPtr))) {
        sfree(bssp);
        return NULL;
    }

    return (BlastSeqSrc*) (*destructor_fnptr)(bssp);
}

/******************** BlastSeqSrcIterator API *******************************/

BlastSeqSrcIterator* BlastSeqSrcIteratorNew(unsigned int chunk_sz)
{
    BlastSeqSrcIterator* itr = NULL;

    itr = (BlastSeqSrcIterator*) calloc(1, sizeof(BlastSeqSrcIterator));
    if (!itr) {
        return NULL;
    }

    /* Should employ lazy initialization? */
    itr->oid_list = (unsigned int*)malloc(chunk_sz * sizeof(unsigned int));
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

Int4 BlastSeqSrcIteratorNext(const BlastSeqSrc* bssp, BlastSeqSrcIterator* itr)
{
    ASSERT(bssp);
    ASSERT(itr);
    ASSERT(bssp->IterNext);

    return (*bssp->IterNext)((void*) bssp, itr);
}

/*****************************************************************************/

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

/* Note there's no ; after these macros! */
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(void*, DataStructure, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetAvgSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(GetStrFnPtr, GetName, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetStrFnPtr, GetDefinition, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetStrFnPtr, GetDate, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetBoolFnPtr, GetIsProt, BlastSeqSrc*)

DEFINE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetStrFnPtr, GetSeqIdStr, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetGenDataFnPtr, GetSeqId, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetGenDataFnPtr, GetSeqLoc, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetSeqLen, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetNextChunkFnPtr, GetNextChunk, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(AdvanceIteratorFnPtr, IterNext, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetGenDataFnPtr, GetError, BlastSeqSrc*)
DEFINE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, RetSequence, BlastSeqSrc*)
