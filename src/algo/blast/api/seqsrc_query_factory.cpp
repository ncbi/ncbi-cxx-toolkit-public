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

/// @file seqsrc_query_factory.cpp
/// Implementation of the BlastSeqSrc interface for a query factory

#include <ncbi_pch.hpp>
#include "seqsrc_query_factory.hpp"
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/blast_exception.hpp>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include "bioseq_extract_data_priv.hpp"
#include "blast_objmgr_priv.hpp"        // for SetupSubjects

/** @addtogroup AlgoBlast
 *
 * @{
 */

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/////////////////////////////////////////////////////////////////////////////
//
// CQueryFactoryInfo
//
/////////////////////////////////////////////////////////////////////////////
    
/// Contains information about all sequences in a set.
class CQueryFactoryInfo : public CObject 
{
public: 
    /// Constructor from a vector of sequence location/scope pairs and a 
    /// BLAST program type.
    CQueryFactoryInfo(CRef<IQueryFactory> qf, EBlastProgramType program);
    CQueryFactoryInfo(const TSeqLocVector& subject_seqs, 
                      EBlastProgramType program);
    ~CQueryFactoryInfo();
    /// Setter and getter functions for the private fields
    Uint4 GetMaxLength();
    Uint4 GetMinLength();
    Uint4 GetAvgLength();
    void SetAvgLength(Uint4 val);
    bool GetIsProtein();
    Uint4 GetNumSeqs();
    BLAST_SequenceBlk* GetSeqBlk(Uint4 index);
private:
    bool m_IsProt; ///< Are these sequences protein or nucleotide? 
    vector<BLAST_SequenceBlk*> m_SeqBlkVector; ///< Vector of sequence blocks
    unsigned int m_MaxLength; ///< Length of the longest sequence in this set
    unsigned int m_MinLength; ///< Length of the longest sequence in this set
    unsigned int m_AvgLength; ///< Average length of sequences in this set
    /// local query data obtained from the query factory
    CRef<IBlastQuerySource> m_QuerySource;
    Uint4 m_NumSeqs;    ///< Number of sequences
};

/// Constructor
CQueryFactoryInfo::CQueryFactoryInfo(CRef<IQueryFactory> query_factory, 
                                     EBlastProgramType program)
: m_IsProt(Blast_SubjectIsProtein(program) ? true : false), m_MaxLength(0),
      m_MinLength(1), m_AvgLength(0), m_QuerySource(0), m_NumSeqs(0)
{
    CRef<IRemoteQueryData> query_data(query_factory->MakeRemoteQueryData());
    CRef<CBioseq_set> bss(query_data->GetBioseqSet());
    _ASSERT(bss.NotEmpty());
    m_QuerySource.Reset(new CBlastQuerySourceBioseqSet(*bss, m_IsProt));
    if ( !m_QuerySource ) {
        NCBI_THROW(CBlastException, eSeqSrcInit,
                   "Failed to initialize sequences for IQueryFactory");
    }

    // TODO support for m_MinLength
    SetupSubjects_OMF(*m_QuerySource, program, &m_SeqBlkVector, &m_MaxLength);
    m_NumSeqs = static_cast<Uint4>(m_QuerySource->Size());
    _ASSERT(!m_SeqBlkVector.empty());
}

CQueryFactoryInfo::CQueryFactoryInfo(const TSeqLocVector& subj_seqs,
                                     EBlastProgramType program)
: m_IsProt(Blast_SubjectIsProtein(program) ? true : false), m_MaxLength(0),
      m_MinLength(1), m_AvgLength(0), m_QuerySource(0), m_NumSeqs(subj_seqs.size())
{
    // Fix subject location for tblast[nx].  
    if (Blast_SubjectIsTranslated(program))
    {
        TSeqLocVector temp_slv;
        vector<Int2> strand_v;
        ITERATE(TSeqLocVector, iter, subj_seqs)
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

        SetupSubjects(temp_slv, program, &m_SeqBlkVector, &m_MaxLength);

        int index=0;
        ITERATE(vector<Int2>, s_iter, strand_v)
        {
        	m_SeqBlkVector[index++]->subject_strand = *s_iter;
        }
    }
    else
    	SetupSubjects(const_cast<TSeqLocVector&>(subj_seqs), program, &m_SeqBlkVector, &m_MaxLength);

    _ASSERT(!m_SeqBlkVector.empty());
}

