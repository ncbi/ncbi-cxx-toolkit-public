#ifndef REMOTEDB_HPP
#define REMOTEDB_HPP

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
*  Author: Kevin Bealer
*
*  File Description:
*   SeqDB-like object that fetches sequences remotely.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <objtools/blast/seqdb_reader/seqdb.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seqset/Seq_entry.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CRemoteDb_DataMap {
public:
    CRemoteDb_DataMap()
        : m_SliceSize(0), m_Length(0)
    {
    }
    
    void SetLength(int slice_size, int seq_length)
    {
        _ASSERT(m_Data.size() == 0);
        
        m_Length = seq_length;
        m_SliceSize = slice_size;
        
        int slices = (seq_length + slice_size - 1) / slice_size;
        
        m_Data.resize(slices);
    }
    
    CRef<CSeq_data> & SetSlice(int begin, int end)
    {
        _ASSERT(m_SliceSize);
        _ASSERT(m_Length);
        _ASSERT(m_Data.size());
        
        size_t i = begin / m_SliceSize;
        
        _ASSERT((begin % m_SliceSize) == 0);
        _ASSERT(end == (begin + m_SliceSize) || (i+1 == m_Data.size()));
        _ASSERT(m_Data.size() > i);
        
        CRef<CSeq_data> & data = m_Data[i];
        
        return data;
    }
    
    CRef<CSeq_data> GetSlice(int begin, int end)
    {
        CRef<CSeq_data> data = SetSlice(begin, end);
        return data;
    }
    
private:
    int m_SliceSize;
    int m_Length;
    vector< CRef<CSeq_data> > m_Data;
};

struct SRemoteDb_SeqData {
    SRemoteDb_SeqData()
        : oid(0), length(0), found(false)
    {
    }
    
    CRef<CSeq_id>         id;
    CRef<CBioseq>         bioseq;
    CRemoteDb_DataMap     seqdata;
    int                   oid;
    int                   length;
    bool                  found;
    list< CRef<CSeq_id> > idlist;
};
    
/// Wrapper for RemoteBlast and database-relevant information.
///
/// This class provides a simple interface to the functionality in
/// CRemoteBlast that allows for fetching sequence data.  The design
/// of the API borrows from the CSeqDB interface.

class CRemoteDb : public CObject {
public:
    /// Construct a remote database access object.
    /// @param dbname Name of the database to access.
    /// @param seqtype Type of sequences to get.
    CRemoteDb(string dbname, CSeqDB::ESeqType seqtype);
    
    /// Get the sequence type used by this object.
    ///
    /// Unlike SeqDB, this object will not resolve the sequence type
    /// to protein or nucleotide if auto is specified.
    ///
    /// @return The sequence type.
    CSeqDB::ESeqType GetSequenceType();
    
    /// Get the length of the sequence.
    /// @param oid An ID for this sequence in this db.
    /// @return Sequence length (in bases).
    int GetSeqLength(int oid);
    
    /// Get a CBioseq for the sequence.
    ///
    /// If target is specified, that defline will be promoted to the
    /// top of the CBioseq object.  (NOTE: The current design does not
    /// implement this promotion; the blast4 service will promote
    /// whichever Seq-id was used to fetch the OID, which in practice
    /// should be the same one.)
    ///
    /// @param oid An ID for this sequence in this db.
    /// @param target The Seq-id to select a defline to target.
    CRef<CBioseq> GetBioseqNoData(int oid, const CSeq_id & target);
    
    /// Get all or part of the sequence data as a Seq-data object.
    /// @param oid    Identifies which sequence to get.
    /// @param begin  Starting offset of the section to get.
    /// @param end    Ending offset of the section to get.
    /// @return       The sequence data.
    CRef<CSeq_data> GetSequence(int oid, int begin, int end);
    
    /// Find a Seq-id in the database and get an OID if found.
    ///
    /// If the Seq-id is found, this method returns true; an OID will
    /// be generated that identifies this sequence, for methods like
    /// GetBioseq.
    ///
    /// @param id The Seq-id to find.
    /// @param oid An ID for this sequence (if it was found).
    /// @param slice_size The size of chunk of sequence data to fetch.
    /// @return True if the sequence was found in the remote database.
    bool SeqidToOid(const CSeq_id & id, int & oid, int slice_size);
    
    /// Get the list of Seq-ids for the given OID.
    list< CRef<CSeq_id> > GetSeqIDs(int oid);
    
private:
    typedef SRemoteDb_SeqData TSeqData;
    
    CRef<CSeq_data> x_FetchData(TSeqData & sdata,
                                int        begin,
                                int        end);
    
    // This design (the map<T> containers) might not be the most
    // efficient approach.  For this class, remote fetch operations
    // probably dominate the time expenditures.
    
    int                m_NextOid;
    map<string, int>   m_IdToOid; ///< Map 'fasta string' to oid.
    map<int, TSeqData> m_SeqData; ///< Map OID to sequence data.
    string             m_Dbname;
    CSeqDB::ESeqType   m_Seqtype;
};

END_SCOPE(objects)
END_NCBI_SCOPE

#endif
