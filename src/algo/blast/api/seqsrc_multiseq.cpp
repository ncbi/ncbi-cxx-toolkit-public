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

/// @file seqsrc_multiseq.cpp
/// Implementation of the BlastSeqSrc interface for a vector of sequence 
/// locations.

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/seqsrc_multiseq.hpp>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include "blast_objmgr_priv.hpp"

#include <memory>

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Contains information about all sequences in a set.
class CMultiSeqInfo : public CObject 
{
public: 
    /// Constructor from a vector of sequence location/scope pairs and a 
    /// BLAST program type.
    CMultiSeqInfo(TSeqLocVector& seq_vector, EBlastProgramType program, bool dbscan_mode);
    ~CMultiSeqInfo();
    /// Setter and getter functions for the private fields
    Uint4 GetMaxLength();
    void SetMaxLength(Uint4 val);
    Uint4 GetAvgLength();
    void SetAvgLength(Uint4 val);
    Int8 GetTotLength();
    bool GetIsProtein();
    Uint4 GetNumSeqs();
    BLAST_SequenceBlk* GetSeqBlk(int index);
private:
    /// Internal fields
    bool m_ibIsProt; ///< Are these sequences protein or nucleotide? 
    vector<BLAST_SequenceBlk*> m_ivSeqBlkVec; ///< Vector of sequence blocks
    unsigned int m_iMaxLength; ///< Length of the longest sequence in this set
    unsigned int m_iAvgLength; ///< Average length of sequences in this set
    Int8 m_iTotalLength; ///< Total length of sequences in this set
    bool m_DbScanMode; ///< Database scanning mode (not pairwise)
};

/// Returns maximal length of a set of sequences
inline Uint4 CMultiSeqInfo::GetMaxLength()
{
    return m_iMaxLength;
}

/// Sets maximal length
inline void CMultiSeqInfo::SetMaxLength(Uint4 length)
{
    m_iMaxLength = length;
}

/// Returns average length
inline Uint4 CMultiSeqInfo::GetAvgLength()
{
    return m_iAvgLength;
}

/// Sets average length
inline void CMultiSeqInfo::SetAvgLength(Uint4 length)
{
    m_iAvgLength = length;
}

/// Returns total length
inline Int8 CMultiSeqInfo::GetTotLength()
{
    return m_iTotalLength;
}

/// Answers whether sequences in this object are protein or nucleotide
inline bool CMultiSeqInfo::GetIsProtein()
{
    return m_ibIsProt;
}

/// Returns number of sequences
inline Uint4 CMultiSeqInfo::GetNumSeqs()
{
    return (Uint4) m_ivSeqBlkVec.size();
}

/// Returns sequence block structure for one of the sequences
/// @param index Which sequence to retrieve sequence block for? [in]
/// @return The sequence block.
inline BLAST_SequenceBlk* CMultiSeqInfo::GetSeqBlk(int index)
{
    _ASSERT(!m_ivSeqBlkVec.empty());
    _ASSERT((int)m_ivSeqBlkVec.size() > index);
    return m_ivSeqBlkVec[index];
}

/// Constructor
CMultiSeqInfo::CMultiSeqInfo(TSeqLocVector& seq_vector, 
                             EBlastProgramType program,
                             bool dbscan_mode)
{
    m_ibIsProt = Blast_SubjectIsProtein(program) ? true : false;
    m_DbScanMode = dbscan_mode;
    m_iTotalLength=0;
    
    // Fix subject location for tblast[nx].  
    if (Blast_SubjectIsTranslated(program))
    {
        TSeqLocVector temp_slv;
        vector<Int2> strand_v;
        ITERATE(TSeqLocVector, iter, seq_vector)
        {
            strand_v.push_back((Int2) (*iter).seqloc->GetStrand());
            CRef<CSeq_loc> sl(new CSeq_loc);
            sl->Assign(*((*iter).seqloc));
            sl->SetStrand(eNa_strand_both);
            if ((*iter).mask) 
            {
                CRef<CSeq_loc> mask_sl(new CSeq_loc);
                mask_sl->Assign(*((*iter).mask));
            	SSeqLoc sseq_loc(*sl, *((*iter).scope), *mask_sl);
            	temp_slv.push_back(sseq_loc);
            }
            else
            {
                SSeqLoc sseq_loc(*sl, *((*iter).scope));
            	temp_slv.push_back(sseq_loc);
            }
        }

        SetupSubjects(temp_slv, program, &m_ivSeqBlkVec, &m_iMaxLength);

        int index=0;
        ITERATE(vector<Int2>, s_iter, strand_v)
        {
        	m_ivSeqBlkVec[index++]->subject_strand = *s_iter;
        }
    }
    else
    	SetupSubjects(seq_vector, program, &m_ivSeqBlkVec, &m_iMaxLength);

    if(dbscan_mode)
    {
	ITERATE(vector<BLAST_SequenceBlk*>, iter, m_ivSeqBlkVec)
	{
            m_iTotalLength += (Int8) (*iter)->length;
	}
    }
    // Do not set right away
    m_iAvgLength = 0;
}