/// Destructor
CQueryFactoryInfo::~CQueryFactoryInfo()
{
    NON_CONST_ITERATE(vector<BLAST_SequenceBlk*>, itr, m_SeqBlkVector) {
        *itr = BlastSequenceBlkFree(*itr);
    }
    m_SeqBlkVector.clear();
    m_QuerySource.Reset();
}


/// Returns maximal length of a set of sequences
inline Uint4 CQueryFactoryInfo::GetMaxLength()
{
    return m_MaxLength;
}

/// Returns minimal length of a set of sequences
inline Uint4 CQueryFactoryInfo::GetMinLength()
{
    return m_MinLength;
}

/// Returns average length
inline Uint4 CQueryFactoryInfo::GetAvgLength()
{
    return m_AvgLength;
}

/// Sets average length
inline void CQueryFactoryInfo::SetAvgLength(Uint4 length)
{
    m_AvgLength = length;
}

/// Answers whether sequences in this object are protein or nucleotide
inline bool CQueryFactoryInfo::GetIsProtein()
{
    return m_IsProt;
}

/// Returns number of sequences
inline Uint4 CQueryFactoryInfo::GetNumSeqs()
{
    return m_NumSeqs;
}

/// Returns sequence block structure for one of the sequences
/// @param index Which sequence to retrieve sequence block for? [in]
/// @return The sequence block.
inline BLAST_SequenceBlk* CQueryFactoryInfo::GetSeqBlk(Uint4 index)
{
    // N.B.: we're not using the at() method for compatibility with GCC 2.95
    if (index >= GetNumSeqs()) {
        throw std::out_of_range("");
    }
    return m_SeqBlkVector[index];
}

/// The following functions interact with the C API, and have to be 
/// declared extern "C".

extern "C" {

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_QueryFactoryGetMaxLength(void* multiseq_handle, void*)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    _ASSERT(seq_info);
    return (*seq_info)->GetMaxLength();
}

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_QueryFactoryGetMinLength(void* multiseq_handle, void*)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    _ASSERT(seq_info);
    return (*seq_info)->GetMinLength();
}

/// Retrieves the average length of the sequence in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_QueryFactoryGetAvgLength(void* multiseq_handle, void*)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    _ASSERT(seq_info);

    if ((*seq_info)->GetAvgLength() == 0) {
        const Uint4 num_seqs((*seq_info)->GetNumSeqs());
        _ASSERT(num_seqs > 0);

        Int8 total_length(0);
        for (Uint4 index = 0; index < num_seqs; ++index) 
            total_length += (Int8) (*seq_info)->GetSeqBlk(index)->length;
        (*seq_info)->SetAvgLength((Uint4) (total_length / num_seqs));
    }
    return (*seq_info)->GetAvgLength();
}

/// Retrieves the number of sequences in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_QueryFactoryGetNumSeqs(void* multiseq_handle, void*)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    _ASSERT(seq_info);
    return (*seq_info)->GetNumSeqs();
}


/// Returns zero as this implementation does not use an alias file.
static Int4 
s_QueryFactoryGetNumSeqsStats(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Returns 0 as total length, indicating that this is NOT a database!
static Int8 
s_QueryFactoryGetTotLen(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Returns 0 as total statistic length, as this implementation does not use alias files.
static Int8 
s_QueryFactoryGetTotLenStats(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Always returns NcbiEmptyCStr
static const char* 
s_QueryFactoryGetName(void* /*multiseq_handle*/, void*)
{
    return NcbiEmptyCStr;
}

/// Answers whether this object is for protein or nucleotide sequences.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Boolean 
s_QueryFactoryGetIsProt(void* multiseq_handle, void*)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    _ASSERT(seq_info);
    return (Boolean) (*seq_info)->GetIsProtein();
}

/// Retrieves the sequence for a given index, in a given encoding.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
/// @param args Pointer to BlastSeqSrcGetSeqArg structure, containing sequence index and 
///             encoding. [in]
/// @return return codes defined in blast_seqsrc.h
static Int2 
s_QueryFactoryGetSequence(void* multiseq_handle, BlastSeqSrcGetSeqArg* args)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);

    _ASSERT(seq_info);
    _ASSERT(args);

    if ((*seq_info)->GetNumSeqs() == 0 || !args)
        return BLAST_SEQSRC_ERROR;

    BLAST_SequenceBlk* seq_blk(0);
    try { seq_blk = (*seq_info)->GetSeqBlk(args->oid); }
    catch (const std::out_of_range&) {
        return BLAST_SEQSRC_EOF;
    }
    _ASSERT(seq_blk);

    BlastSequenceBlkCopy(&args->seq, seq_blk);
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

    args->seq->oid = args->oid;
    return BLAST_SEQSRC_SUCCESS;
}

