#ifndef ASN_CACHE_CHUNK_FILE_HPP__
#define ASN_CACHE_CHUNK_FILE_HPP__
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
 * Represent a chunk file -- a part of the ASN Cache ISAM storage model.  The
 * ISAM storage is broken into file-size chunks to prevent a block from
 * getting too large.  Chunks are serially numbered and cleverly named "chunk."
 * The class keeps track of the currently used chunk and maintains a stream
 * to it.
 */

#include <string>
#include <fstream>

#include <corelib/ncbifile.hpp>

#include <util/simple_buffer.hpp>

#include <serial/objostrasnb.hpp>

#include <objtools/data_loaders/asn_cache/Cache_blob.hpp>
#include <objtools/data_loaders/asn_cache/file_names.hpp>

BEGIN_NCBI_SCOPE
using objects::CCache_blob;

class CChunkFile : public CFile
{
public:
    CChunkFile( const std::string &root_path = "", unsigned int chunk = 1)
    : m_ChunkSerialNum(chunk)
    , m_ChunkSize(-1)
    , m_OpenFileRootPath(root_path)
    { x_ReserveBufferSpace(); }

    void    OpenForWrite( const std::string & root_path  = "");
    void    OpenForRead( const std::string & root_path  = "",
                         unsigned int chunk = 0);
    void    Write( const CCache_blob & cache_blob );
    void    RawWrite( const char * raw_blob, size_t raw_blob_size );
    void    Read( CCache_blob & target, std::streampos offset, size_t blob_size );
    void    RawRead( std::streampos offset, char * raw_blob, size_t raw_blob_size );
    size_t  Append( const string & root_path,
                    const CFile & input_chunk_file,
                    Uint8 input_offset = 0 );
    Int8    GetOffset() { return m_FileStream.tellp(); }
    unsigned int    GetChunkSerialNum() const { return m_ChunkSerialNum; }

    static std::string s_MakeChunkFileName( const std::string & root_path, unsigned int serial_num );
    static unsigned int    s_FindNextChunk( const std::string & root_path,
                                            unsigned int serial_num,
                                            Uint8 &size );
    static unsigned int    s_FindLastChunk( const std::string & root_path );

private:
    unsigned int    m_ChunkSerialNum;
    Int8            m_ChunkSize;
    CNcbiFstream    m_FileStream;
    std::string     m_OpenFilePath;
    std::string     m_OpenFileRootPath;
    CSimpleBufferT<char>   m_Buffer;

    void    x_ReserveBufferSpace() { m_Buffer.reserve(128 * 1024); }

    static const Int8  m_kMaxChunkSize = 4 * 1024 * 1024 * 1024LL;
};

END_NCBI_SCOPE
#endif  // ASN_CACHE_CHUNK_FILE_HPP__

