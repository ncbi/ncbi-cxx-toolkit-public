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
 * File Description:  See chunk_file.hpp
 */

#include <ncbi_pch.hpp>

#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>

#include <corelib/ncbifile.hpp>
#include <corelib/ncbistre.hpp>

#include <serial/serial.hpp>
#include <serial/objostrasnb.hpp>
#include <serial/objistrasnb.hpp>

#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_exception.hpp>
#include <objtools/data_loaders/asn_cache/chunk_file.hpp>

BEGIN_NCBI_SCOPE
using objects::CCache_blob;


void    CChunkFile::OpenForWrite( const string & root_path )
{
    if (!root_path.empty() && m_OpenFileRootPath != root_path) {
        m_OpenFileRootPath = root_path;
        m_ChunkSerialNum = 1;
        m_ChunkSize = -1;
    }
    if (m_ChunkSize < 0) {
        Uint8 new_chunk_size;
        m_ChunkSerialNum = s_FindNextChunk( m_OpenFileRootPath, m_ChunkSerialNum, new_chunk_size );
        m_ChunkSize = new_chunk_size;
    } else if (m_ChunkSize > m_kMaxChunkSize) {
        ++m_ChunkSerialNum;
        m_ChunkSize = 0;
    }
    string file_path = s_MakeChunkFileName( m_OpenFileRootPath, m_ChunkSerialNum );

    if ( file_path != GetPath() ) {
        Reset( file_path );
    
        m_FileStream.close();
        m_FileStream.clear();
        if( Exists() ) {
            m_FileStream.open( file_path.c_str(),
                               ios::out | ios::binary | ios::app );
        } else {
            m_FileStream.open( file_path.c_str(), ios::out | ios::binary );
            LOG_POST( Info << "Chunk file " << file_path
                      << " does not exist.  Creating." );
        }
    }

    if ( ! m_FileStream ) {
        int saved_errno = NCBI_ERRNO_CODE_WRAPPER();
        string error_string =
            "Unable to open a chunk file for writing at " + file_path ;
        error_string += " (errno = " + NStr::NumericToString( saved_errno ) + ": ";
        error_string += string( NCBI_ERRNO_STR_WRAPPER( saved_errno ) +
                                string( ")" ) );
        LOG_POST( Error << error_string );
        NCBI_THROW( CASNCacheException, eCantOpenChunkFile, error_string );
    }
}


void    CChunkFile::OpenForRead( const string & root_path,
                                 unsigned int chunk )
{
    if (!root_path.empty() && m_OpenFileRootPath != root_path) {
        m_OpenFileRootPath = root_path;
        m_ChunkSerialNum = 1;
        m_ChunkSize = -1;
    }
    if (chunk) {
        m_ChunkSerialNum = chunk;
    }
    string file_path = s_MakeChunkFileName( m_OpenFileRootPath, m_ChunkSerialNum );
    if ( file_path != GetPath() ) {
        Reset( file_path );
    
        if( Exists() ) {
            m_FileStream.close();
            m_FileStream.clear();

            m_FileStream.open( file_path.c_str(), ios::in | ios::binary );
            if ( ! m_FileStream ) {
                int saved_errno = NCBI_ERRNO_CODE_WRAPPER();
                string error_string =
                    "Unable to open a chunk file for reading at " + file_path ;
                error_string += " (errno = " +
                    NStr::NumericToString( saved_errno ) + ": ";
                error_string += string( NCBI_ERRNO_STR_WRAPPER( saved_errno ) +
                                        string( ")" ) );
                LOG_POST( Error << error_string );
                NCBI_THROW( CASNCacheException, eCantOpenChunkFile,
                            error_string );
            }
        } else {
            string error_string = "Tried to read nonexistant chunk file at "
                + file_path;
            LOG_POST( Error << error_string );
            NCBI_THROW( CASNCacheException, eCantOpenChunkFile, error_string );
        }
    }
}


void    CChunkFile::Write( const CCache_blob & cache_blob )
{
    CObjectOStreamAsnBinary asn_stream( m_FileStream );
    asn_stream << cache_blob;
    asn_stream.Flush();
    m_ChunkSize += asn_stream.GetStreamPos();
}


void    CChunkFile::RawWrite( const char * raw_data, size_t raw_data_size )
{
    m_FileStream.write( raw_data, raw_data_size );
    m_ChunkSize += raw_data_size;
}


