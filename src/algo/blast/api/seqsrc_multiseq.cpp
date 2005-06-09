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
#include <algo/blast/core/blast_def.h>
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
    CMultiSeqInfo(const TSeqLocVector& seq_vector, EBlastProgramType program);
    ~CMultiSeqInfo();
    /// Setter and getter functions for the private fields
    Uint4 GetMaxLength();
    void SetMaxLength(Uint4 val);
    Uint4 GetAvgLength();
    void SetAvgLength(Uint4 val);
    bool GetIsProtein();
    Uint4 GetNumSeqs();
    BLAST_SequenceBlk* GetSeqBlk(int index);
private:
    /// Internal fields
    bool m_ibIsProt; ///< Are these sequences protein or nucleotide? 
    vector<BLAST_SequenceBlk*> m_ivSeqBlkVec; ///< Vector of sequence blocks
    unsigned int m_iMaxLength; ///< Length of the longest sequence in this set
    unsigned int m_iAvgLength; ///< Average length of sequences in this set
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
    return m_ivSeqBlkVec[index];
}

/// Constructor
CMultiSeqInfo::CMultiSeqInfo(const TSeqLocVector& seq_vector, 
                             EBlastProgramType program)
{
    m_ibIsProt = (program == eBlastTypeBlastp || program == eBlastTypeBlastx || 
                  program == eBlastTypePsiBlast || program == eBlastTypeRpsBlast);
    
    SetupSubjects(seq_vector, program, &m_ivSeqBlkVec, &m_iMaxLength);

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
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);
    
    if ((retval = seq_info->GetMaxLength()) > 0)
        return retval;

    for (index=0; index<seq_info->GetNumSeqs(); ++index)
        retval = MAX(retval, seq_info->GetSeqBlk(index)->length);
    seq_info->SetMaxLength(retval);

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

/// Retrieves the number of sequences in the BlastSeqSrc.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Int4 
s_MultiSeqGetNumSeqs(void* multiseq_handle, void*)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);
    return seq_info->GetNumSeqs();
}

/// Returns 0 as total length, indicating that this is NOT a database!
static Int8 
s_MultiSeqGetTotLen(void* /*multiseq_handle*/, void*)
{
    return 0;
}

/// Needed for completeness only. Returns NULL as "database" name
static const char* 
s_MultiSeqGetName(void* /*multiseq_handle*/, void*)
{
    return NULL;
}

/// Answers whether this object is for protein or nucleotide sequences.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
static Boolean 
s_MultiSeqGetIsProt(void* multiseq_handle, void*)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(seq_info);

    return (Boolean) seq_info->GetIsProtein();
}

/// Retrieves the sequence for a given index, in a given encoding.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
/// @param args Pointer to BlastSeqSrcGetSeqArg structure, containing sequence index and 
///             encoding. [in]
/// @return return codes defined in blast_seqsrc.h
static Int2 
s_MultiSeqGetSequence(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;
    BlastSeqSrcGetSeqArg* seq_args = (BlastSeqSrcGetSeqArg*) args;
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
       pointer. That buffer has an extra sentinel byte for blastn, but
       no sentinel byte for translated programs. */
    if (seq_args->encoding == eBlastEncodingNucleotide) {
        seq_args->seq->sequence = seq_args->seq->sequence_start + 1;
    } else if (seq_args->encoding == eBlastEncodingNcbi4na) {
        seq_args->seq->sequence = seq_args->seq->sequence_start;
    }

    seq_args->seq->oid = index;
    return BLAST_SEQSRC_SUCCESS;
}

/// Deallocates the uncompressed sequence buffer if necessary.
/// @param args Pointer to BlastSeqSrcGetSeqArg structure [in]
static void
s_MultiSeqReleaseSequence(void* /*multiseq_handle*/, void* args)
{
    BlastSeqSrcGetSeqArg* seq_args = (BlastSeqSrcGetSeqArg*) args;
    ASSERT(seq_args);
    if (seq_args->seq->sequence_start_allocated)
        sfree(seq_args->seq->sequence_start);
}

