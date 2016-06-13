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
 * Update an existing cache from ID dump updates
 *
 */

#include <ncbi_pch.hpp>

#include <string>
#include <map>
#include <ostream>
#include <utility>
#include <algorithm>

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
#include <objtools/data_loaders/asn_cache/dump_asn_index.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_exception.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_util.hpp>
#include "unix_lockfile.hpp"
#include "extract_satkey.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);



typedef std::pair<Uint4, Uint8> TChunkIdSizePair;
typedef std::map<Int2, TChunkIdSizePair>   TPreviousUpdateMap;
typedef TPreviousUpdateMap::value_type  TPreviousUpdateMapEntry;



/////////////////////////////////////////////////////////////////////////////
//  CUpdateCacheApplication::


class CUpdateCacheApplication : public CNcbiApplication
{
private:
    virtual void Init(void);
    virtual int  Run(void);
    virtual void Exit(void);
    
    void    x_SetupUpdate(bool multiple_updates);
    TPreviousUpdateMap x_ReadLastUpdate() const;
    TChunkIdSizePair   x_FindLastChunkSize( const CDir & dump_subdir );
    Uint8   x_FindSeqIdChunkSize( const CDir & dump_subdir );
    void    x_UpdateCache( const CDirEntry & dump_subdir,
                            const TChunkIdSizePair & last_chunk_size,
                            const TChunkIdSizePair & previous_last_chunk_size,
                            Uint8 seq_id_chunk_size,
                            Uint8 previous_seq_id_chunk_size );
    void    x_UpdateIndex( CAsnIndex::E_index_type index_type,
                            const CDirEntry & dump_subdir,
                            const TChunkIdSizePair & previous_last_chunk_size,
                            Int8 offset_delta,
                            Uint4 dump_chunk_id = 0,
                            Uint4 cache_chunk_id = 0);
    void    x_WriteLastUpdateMap( const TPreviousUpdateMap & last_update_map );

    CDir    m_DumpRoot;
    CDir    m_CacheRoot;
    bool    m_PreReadIndex;
    Uint4   m_TotalUpdatedEntries;
};


/// Caution!  The operator<< and operator>> are being used for unformatted, binary I/O!
/// Warning!  The following code ain't pretty.  It's not standard conforming either.
/// The standard only allows template specialization to be injected into namespace std,
/// and not overrides.
BEGIN_STD_SCOPE
inline std::ostream &
    operator<< ( std::ostream & the_stream, const TPreviousUpdateMapEntry & map_entry )
{
    the_stream.write( reinterpret_cast<const char *>( &map_entry.first ), sizeof( map_entry.first ) );
    the_stream.write( reinterpret_cast<const char *>( &map_entry.second.first ),
                                                        sizeof( map_entry.second.first ) );
    return  the_stream.write( reinterpret_cast<const char *>( &map_entry.second.second ),
                                                                sizeof( map_entry.second.second ) );
}


inline std::istream &
    operator>> ( std::istream & the_stream, TPreviousUpdateMap::value_type & map_entry )
{
    the_stream.read( const_cast<char *>(
        reinterpret_cast<const char *>( &map_entry.first ) ), sizeof( map_entry.first ) );
    the_stream.read( reinterpret_cast<char *>( &map_entry.second.first ),
                                                sizeof( map_entry.second.first ) );
    return  the_stream.read( reinterpret_cast<char *>( &map_entry.second.second ),
                                                        sizeof( map_entry.second.second ) );
}
END_STD_SCOPE
/// Ok, you can open your eyes again.


/////////////////////////////////////////////////////////////////////////////
//  Init test for all different types of arguments


void CUpdateCacheApplication::Init(void)
{
    // Create command-line argument descriptions class
    auto_ptr<CArgDescriptions> arg_desc(new CArgDescriptions);

    // Specify USAGE context
    arg_desc->SetUsageContext(GetArguments().GetProgramBasename(),
                              "Update an ASN cache from ID dump updates.");

    arg_desc->AddKey("dump", "IDDump",
                     "Path to the ID dump directory.",
                     CArgDescriptions::eInputFile);

    arg_desc->AddKey("cache", "Cache",
                     "Directory path for the ASN cache to be updated.",
                     CArgDescriptions::eInputFile);

    arg_desc->AddFlag("nopreread", "Do not preread the dump index "
                        "(Use if the ID dump is not on panfs).", false );

    arg_desc->AddFlag("single-update-dir", "\"dump\" points to one update dump directory, not to the parent directory");

    arg_desc->AddFlag("expect-duplicates", "duplicate keys in this update are expected, don't print error message");


    // Setup arg.descriptions for this application
    SetupArgDescriptions(arg_desc.release());
}



