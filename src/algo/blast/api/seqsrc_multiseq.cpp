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
* Author:  Ilya Dondoshansky
*
*/

/// @file multiseq_src.cpp
/// Implementation of the BlastSeqSrc interface for a vector of sequence 
/// locations.

#include <objects/seqloc/Seq_id.hpp>
#include <objmgr/util/sequence.hpp>
#include <algo/blast/api/multiseq_src.hpp>
#include <algo/blast/core/blast_def.h>
#include "blast_setup.hpp"
#include <algo/blast/api/blast_exception.hpp>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

CMultiSeqInfo::CMultiSeqInfo(const TSeqLocVector& seq_vector, EProgram program)
    : m_vSeqVector(seq_vector)
{
    m_ibIsProt = (program == eBlastp || program == eBlastx);
    
    try {
        SetupSubjects(seq_vector, program, &m_ivSeqBlkVec, &m_iMaxLength);
    } catch (const CBlastException& exptn) {
        Blast_MessageWrite(&m_icErrMsg, BLAST_SEV_ERROR, exptn.GetErrCode(),
                           1, exptn.GetErrCodeString());
    }

    // Do not set right away
    m_iAvgLength = 0;
}

CMultiSeqInfo::~CMultiSeqInfo()
{
    NON_CONST_ITERATE(vector<BLAST_SequenceBlk*>, itr, m_ivSeqBlkVec) {
        *itr = BlastSequenceBlkFree(*itr);
    }
    m_ivSeqBlkVec.clear();
}

void* CMultiSeqInfo::GetSeqId(int index)
{
    CSeq_id* seqid = 
        const_cast<CSeq_id*>(&sequence::GetId(*m_vSeqVector[index].seqloc,
                                              m_vSeqVector[index].scope));

    return (void*) seqid;
}

void* CMultiSeqInfo::GetSeqLoc(int index)
{
    return (void*) &m_vSeqVector[index];
}

/** Retrieves the length of the longest sequence in the BlastSeqSrc.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 */
extern "C" {
static Int4 MultiSeqGetMaxLength(void* multiseq_handle, void*)
{
    Int4 retval = 0;
    Uint4 index;
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);
    
    if ((retval = seq_info->GetMaxLength()) > 0)
        return retval;

    for (index=0; index<seq_info->GetNumSeqs(); ++index)
        retval = MAX(retval, seq_info->GetSeqBlk(index)->length);
    seq_info->SetMaxLength(retval);

    return retval;
}

/** Retrieves the length of the longest sequence in the BlastSeqSrc.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 */
static Int4 MultiSeqGetAvgLength(void* multiseq_handle, void*)
{
    Int8 total_length = 0;
    Uint4 num_seqs = 0;
    Uint4 avg_length;
    Uint4 index;
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);

    if ((avg_length = seq_info->GetAvgLength()) > 0)
        return avg_length;

    if ((num_seqs = seq_info->GetNumSeqs()) == 0)
        return 0;
    for (index = 0; index < num_seqs; ++index) 
        total_length += (Int8) seq_info->GetSeqBlk(index)->length;
    avg_length = (Uint4) (total_length / num_seqs);
    seq_info->SetAvgLength(avg_length);

    return avg_length;
}

/** Retrieves the number of sequences in the BlastSeqSrc.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 */
static Int4 MultiSeqGetNumSeqs(void* multiseq_handle, void*)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);
    return seq_info->GetNumSeqs();
}

/** Returns 0 as total length, indicating that this is NOT a database!
 */
static Int8 MultiSeqGetTotLen(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/** Needed for completeness only.
 */
static char* MultiSeqGetName(void* /*multiseq_handle*/, void*)
{
    return NULL;
}

/** Needed for completeness only
 */
static char* MultiSeqGetDefinition(void* /*multiseq_handle*/, void*)
{
    return NULL;
}

/** Needed for completeness only.
 */
static char* MultiSeqGetDate(void* /*multiseq_handle*/, void*)
{
    return NULL;
}

/** Retrieves the date of the BLAST database.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 */
static Boolean MultiSeqGetIsProt(void* multiseq_handle, void*)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);

    return (Boolean) seq_info->GetIsProtein();
}

/** Retrieves the sequence meeting the criteria defined by its second argument.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 * @param args Pointer to GetSeqArg structure [in]
 * @return return codes defined in blast_seqsrc.h
 */