/// Deallocates the uncompressed sequence buffer if necessary.
/// @param args Pointer to BlastSeqSrcGetSeqArg structure [in]
static void
s_QueryFactoryReleaseSequence(void* /*multiseq_handle*/, BlastSeqSrcGetSeqArg* args)
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
s_QueryFactoryGetSeqLen(void* multiseq_handle, void* args)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);
    Int4 index;

    _ASSERT(seq_info);
    _ASSERT(args);

    index = *((Int4*) args);
    return (*seq_info)->GetSeqBlk(index)->length;
}

/// Mirrors the database iteration interface. Next chunk of indices retrieval 
/// is really just a check that current index has not reached the end.
/// @todo Does this need to be so complicated? Why not simply have all logic in 
///       s_QueryFactoryIteratorNext? - Answer: as explained in the comments, the
///       GetNextChunk functionality is provided as a convenience to provide
///       MT-safe iteration over a BlastSeqSrc implementation.
/// @param multiseq_handle Pointer to the multiple sequence object [in]
/// @param itr Iterator over multiseq_handle [in] [out]
/// @return Status.
static Int2 
s_QueryFactoryGetNextChunk(void* multiseq_handle, BlastSeqSrcIterator* itr)
{
    CRef<CQueryFactoryInfo>* seq_info = 
        static_cast<CRef<CQueryFactoryInfo>*>(multiseq_handle);

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
s_QueryFactoryResetChunkIter(void* /*multiseq_handle*/)
{
    return;
}

/// Gets the next sequence index, given a BlastSeqSrc pointer.
/// @param multiseq_handle Handle to access the underlying object over which
///                        iteration occurs. [in]
/// @param itr Iterator over seqsrc [in] [out]
/// @return Next index in the sequence set
static Int4 
s_QueryFactoryIteratorNext(void* multiseq_handle, BlastSeqSrcIterator* itr)
{
    Int4 retval = BLAST_SEQSRC_EOF;
    Int2 status = 0;

    _ASSERT(multiseq_handle);
    _ASSERT(itr);

    if ((status = s_QueryFactoryGetNextChunk(multiseq_handle, itr))
        == BLAST_SEQSRC_EOF) {
        return status;
    }
    retval = itr->current_pos++;

    return retval;
}

/// Encapsulates the arguments needed to initialize multi-sequence source.
struct SQueryFactorySrcNewArgs {
    CRef<IQueryFactory> query_factory;  ///< The query factory
    TSeqLocVector subj_seqs;            ///< The subject sequences
    EBlastProgramType program; ///< BLAST program

    /// Constructor
    SQueryFactorySrcNewArgs(CRef<IQueryFactory> qf, 
                            const TSeqLocVector& subj_seqs,
                            EBlastProgramType p)
        : query_factory(qf), subj_seqs(subj_seqs), program(p) {}
};

/// Multi sequence source destructor: frees its internal data structure
/// @param seq_src BlastSeqSrc structure to free [in]
/// @return NULL
static BlastSeqSrc* 
s_QueryFactorySrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CQueryFactoryInfo>* seq_info = static_cast<CRef<CQueryFactoryInfo>*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    delete seq_info;
    return NULL;
}

/// Multi-sequence sequence source copier: creates a new reference to the
/// CQueryFactoryInfo object and copies the rest of the BlastSeqSrc structure.
/// @param seq_src BlastSeqSrc structure to copy [in]
/// @return Pointer to the new BlastSeqSrc.
static BlastSeqSrc* 
s_QueryFactorySrcCopy(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CRef<CQueryFactoryInfo>* seq_info = static_cast<CRef<CQueryFactoryInfo>*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    CRef<CQueryFactoryInfo>* seq_info2 = new CRef<CQueryFactoryInfo>(*seq_info);

    _BlastSeqSrcImpl_SetDataStructure(seq_src, (void*) seq_info2);
    
    return seq_src;
}