int CUpdateCacheApplication::Run(void)
{
    CFileAPI::SetLogging( eOn );
    const CArgs& args = GetArgs();

    m_DumpRoot = CDir( args["dump"].AsString() );
    if (! m_DumpRoot.Exists() ) {
        LOG_POST( Error << "ID dump directory " << m_DumpRoot.GetPath() << " does not exist!" );
        return 1;
    } else if ( ! m_DumpRoot.IsDir() ) {
        LOG_POST( Error << m_DumpRoot.GetPath() << " does not point to a "
                    << "valid ID dump directory!" );
        return 2;
    }
    
    m_CacheRoot = CDir( args["cache"].AsString() );
    if ( ! m_CacheRoot.Exists() ) {
        LOG_POST( Error << "Cache " << m_CacheRoot.GetPath() << " does not exist!" );
        return 3;
    }
    
    m_PreReadIndex = args["nopreread"];

    m_TotalUpdatedEntries = 0;

    x_SetupUpdate(!args["single-update-dir"]);
    
    return 0;
}


void    CUpdateCacheApplication::x_SetupUpdate(bool multiple_updates)
{
    CStopWatch  update_sw;
    update_sw.Start();
    
    const short kSleepTime = 1; // seconds
    const short kRetryCount = 5;
        
    Uint2   updated_satkey_count = 0;
    Uint2   skipped_satkey_count = 0;
    
    TPreviousUpdateMap  last_update_map;
    if(multiple_updates)
        last_update_map = x_ReadLastUpdate();
    
    CDir::TEntries  dump_subdir_list;
    if(multiple_updates)
        dump_subdir_list = m_DumpRoot.GetEntries( "*.0_0", CDir::eIgnoreRecursive );
    else
        dump_subdir_list.push_back(new CDir(m_DumpRoot));

    CExtractUpdateSatKey    satkey_extractor;
    ITERATE( CDir::TEntries, dump_dir_itr, dump_subdir_list ) {
      try {
        std::string dump_subdir_path = (*dump_dir_itr)->GetPath();
        //  ID uses a Unix file lock to guard access to the dump.
        LOG_POST( Info << "Creating lockfile: "
                << NASNCacheFileName::GenerateLockfilePath( dump_subdir_path ) );
        CUnixLockFile   file_lock( NASNCacheFileName::GenerateLockfilePath( dump_subdir_path ),
                                    kSleepTime,
                                    kRetryCount );
        /// if multiple_updates is false, value of satkey doesn't matter; last_update_map is always empty
        /// so accessing it will always return 0 value, so cache will be updated with entire contents of directory
        Uint2 satkey;
        if(multiple_updates){
            /// Check that this is one of the update directories
            std::pair<bool, Uint2>  extracted_satkey = satkey_extractor( dump_subdir_path );
            if ( ! extracted_satkey.first ) continue;
            satkey = extracted_satkey.second;
            _ASSERT( satkey > 0 );
        }
        
        file_lock.Lock();
        TChunkIdSizePair   last_chunk_size = x_FindLastChunkSize( dump_subdir_path );
        TChunkIdSizePair   previous_last_chunk_size = last_update_map[ satkey ];
        Uint8   seq_id_chunk_size = x_FindSeqIdChunkSize( dump_subdir_path );
        Uint8   previous_seq_id_chunk_size = last_update_map[ -satkey ].second;
        if ( last_chunk_size > previous_last_chunk_size ) {
            LOG_POST( Info << "Updating satkey " << satkey );
            updated_satkey_count++;
            x_UpdateCache( **dump_dir_itr, last_chunk_size, previous_last_chunk_size, seq_id_chunk_size, previous_seq_id_chunk_size );
            last_update_map[ satkey ] = last_chunk_size;
            last_update_map[ -satkey ] = make_pair(0U, seq_id_chunk_size);
        }
        else {
            LOG_POST( Info << "Skipping satkey " << satkey );
            skipped_satkey_count++;
            _ASSERT( last_chunk_size == previous_last_chunk_size );
        }
      } catch (CUnixLockFileException & e){
        LOG_POST( Error << "Failed to acquire lock on " << (*dump_dir_itr)->GetPath()
                        << "; skipping updates in this directory, will try again tomorrow" );
      }
    }

    if(multiple_updates)
        x_WriteLastUpdateMap( last_update_map );
    LOG_POST( Error << "Updated " << m_TotalUpdatedEntries << " entries in " << updated_satkey_count
                << " satkeys and skipped " << skipped_satkey_count << " satkeys in "
                << update_sw.Elapsed() << " seconds." );
}