/// Destructor
CMultiSeqInfo::~CMultiSeqInfo()
{
    NON_CONST_ITERATE(vector<BLAST_SequenceBlk*>, itr, m_ivSeqBlkVec) {
        *itr = BlastSequenceBlkFree(*itr);
    }
    m_ivSeqBlkVec.clear();
}

/// The following functions interact with the C API, and have to be 
/// declared extern "C".

extern "C" {

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_MultiSeqGetMaxLength(void* multiseq_handle, void*)
{
    Int4 retval = 0;
    Uint4 index;
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());
    
    if ((retval = (*seq_info)->GetMaxLength()) > 0)
        return retval;

    for (index=0; index<(*seq_info)->GetNumSeqs(); ++index)
        retval = MAX(retval, (*seq_info)->GetSeqBlk(index)->length);
    (*seq_info)->SetMaxLength(retval);

    return retval;
}

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_MultiSeqGetMinLength(void* multiseq_handle, void*)
{
    Int4 retval = INT4_MAX;
    Uint4 index;
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    for (index=0; index<(*seq_info)->GetNumSeqs(); ++index)
        retval = MIN(retval, (*seq_info)->GetSeqBlk(index)->length);

    if(retval < BLAST_SEQSRC_MINLENGTH)
	retval = BLAST_SEQSRC_MINLENGTH;

    return retval;
}

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_MultiSeqGetAvgLength(void* multiseq_handle, void*)
{
    Int8 total_length = 0;
    Uint4 num_seqs = 0;
    Uint4 avg_length;
    Uint4 index;
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());

    if ((avg_length = (*seq_info)->GetAvgLength()) > 0)
        return avg_length;

    if ((num_seqs = (*seq_info)->GetNumSeqs()) == 0)
        return 0;
    for (index = 0; index < num_seqs; ++index) 
        total_length += (Int8) (*seq_info)->GetSeqBlk(index)->length;
    avg_length = (Uint4) (total_length / num_seqs);
    (*seq_info)->SetAvgLength(avg_length);

    return avg_length;
}

/// Retrieves the number of sequences in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_MultiSeqGetNumSeqs(void* multiseq_handle, void*)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());
    return (*seq_info)->GetNumSeqs();
}

/// Returns zero as this implementation does not support alias files.
static Int4 
s_MultiSeqGetNumSeqsStats(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Returns total length in db scan mode.
/// Returns 0 as total length, indicating that this is not a database search.
static Int8 
s_MultiSeqGetTotLen(void* multiseq_handle, void*)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);
    return (*seq_info)->GetTotLength();
}

/// Returns 0 as this implementation does not use alias files.
static Int8 
s_MultiSeqGetTotLenStats(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Always returns NcbiEmptyCStr
static const char* 
s_MultiSeqGetName(void* /*multiseq_handle*/, void*)
{
    return NcbiEmptyCStr;
}

/// Answers whether this object is for protein or nucleotide sequences.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Boolean 
s_MultiSeqGetIsProt(void* multiseq_handle, void*)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());

    return (Boolean) (*seq_info)->GetIsProtein();
}

