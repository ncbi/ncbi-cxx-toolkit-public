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
 * Authors:  Cheinan Marks, Eyal Mozes
 *
 * File Description:
 * Merge a set of subcaches located in a set of subdirectories into a
 * single main cache.  The merge works in two stages.  In the first stage,
 * the subcaches are walked; the blobs appended into a single ISAM
 * block, and the SeqIds are appended into a separate ISAM block. The index
 * files of the sub-caches are read in and the index entries are stored in memory.
 *
 * In the second stage, the in-memory index entries are sorted, and then the BDB
 * index files are created out of them.
 *
 * Nominally the app was designed to merge the ID dumps and updates into the
 * working ASN cache, but because the ID dumps are stored in the same format
 * as the cache itself, it should work with any set of ASN caches in the
 * correct directory structure; a main directory containing a set of
 * subcdirectories, each with an ASN cache in it (blobs and indices).
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbiapp.hpp>
#include <corelib/ncbienv.hpp>
#include <corelib/ncbiargs.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/ncbistr.hpp>
#include <corelib/ncbitime.hpp>
#include <db/bdb/bdb_cursor.hpp>

#include <objtools/data_loaders/asn_cache/asn_index.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_exception.hpp>
#include "unix_lockfile.hpp"
#include "extract_satkey.hpp"
#include "merge_sub_indices.hpp"

#include <string>
#include <vector>
#include <cstdlib>
#include <algorithm>
#include <functional>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


static const int k_AverageSeqIdSize = 16;
static const int k_MaxSeqIdsPerDump = 30000000;

static Int8 s_MaxEntriesInMemory;




/////////////////////////////////////////////////////////////////////////////
//  CMergeSubcachesApplication::

