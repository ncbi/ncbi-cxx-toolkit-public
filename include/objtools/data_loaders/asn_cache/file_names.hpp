#ifndef ASN_CACHE_FILE_NAMES_HPP__
#define ASN_CACHE_FILE_NAMES_HPP__
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
 * Class that returns all the file names used in the library.
 */

#include <string>

#include <corelib/ncbifile.hpp>

#include <objtools/data_loaders/asn_cache/asn_index.hpp>

BEGIN_NCBI_SCOPE

namespace NASNCacheFileName
{
    inline string GetBDBIndex() { return string( "asn_cache.idx" ); }
    inline string GetSeqIdIndex() { return string( "seq_id_cache.idx" ); }
    inline string GetBDBIndex( const string & root_dir, CAsnIndex::E_index_type type )
    {
        return CDirEntry::ConcatPath( root_dir,
                                      type == CAsnIndex::e_main ? GetBDBIndex()
                                                                : GetSeqIdIndex() );
    }

    inline string GetChunkPrefix() { return string( "chunk." ); }
    inline string GetSeqIdChunk() { return string( "seq_id_chunk" ); }
    inline string GetSeqIdChunk( const string & root_dir )
    {
        return CDirEntry::ConcatPath( root_dir, GetSeqIdChunk() );
    }
     
    inline string GetIntermediateFilePrefix() { return string( "intermediate." ); }

    inline string GetHeader() { return string( "header" ); }
    inline string GetGIIndex() { return string( "gi_index" ); }
    inline string GetLastUpdate() { return string ( "last_update" ); }
    inline string GetLockfilePrefix() { return string( ".asndump_lock." ); }
    
    inline string  GenerateLockfilePath( const string & dump_path )
    {
        string lockfile_path = CDirEntry::DeleteTrailingPathSeparator( dump_path );
        size_t  path_separator_index = lockfile_path.find_last_of( CDirEntry::GetPathSeparator() );
        _ASSERT( string::npos != path_separator_index );
        lockfile_path.insert( path_separator_index + 1, GetLockfilePrefix() );

        return  lockfile_path;
    }
    

}

END_NCBI_SCOPE

#endif  //  ASN_CACHE_FILE_NAMES_HPP__
