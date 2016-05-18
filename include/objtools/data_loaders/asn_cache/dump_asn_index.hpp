#ifndef DUMP_ASN_INDEX_HPP__
#define DUMP_ASN_INDEX_HPP__

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
 *   Produce an ASN.1 cache index from CSeq_entry blobs passed from ID.
 *   This module is built into a library designed to be linked into an
 *   ID team machine with direct access to the databases.
 */

#include <corelib/ncbistd.hpp>

#include <vector>
#include <string>
#include <utility>
#include <ios>

#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>
#include <objtools/data_loaders/asn_cache/temp_stopwatch.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
class CSeq_entry;
END_SCOPE(objects)

const   Uint2   kMajorVersion = 2;
const   Uint2   kMinorVersion = 0;

struct SSatSatKey
{
    Uint2   m_Sat;
    Uint4   m_SatKey;

    SSatSatKey( Uint2 sat, Uint4 sat_key ) : m_Sat ( sat ), m_SatKey( sat_key ) {}
};

struct SSatSatKeyRange
{
    Uint2   m_Sat;
    Uint4   m_SatKeyStart;
    Uint4   m_SatKeyEnd;

    SSatSatKeyRange( Uint2 sat, Uint4 sat_key_start, Uint4 sat_key_end )
        : m_Sat( sat ), m_SatKeyStart( sat_key_start ), m_SatKeyEnd( sat_key_end ) {}
    string AsString() const
    {
        return NStr::IntToString(m_Sat) + "." +
            NStr::IntToString(m_SatKeyStart) + "_" +
            NStr::IntToString(m_SatKeyEnd);
    }
};


inline std::ostream & operator<<( std::ostream & the_stream,
                                    const SSatSatKeyRange & the_sat_satkey_range )
{
    return the_stream << the_sat_satkey_range.AsString();
}


inline bool operator==( const ncbi::SSatSatKey& this_one, const ncbi::SSatSatKey& other_one )
{
    return ( this_one.m_Sat == other_one.m_Sat )
            && (this_one.m_SatKey== other_one.m_SatKey );
}


class CTime;
class CDumpASNIndex
{
public:
    CDumpASNIndex( const std::string & root_dir_path
                    , SSatSatKeyRange sat_id
                    , size_t max_bad_blob_count = 0 // Throw an exception when exceeded.
                    , unsigned int thread_count = 1
                    ) : m_RootDirPath( root_dir_path )
                        , m_SatId( sat_id )
                        , m_MaxBadBlobCount( max_bad_blob_count )
                        , m_ThreadCount( thread_count )
                        , m_MainIndex( CAsnIndex::e_main )
                        , m_SeqIdIndex( CAsnIndex::e_seq_id )
    {
        x_CreateRootDir();
        x_WriteHeader();
        m_MainIndex.SetCacheSize(1 * 1024 * 1024 * 1024);
        m_MainIndex.Open( NASNCacheFileName::GetBDBIndex(m_RootDirPath, CAsnIndex::e_main),
               CBDB_RawFile::eReadWriteCreate);
        m_SeqIdIndex.SetCacheSize(1 * 1024 * 1024 * 1024);
        m_SeqIdIndex.Open( NASNCacheFileName::GetBDBIndex(m_RootDirPath, CAsnIndex::e_seq_id),
               CBDB_RawFile::eReadWriteCreate);
    }

    //  This is the main functional method of the class.  Call it repeatedly, passing an
    //  uncompressed blob as a CSeq_entry.  The blob ID will be used in reporting back errors.
    void    DumpBlob ( const CRef<objects::CSeq_entry> entry,
                        const CTime & timestamp,
                        SSatSatKey blob_id );

    //  Test this method to see if the indexing threads have finished working and you can shut down.
    bool    IsBusyIndexing() const { return false; }; // Currently we have one thread only.
    std::vector<SSatSatKey> GetBadBlobList() const { return m_BadBlobList; }
    size_t  GetBadBlobCount() const { return m_BadBlobList.size(); }
    
    //  Call this when you're finished calling Dump.  Currently doesn't do anything.
    void    Done();

private:
    std::string  m_RootDirPath;
    SSatSatKeyRange m_SatId;
    std::vector<SSatSatKey> m_BadBlobList;
    size_t  m_MaxBadBlobCount;
    unsigned int m_ThreadCount;
    objects::CCache_blob m_CacheBlob;
    CAsnIndex   m_MainIndex;
    CAsnIndex   m_SeqIdIndex;
    CChunkFile  m_ChunkFile;
    CSeqIdChunkFile  m_SeqIdChunkFile;
    std::streampos  m_Offset;
    size_t  m_Size;

    void    x_CreateBlob ( const objects::CSeq_entry & entry, const CTime & timestamp );
    void    x_WriteBlob();
    void    x_BuildIndexAndSeqIdInfo( const objects::CSeq_entry & entry );
    void    x_CreateRootDir();
    Uint4   x_GetTaxId( const objects::CBioseq & bio_seq );
    void    x_WriteHeader();

    enum {
        // Change the following line to compile out the stopwatches.
        EIsInstantiated = true
    };

    CTempStopWatch<EIsInstantiated> m_CreateBlobSW;
    CTempStopWatch<EIsInstantiated> m_WriteBlobSW;
    CTempStopWatch<EIsInstantiated> m_BuildIndexEntrySW;
    CTempStopWatch<EIsInstantiated> m_DumpSW;

public:
    void    DumpTiming();

    const std::string & GetRootDir() { return m_RootDirPath; }
};


END_NCBI_SCOPE


#endif  //  DUMP_ASN_INDEX_HPP__

