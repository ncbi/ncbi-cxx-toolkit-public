#ifndef ASN_CACHE_APP_MERGE_SUB_INDICES__HPP__
#define ASN_CACHE_APP_MERGE_SUB_INDICES__HPP__


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
 * Authors:  Cheinan Marks
 *
 * File Description:
 * Merge a set of CAsnIndex indices whose paths are passed in the constructor.
 *
 */

#include <string>
#include <vector>
#include <queue>
#include <utility>
#include <functional>

#include <corelib/ncbistd.hpp>
#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE

class CAsnIndex;

class CMergeSubcachesApplication;

//  Functor to merge subindices.  Construct it with a list of subindices
//  and a master index, then execute operator() to perform the merge.
class CMergeSubIndices
{
private:
    /// Structure for holding the index data in memory. Combines the index data for
    /// the main index and the SeqId index.
    ///
    /// The SeqId is represented as a C rather than a C++ string. This makes it
    /// possible in merge_sub_caches to keep all SeqIds concatenated in a buffer,
    /// greatly improving performance. If used in other contexts, this could cause
    /// a memory leak; to avoid that danger, we made this an internal class so it can't
    /// be used elsewhere.
    struct SIndexLocator {
        const char *            m_SeqId;
        CAsnIndex::TVersion     m_Version;
        CAsnIndex::TGi          m_GI;
        CAsnIndex::TTimestamp   m_Timestamp;
        CAsnIndex::TChunkId     m_ChunkId;
        CAsnIndex::TOffset      m_Offset;
        CAsnIndex::TSize        m_ChunkSize;
        CAsnIndex::TOffset      m_SeqIdOffset;
        CAsnIndex::TSize        m_SeqIdChunkSize;
        CAsnIndex::TSeqLength   m_SeqLength;
        CAsnIndex::TTaxId       m_TaxId;
    
        static const int        k_MaxSeqIdLength = 500;
    
        /// A SIndexLocator object may have data for the main index only or for the
        /// SeqId index as well.  The default value 0 for m_SeqIdChunkSize
        /// indicates there's no SeqId index data
        SIndexLocator() : m_SeqIdChunkSize(0) {}
    
        bool HasSeqIdIndexData() const { return m_SeqIdChunkSize > 0; }
    
        /// Check whether index is pointing to the correct index location,
        /// represented by this SIndexLocator object
        bool CorrectIndexLocation(const CAsnIndex &index) const {
            return m_SeqId == index.GetSeqId() && m_Version == index.GetVersion() &&
                m_GI == index.GetGi() && m_Timestamp == index.GetTimestamp();
        }
    
        string AsString () const
        {
            return
                string(m_SeqId) + " | " +
                NStr::IntToString(m_Version) + " | " +
                NStr::UInt8ToString(m_GI) + " | " +
                NStr::UInt8ToString(m_Timestamp);
        }
    
        CNcbiOstream & WriteTo(CNcbiOstream &os) const {
            short id_length = strlen(m_SeqId);
            os.write(reinterpret_cast<char *>(&id_length), sizeof(id_length)); 
            os.write(m_SeqId, id_length);
            os.write(reinterpret_cast<const char *>(&m_Version), sizeof(m_Version)); 
            os.write(reinterpret_cast<const char *>(&m_GI), sizeof(m_GI)); 
            os.write(reinterpret_cast<const char *>(&m_Timestamp), sizeof(m_Timestamp)); 
            os.write(reinterpret_cast<const char *>(&m_ChunkId), sizeof(m_ChunkId)); 
            os.write(reinterpret_cast<const char *>(&m_Offset), sizeof(m_Offset)); 
            os.write(reinterpret_cast<const char *>(&m_ChunkSize), sizeof(m_ChunkSize)); 
            os.write(reinterpret_cast<const char *>(&m_SeqIdOffset), sizeof(m_SeqIdOffset)); 
            os.write(reinterpret_cast<const char *>(&m_SeqIdChunkSize), sizeof(m_SeqIdChunkSize)); 
            os.write(reinterpret_cast<const char *>(&m_SeqLength), sizeof(m_SeqLength)); 
            os.write(reinterpret_cast<const char *>(&m_TaxId), sizeof(m_TaxId)); 
            return os;
        }
    
        CNcbiIstream & ReadFrom(CNcbiIstream &is) {
            short id_length;
            is.read(reinterpret_cast<char *>(&id_length), sizeof(id_length)); 
            is.read(const_cast<char *>(m_SeqId), id_length);
            const_cast<char &>(m_SeqId[id_length]) = '\0';
            is.read(reinterpret_cast<char *>(&m_Version), sizeof(m_Version)); 
            is.read(reinterpret_cast<char *>(&m_GI), sizeof(m_GI)); 
            is.read(reinterpret_cast<char *>(&m_Timestamp), sizeof(m_Timestamp)); 
            is.read(reinterpret_cast<char *>(&m_ChunkId), sizeof(m_ChunkId)); 
            is.read(reinterpret_cast<char *>(&m_Offset), sizeof(m_Offset)); 
            is.read(reinterpret_cast<char *>(&m_ChunkSize), sizeof(m_ChunkSize)); 
            is.read(reinterpret_cast<char *>(&m_SeqIdOffset), sizeof(m_SeqIdOffset)); 
            is.read(reinterpret_cast<char *>(&m_SeqIdChunkSize), sizeof(m_SeqIdChunkSize)); 
            is.read(reinterpret_cast<char *>(&m_SeqLength), sizeof(m_SeqLength)); 
            is.read(reinterpret_cast<char *>(&m_TaxId), sizeof(m_TaxId)); 
            return is;
        }
    
