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

static char const rcsid[] = "$Id$";

#include <blast_seqsrc.h>

/** Complete type definition of Blast Sequence Source ADT */
typedef struct BlastSeqSrc {

    BlastSeqSrcConstructor NewFnPtr;       /**< Constructor */
    BlastSeqSrcDestructor  DeleteFnPtr;    /**< Destructor */

    GetInt4FnPtr    GetNumSeqs;     /**< Get number of sequences in set */
    GetInt4FnPtr    GetMaxSeqLen;   /**< Get length of longest seq in set */
    GetInt8FnPtr    GetTotLen;      /**< Get total length of all seqs in set */
    GetSeqBlkFnPtr  GetSequence;    /**< Retrieve individual sequence */
    GetSeqIdFnPtr   GetSeqIdStr;    /**< Retrieve sequence identifier */

    void*           DataStructure;  /**< ADT holding the sequence data */

} BlastSeqSrc;

BlastSeqSrcPtr BlastSeqSrcNew(BlastSeqSrcNewInfoPtr bssn_info)
{
    BlastSeqSrcPtr retval = NULL;

    if (!bssn_info)
        return NULL;

    if ( !(retval = (BlastSeqSrcPtr) calloc(1, sizeof(BlastSeqSrc))))
        return NULL;

    /* Save the constructor and invoke it */
    if ((retval->NewFnPtr = bssn_info->constructor))
        retval = (*retval->NewFnPtr)(retval, bssn_info->ctor_argument);
    else
      sfree(retval);

    return retval;
}

BlastSeqSrcPtr BlastSeqSrcFree(BlastSeqSrcPtr bssp)
{
    BlastSeqSrcDestructor destructor_fnptr = NULL;

    if (!bssp)
        return (BlastSeqSrcPtr) NULL;

    /* This could leave a memory leak if destructor function pointer is not
     * initialized! It is the implementation's resposibility to provide this */
    if ( !(destructor_fnptr = (*bssp->DeleteFnPtr)))
      {
	sfree(bssp);
	return NULL;
      }

    return (BlastSeqSrcPtr) (*destructor_fnptr)(bssp);
}

#define DECLARE_MEMBER_FUNCTIONS(member_type, member, data_structure_type) \
DECLARE_ACCESSOR(member_type, member, data_structure_type) \
DECLARE_MUTATOR(member_type, member, data_structure_type)

#define DECLARE_ACCESSOR(member_type, member, data_structure_type) \
member_type Get##member(data_structure_type var) \
{ \
    if (var) \
        return var->member; \
    else \
        return (member_type) NULL; \
}

#define DECLARE_MUTATOR(member_type, member, data_structure_type) \
void Set##member(data_structure_type var, member_type arg) \
{ if (var) var->member = arg; }

/* Note there's no ; after these macros! */
DECLARE_MEMBER_FUNCTIONS(BlastSeqSrcConstructor, NewFnPtr, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(BlastSeqSrcDestructor, DeleteFnPtr, BlastSeqSrcPtr)

DECLARE_MEMBER_FUNCTIONS(void*, DataStructure, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetNumSeqs, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(GetInt4FnPtr, GetMaxSeqLen, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(GetInt8FnPtr, GetTotLen, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(GetSeqBlkFnPtr, GetSequence, BlastSeqSrcPtr)
DECLARE_MEMBER_FUNCTIONS(GetSeqIdFnPtr, GetSeqIdStr, BlastSeqSrcPtr)
