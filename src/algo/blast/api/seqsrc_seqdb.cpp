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

/// @file seqsrc_seqdb.cpp
/// Implementation of the BlastSeqSrc interface for a C++ BLAST databases API

#include <ncbi_pch.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <algo/blast/api/seqsrc_seqdb.hpp>
#include <algo/blast/core/blast_util.h>
#include <algo/blast/core/blast_seqsrc_impl.h>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include "blast_setup.hpp"

/** @addtogroup AlgoBlast
 *
 * @{
 */

#ifndef SKIP_DOXYGEN_PROCESSING
USING_NCBI_SCOPE;
USING_SCOPE(objects);
USING_SCOPE(blast);
#endif /* SKIP_DOXYGEN_PROCESSING */

/// Simple container to support SeqSrc-local data.
struct SSeqDB_SeqSrc_Data {
    /// Constructor.
    SSeqDB_SeqSrc_Data()
        : copied(false)
    {
    }
    
    /// Constructor.
    SSeqDB_SeqSrc_Data(CSeqDB * ptr, int id, ESubjectMaskingType type)
        : seqdb((CSeqDBExpert*) ptr), 
          mask_algo_id(id),
          mask_type(type),
          copied(false)
    {
    }
    
    /// Make a copy of this object, sharing the same SeqDB object.
    SSeqDB_SeqSrc_Data * clone()
    {
        return new SSeqDB_SeqSrc_Data(&* seqdb, mask_algo_id, mask_type);
    }
    
    /// Convenience to allow datap->method to use SeqDB methods.
    CSeqDBExpert * operator->()
    {
        _ASSERT(! seqdb.Empty());
        return &*seqdb;
    }
    
    /// Convenience to allow datap->method to use SeqDB methods.
    CSeqDBExpert & operator*()
    {
        _ASSERT(! seqdb.Empty());
        return *seqdb;
    }
    
    /// SeqDB object.
    CRef<CSeqDBExpert> seqdb;
    
    /// Algorithm ID and type for mask data fetching.
    int mask_algo_id;
    ESubjectMaskingType mask_type;
    bool copied;
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /// Ranges of the sequence to include (for masking).
    CSeqDB::TSequenceRanges seq_ranges;
#endif
};

typedef SSeqDB_SeqSrc_Data TSeqDBData;

extern "C" {

#ifdef KAPPA_PRINT_DIAGNOSTICS

static Blast_GiList*
s_SeqDbGetGiList(void* seqdb_handle, void* args)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    
    Int4* oid = (Int4*) args;
    
    if (!datap || !oid)
       return NULL;

    vector<int> gis;
    datap->GetGis(*oid, gis);

    Blast_GiList* retval = Blast_GiListNewEx(gis.size());
    copy(gis.begin(), gis.end(), retval->data);
    retval->num_used = gis.size();

    return retval;
}

#endif /* KAPPA_PRINT_DIAGNOSTICS */

/// Retrieves the length of the longest sequence in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetMaxLength(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetMaxLength();
}

/// Retrieves the length of the shortest sequence in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetMinLength(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetMinLength();
}

/// Setting number of threads in MT mode
/// @param n_threads number of threads [in]
static void
s_SeqDbSetNumberOfThreads(void* seqdb_handle, int n)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    seqdb.SetNumberOfThreads(n);
}

/// Retrieves the number of sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetNumSeqs(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetNumSeqs();
}

/// Retrieves the number of sequences from alias file to be used for
//  search-space calculations.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int4 
s_SeqDbGetNumSeqsStats(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetNumSeqsStats();
}

/// Retrieves the total length of all sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int8 
s_SeqDbGetTotLen(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetTotalLength();
}

/// Retrieves the total length of all sequences from alias file
// to be used for search space calculations.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Int8 
s_SeqDbGetTotLenStats(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetTotalLengthStats();  
}

/// Retrieves the average length of sequences in the BlastSeqSrc.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param ignoreme Unused by this implementation [in]
static Int4 
s_SeqDbGetAvgLength(void* seqdb_handle, void* ignoreme)
{
   Int8 total_length = s_SeqDbGetTotLen(seqdb_handle, ignoreme);
   Int4 num_seqs = MAX(1, s_SeqDbGetNumSeqs(seqdb_handle, ignoreme));

   return (Int4) (total_length/num_seqs);
}

