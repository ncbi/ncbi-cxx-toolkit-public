/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: byte reader with gzip decompression
*
*/

#include <objmgr/impl/reader_zlib.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <util/compress/zlib.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CResultZBtSrc::CResultZBtSrc(CByteSourceReader* src)
    : m_Src(src)
{
}


CResultZBtSrc::~CResultZBtSrc()
{
}



CRef<CByteSourceReader> CResultZBtSrc::Open(void)
{
    return CRef<CByteSourceReader>(new CResultZBtSrcRdr(m_Src));
}


class NCBI_XOBJMGR_EXPORT CResultZBtSrcX
{
public:
    CResultZBtSrcX(CByteSourceReader* reader);
    ~CResultZBtSrcX(void);

    size_t Read(char* buffer, size_t bufferLength);
    void ReadLength(void);

    size_t x_Read(char* buffer, size_t bufferLength);

    enum {
        kMax_UncomprSize = 128*1024,
        kMax_ComprSize = 128*1024
    };

private:
    CResultZBtSrcX(const CResultZBtSrcX&);
    const CResultZBtSrcX& operator=(const CResultZBtSrcX&);

    CRef<CByteSourceReader> m_Src;
    vector<char>      m_Buffer;
    size_t            m_BufferPos;
    size_t            m_BufferEnd;
    CZipCompression   m_Decompressor;
    vector<char>      m_Compressed;
};


CResultZBtSrcRdr::CResultZBtSrcRdr(CByteSourceReader* src)
    : m_Src(src), m_Type(eType_unknown)
{
}


CResultZBtSrcRdr::~CResultZBtSrcRdr()
{
}


size_t CResultZBtSrcRdr::Read(char* buffer, size_t buffer_length)
{
    EType type = m_Type;
    if ( type == eType_plain ) {
        return m_Src->Read(buffer, buffer_length);
    }

    if ( type == eType_unknown ) {
        const size_t kHeaderSize = 4;
        if ( buffer_length < kHeaderSize) {
            NCBI_THROW(CLoaderException, eCompressionError,
                       "Too small buffer to determine compression type");
        }
        const char* header = buffer;
        size_t got_already = 0;
        do {
            size_t need_more = kHeaderSize - got_already;
            size_t cnt = m_Src->Read(buffer, need_more);
            buffer += cnt;
            got_already += cnt;
            buffer_length -= cnt;
            if ( cnt == 0 || memcmp(header, "ZIP", got_already) != 0 ) {
                // too few bytes - assume non "ZIP"
                // or header is not "ZIP"
                _TRACE("CResultZBtSrcRdr: non-ZIP: " << got_already);
                m_Type = eType_plain;
                return got_already;
            }
        } while ( got_already != kHeaderSize );
        // "ZIP"
        m_Type = eType_zlib;
        // reset buffer
        buffer -= kHeaderSize;
        buffer_length += kHeaderSize;
        m_Decompressor.reset(new CResultZBtSrcX(m_Src));
    }

    return m_Decompressor->Read(buffer, buffer_length);
}


CResultZBtSrcX::CResultZBtSrcX(CByteSourceReader* src)
    : m_Src(src), m_BufferPos(0), m_BufferEnd(0)
{
}


CResultZBtSrcX::~CResultZBtSrcX(void)
{
}


size_t CResultZBtSrcX::x_Read(char* buffer, size_t buffer_length)
{
    size_t ret = 0;
    while ( buffer_length > 0 ) {
#if 0 && defined _DEBUG
        NcbiCout << "Ask Data: " << buffer_length << NcbiEndl;
#endif
        size_t cnt = m_Src->Read(buffer, buffer_length);
        if ( cnt == 0 ) {
#if 0 && defined _DEBUG
            NcbiCout << "No Data" << NcbiEndl;
#endif
            break;
        }
        else {
#if 0 && defined _DEBUG
            NcbiCout << "Got Data "<<cnt<<" :";
            for ( size_t i = 0; i < cnt; ++i ) {
                NcbiCout << ' ' <<
                    "0123456789abcdef"[(buffer[i]>>4)&15] << 
                    "0123456789abcdef"[(buffer[i])&15];
            }
            NcbiCout << NcbiEndl;
#endif
            buffer_length -= cnt;
            buffer += cnt;
            ret += cnt;
        }
    }
    return ret;
}


void CResultZBtSrcX::ReadLength(void)
{
    char header[8];
    if ( x_Read(header, 8) != 8 ) {
        NCBI_THROW(CLoaderException, eCompressionError,
                   "Too few header bytes");
    }
    unsigned int compr_size = 0;
    for ( size_t i = 0; i < 4; ++i ) {
        compr_size = (compr_size<<8) | (unsigned char)header[i];
    }
    unsigned int uncompr_size = 0;
    for ( size_t i = 4; i < 8; ++i ) {
        uncompr_size = (uncompr_size<<8) | (unsigned char)header[i];
    }

    if ( compr_size > kMax_ComprSize ) {
        NCBI_THROW(CLoaderException, eCompressionError,
                   "Compressed size is too large");
    }
    if ( uncompr_size > kMax_UncomprSize ) {
        NCBI_THROW(CLoaderException, eCompressionError,
                   "Uncompressed size is too large");
    }
    m_Compressed.reserve(compr_size);
    if ( x_Read(&m_Compressed[0], compr_size) != compr_size ) {
        NCBI_THROW(CLoaderException, eCompressionError,
                   "Compressed data is not complete");
    }
    m_BufferPos = m_BufferEnd;
    m_Buffer.reserve(uncompr_size);
    if ( !m_Decompressor.DecompressBuffer(&m_Compressed[0], compr_size,
                                          &m_Buffer[0], uncompr_size,
                                          &uncompr_size) ) {
        NCBI_THROW(CLoaderException, eCompressionError,
                   "Decompression failed");
    }
    m_BufferEnd = uncompr_size;
    m_BufferPos = 0;
}


size_t CResultZBtSrcX::Read(char* buffer, size_t buffer_length)
{
    while ( m_BufferPos >= m_BufferEnd ) {
        ReadLength();
    }
    size_t cnt = min(buffer_length, m_BufferEnd - m_BufferPos);
    memcpy(buffer, &m_Buffer[m_BufferPos], cnt);
    m_BufferPos += cnt;
    return cnt;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* $Log$
* Revision 1.3  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.2  2003/07/24 20:35:42  vasilche
* Added private constructor to make MSVC-DLL happy.
*
* Revision 1.1  2003/07/24 19:28:09  vasilche
* Implemented SNP split for ID1 loader.
*
*/
