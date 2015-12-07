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
 *   C++ class to handle GPipe manifest files.  These are action node
 *   newline-separated, one column file path manifests.  Do not confuse
 *   this manifest with the queue XML manifest in gpipe/objects/manifest.
 */

#include <ncbi_pch.hpp>

#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iterator>

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/ncbifile.hpp>
//#include <util/xregexp/regexp.hpp>
#include <util/file_manifest.hpp>


BEGIN_NCBI_SCOPE


CFileManifest::CFileManifest( const std::string & manifest_path ) :
        m_ManifestPath( manifest_path )
{
    x_Init();
}

CFileManifest::CFileManifest( const CArgValue& manifest_path ) :
        m_ManifestPath( manifest_path.AsString() )
{
    x_Init();
}

void CFileManifest::x_Init()
{
    if ( m_ManifestPath.empty() ) {
        NCBI_THROW( CManifestException, eEmptyManifestName, "" );
    }
}

void    CFileManifest::Validate() const
{
    //  Check each file whether it can be open for reading.
    CFile manifest_file( m_ManifestPath );
    if ( ! manifest_file.IsFile() ) {
        NCBI_THROW( CManifestException, eCantOpenManifestForRead, m_ManifestPath );
    }
    CNcbiIfstream   manifest_stream( m_ManifestPath.c_str() );
    if ( ! manifest_stream ) {
        NCBI_THROW( CManifestException, eCantOpenManifestForRead, m_ManifestPath );
    }
    
    CManifest_CI    manifest_line_iter( manifest_stream );
    CManifest_CI    end_iter;
    while ( manifest_line_iter != end_iter ) {
        CFile file( *manifest_line_iter );
        if ( ! file.IsFile() ) {
            std::string error_string = "Manifest: " + m_ManifestPath;
            error_string += " Bad file: ";
            error_string += *manifest_line_iter;
            NCBI_THROW( CManifestException, eCantOpenFileInManifest, error_string );
        }
        CNcbiIfstream   manifest_file_stream( manifest_line_iter->c_str() );
        if ( ! manifest_file_stream ) {
            std::string error_string = "Manifest: " + m_ManifestPath;
            error_string += " Cannot read file: ";
            error_string += *manifest_line_iter;
            NCBI_THROW( CManifestException, eCantOpenFileInManifest, error_string );
        }
        ++manifest_line_iter;
    }
}


vector<string>    CFileManifest::GetAllFilePaths() const
{
    CNcbiIfstream   manifest_stream( m_ManifestPath.c_str() );
    if ( ! manifest_stream ) {
        NCBI_THROW( CManifestException, eCantOpenManifestForRead,
                    m_ManifestPath );
    }

    CManifest_CI    manifest_line_iter( manifest_stream );
    CManifest_CI    end_iter;

    vector<string>    file_paths;
    string path;
    for( ; manifest_line_iter != end_iter; ++manifest_line_iter) {
        path = CDirEntry::CreateAbsolutePath(*manifest_line_iter);
        path = CDirEntry::NormalizePath(path);
        file_paths.push_back(path);
    }

    return  file_paths;
}


std::string CFileManifest::GetSingleFilePath() const
{
    //  Read the first file path and check for a second.  Throw if a second is present.
    //  An empty manifest is ok.  We return an empty string.
    std::string only_file_path;
    
    CNcbiIfstream   manifest_stream( m_ManifestPath.c_str() );
    if ( ! manifest_stream ) {
        NCBI_THROW( CManifestException, eCantOpenManifestForRead, m_ManifestPath );
    }

    CManifest_CI    manifest_line_iter( manifest_stream );
    CManifest_CI    end_iter;
    if ( manifest_line_iter != end_iter ) {
        only_file_path = *manifest_line_iter++;
        if ( manifest_line_iter != end_iter ) {
            NCBI_THROW( CManifestException, eMoreThanOneFile, m_ManifestPath );
        }
    }
    
    return  only_file_path;
}


void    CFileManifest::WriteManyFilePaths( const std::vector<std::string> & file_paths )
{
    CNcbiOfstream   manifest_stream( m_ManifestPath.c_str() );
    if ( ! manifest_stream ) {
        NCBI_THROW( CManifestException, eCantOpenManifestForWrite, m_ManifestPath );
    }

    std::ostream_iterator<std::string>  manifest_iterator( manifest_stream, "\n" );
    std::copy( file_paths.begin(), file_paths.end(), manifest_iterator );
}


END_NCBI_SCOPE