TPreviousUpdateMap  CUpdateCacheApplication::x_ReadLastUpdate() const
{
    /// The last update list maps the ID dump update satkey with the size of the
    /// last chunk file, and the -satkey with the size of the seqid chunk file.
    /// If the directory was updated, the chunk file sizes will not match.
    std::string last_update_path
        = CDirEntry::ConcatPath( m_CacheRoot.GetPath(), NASNCacheFileName::GetLastUpdate() );
    CNcbiIfstream   last_update_input_stream( last_update_path.c_str() );
    std::istream_iterator<TPreviousUpdateMap::value_type>
        last_update_istream_itr( last_update_input_stream );
    std::istream_iterator<TPreviousUpdateMap::value_type>   end_iterator;

    TPreviousUpdateMap result;
    copy( last_update_istream_itr, end_iterator,
          inserter( result, result.end() ) );
    return result;
}


TChunkIdSizePair    CUpdateCacheApplication::x_FindLastChunkSize( const CDir & dump_subdir_path)
{
    TChunkIdSizePair result;
    result.first = CChunkFile::s_FindNextChunk( dump_subdir_path.GetPath(), 1, result.second);
    
    return result;
}


Uint8    CUpdateCacheApplication::x_FindSeqIdChunkSize( const CDir & dump_subdir_path)
{
    CFile   chunk_file( NASNCacheFileName::GetSeqIdChunk(dump_subdir_path.GetPath()));
    return chunk_file.Exists() ? chunk_file.GetLength() : 0;
}