/// Retrieves the name of the BLAST database.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static const char* 
s_SeqDbGetName(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetDBNameList().c_str();
}

/// Checks whether database is protein or nucleotide.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @return TRUE if database is protein, FALSE if nucleotide.
static Boolean 
s_SeqDbGetIsProt(void* seqdb_handle, void*)
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;

    return (seqdb.GetSequenceType() == CSeqDB::eProtein);
}

/// Determine if partial fetching should be enabled
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
static Boolean
s_SeqDbGetSupportsPartialFetching(void* seqdb_handle, void*) 
{
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    
    if (seqdb.GetSequenceType() != CSeqDB::eNucleotide) {
       // don't bother doing this for proteins as the sequences are
       // never long enough to cause performance degredation
       return false;
    }

    // If longest sequence is below this we quit
    static const int kMaxLengthCutoff = 5000;
    if (seqdb.GetMaxLength() < kMaxLengthCutoff) {
       return false;
    }

    // If average length is below this amount we quit
    static const int kAvgLengthCutoff = 2048;
    Int8 total_length = seqdb.GetTotalLength();
    Int4 num_seqs = MAX(1, seqdb.GetNumSeqs());
    if ((Int4)(total_length/num_seqs) < kAvgLengthCutoff) {
       return false;
    }

    return true;
}


/// Set sequence ranges for partial fetching
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to BlastSeqSrcSetRangesArg structure [in]
static void
s_SeqDbSetRanges(void* seqdb_handle, BlastSeqSrcSetRangesArg* args)
{
    if (!seqdb_handle || !args) return;

    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
        
    CSeqDB::TRangeList ranges;
    for (int i=0; i< args->num_ranges; ++i) {
        ranges.insert(pair<int,int> (args->ranges[i*2], args->ranges[i*2+1]));
    }

    seqdb.SetOffsetRanges(args->oid, ranges, false, false);
}

/// Retrieves the sequence meeting the criteria defined by its second argument.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to BlastSeqSrcGetSeqArg structure [in]
/// @return return codes defined in blast_seqsrc.h
static Int2 
s_SeqDbGetSequence(void* seqdb_handle, BlastSeqSrcGetSeqArg* args)
{
    Int4 oid = -1, len = 0;
    Boolean has_sentinel_byte;
    
    if (!seqdb_handle || !args)
        return BLAST_SEQSRC_ERROR;
    
    TSeqDBData * datap = (TSeqDBData *) seqdb_handle;
    
    CSeqDBExpert & seqdb = **datap;
    
    oid = args->oid;

    // If we are asked to check for OID exclusion, and if the database
    // has a GI list, then we check whether all the seqids have been
    // removed by filtering.  If so we return an error.  The traceback
    // code will exclude this HSP list.
    
    if (args->check_oid_exclusion) {
        if (! seqdb.GetIdSet().Blank()) {
            list< CRef<CSeq_id> > seqids = seqdb.GetSeqIDs(oid);
            
            if (seqids.empty()) {
                return BLAST_SEQSRC_EXCLUDED;
            }
        }
    }
    
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    if (datap->mask_type != eNoSubjMasking) { 
        ASSERT(datap->mask_algo_id != -1);
        seqdb.GetMaskData(oid, datap->mask_algo_id, datap->seq_ranges);
    }
#endif

    datap->copied = false;
    
    if ( args->encoding == eBlastEncodingNucleotide 
      || args->encoding == eBlastEncodingNcbi4na 
      || (datap->mask_type == eHardSubjMasking 
              && !(datap->seq_ranges.empty())
              && args->check_oid_exclusion))  datap->copied = true;

    has_sentinel_byte = (args->encoding == eBlastEncodingNucleotide);
    
    /* free buffers if necessary */
    if (args->seq) BlastSequenceBlkClean(args->seq);
    
    /* This occurs if the pre-selected partial sequence in the traceback stage
     * was too small to perform the traceback. Only do this for nucleotide
     * sequences as proteins are not long enough to be of significance */
    if (args->reset_ranges && seqdb.GetSequenceType() == CSeqDB::eNucleotide) {
        seqdb.RemoveOffsetRanges(oid);
    }
    
    const char *buf;
    len = (datap->copied) 
           /* This will consume and clear datap->seq_ranges */
        ?  seqdb.GetAmbigSeqAlloc(oid, 
                                  const_cast<char **>(&buf), 
                                  has_sentinel_byte, 
                                  eMalloc,
                                  ((datap->mask_type == eHardSubjMasking) ?
                                       &(datap->seq_ranges) : NULL))
        :  seqdb.GetSequence(oid, &buf);
    
    if (len <= 0) return BLAST_SEQSRC_ERROR;
    
    BlastSetUp_SeqBlkNew((Uint1*)buf, len, &args->seq, datap->copied);
    
    /* If there is no sentinel byte, and buffer is allocated, i.e. this is
       the traceback stage of a translated search, set "sequence" to the same 
       position as "sequence_start". */
    if (datap->copied && !has_sentinel_byte)
        args->seq->sequence = args->seq->sequence_start;
    
    /* For preliminary stage, even though sequence buffer points to a memory
       mapped location, we still need to call ReleaseSequence. This can only be
       guaranteed by making the engine believe tat sequence is allocated.
    */
    if (!datap->copied) args->seq->sequence_allocated = TRUE;
    
    args->seq->oid = oid;

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    /* If masks have not been consumed (scanning phase), pass on to engine */
    if (datap->mask_type != eNoSubjMasking) {
        if (BlastSeqBlkSetSeqRanges(args->seq, 
                                (SSeqRange*) datap->seq_ranges.get_data(),
                                datap->seq_ranges.size() + 1, false, datap->mask_type) != 0) {
            return BLAST_SEQSRC_ERROR;
        }
    }
#endif
    
    return BLAST_SEQSRC_SUCCESS;
}