/// Retrieves the sequence for a given index, in a given encoding.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
/// @param args Pointer to BlastSeqSrcGetSeqArg structure, containing sequence index and 
///             encoding. [in]
/// @return return codes defined in blast_seqsrc.h
static Int2 
s_MultiSeqGetSequence(void* multiseq_handle, BlastSeqSrcGetSeqArg* args)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);
    Int4 index;

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());
    _ASSERT(args);

    if ((*seq_info)->GetNumSeqs() == 0 || !args)
        return BLAST_SEQSRC_ERROR;

    index = args->oid;

    if (index >= (Int4) (*seq_info)->GetNumSeqs())
        return BLAST_SEQSRC_EOF;

    BlastSequenceBlkCopy(&args->seq, (*seq_info)->GetSeqBlk(index));
    /* If this is a nucleotide sequence, and it is the traceback stage, 
       we need the uncompressed buffer, stored in the 'sequence_start' 
       pointer. That buffer has an extra sentinel byte for blastn, but
       no sentinel byte for translated programs. */
    if (args->encoding == eBlastEncodingNucleotide) {
        args->seq->sequence = args->seq->sequence_start + 1;
    } else if (args->encoding == eBlastEncodingNcbi4na) {
        args->seq->sequence = args->seq->sequence_start;
    }

    // these are not applicable to encode subject masks, instead seq_ranges
    // should be utilized
    _ASSERT(args->seq->lcase_mask == NULL);
    _ASSERT(args->seq->lcase_mask_allocated == FALSE);

    args->seq->oid = index;
    return BLAST_SEQSRC_SUCCESS;
}

/// Deallocates the uncompressed sequence buffer if necessary.
/// @param args Pointer to BlastSeqSrcGetSeqArg structure [in]
static void
s_MultiSeqReleaseSequence(void* /*multiseq_handle*/, BlastSeqSrcGetSeqArg* args)
{
    _ASSERT(args);
    if (args->seq->sequence_start_allocated)
        sfree(args->seq->sequence_start);
}

/// Retrieve length of a given sequence.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
/// @param args Pointer to integer indicating index into the sequences 
///             vector [in]
/// @return Length of the sequence or BLAST_SEQSRC_ERROR.
static Int4 
s_MultiSeqGetSeqLen(void* multiseq_handle, void* args)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);
    Int4 index;

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());
    _ASSERT(args);

    index = *((Int4*) args);
    return (*seq_info)->GetSeqBlk(index)->length;
}

/// Mirrors the database iteration interface. Next chunk of indices retrieval 
/// is really just a check that current index has not reached the end.
/// @todo Does this need to be so complicated? Why not simply have all logic in 
///       s_MultiSeqIteratorNext? - Answer: as explained in the comments, the
///       GetNextChunk functionality is provided as a convenience to provide
///       MT-safe iteration over a BlastSeqSrc implementation.
/// @param multiseq_handle Pointer to the multiple sequence object [in]
/// @param itr Iterator over multiseq_handle [in] [out]
/// @return Status.
static Int2 
s_MultiSeqGetNextChunk(void* multiseq_handle, BlastSeqSrcIterator* itr)
{
    CRef<CMultiSeqInfo>* seq_info =
        static_cast<CRef<CMultiSeqInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(seq_info->NotEmpty());
    _ASSERT(itr);

    if (itr->current_pos == UINT4_MAX) {
        itr->current_pos = 0;
    }

    if (itr->current_pos >= (*seq_info)->GetNumSeqs())
        return BLAST_SEQSRC_EOF;

    return BLAST_SEQSRC_SUCCESS;
}

/// Resets the internal bookmark iterator (N/A in this case)
static void
s_MultiSeqResetChunkIter(void* /*multiseq_handle*/)
{
    return;
}

/// Gets the next sequence index, given a BlastSeqSrc pointer.
/// @param multiseq_handle Handle to access the underlying object over which
///                        iteration occurs. [in]
/// @param itr Iterator over seqsrc [in] [out]
/// @return Next index in the sequence set
static Int4 
s_MultiSeqIteratorNext(void* multiseq_handle, BlastSeqSrcIterator* itr)
{
    Int4 retval = BLAST_SEQSRC_EOF;
    Int2 status = 0;

    _ASSERT(multiseq_handle);
    _ASSERT(itr);

    if ((status = s_MultiSeqGetNextChunk(multiseq_handle, itr))
        == BLAST_SEQSRC_EOF) {
        return status;
    }
    retval = itr->current_pos++;

    return retval;
}

/// Encapsulates the arguments needed to initialize multi-sequence source.
struct SMultiSeqSrcNewArgs {
    TSeqLocVector seq_vector;  ///< Vector of sequences
    EBlastProgramType program; ///< BLAST program
    bool dbscan_mode; ///< Database mode (not pairwise)
    /// Constructor
    SMultiSeqSrcNewArgs(TSeqLocVector sv, EBlastProgramType p, bool db)
        : seq_vector(sv), program(p), dbscan_mode(db) {}
};