/// Retrieve length of a given sequence.
/// @param multiseq_handle Pointer to the structure containing sequences [in]
/// @param args Pointer to integer indicating index into the sequences 
///             vector [in]
/// @return Length of the sequence or BLAST_SEQSRC_ERROR.
static Int4 
s_MultiSeqGetSeqLen(void* multiseq_handle, void* args)
{
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*)multiseq_handle;
    Int4 index;

    ASSERT(seq_info);
    ASSERT(args);

    index = *((Int4*) args);
    return seq_info->GetSeqBlk(index)->length;
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
    CMultiSeqInfo* seq_info = (CMultiSeqInfo*) multiseq_handle;

    ASSERT(itr);

    if (itr->current_pos == UINT4_MAX) {
        itr->current_pos = 0;
    }

    if (itr->current_pos >= seq_info->GetNumSeqs())
        return BLAST_SEQSRC_EOF;

    return BLAST_SEQSRC_SUCCESS;
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

    ASSERT(multiseq_handle);
    ASSERT(itr);

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
    /// Constructor
    SMultiSeqSrcNewArgs(TSeqLocVector sv, EBlastProgramType p)
        : seq_vector(sv), program(p) {}
};

/// Multi sequence source destructor: frees its internal data structure and the
/// BlastSeqSrc structure itself.
/// @param seq_src BlastSeqSrc structure to free [in]
/// @return NULL
static BlastSeqSrc* 
s_MultiSeqSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    CMultiSeqInfo* seq_info = static_cast<CMultiSeqInfo*>
                                (_BlastSeqSrcImpl_GetDataStructure(seq_src));

    delete seq_info;
    sfree(seq_src);

    return NULL;
}