/// Returns the memory allocated for the sequence buffer to the CSeqDB 
/// interface.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to the BlastSeqSrcGetSeqArgs structure, 
/// containing sequence block with the buffer that needs to be deallocated. [in]
static void
s_SeqDbReleaseSequence(void* seqdb_handle, BlastSeqSrcGetSeqArg* args)
{
    TSeqDBData * datap = (TSeqDBData *) seqdb_handle;
    
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;

    _ASSERT(seqdb_handle);
    _ASSERT(args);
    _ASSERT(args->seq);

    if (args->seq->sequence_start_allocated) {
        ASSERT (datap->copied);
        sfree(args->seq->sequence_start);
        args->seq->sequence_start_allocated = FALSE;
        args->seq->sequence_start = NULL;
    }
    if (args->seq->sequence_allocated) {
        if (datap->copied) sfree(args->seq->sequence);
        else seqdb.RetSequence((const char**)&args->seq->sequence);
        args->seq->sequence_allocated = FALSE;
        args->seq->sequence = NULL;
    }
}

/// Retrieve length of a given database sequence.
/// @param seqdb_handle Pointer to initialized CSeqDB object [in]
/// @param args Pointer to integer indicating ordinal id [in]
/// @return Length of the database sequence or BLAST_SEQSRC_ERROR.
static Int4 
s_SeqDbGetSeqLen(void* seqdb_handle, void* args)
{
    Int4* oid = (Int4*) args;

    if (!seqdb_handle || !oid)
       return BLAST_SEQSRC_ERROR;

    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    return seqdb.GetSeqLength(*oid);
}

/// Assigns next chunk of the database to the sequence source iterator.
/// @param seqdb_handle Reference to the database object, cast to void* to 
///                     satisfy the signature requirement. [in]
/// @param itr Iterator over the database sequence source. [in|out]
static Int2 
s_SeqDbGetNextChunk(void* seqdb_handle, BlastSeqSrcIterator* itr)
{
    if (!seqdb_handle || !itr)
        return BLAST_SEQSRC_ERROR;
    
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    
    vector<int> oid_list;

    CSeqDB::EOidListType chunk_type = 
        seqdb.GetNextOIDChunk(itr->oid_range[0], itr->oid_range[1], 
                              itr->chunk_sz, oid_list);
    
    if (itr->oid_range[1] <= itr->oid_range[0])
        return BLAST_SEQSRC_EOF;

    if (chunk_type == CSeqDB::eOidRange) {
        itr->itr_type = eOidRange;
        itr->current_pos = itr->oid_range[0];
    } else if (chunk_type == CSeqDB::eOidList) {
        Uint4 new_sz = (Uint4) oid_list.size();
        itr->itr_type = eOidList;
        if (new_sz > 0) {
            itr->current_pos = 0;
            Uint4 index;
            if (itr->chunk_sz < new_sz) { 
                sfree(itr->oid_list);
                itr->oid_list = (int *) malloc (new_sz * sizeof(unsigned int));
            }
            itr->chunk_sz = new_sz;
            for (index = 0; index < new_sz; ++index)
                itr->oid_list[index] = oid_list[index];
        } else {
            return s_SeqDbGetNextChunk(seqdb_handle, itr);
        }
    }

    return BLAST_SEQSRC_SUCCESS;
}