class CAsnIndexWrapper;
class CMergeSubcachesApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    
    void    x_CombineSubcacheData( const CDir & sub_cache_root );
    void    x_TranslateChunkIdInIndex( const CDir & sub_cache_dir,
                                                CAsnIndex & sub_cache_index,
                                                unsigned int this_index_data_start,
                                                CAsnIndex::TChunkId master_chunk_id[],
                                                CAsnIndex::TOffset offset_delta[] );
    void    x_CreateIndices( );

    void    x_CreateIntermediateFile( );

    void    x_SortIndexData( );

    string m_SeqIdBlob;
    char * m_SeqIdBlobNext;
    char * m_SeqIdBlobEnd;

    auto_ptr<CDir> m_MasterCacheRoot;

    /// Flag indicating whether we fuond a SeqId index in at least one of the subcaches
    bool m_FoundSeqIdIndices;

    int m_NumIntermediateFilesCreated;

    vector<CMergeSubIndices::SIndexLocator> m_IndexData;

    vector<Uint4> m_IndexDataHandles;

};


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CMergeSubcachesApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Merge an ID dump into an ASN cache.");

    arg_desc->AddKey("dump", "IDDump",
                     "Path to the directory containing the ID dump.",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("cache", "Cache",
                     "Directory path for the ASN cache that will be created.",
                     CArgDescriptions::eInputFile);

    arg_desc->AddDefaultKey("size", "MaxEntriesInMemory",
                            "maximum number of index entries that can be held in memory",
                            CArgDescriptions::eInteger,
                            "20000000");

    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CMergeSubcachesApplication::Run(void)
{
    CFileAPI::SetLogging( eOn );
    const CArgs& args = GetArgs();

    CDir    sub_caches_root( args["dump"].AsString() );
    if (! sub_caches_root.Exists() ) {
        LOG_POST( Error << "ID dump directory " << sub_caches_root.GetPath() << " does not exist!" );
        return 1;
    } else if ( ! sub_caches_root.IsDir() ) {
        LOG_POST( Error << sub_caches_root.GetPath() << " does not point to a "
                    << "valid ID dump directory!" );
        return 2;
    }
    
    m_MasterCacheRoot.reset( new CDir(args["cache"].AsString() ));
    if ( m_MasterCacheRoot->Exists() ) {
        LOG_POST( Warning << "Cache " << m_MasterCacheRoot->GetPath() << " already exists!" );
    } else if ( ! m_MasterCacheRoot->CreatePath() ) {
        LOG_POST( Error << "Unable to create a cache at " << m_MasterCacheRoot->GetPath() );
        return 3;
    }
    
    s_MaxEntriesInMemory = args["size"].AsInteger();

    if(s_MaxEntriesInMemory <= 0){
        LOG_POST( Error << "Invalid size parameter: " << s_MaxEntriesInMemory );
        return 4;
    }

    Uint8 seq_id_blob_size = k_AverageSeqIdSize * s_MaxEntriesInMemory;

    /// Reserve space for the index data vector, and for a blob with the concatenated
    /// seq-ids; include plenty of extra space for reading in one more dump directory
    /// after reaching limit.
    m_SeqIdBlob.reserve(seq_id_blob_size + CMergeSubIndices::SIndexLocator::k_MaxSeqIdLength * k_MaxSeqIdsPerDump);
    m_SeqIdBlobNext = const_cast<char *>(m_SeqIdBlob.data());
    m_SeqIdBlobEnd = m_SeqIdBlobNext + seq_id_blob_size;

    LOG_POST(Info << "Allocated initial seq-id blob of size " << seq_id_blob_size);

    m_IndexData.reserve(s_MaxEntriesInMemory + k_MaxSeqIdsPerDump);
    
    m_FoundSeqIdIndices = false;

    m_NumIntermediateFilesCreated = 0;

    x_CombineSubcacheData( sub_caches_root );
    
    x_CreateIndices( );

    return 0;
}


void      CMergeSubcachesApplication::x_CombineSubcacheData( const CDir & sub_caches_root )
{
    CStopWatch  sw;
    sw.Start();

    // Stage 1:  Copy the blobs to the new master cache and adjust index.
    size_t  dir_count = 0;
    CAsnIndex::TChunkId  master_chunk_id = 1;
    CAsnIndex::TOffset main_offset_delta = 0, seq_id_offset_delta = 0;
    /// For each chunk file in the current cub-cache, the chunk and offset it corresponds to in th
    /// master cache. Entry 0 represents the SeqId cache.
    CAsnIndex::TOffset current_sub_cache_offsets[1000];
    CAsnIndex::TChunkId current_sub_cache_chunk_ids[1000];

    CExtractUpdateSatKey    update_satkey_extractor;

    CDir::TEntries  subdir_list = sub_caches_root.GetEntries( kEmptyStr, CDir::eIgnoreRecursive
                                                                | CDir::fCreateObjects );
    ITERATE( CDir::TEntries, cache_dir_itr, subdir_list ) {
        if ( ! (*cache_dir_itr)->IsDir() ) {
            continue;
        }
        CDir & cache_dir = dynamic_cast<CDir &>(**cache_dir_itr);
        CRegexp subcache_pat1("^(\\d+)\\.(\\d+)_(\\d+)$");
        CRegexp subcache_pat2("^(\\d+)\\.(\\d+)_(\\d+)\\.aso$");
        if (!subcache_pat1.IsMatch(cache_dir.GetName()) &&
            !subcache_pat2.IsMatch(cache_dir.GetName()))
        {
            continue;
        }
        if ( update_satkey_extractor( cache_dir.GetPath() ).first ) {
            continue; // Skip updates.
        }

        if(m_IndexData.size() > s_MaxEntriesInMemory  ||
           m_SeqIdBlobNext > m_SeqIdBlobEnd)
        {
           /// We've gone over the limit, either in the index-data vector or in
           /// the seq-id blob; write out data to intermediate file
           x_CreateIntermediateFile();
	}

        size_t    this_index_data_start = m_IndexData.size();

        CStopWatch  disk_time;
        CStopWatch  indexing_time;
        
        dir_count++;

        std::string subcache_main_index_path =
            NASNCacheFileName::GetBDBIndex( cache_dir.GetPath(),
                                            CAsnIndex::e_main );

        // Panasas optimization: reading through the subcache sequentially
        // significantly improves subsequent random access.
        ReadThroughFile( subcache_main_index_path );

        CAsnIndex    sub_cache_main_index(CAsnIndex::e_main);
        sub_cache_main_index.Open( subcache_main_index_path,
                                   CBDB_RawFile::eReadOnly );
        
        // Iterate through the chunk files in the sub cache and copy them into
        // the master cache, appending them to the existing chunks.  The
        // offsets in the index are adjusted for the new master cache.
        CDir::TEntries  chunk_list
            = cache_dir.GetEntries( NASNCacheFileName::GetChunkPrefix() + "*", 
                                    CDir::eIgnoreRecursive |
                                    CDir::fCreateObjects );
        size_t  chunk_count = 0;
        ITERATE( CDir::TEntries, chunk_file_itr, chunk_list ) {
            disk_time.Start();
            const CFile & source_chunk = dynamic_cast<CFile &>(**chunk_file_itr);
            CChunkFile  master_chunk( m_MasterCacheRoot->GetPath(),
                                      master_chunk_id );
            size_t  appended_chunk_id =
                master_chunk.Append( m_MasterCacheRoot->GetPath(),
                                     source_chunk );
            if ( appended_chunk_id != master_chunk_id ) {
                master_chunk_id = appended_chunk_id;
                main_offset_delta = 0;
            }
            disk_time.Stop();
            current_sub_cache_chunk_ids[++chunk_count] = master_chunk_id;
            current_sub_cache_offsets[chunk_count] = main_offset_delta;
            main_offset_delta += source_chunk.GetLength();
        }
        indexing_time.Start();
        x_TranslateChunkIdInIndex(cache_dir,
                                      sub_cache_main_index,
                                      this_index_data_start,
                                      current_sub_cache_chunk_ids, current_sub_cache_offsets );
        indexing_time.Stop();

        LOG_POST( Info << disk_time.Elapsed() << " seconds to append "
                  << chunk_count << " Cache_blob chunks." );
        LOG_POST( Info << indexing_time.Elapsed()
                  << " seconds to update main index with "
                  << m_IndexData.size() - this_index_data_start
                  << " total entries." );

        std::string subcache_seq_id_index_path =
            NASNCacheFileName::GetBDBIndex( cache_dir.GetPath(),
                                            CAsnIndex::e_seq_id );
        CFile source_seq_id_chunk(NASNCacheFileName::GetSeqIdChunk(cache_dir.GetPath()));
        if (!CDirEntry(subcache_seq_id_index_path).Exists()  ||
            !source_seq_id_chunk.Exists()) {
            ///
            /// Sub-cache directory was written with old version, and has no
            /// seq-id cache
            ///
            continue;
        }

        m_FoundSeqIdIndices = true;

        ReadThroughFile( subcache_seq_id_index_path );

        CAsnIndex    sub_cache_seq_id_index(CAsnIndex::e_seq_id);
        sub_cache_seq_id_index.Open( subcache_seq_id_index_path, CBDB_RawFile::eReadOnly );

        disk_time.Restart();
        CSeqIdChunkFile  master_seq_id_chunk;
        master_seq_id_chunk.Append( m_MasterCacheRoot->GetPath(), source_seq_id_chunk );
        disk_time.Stop();
        current_sub_cache_chunk_ids[0] = 0;
        current_sub_cache_offsets[0] = seq_id_offset_delta;
        indexing_time.Restart();
        x_TranslateChunkIdInIndex ( cache_dir,
                                    sub_cache_seq_id_index,
                                    this_index_data_start,
                                    current_sub_cache_chunk_ids, current_sub_cache_offsets );

        seq_id_offset_delta += source_seq_id_chunk.GetLength();
        indexing_time.Stop();

        LOG_POST( Info << disk_time.Elapsed() << " seconds to append SeqId chunk.");
        LOG_POST( Info << indexing_time.Elapsed() << " seconds to update SeqId index." );
    }

    LOG_POST( Info << dir_count << " directories written in "
                << sw.Elapsed() << " seconds." );
}


/// Copy data from sub-cache index into m_IndexData, translating offsets by offset_delta.
/// For each sub-cache, this function will be called twice, once for the main index and
/// once for the SeqId index
void    CMergeSubcachesApplication::x_TranslateChunkIdInIndex( const CDir & sub_cache_dir,
                        CAsnIndex & sub_cache_index,
                        unsigned int this_index_data_start,
                        CAsnIndex::TChunkId new_chunk_id[],
                        CAsnIndex::TOffset offset_delta[] )
{
    std::string id_dump_path = CFile( sub_cache_index.GetFileName() ).GetDir();
    CUnixLockFile   lockfile( NASNCacheFileName::GenerateLockfilePath( id_dump_path ) );
    lockfile.Lock();
    
    CBDB_FileCursor subcache_cursor( sub_cache_index );
    subcache_cursor.InitMultiFetch(1 * 1024 * 1024 * 1024);
    subcache_cursor.SetCondition( CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast) ;

    vector<CMergeSubIndices::SIndexLocator>::iterator index_data_iter = m_IndexData.begin() + this_index_data_start;
    
    while (subcache_cursor.Fetch() == eBDB_Ok) {
        if(sub_cache_index.GetIndexType() == CAsnIndex::e_seq_id){
            /// This is the second we're calling the function for this sub-cache; we already
            /// the index data entry for it in memory, we just need to update the SeqId pointer
            NCBI_ASSERT(index_data_iter != m_IndexData.end(),
                        (sub_cache_dir.GetPath() + " main index has " +
                         NStr::IntToString(m_IndexData.size() - this_index_data_start) + " entries, SeqID index has more").c_str()); 
            NCBI_ASSERT(index_data_iter->CorrectIndexLocation(sub_cache_index),
                        ("Mismatch in sub-cache main and seq-id indices in sub-cache " +
                            sub_cache_dir.GetPath() + ": index is at " +
                            sub_cache_index.CurrentLocationAsString() + ", index data is " + 
                            index_data_iter->AsString()).c_str() );
            index_data_iter->m_SeqIdOffset = sub_cache_index.GetOffset() + offset_delta[0];
            index_data_iter->m_SeqIdChunkSize = sub_cache_index.GetSize();
            ++index_data_iter;
        } else {
            CAsnIndex::TSeqId seq_id = sub_cache_index.GetSeqId();

            CMergeSubIndices::SIndexLocator index_entry;
            index_entry.m_SeqId = strcpy(m_SeqIdBlobNext, seq_id.c_str());
            m_SeqIdBlobNext += seq_id.size() + 1;
            index_entry.m_Version = sub_cache_index.GetVersion();
            index_entry.m_GI = sub_cache_index.GetGi();
            index_entry.m_Timestamp = sub_cache_index.GetTimestamp();
            index_entry.m_ChunkSize = sub_cache_index.GetSize();
            index_entry.m_SeqLength = sub_cache_index.GetSeqLength();
            index_entry.m_TaxId = sub_cache_index.GetTaxId();
            index_entry.m_Offset = sub_cache_index.GetOffset() +
                                        offset_delta[sub_cache_index.GetChunkId()];
            index_entry.m_ChunkId = new_chunk_id[sub_cache_index.GetChunkId()];
            m_IndexData.push_back(index_entry);
        }
    }
}


/// Define a comparator over container indices, which cen be used to sort them based
/// on the order of the elements they point to
template<class ContainerClass, class ElementComparator, class HandleType>
class HandleComparator
{
public:
    HandleComparator(const ContainerClass &elements, const ElementComparator &comparator)
        : m_Elements(elements), m_Comparator(comparator)
        {} 

    bool operator()(HandleType handle1, HandleType handle2)
    {
        return m_Comparator(m_Elements[handle1], m_Elements[handle2]);
    }

private:
    const ContainerClass &m_Elements;
    const ElementComparator &m_Comparator;
};

void    CMergeSubcachesApplication::x_SortIndexData( )
{
    m_IndexDataHandles.reserve(m_IndexData.size());
    m_IndexDataHandles.clear();
    for(Uint4 i=0; i < m_IndexData.size(); i++)
        m_IndexDataHandles.push_back(i);

    sort(m_IndexDataHandles.begin(), m_IndexDataHandles.end());
}

void    CMergeSubcachesApplication::x_CreateIntermediateFile( )
{
    CNcbiOfstream intermediate_file((m_MasterCacheRoot->GetPath() + "/" +
                    NASNCacheFileName::GetIntermediateFilePrefix() +
                    NStr::IntToString(++m_NumIntermediateFilesCreated)).c_str());

    x_SortIndexData();

    ITERATE(vector<Uint4>, it, m_IndexDataHandles){
        m_IndexData[*it].WriteTo(intermediate_file);
#ifdef _DEBUG
        LOG_POST(Info << "Intermediate index entry " << m_IndexData[*it].AsString() << ": " << m_IndexData[*it] << " intermediate file location " << intermediate_file.tellp());
#endif
    }

    m_IndexData.clear();
    m_SeqIdBlobNext = const_cast<char *>(m_SeqIdBlob.data());
}

void    CMergeSubcachesApplication::x_CreateIndices( )
{
    CMergeSubIndices merge(m_MasterCacheRoot->GetPath(),
                           m_NumIntermediateFilesCreated+1,
                           m_FoundSeqIdIndices,
                           m_SeqIdBlob.data());

    if(m_NumIntermediateFilesCreated > 0){
        /// Memory wasn't sufficient for holding all index data; write what we have now
        // into index file, and then merge all files
        x_CreateIntermediateFile();
        merge();
        for(int i=1; i <= m_NumIntermediateFilesCreated; ++i){
            CDirEntry   intermediate_file(m_MasterCacheRoot->GetPath() + "/" +
                    NASNCacheFileName::GetIntermediateFilePrefix() + NStr::IntToString(i));
            intermediate_file.Remove( CDirEntry::eNonRecursive );
            LOG_POST( Info << "Deleted intermediate index " << intermediate_file.GetPath() );
        }
    } else {
        /// All index data is in memory; sort it and write it into the two indices
        x_SortIndexData();
    
        ITERATE(vector<Uint4>, it, m_IndexDataHandles){
            merge.WriteIndexEntry(m_IndexData[*it]);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CMergeSubcachesApplication::Exit(void)
{
    SetDiagStream(0);
}

END_NCBI_SCOPE

/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return ncbi::CMergeSubcachesApplication().AppMain(argc, argv, 0, ncbi::eDS_Default, 0);
}
