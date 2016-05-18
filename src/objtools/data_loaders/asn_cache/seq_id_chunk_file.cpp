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

#include <objects/seq/Bioseq.hpp>

#include <objtools/data_loaders/asn_cache/file_names.hpp>
#include <objtools/data_loaders/asn_cache/asn_cache_exception.hpp>
#include <objtools/data_loaders/asn_cache/seq_id_chunk_file.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

void    CSeqIdChunkFile::OpenForWrite( const string & root_path )
{
    if (!root_path.empty() && m_OpenFileRootPath != root_path) {
        m_OpenFileRootPath = root_path;
        if ( m_FileStream.is_open() ) {
            m_FileStream.close();
        }
        string file_path = NASNCacheFileName::GetSeqIdChunk(root_path);
        Reset( file_path );
    }
    if (!m_FileStream.is_open()) {
        if( Exists() ) {
            m_FileStream.open( GetPath().c_str(),
                               ios::out | ios::binary | ios::app );
        } else {
            m_FileStream.open( GetPath().c_str(), ios::out | ios::binary );
            LOG_POST( Info << "SeqId chunk file " << GetPath()
                      << " does not exist.  Creating." );
        }
    }

    if ( ! m_FileStream ) {
        int saved_errno = NCBI_ERRNO_CODE_WRAPPER();
        string error_string = "Unable to open a seqid chunk file for writing at " + GetPath() ;
        error_string += " (errno = " + NStr::NumericToString( saved_errno ) + ": ";
        error_string += string( NCBI_ERRNO_STR_WRAPPER( saved_errno ) + string( ")" ) );
        LOG_POST( Error << error_string );
        NCBI_THROW( CASNCacheException, eCantOpenChunkFile, error_string );
    }
}


void    CSeqIdChunkFile::OpenForRead( const string & root_path )
{
    if (!root_path.empty() && m_OpenFileRootPath != root_path) {
        m_OpenFileRootPath = root_path;
        if ( m_FileStream.is_open() ) {
            m_FileStream.close();
        }
        string file_path = NASNCacheFileName::GetSeqIdChunk(root_path);
        Reset( file_path );
    }
    
    if (! m_FileStream.is_open() ) {
        if( Exists() ) {
            m_FileStream.open( GetPath().c_str(), ios::in | ios::binary );
            if ( ! m_FileStream ) {
                int saved_errno = NCBI_ERRNO_CODE_WRAPPER();
                string error_string = "Unable to open a seqid chunk file for reading at " + GetPath() ;
                error_string += " (errno = " + NStr::NumericToString( saved_errno ) + ": ";
                error_string += string( NCBI_ERRNO_STR_WRAPPER( saved_errno ) + string( ")" ) );
                LOG_POST( Error << error_string );
                NCBI_THROW( CASNCacheException, eCantOpenChunkFile, error_string );
            }
        } else {
            string error_string = "Tried to read nonexistant seqid chunk file at "
                + GetPath();
            LOG_POST( Error << error_string );
            NCBI_THROW( CASNCacheException, eCantOpenChunkFile, error_string );
        }
    }
}


void    CSeqIdChunkFile::Write( const CBioseq::TId & seq_ids )
{
    CObjectOStreamAsnBinary asn_stream( m_FileStream );
    ITERATE(CBioseq::TId, it, seq_ids)
        asn_stream << **it;
    asn_stream.Flush();
}


void    CSeqIdChunkFile::Write( const vector<CSeq_id_Handle> & seq_ids )
{
    CObjectOStreamAsnBinary asn_stream( m_FileStream );
    ITERATE(vector<CSeq_id_Handle>, it, seq_ids)
        asn_stream << *it->GetSeqId();
    asn_stream.Flush();
}


void    CSeqIdChunkFile::RawWrite( const char * raw_seq_ids, size_t raw_seq_ids_size )
{
    m_FileStream.write( raw_seq_ids, raw_seq_ids_size );
}


void    CSeqIdChunkFile::Read( vector<CSeq_id_Handle> & target, streampos offset, size_t seq_ids_size )
{
    m_Buffer.clear();
    m_Buffer.resize( seq_ids_size );

    m_FileStream.seekg( offset );
    m_FileStream.read( &m_Buffer[0], seq_ids_size );

    CObjectIStreamAsnBinary asn_stream( &m_Buffer[0], seq_ids_size );
    CSeq_id id;
    while(asn_stream.GetStreamPos() < seq_ids_size){
        asn_stream >> id;
        target.push_back(CSeq_id_Handle::GetHandle(id));
    }
}


void    CSeqIdChunkFile::RawRead( streampos offset, char * raw_seq_ids, size_t raw_seq_ids_size )
{
    if (raw_seq_ids_size > static_cast< size_t >(std::numeric_limits< streamsize >::max())) {
        NCBI_THROW(CException, eUnknown,
                   "CSeqIdChunkFile::RawRead(): "
                   "requested a larger than supported number of bytes: " +
                    NStr::NumericToString(raw_seq_ids_size));
    }
    streamsize size(raw_seq_ids_size);
    m_FileStream.seekg( offset );
    m_FileStream.read( raw_seq_ids, size );
    if ( m_FileStream.gcount() != size ) {
        NCBI_THROW(CException, eUnknown,
                   "CChunkFile::RawRead(): "
                   "failed to read specified number of bytes: got " +
                   NStr::Int8ToString(m_FileStream.gcount()) + ", expected " +
                   NStr::UInt8ToString(raw_seq_ids_size) +
                   " (offset=" + NStr::UInt8ToString(offset) + ")");
    }
}


bool    CSeqIdChunkFile::Append( const string & root_path, const CFile & input_seq_id_chunk_file,
                                Uint8 input_offset )
{
    string file_path = NASNCacheFileName::GetSeqIdChunk(root_path);
    Reset( file_path );
    
    m_FileStream.open( file_path.c_str(), ios::app | ios::out | ios::binary );
    CNcbiIfstream   input_seq_id_chunk_stream( input_seq_id_chunk_file.GetPath().c_str(), ios::in | ios::binary );
    input_seq_id_chunk_stream.seekg( input_offset );
    if (! NcbiStreamCopy( m_FileStream, input_seq_id_chunk_stream ) ) {
        LOG_POST( Error << "Append of " << input_seq_id_chunk_file.GetPath() << " to " << file_path
                    << " at offset " << input_offset << " failed." );
        return false;
    }
    return true;
}


END_NCBI_SCOPE