static Int2 MultiSeqGetSequence(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;
    GetSeqArg* seq_args = (GetSeqArg*) args;
    Int4 index;

    ASSERT(seq_info);
    ASSERT(args);

    if (seq_info->GetNumSeqs() == 0 || !seq_args)
        return BLAST_SEQSRC_ERROR;

    index = seq_args->oid;

    if (index >= (Int4) seq_info->GetNumSeqs())
        return BLAST_SEQSRC_EOF;

    BlastSequenceBlkCopy(&seq_args->seq, seq_info->GetSeqBlk(index));
    /* If this is a nucleotide sequence, and it is the traceback stage, 
       we need the uncompressed buffer, stored in the 'sequence_start' 
       pointer. */
    if (seq_args->encoding != BLASTP_ENCODING &&
        seq_args->encoding != NCBI2NA_ENCODING) {
        seq_args->seq->sequence = seq_args->seq->sequence_start + 1;
    }
    seq_args->seq->oid = index;
    return BLAST_SEQSRC_SUCCESS;
}

/** Retrieves the sequence identifier given its index into the sequence vector.
 * Client code is responsible for deallocating the return value. 
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 * @param args Pointer to integer indicating ordinal id [in]
 * @return Sequence id structure generated from ASN.1 spec, 
 *         cast to a void pointer.
 */
static ListNode* MultiSeqGetSeqId(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;
    Int4 index;
    ListNode* seqid_wrap;

    ASSERT(seq_info);
    ASSERT(args);

    index = *((Int4*) args);

    seqid_wrap = ListNodeAddPointer(NULL, BLAST_SEQSRC_CPP_SEQID, 
                                    seq_info->GetSeqId(index));
    return seqid_wrap;
}

/** Retrieves the sequence identifier in character string form.
 * Client code is responsible for deallocating the return value. 
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 * @param args Pointer to integer indicating index into the vector [in]
 */
static char* MultiSeqGetSeqIdStr(void* multiseq_handle, void* args)
{
    CSeq_id* seqid;
    char *seqid_str = NULL;
    ListNode* seqid_wrap;

    ASSERT(args);
    seqid_wrap = MultiSeqGetSeqId(multiseq_handle, args);
    if (seqid_wrap->choice != BLAST_SEQSRC_CPP_SEQID)
        return NULL;
    seqid = (CSeq_id*) seqid_wrap->ptr;

    if (seqid)
        seqid_str = strdup(seqid->GetSeqIdString().c_str());

    return seqid_str;
}

/** Retrieves the sequence identifier given its index into the sequence vector.
 * Client code is responsible for deallocating the return value. 
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 * @param args Pointer to integer indicating ordinal id [in]
 * @return Sequence id structure generated from ASN.1 spec, 
 *         cast to a void pointer.
 */
static ListNode* MultiSeqGetSeqLoc(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;
    Int4 index;
    ListNode* seqloc_wrap;

    ASSERT(seq_info);
    ASSERT(args);

    index = *((Int4*) args);

    seqloc_wrap = ListNodeAddPointer(NULL, BLAST_SEQSRC_CPP_SEQLOC, 
                                     seq_info->GetSeqLoc(index));
    return seqloc_wrap;
}

/** Retrieve length of a given sequence.
 * @param multiseq_handle Pointer to the structure containing sequences [in]
 * @param args Pointer to integer indicating index into the sequences 
 *             vector [in]
 * @return Length of the sequence or BLAST_SEQSRC_ERROR.
 */
static Int4 MultiSeqGetSeqLen(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*)multiseq_handle;
    Int4 index;

    ASSERT(seq_info);
    ASSERT(args);

    index = *((Int4*) args);
    return seq_info->GetSeqBlk(index)->length;
}

/** Gets the next sequence index, given a BlastSeqSrc pointer. */
static Int4 MultiSeqIteratorNext(void* seqsrc, BlastSeqSrcIterator* itr)
{
    BlastSeqSrc* seq_src = (BlastSeqSrc*) seqsrc;
    Int4 retval = BLAST_SEQSRC_EOF;
    Int2 status = 0;

    ASSERT(seq_src);
    ASSERT(itr);

    if ((status = BLASTSeqSrcGetNextChunk(seq_src, itr))
        == BLAST_SEQSRC_EOF) {
        return status;
    }
    retval = itr->current_pos++;

    return retval;
}