/// Multi sequence source destructor: frees its internal data structure
/// @param seq_src BlastSeqSrc structure to free [in]
/// @return NULL
static BlastSeqSrc* 
s_MultiSeqSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CMultiSeqInfo>* seq_info = static_cast<CRef<CMultiSeqInfo>*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    delete seq_info;
    return NULL;
}

/// Multi-sequence sequence source copier: creates a new reference to the
/// CMultiSeqInfo object and copies the rest of the BlastSeqSrc structure.
/// @param seq_src BlastSeqSrc structure to copy [in]
/// @return Pointer to the new BlastSeqSrc.
static BlastSeqSrc* 
s_MultiSeqSrcCopy(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CMultiSeqInfo>* seq_info = static_cast<CRef<CMultiSeqInfo>*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    CRef<CMultiSeqInfo>* seq_info2 = new CRef<CMultiSeqInfo>(*seq_info);

    _BlastSeqSrcImpl_SetDataStructure(seq_src, (void*) seq_info2);
    
    return seq_src;
}

/// Multi-sequence source constructor 
/// @param retval BlastSeqSrc structure (already allocated) to populate [in]
/// @param args Pointer to MultiSeqSrcNewArgs structure above [in]
/// @return Updated bssp structure (with all function pointers initialized
static BlastSeqSrc* 
s_MultiSeqSrcNew(BlastSeqSrc* retval, void* args)
{
    _ASSERT(retval);
    _ASSERT(args);

    SMultiSeqSrcNewArgs* seqsrc_args = (SMultiSeqSrcNewArgs*) args;
    
    CRef<CMultiSeqInfo>* seq_info = new CRef<CMultiSeqInfo>(0);
    try {
        seq_info->Reset(new CMultiSeqInfo(seqsrc_args->seq_vector, 
                                          seqsrc_args->program,
                                          seqsrc_args->dbscan_mode));
    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.ReportAll().c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CMultiSeqInfo constructor"));
    }

    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seq_info */
    _BlastSeqSrcImpl_SetDeleteFnPtr(retval, &s_MultiSeqSrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr(retval, &s_MultiSeqSrcCopy);
    _BlastSeqSrcImpl_SetDataStructure(retval, (void*) seq_info);
    _BlastSeqSrcImpl_SetGetNumSeqs(retval, &s_MultiSeqGetNumSeqs);
    _BlastSeqSrcImpl_SetGetNumSeqsStats(retval, &s_MultiSeqGetNumSeqsStats);
    _BlastSeqSrcImpl_SetGetMaxSeqLen(retval, &s_MultiSeqGetMaxLength);
    _BlastSeqSrcImpl_SetGetMinSeqLen(retval, &s_MultiSeqGetMinLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen(retval, &s_MultiSeqGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen(retval, &s_MultiSeqGetTotLen);
    _BlastSeqSrcImpl_SetGetTotLenStats(retval, &s_MultiSeqGetTotLenStats);
    _BlastSeqSrcImpl_SetGetName(retval, &s_MultiSeqGetName);
    _BlastSeqSrcImpl_SetGetIsProt(retval, &s_MultiSeqGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence(retval, &s_MultiSeqGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen(retval, &s_MultiSeqGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext(retval, &s_MultiSeqIteratorNext);
    _BlastSeqSrcImpl_SetResetChunkIterator(retval, &s_MultiSeqResetChunkIter);
    _BlastSeqSrcImpl_SetReleaseSequence(retval, &s_MultiSeqReleaseSequence);

    return retval;
}

} // extern "C"

BlastSeqSrc*
MultiSeqBlastSeqSrcInit(TSeqLocVector& seq_vector, 
                        EBlastProgramType program,
			bool dbscan_mode)
{
    BlastSeqSrc* seq_src = NULL;
    BlastSeqSrcNewInfo bssn_info;

    auto_ptr<SMultiSeqSrcNewArgs> args
        (new SMultiSeqSrcNewArgs(const_cast<TSeqLocVector&>(seq_vector),
                                 program, dbscan_mode));

    bssn_info.constructor = &s_MultiSeqSrcNew;
    bssn_info.ctor_argument = (void*) args.get();

    seq_src = BlastSeqSrcNew(&bssn_info);
    return seq_src;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