/// Multi-sequence source constructor 
/// @param retval BlastSeqSrc structure (already allocated) to populate [in]
/// @param args Pointer to MultiSeqSrcNewArgs structure above [in]
/// @return Updated bssp structure (with all function pointers initialized
static BlastSeqSrc* 
s_MultiSeqSrcNew(BlastSeqSrc* retval, void* args)
{
    ASSERT(retval);
    ASSERT(args);

    SMultiSeqSrcNewArgs* seqsrc_args = (SMultiSeqSrcNewArgs*) args;
    
    CMultiSeqInfo* seq_info =  NULL;
    try {
        seq_info = new CMultiSeqInfo(seqsrc_args->seq_vector, 
                                     seqsrc_args->program);
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
    /// @todo FIXME Must there be a copy function in this implementation?
    ///       If so, should CMultiSeqInfo* be changed to a CRef
    _BlastSeqSrcImpl_SetDataStructure(retval, (void*) seq_info);
    _BlastSeqSrcImpl_SetGetNumSeqs(retval, &s_MultiSeqGetNumSeqs);
    _BlastSeqSrcImpl_SetGetMaxSeqLen(retval, &s_MultiSeqGetMaxLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen(retval, &s_MultiSeqGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen(retval, &s_MultiSeqGetTotLen);
    _BlastSeqSrcImpl_SetGetName(retval, &s_MultiSeqGetName);
    _BlastSeqSrcImpl_SetGetIsProt(retval, &s_MultiSeqGetIsProt);
    _BlastSeqSrcImpl_SetGetSequence(retval, &s_MultiSeqGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen(retval, &s_MultiSeqGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext(retval, &s_MultiSeqIteratorNext);
    _BlastSeqSrcImpl_SetReleaseSequence(retval, &s_MultiSeqReleaseSequence);

    return retval;
}

} // extern "C"

BlastSeqSrc*
MultiSeqBlastSeqSrcInit(const TSeqLocVector& seq_vector, 
                        EBlastProgramType program)
{
    BlastSeqSrc* seq_src = NULL;
    BlastSeqSrcNewInfo bssn_info;

    auto_ptr<SMultiSeqSrcNewArgs> args
        (new SMultiSeqSrcNewArgs(const_cast<TSeqLocVector&>(seq_vector),
                                 program));

    bssn_info.constructor = &s_MultiSeqSrcNew;
    bssn_info.ctor_argument = (void*) args.get();

    seq_src = BlastSeqSrcNew(&bssn_info);
    return seq_src;
}

END_SCOPE(blast)
END_NCBI_SCOPE


/* @} */

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.35  2005/06/09 20:35:29  camacho
 * Use new private header blast_objmgr_priv.hpp
 *
 * Revision 1.34  2005/05/25 12:47:56  camacho
 * doxygen fix
 *
 * Revision 1.33  2005/05/10 16:08:39  camacho
 * Changed *_ENCODING #defines to EBlastEncoding enumeration
 *
 * Revision 1.32  2005/05/03 21:04:50  dondosha
 * Small doxygen fix
 *
 * Revision 1.31  2005/04/18 14:00:44  camacho
 * Updates following BlastSeqSrc reorganization
 *
 * Revision 1.30  2005/04/13 22:34:47  camacho
 * Renamed BlastSeqSrc RetSequence to ReleaseSequence
 *
 * Revision 1.29  2005/04/06 21:06:18  dondosha
 * Use EBlastProgramType instead of EProgram in non-user-exposed functions
 *
 * Revision 1.28  2005/02/09 21:03:36  dondosha
 * Minor doxygen fixes
 *
 * Revision 1.27  2005/02/08 18:50:28  bealer
 * - Fix type truncation warnings.
 *
 * Revision 1.26  2005/02/02 05:04:47  dondosha
 * Doxygen comments
 *
 * Revision 1.25  2005/01/26 21:03:29  dondosha
 * Made internal functions static, moved internal class from .hpp file
 *
 * Revision 1.24  2004/12/20 20:17:00  camacho
 * + PSI-BLAST
 *
 * Revision 1.23  2004/11/17 20:22:38  camacho
 * 1. Implemented error handling at initialization time.
 * 2. Made initialization function name consistent with other BlastSeqSrc
 *    implementations
 *
 * Revision 1.22  2004/10/06 14:56:13  dondosha
 * Remove methods that are no longer supported by interface
 *
 * Revision 1.21  2004/09/22 13:32:17  kononenk
 * "Diagnostic Message Filtering" functionality added.
 * Added function SetDiagFilter()
 * Added class CDiagCompileInfo and macro DIAG_COMPILE_INFO
 * Module, class and function attribute added to CNcbiDiag and CException
 * Parameters __FILE__ and __LINE in CNcbiDiag and CException changed to
 * 	CDiagCompileInfo + fixes on derived classes and their usage
 * Macro NCBI_MODULE can be used to set default module name in cpp files
 *
 * Revision 1.20  2004/07/19 14:58:47  dondosha
 * Renamed multiseq_src to seqsrc_multiseq, seqdb_src to seqsrc_seqdb
 *
 * Revision 1.19  2004/07/19 13:54:31  dondosha
 * Removed GetSeqLoc method
 *
 * Revision 1.18  2004/06/02 20:43:30  ucko
 * +<memory> (previously included indirectly via sequence.hpp) due to
 * use of auto_ptr<>.
 *
 * Revision 1.17  2004/06/02 15:57:57  bealer
 * - Isolate object manager dependant code.
 *
 * Revision 1.16  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.15  2004/04/28 20:09:43  dondosha
 * Commented out unused argument name
 *
 * Revision 1.14  2004/04/28 19:38:20  dondosha
 * Added implementation of BlastSeqSrcReleaseSequence function
 *
 * Revision 1.13  2004/03/26 19:18:40  dondosha
 * Minor correction in assigning sequence pointer in returned sequence block
 *
 * Revision 1.12  2004/03/24 22:12:46  dondosha
 * Fixed memory leaks
 *
 * Revision 1.11  2004/03/24 19:14:48  dondosha
 * Fixed memory leaks
 *
 * Revision 1.10  2004/03/23 21:48:34  camacho
 * Avoid memory leak in exceptional conditions
 *
 * Revision 1.9  2004/03/23 18:25:33  dondosha
 * Use auto_ptr and operator new to make sure SMultiSeqSrcNewArgs structure is deleted when no longer needed
 *
 * Revision 1.8  2004/03/23 14:13:52  camacho
 * Moved doxygen comment to header
 *
 * Revision 1.7  2004/03/22 16:15:21  camacho
 * doxygen fix
 *
 * Revision 1.6  2004/03/19 19:22:55  camacho
 * Move to doxygen group AlgoBlast, add missing CVS logs at EOF
 *
 *
 * ===========================================================================
 */