/// Finds the next not searched ordinal id in the iteration over BLAST database.
/// @param seqdb_handle Reference to the database object, cast to void* to 
///                     satisfy the signature requirement. [in]
/// @param itr Iterator of the BlastSeqSrc pointed by ptr. [in]
/// @return Next ordinal id.
static Int4 
s_SeqDbIteratorNext(void* seqdb_handle, BlastSeqSrcIterator* itr)
{
    Int4 retval = BLAST_SEQSRC_EOF;
    Int4 status = BLAST_SEQSRC_SUCCESS;

    _ASSERT(seqdb_handle);
    _ASSERT(itr);

    /* If internal iterator is uninitialized/invalid, retrieve the next chunk 
       from the BlastSeqSrc */
    if (itr->current_pos == UINT4_MAX) {
        status = s_SeqDbGetNextChunk(seqdb_handle, itr);
        if (status == BLAST_SEQSRC_ERROR || status == BLAST_SEQSRC_EOF) {
            return status;
        }
    }

    Uint4 last_pos = 0;

    if (itr->itr_type == eOidRange) {
        retval = itr->current_pos;
        last_pos = itr->oid_range[1];
    } else if (itr->itr_type == eOidList) {
        retval = itr->oid_list[itr->current_pos];
        last_pos = itr->chunk_sz;
    } else {
        /* Unsupported/invalid iterator type! */
        fprintf(stderr, "Invalid iterator type: %d\n", itr->itr_type);
        abort();
    }

    ++itr->current_pos;
    if (itr->current_pos >= last_pos) {
        itr->current_pos = UINT4_MAX;  /* invalidate internal iteration */
    }

    return retval;
}

/// Resets CSeqDB's internal chunk bookmark
/// @param seqdb_handle Reference to the database object, cast to void* to 
///                     satisfy the signature requirement. [in]
static void
s_SeqDbResetChunkIterator(void* seqdb_handle)
{
    _ASSERT(seqdb_handle);
    CSeqDB & seqdb = **(TSeqDBData *) seqdb_handle;
    seqdb.ResetInternalChunkBookmark();
    seqdb.FlushOffsetRangeCache();
}

}

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);
BEGIN_SCOPE(blast)

/// Encapsulates the arguments needed to initialize CSeqDB.
class CSeqDbSrcNewArgs {
public:
    /// Constructor
    CSeqDbSrcNewArgs(const string& db, bool is_prot,
                     Uint4 first_oid = 0, Uint4 final_oid = 0,
                     Int4 mask_algo_id = -1, 
                     ESubjectMaskingType mask_type = eNoSubjMasking)
        : m_DbName(db), m_IsProtein(is_prot), 
          m_FirstDbSeq(first_oid), m_FinalDbSeq(final_oid),
          m_MaskAlgoId(mask_algo_id), m_MaskType(mask_type)
    {}

    /// Getter functions for the private fields
    const string GetDbName() const { return m_DbName; }
    /// Returns database type: protein or nucleotide
    char GetDbType() const { return m_IsProtein ? 'p' : 'n'; }
    /// Returns first database ordinal id covered by this BlastSeqSrc
    Uint4 GetFirstOid() const { return m_FirstDbSeq; }
    /// Returns last database ordinal id covered by this BlastSeqSrc
    Uint4 GetFinalOid() const { return m_FinalDbSeq; }
    /// Returns the default filtering algorithm to use with sequence data
    /// extracted from this BlastSeqSrc
    Int4 GetMaskAlgoId() const { return m_MaskAlgoId; }
    ESubjectMaskingType GetMaskType() const { return m_MaskType; }

private:
    string m_DbName;        ///< Database name
    bool m_IsProtein;       ///< Is this database protein?
    Uint4 m_FirstDbSeq;     ///< Ordinal id of the first sequence to search
    Uint4 m_FinalDbSeq;     ///< Ordinal id of the last sequence to search
    /// filtering algorithm ID to use when retrieving sequence data
    Int4 m_MaskAlgoId;
    ESubjectMaskingType m_MaskType;
};

