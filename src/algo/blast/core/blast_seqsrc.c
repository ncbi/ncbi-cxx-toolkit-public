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
 * Definition of ADT to retrieve sequences for the BLAST engine and
 * low level details of the implementation of the BlastSeqSrc framework.
 */

#ifndef SKIP_DOXYGEN_PROCESSING
#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = 
    "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */
#endif

#include <algo/blast/core/blast_seqsrc.h>
#include <algo/blast/core/blast_seqsrc_impl.h>

/** Complete type definition of Blast Sequence Source ADT.
 * The members of this structure should only be accessed by BlastSeqSrc
 * implementations using the _BlastSeqSrcImpl_* functions.
 */
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

   /* Functions that deal with individual sequences */
    GetSeqBlkFnPtr    GetSequence;    /**< Retrieve individual sequence */
    GetInt4FnPtr      GetSeqLen;      /**< Retrieve given sequence length */
    ReleaseSeqBlkFnPtr ReleaseSequence; /**< Deallocate individual sequence 
                                         (if applicable) */

   /* Functions to iterate over sequences in the database */
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

Int4
BlastSeqSrcGetNumSeqs(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetNumSeqs);
    return (*seq_src->GetNumSeqs)(seq_src->DataStructure, NULL);
}

Int4
BlastSeqSrcGetMaxSeqLen(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetMaxSeqLen);
    return (*seq_src->GetMaxSeqLen)(seq_src->DataStructure, NULL);
}

Int4
BlastSeqSrcGetAvgSeqLen(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetAvgSeqLen);
    return (*seq_src->GetAvgSeqLen)(seq_src->DataStructure, NULL);
}

Int8
BlastSeqSrcGetTotLen(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetTotLen);
    return (*seq_src->GetTotLen)(seq_src->DataStructure, NULL);
}

const char*
BlastSeqSrcGetName(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetName);
    return (*seq_src->GetName)(seq_src->DataStructure, NULL);
}

Boolean
BlastSeqSrcGetIsProt(const BlastSeqSrc* seq_src)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetIsProt);
    return (*seq_src->GetIsProt)(seq_src->DataStructure, NULL);
}

Int2
BlastSeqSrcGetSequence(const BlastSeqSrc* seq_src, 
                       void* sequence)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetSequence);
    ASSERT(sequence);
    return (*seq_src->GetSequence)(seq_src->DataStructure, sequence);
}

Int4
BlastSeqSrcGetSeqLen(const BlastSeqSrc* seq_src, void* oid)
{
    ASSERT(seq_src);
    ASSERT(seq_src->GetSeqLen);
    return (*seq_src->GetSeqLen)(seq_src->DataStructure, oid);
}

void
BlastSeqSrcReleaseSequence(const BlastSeqSrc* seq_src,
                           void* sequence)
{
    ASSERT(seq_src);
    ASSERT(seq_src->ReleaseSequence);
    ASSERT(sequence);
    (*seq_src->ReleaseSequence)(seq_src->DataStructure, sequence);
}

/******************** BlastSeqSrcIterator API *******************************/

BlastSeqSrcIterator* BlastSeqSrcIteratorNew()
{
    return BlastSeqSrcIteratorNewEx(0);
}

const unsigned int kBlastSeqSrcDefaultChunkSize = 1024;

BlastSeqSrcIterator* BlastSeqSrcIteratorNewEx(unsigned int chunk_sz)
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

Int4 BlastSeqSrcIteratorNext(BlastSeqSrc* seq_src, BlastSeqSrcIterator* itr)
{
    ASSERT(seq_src);
    ASSERT(itr);
    ASSERT(seq_src->IterNext);

    return (*seq_src->IterNext)(seq_src->DataStructure, itr);
}

/*****************************************************************************/

/* The following macros implement the "member functions" of the BlastSeqSrc
 * structure so that its various fields can be accessed by the implementors of
 * the BlastSeqSrc interface */

#ifndef SKIP_DOXYGEN_PROCESSING

#define DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(member_type, member) \
DEFINE_BLAST_SEQ_SRC_ACCESSOR(member_type, member) \
DEFINE_BLAST_SEQ_SRC_MUTATOR(member_type, member)

#define DEFINE_BLAST_SEQ_SRC_ACCESSOR(member_type, member) \
member_type \
_BlastSeqSrcImpl_Get##member(const BlastSeqSrc* var) \
{ \
    if (var) \
        return var->member; \
    else \
        return (member_type) NULL; \
}

#define DEFINE_BLAST_SEQ_SRC_MUTATOR(member_type, member) \
void \
_BlastSeqSrcImpl_Set##member(BlastSeqSrc* var, member_type arg) \
{ if (var) var->member = arg; }

#endif

/* Note there's no ; after these macros! */
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(BlastSeqSrcCopier, CopyFnPtr)

DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(void*, DataStructure)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(char*, InitErrorStr)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetAvgSeqLen)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen)

DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetStrFnPtr, GetName)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetBoolFnPtr, GetIsProt)

DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(GetInt4FnPtr, GetSeqLen)
DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(ReleaseSeqBlkFnPtr, ReleaseSequence)

DEFINE_BLAST_SEQ_SRC_MEMBER_FUNCTIONS(AdvanceIteratorFnPtr, IterNext)