/** Gets the next sequence index, given a MultiSeqInfo pointer. */
static Int2 MultiSeqGetNextChunk(void* multiseq_handle, 
                                 BlastSeqSrcIterator* itr)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(itr);

    if (itr->current_pos == UINT4_MAX) {
        itr->current_pos = 0;
    }

    if (itr->current_pos >= seq_info->GetNumSeqs())
        return BLAST_SEQSRC_EOF;

    return BLAST_SEQSRC_SUCCESS;
}

ListNode* MultiSeqGetErrorMessage(void* multiseq_handle, void*)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;
    Blast_Message* error_msg = seq_info->GetErrorMessage();
    ListNode* retval = NULL;
    if (error_msg) {
        retval = 
            ListNodeAddPointer(NULL, BLAST_SEQSRC_MESSAGE, (void*) error_msg);
    }
    return retval;
}

BlastSeqSrc* MultiSeqSrcNew(BlastSeqSrc* retval, void* args)
{
    SMultiSeqSrcNewArgs* seqsrc_args = (SMultiSeqSrcNewArgs*) args;

    if (!retval)
        return NULL;
    
    CMultiSeqInfo* seq_info = 
        new CMultiSeqInfo(seqsrc_args->seq_vector, seqsrc_args->program);
    
    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seq_info */
    SetDeleteFnPtr(retval, &MultiSeqSrcFree);
    SetDataStructure(retval, (void*) seq_info);
    SetGetNumSeqs(retval, &MultiSeqGetNumSeqs);
    SetGetMaxSeqLen(retval, &MultiSeqGetMaxLength);
    SetGetAvgSeqLen(retval, &MultiSeqGetAvgLength);
    SetGetTotLen(retval, &MultiSeqGetTotLen);
    SetGetName(retval, &MultiSeqGetName);
    SetGetDefinition(retval, &MultiSeqGetDefinition);
    SetGetDate(retval, &MultiSeqGetDate);
    SetGetIsProt(retval, &MultiSeqGetIsProt);
    SetGetSequence(retval, &MultiSeqGetSequence);
    SetGetSeqIdStr(retval, &MultiSeqGetSeqIdStr);
    SetGetSeqId(retval, &MultiSeqGetSeqId);
    SetGetSeqLoc(retval, &MultiSeqGetSeqLoc);
    SetGetSeqLen(retval, &MultiSeqGetSeqLen);
    SetGetNextChunk(retval, &MultiSeqGetNextChunk);
    SetIterNext(retval, &MultiSeqIteratorNext);
    SetGetError(retval, &MultiSeqGetErrorMessage);

    return retval;
}

} // extern "C"

BlastSeqSrc* MultiSeqSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CMultiSeqInfo* seq_info = static_cast<CMultiSeqInfo*>
                                (GetDataStructure(seq_src));

    delete seq_info;
    sfree(seq_src);

    return NULL;
}

/// @bug There is no guarantee that error_msg->code maps to a 
/// CBlastException error code, this could be meaningless
BlastSeqSrc*
MultiSeqSrcInit(const TSeqLocVector& seq_vector, EProgram program)
{
    BlastSeqSrc* seq_src = NULL;
    BlastSeqSrcNewInfo bssn_info;
    // FIXME: Why are we using calloc? Why not use operator new?
    SMultiSeqSrcNewArgs* args =
        (SMultiSeqSrcNewArgs*) calloc(1, sizeof(SMultiSeqSrcNewArgs));;
    args->seq_vector = (TSeqLocVector) seq_vector;
    args->program = program;
    bssn_info.constructor = &MultiSeqSrcNew;
    bssn_info.ctor_argument = (void*) args;

    seq_src = BlastSeqSrcNew(&bssn_info);
    ListNode* error_wrap = BLASTSeqSrcGetError(seq_src);
    Blast_Message* error_msg = NULL;
    if (error_wrap && error_wrap->choice == BLAST_SEQSRC_MESSAGE)
        error_msg = (Blast_Message*) error_wrap->ptr;
    if (error_msg && error_msg->code < CBlastException::eMaxErrCode) {
        throw CBlastException(__FILE__, __LINE__, 0,
                              (CBlastException::EErrCode) error_msg->code,
                              error_msg->message);
    }
    return seq_src;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.7  2004/03/22 16:15:21  camacho
 * doxygen fix
 *
 * Revision 1.6  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 *
 * ===========================================================================
 */