extern "C" {

/// SeqDb sequence source destructor: frees its internal data structure
/// @param seq_src BlastSeqSrc structure to free [in]
/// @return NULL
static BlastSeqSrc* 
s_SeqDbSrcFree(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    
    TSeqDBData * datap = static_cast<TSeqDBData*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    
    delete datap;
    return NULL;
}

/// SeqDb sequence source copier: creates a new reference to the CSeqDB object
/// and copies the rest of the BlastSeqSrc structure.
/// @param seq_src BlastSeqSrc structure to copy [in]
/// @return Pointer to the new BlastSeqSrc.
static BlastSeqSrc* 
s_SeqDbSrcCopy(BlastSeqSrc* seq_src)
{
    if (!seq_src) 
        return NULL;
    
    TSeqDBData * datap = static_cast<TSeqDBData*>
        (_BlastSeqSrcImpl_GetDataStructure(seq_src));
    
    _BlastSeqSrcImpl_SetDataStructure(seq_src, (void*) datap->clone());
    
    return seq_src;
}

/// Initializes the data structure and function pointers in a SeqDb based 
/// BlastSeqSrc.
/// @param retval Structure to populate [in] [out]
/// @param seqdb Reference to a CSeqDB object [in]
static void 
s_InitNewSeqDbSrc(BlastSeqSrc* retval, TSeqDBData * datap)
{
    _ASSERT(retval);
    _ASSERT(datap);
    
    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seqdb */
    _BlastSeqSrcImpl_SetDeleteFnPtr   (retval, & s_SeqDbSrcFree);
    _BlastSeqSrcImpl_SetCopyFnPtr     (retval, & s_SeqDbSrcCopy);
    _BlastSeqSrcImpl_SetDataStructure (retval, (void*) datap);
    _BlastSeqSrcImpl_SetGetNumSeqs    (retval, & s_SeqDbGetNumSeqs);
    _BlastSeqSrcImpl_SetGetNumSeqsStats(retval, & s_SeqDbGetNumSeqsStats);
    _BlastSeqSrcImpl_SetGetMaxSeqLen  (retval, & s_SeqDbGetMaxLength);
    _BlastSeqSrcImpl_SetGetMinSeqLen  (retval, & s_SeqDbGetMinLength);
    _BlastSeqSrcImpl_SetGetAvgSeqLen  (retval, & s_SeqDbGetAvgLength);
    _BlastSeqSrcImpl_SetGetTotLen     (retval, & s_SeqDbGetTotLen);
    _BlastSeqSrcImpl_SetGetTotLenStats(retval, & s_SeqDbGetTotLenStats);
    _BlastSeqSrcImpl_SetGetName       (retval, & s_SeqDbGetName);
    _BlastSeqSrcImpl_SetGetIsProt     (retval, & s_SeqDbGetIsProt);
    _BlastSeqSrcImpl_SetGetSupportsPartialFetching (retval, & s_SeqDbGetSupportsPartialFetching);
    _BlastSeqSrcImpl_SetSetSeqRange   (retval, & s_SeqDbSetRanges);
    _BlastSeqSrcImpl_SetGetSequence   (retval, & s_SeqDbGetSequence);
    _BlastSeqSrcImpl_SetGetSeqLen     (retval, & s_SeqDbGetSeqLen);
    _BlastSeqSrcImpl_SetIterNext      (retval, & s_SeqDbIteratorNext);
    _BlastSeqSrcImpl_SetResetChunkIterator(retval, & s_SeqDbResetChunkIterator);
    _BlastSeqSrcImpl_SetReleaseSequence   (retval, & s_SeqDbReleaseSequence);
    _BlastSeqSrcImpl_SetSetNumberOfThreads    (retval, & s_SeqDbSetNumberOfThreads);
#ifdef KAPPA_PRINT_DIAGNOSTICS
    _BlastSeqSrcImpl_SetGetGis        (retval, & s_SeqDbGetGiList);
#endif /* KAPPA_PRINT_DIAGNOSTICS */
}

/// Populates a BlastSeqSrc, creating a new reference to the already existing 
/// SeqDb object.
/// @param retval Original BlastSeqSrc [in]
/// @param args Pointer to a reference to CSeqDB object [in]
/// @return retval
static BlastSeqSrc* 
s_SeqDbSrcSharedNew(BlastSeqSrc* retval, void* args)
{
    _ASSERT(retval);
    _ASSERT(args);
    
    TSeqDBData * datap = (TSeqDBData *) args;
    
    s_InitNewSeqDbSrc(retval, datap->clone());
    
    return retval;
}

/// SeqDb sequence source constructor 
/// @param retval BlastSeqSrc structure (already allocated) to populate [in]
/// @param args Pointer to internal CSeqDbSrcNewArgs structure (@sa
/// CSeqDbSrcNewArgs) [in]
/// @return Updated seq_src structure (with all function pointers initialized
static BlastSeqSrc* 
s_SeqDbSrcNew(BlastSeqSrc* retval, void* args)
{
    _ASSERT(retval);
    _ASSERT(args);
    
    CSeqDbSrcNewArgs* seqdb_args = (CSeqDbSrcNewArgs*) args;
    _ASSERT(seqdb_args);
    
    TSeqDBData * datap = new TSeqDBData;
    
    try {
        bool is_protein = (seqdb_args->GetDbType() == 'p');
        
        datap->seqdb.Reset(new CSeqDBExpert(seqdb_args->GetDbName(),
                                            (is_protein
                                             ? CSeqDB::eProtein
                                             : CSeqDB::eNucleotide)));
        
        datap->seqdb->SetIterationRange(seqdb_args->GetFirstOid(),
                                        seqdb_args->GetFinalOid());
        
        datap->mask_algo_id = seqdb_args->GetMaskAlgoId();
        datap->mask_type = seqdb_args->GetMaskType();

        // Validate that the masking algorithm is supported
        if (datap->mask_algo_id > 0) {
            vector<int> supported_algorithms;
            datap->seqdb->GetAvailableMaskAlgorithms(supported_algorithms);
            if (find(supported_algorithms.begin(),
                     supported_algorithms.end(),
                     datap->mask_algo_id) == supported_algorithms.end()) {
                CNcbiOstrstream oss;
                oss << "Masking algorithm ID " << datap->mask_algo_id << " is "
                    << "not supported in " << 
                    (is_protein ? "protein" : "nucleotide") << " '" 
                    << seqdb_args->GetDbName() << "' BLAST database";
                string msg = CNcbiOstrstreamToString(oss);
                throw runtime_error(msg);
            }
        }

    } catch (const ncbi::CException& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
                        strdup(e.ReportThis(eDPF_ErrCodeExplanation).c_str()));
    } catch (const std::exception& e) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, strdup(e.what()));
    } catch (...) {
        _BlastSeqSrcImpl_SetInitErrorStr(retval, 
             strdup("Caught unknown exception from CSeqDB constructor"));
    }
    
    /* Initialize the BlastSeqSrc structure fields with user-defined function
     * pointers and seqdb */
    
    s_InitNewSeqDbSrc(retval, datap);
    
    return retval;
}

}