void    CChunkFile::Read( CCache_blob & target_blob, streampos offset,
                          size_t blob_size )
{
    m_Buffer.clear();
    m_Buffer.resize( blob_size );

    m_FileStream.seekg( offset );
    m_FileStream.read( &m_Buffer[0], blob_size );

    CObjectIStreamAsnBinary asn_stream( &m_Buffer[0], blob_size );
    asn_stream >> target_blob;
}


void    CChunkFile::RawRead( streampos offset, char * raw_data, size_t raw_data_size )
{
    if (raw_data_size > static_cast< size_t >(std::numeric_limits< streamsize >::max())) {
        NCBI_THROW(CException, eUnknown,
                   "CChunkFile::RawRead(): " +
                   GetPath() +
                   ": requested a larger than supported number of bytes: " +
                    NStr::NumericToString(raw_data_size));
    }
    streamsize size(raw_data_size);
    m_FileStream.seekg( offset );
    m_FileStream.read( raw_data, size );
    if ( m_FileStream.gcount() != size ) {
        NCBI_THROW(CException, eUnknown,
                   "CChunkFile::RawRead(): " +
                   GetPath() +
                   ": failed to read specified number of bytes: got " +
                   NStr::Int8ToString(m_FileStream.gcount()) + ", expected " +
                   NStr::UInt8ToString(raw_data_size) +
                   " (offset=" + NStr::UInt8ToString(offset) + ")");
    }
}


size_t    CChunkFile::Append( const string & root_path, const CFile & input_chunk_file,
                                Uint8 input_offset )
{
    if (m_OpenFileRootPath != root_path) {
        m_OpenFileRootPath = root_path;
        m_ChunkSerialNum = 1;
        m_ChunkSize = -1;
    }
    if (m_ChunkSize < 0) {
        Uint8 new_chunk_size;
        m_ChunkSerialNum = s_FindNextChunk( m_OpenFileRootPath, m_ChunkSerialNum, new_chunk_size );
        m_ChunkSize = new_chunk_size;
    } else if (m_ChunkSize > m_kMaxChunkSize) {
        ++m_ChunkSerialNum;
        m_ChunkSize = 0;
    }
    string file_path = s_MakeChunkFileName( m_OpenFileRootPath, m_ChunkSerialNum );
    Reset( file_path );
    
    m_FileStream.open( file_path.c_str(), ios::app | ios::out | ios::binary );
    CNcbiIfstream   input_chunk_stream( input_chunk_file.GetPath().c_str(), ios::in | ios::binary );
    input_chunk_stream.seekg( input_offset );
    if (! NcbiStreamCopy( m_FileStream, input_chunk_stream ) ) {
        LOG_POST( Error << "Append of " << input_chunk_file.GetPath() << " to " << file_path
                    << " at offset " << input_offset << " failed." );
        return 0;
    }

    m_ChunkSize += input_chunk_file.GetLength() - input_offset;
    
    return  m_ChunkSerialNum;
}


string    CChunkFile::s_MakeChunkFileName( const string & root_path, unsigned int serial_number )
{
    ostringstream  serial_number_stream;
    serial_number_stream << setw( 5 ) << setfill( '0' ) << serial_number;
    return  CDirEntry::ConcatPath( root_path, 
        NASNCacheFileName::GetChunkPrefix() + serial_number_stream.str() );
}


unsigned int  CChunkFile::s_FindNextChunk( const string & root_path,
                                           unsigned int serial_num,
                                           Uint8 &size )
{
    while( true ) {
        CFile   chunk_file( s_MakeChunkFileName( root_path, serial_num ) );
        if ( ! chunk_file.Exists() )
        {
            size = 0;
            break;
        } else if ( (size = chunk_file.GetLength()) < m_kMaxChunkSize )
        {
            break;
        }
        ++serial_num;
    }
    
    return  serial_num;
}


unsigned int  CChunkFile::s_FindLastChunk( const string & root_path )
{
    unsigned int serial_num = 1;
    while( true ) {
        CFile   chunk_file( s_MakeChunkFileName( root_path, serial_num ) );
        if ( ! chunk_file.Exists() ) {
            break;
        }
        ++serial_num;
    }
    
    return  serial_num - 1;
}


END_NCBI_SCOPE

