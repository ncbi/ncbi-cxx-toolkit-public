#ifndef OBJTOOLS_DATA_LOADERS_BLASTDB___REMOTE_BLASTDB_ADAPTER__HPP
#define OBJTOOLS_DATA_LOADERS_BLASTDB___REMOTE_BLASTDB_ADAPTER__HPP

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
 *  ===========================================================================
 *
 *  Author: Christiam Camacho
 *
 * ===========================================================================
 */

/** @file remote_blastdb_adapter.hpp
  * Declaration of the CRemoteBlastDbAdapter class.
  */

#include <objtools/data_loaders/blastdb/blastdb_adapter.hpp>
#include <cmath>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// This class defines a "bundle" of elements to cache which are then returned
/// by CRemoteBlastDbAdapter. The actual data for this object comes from the 
/// remote BLAST databases accessed by the blast4 server.
class CCachedSeqDataForRemote : public CObject {
public:
	/// Default constructor, needed to insert objects in std::map
    CCachedSeqDataForRemote() : m_Length(0), m_UseFixedSizeSlices(0) {}

	/// Sets the length of the sequence data for a given Bioseq
    void SetLength(TSeqPos length, bool use_fixed_size_slices) {
        _ASSERT(m_UseFixedSizeSlices == 0);
        m_UseFixedSizeSlices = use_fixed_size_slices;
        _ASSERT(m_SeqDataVector.size() == 0);
        m_Length = length;
        m_SeqDataVector.resize(x_CalculateNumberOfSlices());
        _ASSERT(m_SeqDataVector.size() != 0);
    }

	/// Retrieve the sequence length
    TSeqPos GetLength() const { return m_Length; }

	/// Sets the Seq-id's associated with a given sequence
	/// param idlist IDs to assign to this object [in]
    void SetIdList(const IBlastDbAdapter::TSeqIdList& idlist) {
        m_IdList.clear();
        copy(idlist.begin(), idlist.end(), back_inserter(m_IdList));
    }

	/// Retrieve the Seq-id's associated with a given sequence
    IBlastDbAdapter::TSeqIdList GetIdList() const { return m_IdList; }

	/// Set the Bioseq associated with a given sequence
	/// @param bioseq Bioseq to assign to this object [in]
    void SetBioseq(CRef<CBioseq> bioseq) {
        m_Bioseq = bioseq;
    }

	/// Retrieve the Bioseq associated with a given sequence
    CRef<CBioseq> GetBioseq() const { return m_Bioseq; }

	/// Returns true if this object has been properly initialized and it's
    /// ready to be used
    bool IsValid() {
        return m_Bioseq.NotEmpty() && GetLength() != 0 && !m_IdList.empty();
    }

	/// Returns true if the requested range has sequence data already
	/// @param begin starting offset in the sequence [in]
	/// @param end ending offset in the sequence [in]
    bool HasSequenceData(int begin, int end) {
        return GetSeqDataChunk(begin, end).NotEmpty();
    }

	/// Access the sequence data chunk for a given starting and ending offset
 	/// @param begin starting offset in sequence of interest [in]
 	/// @param end ending offset in sequence of interest [in]
    CRef<CSeq_data>& GetSeqDataChunk(int begin, int end) {
        _ASSERT(m_Length);
        _ASSERT(m_SeqDataVector.size());
        _ASSERT((begin % kRmtSequenceSliceSize) == 0);

        TSeqPos idx = 0;
        if (m_UseFixedSizeSlices) {
            idx = begin / kRmtSequenceSliceSize;
            _ASSERT((end == (begin + (int)kRmtSequenceSliceSize)) || 
                    (idx+1 == m_SeqDataVector.size()));
        } else {
            if (((end-begin) % kRmtSequenceSliceSize) == 0) {
                idx = ilog2( (end-begin)/kRmtSequenceSliceSize );
            } else {
                idx = m_SeqDataVector.size() - 1;
            }
            _ASSERT((end == (begin + (int)(0x1<<idx)*kRmtSequenceSliceSize)) || 
                    ((idx+1) == m_SeqDataVector.size()));
        }
        _ASSERT(m_SeqDataVector.size() > idx);
        CRef<CSeq_data> & retval = m_SeqDataVector[idx];
        return retval;
    }

private:
	/// length of the sequence data
    TSeqPos m_Length;
	/// each element in this vector represents a "chunk" of the sequence data
    vector< CRef<CSeq_data> > m_SeqDataVector;
	/// List of Seq-id's associated with this sequence
    IBlastDbAdapter::TSeqIdList m_IdList;
	/// the bioseq object for this object
    CRef<CBioseq> m_Bioseq;
    /// Determines whether sequences should be fetched in fixed size slices or
    /// in incrementally larger sizes.
    bool m_UseFixedSizeSlices;