BlastSeqSrc* 
SeqDbBlastSeqSrcInit(const string& dbname, bool is_prot, 
                 Uint4 first_seq, Uint4 last_seq,
                 Int4 mask_algo_id, ESubjectMaskingType mask_type)
{
    BlastSeqSrcNewInfo bssn_info;
    BlastSeqSrc* seq_src = NULL;
    CSeqDbSrcNewArgs seqdb_args(dbname, is_prot, first_seq, last_seq,
                                mask_algo_id, mask_type);

    bssn_info.constructor = &s_SeqDbSrcNew; // FIXME: shouldn't this be s_SeqDbSrcSharedNew?
    bssn_info.ctor_argument = (void*) &seqdb_args;
    seq_src = BlastSeqSrcNew(&bssn_info);
    return seq_src;
}

BlastSeqSrc* 
SeqDbBlastSeqSrcInit(CSeqDB * seqdb,
                     Int4 mask_algo_id,
                     ESubjectMaskingType mask_type)
{
    BlastSeqSrcNewInfo bssn_info;
    BlastSeqSrc * seq_src = NULL;

    TSeqDBData data(seqdb, mask_algo_id, mask_type);

    bssn_info.constructor = & s_SeqDbSrcSharedNew;
    bssn_info.ctor_argument = (void*) & data;
    seq_src = BlastSeqSrcNew(& bssn_info);
    return seq_src;
}


END_SCOPE(blast)
END_NCBI_SCOPE

/* @} */
