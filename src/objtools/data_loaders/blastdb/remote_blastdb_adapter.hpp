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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

/// This class defines a "bundle" of elements to cache which are then returned
/// by CRemoteBlastDbAdapter. The actual data for this object comes from the 
/// remote BLAST databases accessed by the blast4 server.
class CCachedSeqDataForRemote : public CObject {
public:
	/// Default constructor, needed to insert objects in std::map
    CCachedSeqDataForRemote() : m_Length(0) {}

	/// Sets the length of the sequence data for a given Bioseq
    void SetLength(TSeqPos length) {
        _ASSERT(m_SeqDataVector.size() == 0);
        m_Length = length;
        const TSeqPos num_slices = (m_Length + kRmtSequenceSliceSize - 1) /
            kRmtSequenceSliceSize;
        m_SeqDataVector.resize(num_slices);
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

        const TSeqPos i = begin / kRmtSequenceSliceSize;

        _ASSERT((begin % kRmtSequenceSliceSize) == 0);
        _ASSERT(end == (begin + (int)kRmtSequenceSliceSize) || 
                (i+1 == m_SeqDataVector.size()));
        _ASSERT(m_SeqDataVector.size() > i);
        
        CRef<CSeq_data> & retval = m_SeqDataVector[i];
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
};

/** This class allows retrieval of sequence data from BLAST databases at NCBI.
 */
class NCBI_XLOADER_BLASTDB_EXPORT CRemoteBlastDbAdapter : public IBlastDbAdapter
{
public:
    /// Constructor
    CRemoteBlastDbAdapter(const string& db_name, CSeqDB::ESeqType db_type);

	/** @inheritDoc */
    virtual CSeqDB::ESeqType GetSequenceType() { return m_DbType; }
	/** @inheritDoc */
    virtual int GetSeqLength(int oid);
	/** @inheritDoc */
    virtual TSeqIdList GetSeqIDs(int oid);
	/** @inheritDoc */
    virtual CRef<CBioseq> GetBioseqNoData(int oid, int target_gi = 0);
	/** @inheritDoc */
    virtual CRef<CSeq_data> GetSequence(int oid, int begin = 0, int end = 0);
	/** @inheritDoc */
    virtual bool SeqidToOid(const CSeq_id & id, int & oid);
    
private:
	/// BLAST database name
    string m_DbName;
	/// Sequence type of the BLAST database
    CSeqDB::ESeqType m_DbType;
	/// Internal cache, maps OIDs to CCachedSeqDataForRemote
    map<int, CCachedSeqDataForRemote> m_Cache;
	/// Our local "OID generator"
    int m_NextLocalId;

    /// This method actually retrieves the sequence data.
	/// @param oid OID for the sequence of interest [in]
    /// @param begin starting offset of the sequence of interest [in]
    /// @param end ending offset of the sequence of interst [in]
    void x_FetchData(int oid, int begin, int end);
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif /* OBJTOOLS_DATA_LOADERS_BLASTDB___REMOTE_BLASTDB_ADAPTER__HPP */