/// Multi-sequence source constructor 
/// @param retval BlastSeqSrc structure (already allocated) to populate [in]
/// @param args Pointer to QueryFactorySrcNewArgs structure above [in]
/// @return Updated bssp structure (with all function pointers initialized
static BlastSeqSrc* 
s_QueryFactorySrcNew(BlastSeqSrc* retval, void* args)
{
    _ASSERT(retval);
    _ASSERT(args);

    SQueryFactorySrcNewArgs* seqsrc_args = (SQueryFactorySrcNewArgs*) args;
    
    CRef<CQueryFactoryInfo>* seq_info =  new CRef<CQueryFactoryInfo>(NULL);
    try {
        if (seqsrc_args->query_factory) {
            seq_info->Reset(new CQueryFactoryInfo(seqsrc_args->query_factory, 
                                                  seqsrc_args->program));
        } else {
            seq_info->Reset(new CQueryFactoryInfo(seqsrc_args->subj_seqs, 
                                                  seqsrc_args->program));
        }
    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.ReportAll().c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CQueryFactoryInfo constructor"));
    }

    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seq_info */
    _BlastSeqSrcImpl_SetDeleteFnPtr(retval, &s_QueryFactorySrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr(retval, &s_QueryFactorySrcCopy);
    _BlastSeqSrcImpl_SetDataStructure(retval, (void*) seq_info);
    _BlastSeqSrcImpl_SetGetNumSeqs(retval, &s_QueryFactoryGetNumSeqs);
    _BlastSeqSrcImpl_SetGetNumSeqsStats(retval, &s_QueryFactoryGetNumSeqsStats);
    _BlastSeqSrcImpl_SetGetMaxSeqLen(retval, &s_QueryFactoryGetMaxLength);
    _BlastSeqSrcImpl_SetGetMinSeqLen(retval, &s_QueryFactoryGetMinLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen(retval, &s_QueryFactoryGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen(retval, &s_QueryFactoryGetTotLen);
    _BlastSeqSrcImpl_SetGetTotLenStats(retval, &s_QueryFactoryGetTotLenStats);
    _BlastSeqSrcImpl_SetGetName(retval, &s_QueryFactoryGetName);
    _BlastSeqSrcImpl_SetGetIsProt(retval, &s_QueryFactoryGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence(retval, &s_QueryFactoryGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen(retval, &s_QueryFactoryGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext(retval, &s_QueryFactoryIteratorNext);
    _BlastSeqSrcImpl_SetResetChunkIterator(retval, 
                                           &s_QueryFactoryResetChunkIter);
    _BlastSeqSrcImpl_SetReleaseSequence(retval, &s_QueryFactoryReleaseSequence);

    return retval;
}

} // extern "C"

static BlastSeqSrc*
s_QueryFactoryBlastSeqSrcInit(CRef<IQueryFactory> query_factory,
                              const TSeqLocVector& subj_seqs,
                              EBlastProgramType program)
{
    BlastSeqSrc* seq_src = NULL;
    BlastSeqSrcNewInfo bssn_info;

    if (query_factory.Empty() && subj_seqs.empty()) {
        NCBI_THROW(CBlastException, eInvalidArgument, 
                   "Must provide either a query factory or subject sequences");
    }

    SQueryFactorySrcNewArgs args(query_factory, subj_seqs, program);

    bssn_info.constructor = &s_QueryFactorySrcNew;
    bssn_info.ctor_argument = (void*) &args;

    seq_src = BlastSeqSrcNew(&bssn_info);
    return seq_src;
}

BlastSeqSrc*
QueryFactoryBlastSeqSrcInit(CRef<IQueryFactory> query_factory, 
                            EBlastProgramType program)
{
    TSeqLocVector empty;
    return s_QueryFactoryBlastSeqSrcInit(query_factory, empty, program);
}

BlastSeqSrc*
QueryFactoryBlastSeqSrcInit(const TSeqLocVector& subj_seqs,
                            EBlastProgramType program)
{
    CRef<IQueryFactory> empty;
    return s_QueryFactoryBlastSeqSrcInit(empty, subj_seqs, program);
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */
