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

#include <ncbi_pch.hpp>
#include <util/compress/reader_zlib.hpp>
#include <util/compress/compress.hpp>

#include <corelib/ncbitime.hpp>
#include <util/compress/zlib.hpp>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class NCBI_XUTIL_EXPORT CResultZBtSrcX
{
public:
    CResultZBtSrcX(CByteSourceReader* reader);
    ~CResultZBtSrcX(void);

    size_t Read(char* buffer, size_t bufferLength);
    void ReadLength(void);

    size_t x_Read(char* buffer, size_t bufferLength);

    size_t GetCompressedSize(void) const
        {
            return m_CompressedSize;
        }

    double GetDecompressionTime(void) const
        {
            return m_DecompressionTime;
        }

    enum {
        kMax_UncomprSize = 1024*1024,
        kMax_ComprSize = 1024*1024
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
    size_t            m_CompressedSize;
    double            m_DecompressionTime;
};


CNlmZipBtRdr::CNlmZipBtRdr(CByteSourceReader* src)
    : m_Src(src), m_Type(eType_unknown),
      m_TotalReadTime(0)
{
}


CNlmZipBtRdr::~CNlmZipBtRdr()
{
}


size_t CNlmZipBtRdr::Read(char* buffer, size_t buffer_length)
{
    CStopWatch sw;
    if ( 1 ) {
        sw.Start();
    }
    EType type = m_Type;
    if ( type == eType_plain ) {
        size_t ret = m_Src->Read(buffer, buffer_length);
        if ( 1 ) {
            m_TotalReadTime += sw.Elapsed();
        }
        return ret;
    }

    if ( type == eType_unknown ) {
        const size_t kHeaderSize = 4;
        if ( buffer_length < kHeaderSize) {
            NCBI_THROW(CCompressionException, eCompression,
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
                _TRACE("CNlmZipBtRdr: non-ZIP: " << got_already);
                m_Type = eType_plain;
                if ( 1 ) {
                    m_TotalReadTime += sw.Elapsed();
                }
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

    size_t ret = m_Decompressor->Read(buffer, buffer_length);
    if ( 1 ) {
        m_TotalReadTime += sw.Elapsed();
    }
    return ret;
}


size_t CNlmZipBtRdr::GetCompressedSize(void) const
{
    return m_Decompressor.get()? m_Decompressor->GetCompressedSize(): 0;
}


double CNlmZipBtRdr::GetDecompressionTime(void) const
{
    return m_Decompressor.get()? m_Decompressor->GetDecompressionTime(): 0.;
}


CResultZBtSrcX::CResultZBtSrcX(CByteSourceReader* src)
    : m_Src(src), m_BufferPos(0), m_BufferEnd(0),
      m_CompressedSize(0), m_DecompressionTime(0)
{
    m_Decompressor.SetFlags(m_Decompressor.fCheckFileHeader |
                            m_Decompressor.GetFlags());
}


CResultZBtSrcX::~CResultZBtSrcX(void)
{
}


size_t CResultZBtSrcX::x_Read(char* buffer, size_t buffer_length)
{
    size_t ret = 0;
    while ( buffer_length > 0 ) {
        size_t cnt = m_Src->Read(buffer, buffer_length);
        if ( cnt == 0 ) {
            break;
        }
        else {
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
        NCBI_THROW(CCompressionException, eCompression,
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
        NCBI_THROW(CCompressionException, eCompression,
                   "Compressed size is too large");
    }
    if ( uncompr_size > kMax_UncomprSize ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "Uncompressed size is too large");
    }
    m_Compressed.reserve(compr_size);
    if ( x_Read(&m_Compressed[0], compr_size) != compr_size ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "Compressed data is not complete");
    }
    m_BufferPos = m_BufferEnd;
    m_Buffer.reserve(uncompr_size);
    CStopWatch sw;
    if ( 1 ) {
        sw.Start();
    }
    if ( !m_Decompressor.DecompressBuffer(&m_Compressed[0], compr_size,
                                          &m_Buffer[0], uncompr_size,
                                          &uncompr_size) ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "Decompression failed");
    }
    if ( 1 ) {
        m_DecompressionTime += sw.Elapsed();
    }
    m_BufferEnd = uncompr_size;
    m_BufferPos = 0;
    m_CompressedSize += compr_size + 8;
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
 * ===========================================================================
 * $Log$
 * Revision 1.4  2004/05/17 21:07:25  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.3  2004/05/10 18:41:47  vasilche
 * Added SetFlags() to skip gzip header.
 *
 * Revision 1.2  2004/04/07 16:16:17  ivanov
 * Cosmeic changes. Fixed header and footer.
 *
 * Revision 1.1  2003/12/23 17:02:09  kuznets
 * Moved from objmgr to util/compress
 *
 * Revision 1.10  2003/11/26 17:55:59  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.9  2003/10/21 14:27:35  vasilche
 * Added caching of gi -> sat,satkey,version resolution.
 * SNP blobs are stored in cache in preprocessed format (platform dependent).
 * Limit number of connections to GenBank servers.
 * Added collection of ID1 loader statistics.
 *
 * Revision 1.8  2003/10/14 22:36:08  ucko
 * Fix typo in last log message
 *
 * Revision 1.7  2003/10/14 22:35:51  ucko
 * Remove initialization of CNlmZipBtRdr::m_CompressedSize (neither
 * declared nor used anywhere)
 *
 * Revision 1.6  2003/10/14 21:06:25  vasilche
 * Fixed compression statistics.
 * Disabled caching of SNP blobs.
 *
 * Revision 1.5  2003/10/14 18:59:55  vasilche
 * Temporarily remove collection of compression statistics.
 *
 * Revision 1.4  2003/10/14 18:31:55  vasilche
 * Added caching support for SNP blobs.
 * Added statistics collection of ID1 connection.
 *
 * Revision 1.3  2003/09/05 17:29:40  grichenk
 * Structurized Object Manager exceptions
 *
 * Revision 1.2  2003/07/24 20:35:42  vasilche
 * Added private constructor to make MSVC-DLL happy.
 *
 * Revision 1.1  2003/07/24 19:28:09  vasilche
 * Implemented SNP split for ID1 loader.
 *
 * ===========================================================================
 */