    /// Calculates the number of slices in the same manner as the
    /// CCachedSequence class in its SplitSeqData method. 
    /// FIXME: these methods should be kept in sync, refactoring is necessary
    TSeqPos x_CalculateNumberOfSlices()
    {
        _ASSERT(m_Length);
        TSeqPos retval = 0;
        if (m_UseFixedSizeSlices) {
            retval = (m_Length + kRmtSequenceSliceSize - 1) /
                kRmtSequenceSliceSize;
        } else {
            TSeqPos slice_size = kRmtSequenceSliceSize;
            for (TSeqPos pos = 0; pos < m_Length; retval++) {
                TSeqPos end = m_Length;
                if ((end - pos) > slice_size) {
                    end = pos + slice_size;
                }
                pos += slice_size;
                slice_size *= kSliceGrowthFactor;
            }
        }
        return retval;
    }

    Int4 ilog2(Int4 x)
    {
        Int4 lg = 0;

        if (x == 0)
            return 0;

        while ((x = x >> 1))
            lg++;

        return lg;
    }

};

/** This class allows retrieval of sequence data from BLAST databases at NCBI.
 */
class CRemoteBlastDbAdapter : public IBlastDbAdapter
{
public:
    /// Constructor
    CRemoteBlastDbAdapter(const string& db_name, CSeqDB::ESeqType db_type,
                          bool use_fixed_size_slices);

	/** @inheritDoc */
    virtual CSeqDB::ESeqType GetSequenceType() { return m_DbType; }
	/** @inheritDoc */
    virtual int GetSeqLength(int oid);
	/** @inheritDoc */
    virtual TSeqIdList GetSeqIDs(int oid);
	/** @inheritDoc */
    virtual CRef<CBioseq> GetBioseqNoData(int oid, TGi target_gi = ZERO_GI, const CSeq_id * target_id = NULL);
	/** @inheritDoc */
    virtual CRef<CSeq_data> GetSequence(int oid, int begin = 0, int end = 0);
    /// Batch-version of GetSequence
    /// @param oids OIDs of the sequences to fetch, must be of same size as
    /// ranges [in]
    /// @param ranges sequence ranges for the OIDs above, must be of same size as
    /// oids. If any of the ranges is TSeqRange::GetEmpty, the whole sequence
    /// will be fetched (assuming no splitting of the sequence occurred),
    /// otherwise the ranges are expected to be spanning a give sequence chunk
    /// @sa x_CalculateNumberOfSlices [in]
    /// @param sequence_data output parameter for the sequence data to fetch
    /// [out]
    void GetSequenceBatch(const vector<int>& oids, 
                          const vector<TSeqRange>& ranges,
                          vector< CRef<CSeq_data> >& sequence_data);
	/** @inheritDoc */
    virtual bool SeqidToOid(const CSeq_id & id, int & oid);
    /// Batch-version of SeqidToOid
    /// @param ids Seq-IDs to fetch [in]
    /// @param oids the OIDs to which the IDs correspond [out]
    bool SeqidToOidBatch(const vector< CRef<CSeq_id> >& ids, 
                         vector<int>& oids);
    
private:
	/// BLAST database name
    string m_DbName;
	/// Sequence type of the BLAST database
    CSeqDB::ESeqType m_DbType;
	/// Internal cache, maps OIDs to CCachedSeqDataForRemote
    map<int, CCachedSeqDataForRemote> m_Cache;
	/// Our local "OID generator"
    int m_NextLocalId;
    /// Determines whether sequences should be fetched in fixed size slices or
    /// in incrementally larger sizes.
    bool m_UseFixedSizeSlices;

    /// This method actually retrieves the sequence data.
	/// @param oid OID for the sequence of interest [in]
    /// @param begin starting offset of the sequence of interest [in]
    /// @param end ending offset of the sequence of interst [in]
    void x_FetchData(int oid, int begin, int end);

    void x_FetchDataByBatch(const vector<int>& oids, 
                            const vector<TSeqRange>& ranges);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___REMOTE_BLASTDB_ADAPTER__HPP */