void    CUpdateCacheApplication::x_UpdateCache( const CDirEntry & dump_subdir,
                                                const TChunkIdSizePair & last_chunk_size,
                                                const TChunkIdSizePair & previous_last_chunk_size,
                                                Uint8 seq_id_chunk_size,
                                                Uint8 previous_seq_id_chunk_size )
{
    LOG_POST( Info << "Updating from " << dump_subdir.GetName() << " with last chunk ID "
                << last_chunk_size.first << " and size " << last_chunk_size.second
                << " and previous chunk ID " << previous_last_chunk_size.first << " and size "
                << previous_last_chunk_size.second );

    Uint8   chunk_offset_to_copy_from = 0;
    Uint8   chunk_size_to_copy = 0;

    for ( Uint4 chunk_id = std::max( previous_last_chunk_size.first, 1U );
                chunk_id <= last_chunk_size.first;
                chunk_id++ ) {
        LOG_POST( Info << "Processing dump chunk ID " << chunk_id );
        CFile   a_dump_chunk( CChunkFile::s_MakeChunkFileName( dump_subdir.GetPath(), chunk_id ) );
        if ( ! a_dump_chunk.Exists() ) {
            NCBI_THROW( CASNCacheException, eCantFindChunkFile, a_dump_chunk.GetPath() );
        }
        
        if ( chunk_id == std::max( previous_last_chunk_size.first, 1U ) ) {
            chunk_size_to_copy = a_dump_chunk.GetLength() - previous_last_chunk_size.second;
            chunk_offset_to_copy_from = previous_last_chunk_size.second;
        } else {
            chunk_size_to_copy = a_dump_chunk.GetLength();
            chunk_offset_to_copy_from = 0;
        }
        
        Uint4   last_cache_chunk_id = CChunkFile::s_FindLastChunk( m_CacheRoot.GetPath() );
        LOG_POST( Info << "Last cache chunk ID is " << last_cache_chunk_id );
        CChunkFile  cache_chunk( m_CacheRoot.GetPath(), last_cache_chunk_id );
        Uint4   appended_chunk_id =
            cache_chunk.Append( m_CacheRoot.GetPath(), a_dump_chunk, chunk_offset_to_copy_from );
        
        Int8    offset_delta = 0;
        if ( appended_chunk_id == last_cache_chunk_id ) {
            offset_delta = cache_chunk.GetLength()
                            - ( chunk_offset_to_copy_from + chunk_size_to_copy );
        } else {
            offset_delta = -chunk_offset_to_copy_from;
        }
        LOG_POST( Info << "Appended dump chunk " << chunk_id << " with offset delta "
                    << offset_delta  << " to cache chunk ID " << appended_chunk_id );
        LOG_POST( Info << "Cache chunk length = " << cache_chunk.GetLength()
                    << " chunk_offset_to_copy_from = " << chunk_offset_to_copy_from
                    << " chunk_size_to_copy " << chunk_size_to_copy );
        
        CStopWatch  index_sw;
        index_sw.Start();
        if ( m_PreReadIndex ) {
            ReadThroughFile( NASNCacheFileName::GetBDBIndex( dump_subdir.GetPath(), CAsnIndex::e_main ));
        }
        x_UpdateIndex( CAsnIndex::e_main, dump_subdir, previous_last_chunk_size,
                        offset_delta, chunk_id, appended_chunk_id);
        LOG_POST( Info << "Index updated in " << index_sw.Elapsed() << " seconds." );
    }

    CFile   cache_seqid_chunk( NASNCacheFileName::GetSeqIdChunk(m_CacheRoot.GetPath()));
    if(seq_id_chunk_size > 0 && cache_seqid_chunk.Exists()){
        /// Both update dump directory and the cache directory have a SeqId chunk file; update
        /// it and its index
        LOG_POST( Info << "Processing dump seqid chunk and index");
        CFile   dump_seqid_chunk( NASNCacheFileName::GetSeqIdChunk(dump_subdir.GetPath()));
        
        chunk_size_to_copy = dump_seqid_chunk.GetLength() - previous_seq_id_chunk_size;
        chunk_offset_to_copy_from = previous_seq_id_chunk_size;
        
        CSeqIdChunkFile  cache_chunk;
        cache_chunk.Append( m_CacheRoot.GetPath(), dump_seqid_chunk, chunk_offset_to_copy_from );
        
        Int8 offset_delta = cache_chunk.GetLength()
                            - ( chunk_offset_to_copy_from + chunk_size_to_copy );
        LOG_POST( Info << "Appended dump SeqId chunk with offset delta "
                    << offset_delta  << " to cache SeqId chunk" );
        LOG_POST( Info << "Cache SeqId chunk length = " << cache_chunk.GetLength()
                    << " chunk_offset_to_copy_from = " << chunk_offset_to_copy_from
                    << " chunk_size_to_copy " << chunk_size_to_copy );
        
        CStopWatch  index_sw;
        index_sw.Start();
        if ( m_PreReadIndex ) {
            ReadThroughFile( NASNCacheFileName::GetBDBIndex( dump_subdir.GetPath(), CAsnIndex::e_seq_id ));
        }
        x_UpdateIndex( CAsnIndex::e_seq_id, dump_subdir, make_pair(0U, previous_seq_id_chunk_size), offset_delta );
        LOG_POST( Info << "Index updated in " << index_sw.Elapsed() << " seconds." );
    }
}


void    CUpdateCacheApplication::x_WriteLastUpdateMap( const TPreviousUpdateMap & last_update_map )
{
    std::string last_update_path
        = CDirEntry::ConcatPath( m_CacheRoot.GetPath(), NASNCacheFileName::GetLastUpdate() );
    CNcbiOfstream   last_update_map_file_stream( last_update_path.c_str(), std::ios::binary );
    std::ostream_iterator<TPreviousUpdateMapEntry>  output_itr( last_update_map_file_stream );
    std::copy( last_update_map.begin(), last_update_map.end(), output_itr );
}

