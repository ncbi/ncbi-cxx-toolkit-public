#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___CACHED_SEQUENCE__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___CACHED_SEQUENCE__HPP

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
*  Author: Christiam Camacho
*
* ===========================================================================
*/
/** @file cached_sequence.hpp
 * Defines the CCachedSequence class
 */

#include <objtools/data_loaders/blastdb/bdbloader.hpp>	// for TIds

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_literal;

/// Manages a TSE and its subordinate chunks for all implementations of
/// the IBlastDbAdapter interface
class CCachedSequence : public CObject {
public:
    /// Construct and pre-process the sequence.
    ///
    /// This constructor starts the processing of the specified
    /// sequence. It loads the bioseq (minus sequence data) and
    /// caches the sequence length.
    ///
    /// @param idh
    ///   A handle to the sequence identifier.
    /// @param blastdb
    ///   The interface to the BLAST database containing the original sequence.
    /// @param oid
    ///   Locates the sequence within the BLAST database.
	/// @param use_fixed_size_slices
	///   Determines the strategy to retrieve the sequence data that is split, 
    ///   see comment for this class' m_UseFixedSizeSlices field.
    /// @param slice_size
    ///   Determines the slice size for splitting the sequence data.
    CCachedSequence(IBlastDbAdapter& blastdb, const CSeq_id_Handle& idh, 
                    int oid, bool use_fixed_size_slices,
                    TSeqPos slice_size = kSequenceSliceSize);
    
    /// Add this sequence's identifiers to a lookup table held by the data
    /// loader.
    ///
    /// This method adds identifiers from this sequence to a
    /// lookup table so that the OID of the sequence can be found
    /// quickly during future processing of the same sequence.
    ///
    /// @param idmap
    ///   A map from CSeq_id_Handle to OID.
    void RegisterIds(CBlastDbDataLoader::TIdMap & idmap);
    
    /// A list of 'chunk' objects, generic sequence related data elements.
    typedef vector< CRef<CTSE_Chunk_Info> > TCTSE_Chunk_InfoVector;
    
    /// Load or split the sequence data chunks.
    ///
    /// The sequence data is stored and a list of the available ranges
    /// of data is returned via the 'chunks' parameter. For large sequences,
    /// these do not contain sequence data, but merely indicate what is
    /// available, in other cases, the entire sequence data will be loaded
    ///
    /// @param chunks
    ///   The sequence data chunks will be returned here.
    void SplitSeqData(TCTSE_Chunk_InfoVector& chunks);
    
    /// Get the top-level seq-entry managed by this object
    CRef<CSeq_entry> GetTSE() const {
        return m_TSE;
    }
    
private:
    /// SeqID handle
    CSeq_id_Handle m_SIH;
    
    /// The Seq entry we handle
    CRef<CSeq_entry> m_TSE;
    
    /// Sequence length in bases
    TSeqPos m_Length;

    /// Database reference
    IBlastDbAdapter& m_BlastDb;
    
    /// Locates this sequence within m_BlastDb.
    int m_OID;

    /// Determines whether sequences should be fetched in fixed size slices or
    /// in incrementally larger sizes. The latter improves performance on
    /// full sequence retrieval of large sequences, but degrades the
    /// performance of retrieval of small sequence segments of large sequences
    bool m_UseFixedSizeSlices;

    /// Specifies the slice size for splitting the sequence data
    TSeqPos m_SliceSize;

    /// Add a chunk of sequence data
    ///
    /// This method builds a description of a specific range of
    /// sequence data, returning it via the 'chunks' parameter.  The
    /// actual data is not built, just a description that identifies
    /// the sequence and the range of that sequence's data represented
    /// by this chunk.
    /// @param chunks object where to add the chunk [in|out]
    /// @param id identifier for the sequence chunk about to be added [in]
    /// @param begin starting offset of the chunk [in]
    /// @param end ending offset of the chunk [in]
    void x_AddSplitSeqChunk(TCTSE_Chunk_InfoVector& chunks,
                            const CSeq_id_Handle& id,
                            TSeqPos               begin,
                            TSeqPos               end);
    
    /// Add an the entire sequence data to this object's TSE as raw data in 
    /// its Seq-data field
    void x_AddFullSeq_data(void);
};

/// Creates a chunk that corresponds to a given OID and its beginning and
/// ending offsets
/// @param blastdb BLAST database interface [in]
/// @param oid OID for the sequence of interest in the blastdb [in]
/// @param begin starting offset of the sequence of interest [in]
/// @param end ending offset of the sequence of interest [in]
CRef<CSeq_literal>
CreateSeqDataChunk(IBlastDbAdapter& blastdb, int oid, TSeqPos begin, TSeqPos end);

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___CACHED_SEQUENCE__HPP */