        bool    operator< ( const SIndexLocator & other )
        {
            int SeqId_order = strcmp(m_SeqId, other.m_SeqId);
            if (SeqId_order < 0) { return true; }
            if (SeqId_order > 0) { return false; }
    
            if (m_Version < other.m_Version) { return true; }
            if (other.m_Version < m_Version) { return false; }
    
            if (m_GI < other.m_GI) { return true; }
            if (other.m_GI < m_GI) { return false; }
    
            return (m_Timestamp < other.m_Timestamp);
        }
    };
    
    
    struct SQueueEntry
    {
        size_t              m_IndexHandle;
        SIndexLocator       m_IndexEntry;

        //  This function must match the BDB indexing criteria.
        inline bool operator> ( const SQueueEntry & rhs ) const
        {
            if ( strcmp(m_IndexEntry.m_SeqId, rhs.m_IndexEntry.m_SeqId) > 0 ) return true;
            if ( strcmp(m_IndexEntry.m_SeqId, rhs.m_IndexEntry.m_SeqId) < 0 ) return false;
            if ( m_IndexEntry.m_Version > rhs.m_IndexEntry.m_Version ) return true;
            if ( m_IndexEntry.m_Version < rhs.m_IndexEntry.m_Version ) return false;
            if ( m_IndexEntry.m_GI > rhs.m_IndexEntry.m_GI ) return true;
            if ( m_IndexEntry.m_GI < rhs.m_IndexEntry.m_GI ) return false;
            return m_IndexEntry.m_Timestamp > rhs.m_IndexEntry.m_Timestamp;
        }
    };

    typedef std::priority_queue<SQueueEntry, std::vector<SQueueEntry>, std::greater<SQueueEntry> >
                TIndexEntryQ;

    CMergeSubIndices (  const std::string & main_index_path,
                        int num_intermediate_files,
                        bool have_seq_id_indices,
                        const char * seq_id_blob )
        : m_MainIndex(CAsnIndex::e_main),
          m_SeqIdIndex(CAsnIndex::e_seq_id),
          m_SubIndices(new auto_ptr<CNcbiIstream>[num_intermediate_files]),
          m_SeqIdBlob(seq_id_blob)
    {
        m_MainIndex.SetCacheSize ( 3 * 1024 *1024UL );
        m_MainIndex.SetPageSize ( 64 * 1024 );
        m_MainIndex.Open( NASNCacheFileName::GetBDBIndex(main_index_path, CAsnIndex::e_main),
                                                        CBDB_RawFile::eReadWriteCreate );
    
        /// Create a SeqId index for the master index if we found such an index in at least
        /// one of the sub-caches
        if(have_seq_id_indices){
            m_SeqIdIndex.SetCacheSize ( 3 * 1024 *1024UL );
            m_SeqIdIndex.SetPageSize ( 64 * 1024 );
            m_SeqIdIndex.Open( NASNCacheFileName::GetBDBIndex(main_index_path, CAsnIndex::e_seq_id),
                                                              CBDB_RawFile::eReadWriteCreate );
        }
        m_Stopwatch.Start();

        for(int i=1; i <= num_intermediate_files; i++)
            m_SubIndexList.push_back(main_index_path + "/" +
                    NASNCacheFileName::GetIntermediateFilePrefix() + NStr::IntToString(i));
    }

    ~CMergeSubIndices()
    {
        m_Stopwatch.Stop();
        LOG_POST(Info << "Merged " << m_SubIndexList.size() << " subindices in " <<
                    m_Stopwatch.Elapsed() << " seconds." );
        delete[] m_SubIndices;
    }

    std::string    operator() ();

    void           WriteIndexEntry(const SIndexLocator &entry);

    void    x_OpenSubIndices();
    void    x_LoadFirstEntries();
    void    x_Merge();

    CAsnIndex                   m_MainIndex;
    CAsnIndex                   m_SeqIdIndex;
    std::vector<std::string>    m_SubIndexList;
    auto_ptr<CNcbiIstream>      *m_SubIndices;
    const char *                m_SeqIdBlob;
    TIndexEntryQ                m_IndexEntryQueue;
    CStopWatch  m_Stopwatch;
    CStopWatch  m_QueueStopwatch;
    CStopWatch  m_BDBStopwatch;

    friend class CMergeSubcachesApplication;
    friend std::ostream &    operator<< ( std::ostream & a_stream, const SIndexLocator & index_locator );
};

inline std::ostream &    operator<< ( std::ostream & a_stream,
                                      const CMergeSubIndices::SIndexLocator & index_locator )
{
    return a_stream << index_locator.m_SeqId << " | "
        << index_locator.m_ChunkId << " | "
        << index_locator.m_Offset << " | "
        << index_locator.m_ChunkSize;
}


END_NCBI_SCOPE

#endif  //  ASN_CACHE_APP_MERGE_SUB_INDICES__HPP__