static string s_BDBErrorString(EBDB_ErrCode error_code) {
    switch (error_code) {
        case eBDB_Ok:
            return "OK";
        case eBDB_NotFound:
            return "Not Found";
        case eBDB_KeyDup:
            return "Duplicate Key";
        case eBDB_KeyEmpty:
            return "Empty Key";
        case eBDB_MultiRowEnd:
            return "Multi Row End";
        default:
            return "Unknown error code: " + NStr::IntToString(error_code);
    }
}

void    CUpdateCacheApplication::x_UpdateIndex( CAsnIndex::E_index_type index_type,
                                                const CDirEntry & dump_subdir,
                                                const TChunkIdSizePair & previous_last_chunk_size,
                                                Int8 offset_delta,
                                                Uint4 dump_chunk_id,
                                                Uint4 cache_chunk_id )
{
    Uint4   previous_last_chunk_id = previous_last_chunk_size.first;
    Uint8   previous_last_offset = previous_last_chunk_size.second;
    
    CAsnIndex   cache_index(index_type);
    cache_index.SetCacheSize( 1 * 1024 * 1024 * 1024 );
    cache_index.Open( NASNCacheFileName::GetBDBIndex( m_CacheRoot.GetPath(), index_type),
                        CBDB_RawFile::eReadWrite );
    
    CAsnIndex   dump_index(index_type);
    dump_index.SetCacheSize( 1 * 1024 * 1024 * 1024 );
    dump_index.Open( NASNCacheFileName::GetBDBIndex( dump_subdir.GetPath(), index_type),
                        CBDB_RawFile::eReadOnly );

    CBDB_FileCursor dump_cursor( dump_index );
    dump_cursor.InitMultiFetch( 1 * 1024 * 1024 * 1024 );
    dump_cursor.SetCondition( CBDB_FileCursor::eFirst, CBDB_FileCursor::eLast);
    
    Uint4   dump_entry_count = 0;
    Uint4   updated_entry_count = 0;
    Uint4   error_entry_count = 0;
    while (dump_cursor.Fetch() == eBDB_Ok) {
        if ( dump_index.GetChunkId() != dump_chunk_id ) continue;
        dump_entry_count++;
        if (( dump_index.GetChunkId() > previous_last_chunk_id )
                || (( dump_index.GetChunkId() == previous_last_chunk_id )
                    && ( dump_index.GetOffset() >= previous_last_offset)) ) {
            cache_index.SetSeqId( dump_index.GetSeqId() );
            cache_index.SetVersion( dump_index.GetVersion() );
            cache_index.SetGi( dump_index.GetGi() );
            cache_index.SetTimestamp( dump_index.GetTimestamp() );
            cache_index.SetChunkId( cache_chunk_id );
            cache_index.SetOffset( dump_index.GetOffset() + offset_delta );
            cache_index.SetSize( dump_index.GetSize() );
            cache_index.SetSeqLength( dump_index.GetSeqLength() );
            cache_index.SetTaxId( dump_index.GetTaxId() );
            
            EBDB_ErrCode    insert_error = cache_index.Insert();
            if ( eBDB_Ok != insert_error ) {
                LOG_POST( Error << "Failed (" << s_BDBErrorString(insert_error)
                         << ") to insert record in"
                         << (index_type == CAsnIndex::e_seq_id ? " SeqId" : "") << " cache: "
                         << cache_index.GetSeqId() << '.' << cache_index.GetVersion()
                         << " gi:" << cache_index.GetGi() << ' '
                         << CTime(cache_index.GetTimestamp()) );
                error_entry_count++;
            } else {
                updated_entry_count++;
            }
        }
    }
    LOG_POST( Info << "Updated " << updated_entry_count << " of " << dump_entry_count
                << " index entries in chunk " << cache_chunk_id );
    m_TotalUpdatedEntries += updated_entry_count;
    if ( error_entry_count ) {
        LOG_POST( Error << error_entry_count << " seq IDs failed to load into cache." );
    }
}


/////////////////////////////////////////////////////////////////////////////
//  Cleanup


void CUpdateCacheApplication::Exit(void)
{
    SetDiagStream(0);
}


/////////////////////////////////////////////////////////////////////////////
//  MAIN


int main(int argc, const char* argv[])
{
    // Execute main application function
    return CUpdateCacheApplication().AppMain(argc, argv, 0, eDS_Default, 0);
}
