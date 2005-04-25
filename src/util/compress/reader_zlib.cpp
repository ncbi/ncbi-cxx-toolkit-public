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


/////////////////////////////////////////////////////////////////////////////
// CDynamicCharArray
/////////////////////////////////////////////////////////////////////////////


CDynamicCharArray::CDynamicCharArray(size_t size)
    : m_Size(size), m_Array(size? new char[size]: 0)
{
}


CDynamicCharArray::~CDynamicCharArray(void)
{
    delete[] m_Array;
}


char* CDynamicCharArray::Alloc(size_t size)
{
    if ( size > m_Size ) {
        delete[] m_Array;
        if ( m_Size == 0 ) {
            m_Size = kInititialSize;
        }
        while ( size > m_Size ) {
            m_Size <<= 1;
            if ( m_Size == 0 ) { // overflow
                m_Size = size;
            }
        }
        m_Array = new char[m_Size];
    }
    return m_Array;
}


/////////////////////////////////////////////////////////////////////////////
// CResultZBtSrcX
/////////////////////////////////////////////////////////////////////////////


class NCBI_XUTIL_EXPORT CResultZBtSrcX
{
public:
    CResultZBtSrcX(CByteSourceReader* reader);
    ~CResultZBtSrcX(void);

    size_t Read(char* buffer, size_t bufferLength);
    void ReadLength(void);

    size_t x_Read(char* buffer, size_t bufferLength);

    enum {
        kMax_UncomprSize = 1024*1024,
        kMax_ComprSize = 1024*1024
    };

private:
    CResultZBtSrcX(const CResultZBtSrcX&);
    const CResultZBtSrcX& operator=(const CResultZBtSrcX&);

    CRef<CByteSourceReader> m_Src;
    CDynamicCharArray m_Buffer;
    size_t            m_BufferPos;
    size_t            m_BufferEnd;
    CZipCompression   m_Decompressor;
    CDynamicCharArray m_Compressed;
};


CResultZBtSrcX::CResultZBtSrcX(CByteSourceReader* src)
    : m_Src(src), m_BufferPos(0), m_BufferEnd(0)
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
    size_t compr_size = 0;
    for ( size_t i = 0; i < 4; ++i ) {
        compr_size = (compr_size<<8) | (unsigned char)header[i];
    }
    size_t uncompr_size = 0;
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
    if ( x_Read(m_Compressed.Alloc(compr_size), compr_size) != compr_size ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "Compressed data is not complete");
    }
    m_BufferPos = m_BufferEnd;
    if ( !m_Decompressor.DecompressBuffer(m_Compressed.Alloc(compr_size),
                                          compr_size,
                                          m_Buffer.Alloc(uncompr_size),
                                          uncompr_size,
                                          &uncompr_size) ) {
        NCBI_THROW(CCompressionException, eCompression,
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
    memcpy(buffer, m_Buffer.At(m_BufferPos), cnt);
    m_BufferPos += cnt;
    return cnt;
}


/////////////////////////////////////////////////////////////////////////////
// CNlmZipBtRdr
/////////////////////////////////////////////////////////////////////////////


CNlmZipBtRdr::CNlmZipBtRdr(CByteSourceReader* src)
    : m_Src(src), m_Type(eType_unknown)
{
}


CNlmZipBtRdr::~CNlmZipBtRdr()
{
}


size_t CNlmZipBtRdr::Read(char* buffer, size_t buffer_length)
{
    EType type = m_Type;
    if ( type == eType_plain ) {
        return m_Src->Read(buffer, buffer_length);
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


bool CNlmZipBtRdr::Pushback(const char* data, size_t size)
{
    if ( m_Type == eType_plain ) {
        return m_Src->Pushback(data, size);
    }
    return CByteSourceReader::Pushback(data, size);
}


/////////////////////////////////////////////////////////////////////////////
// CNlmZipReader
/////////////////////////////////////////////////////////////////////////////


CNlmZipReader::CNlmZipReader(IReader* reader,
                             TOwnership own,
                             EHeader header)
    : m_Reader(reader), m_Own(own), m_Header(header),
      m_BufferPos(0), m_BufferEnd(0)
{
    if ( header == eHeaderNone ) {
        x_StartDecompressor();
    }
}


CNlmZipReader::~CNlmZipReader(void)
{
    if ( m_Own && fOwnReader ) {
        delete m_Reader;
    }
}


ERW_Result CNlmZipReader::PendingCount(size_t* count)
{
    *count = m_BufferEnd - m_BufferPos;
    return eRW_Success;
}


ERW_Result CNlmZipReader::Read(void* buffer, size_t count, size_t* bytes_read)
{
    if ( count == 0 ) {
        if ( bytes_read ) {
            *bytes_read = 0;
        }
        return eRW_Success;
    }

    if ( m_Header != eHeaderNone ) {
        if ( count >= size_t(kHeaderSize) ) {
            // we can use buffer to read header
            size_t bytes = x_ReadZipHeader(static_cast<char*>(buffer));
            if ( bytes ) {
                if ( bytes_read ) {
                    *bytes_read = bytes;
                }
                return eRW_Success;
            }
        }
        else {
            // we have to allocate buffer
            size_t bytes = x_ReadZipHeader(m_Buffer.Alloc(kHeaderSize));
            if ( bytes ) {
                // setup buffer to read
                m_BufferPos = 0;
                m_BufferEnd = bytes;
            }
        }
    }

    
    while ( m_BufferPos == m_BufferEnd ) {
        _ASSERT(m_Header == eHeaderNone);
        if ( !m_Decompressor.get() ) {
            return m_Reader->Read(buffer, count, bytes_read);
        }
        
        ERW_Result result = x_DecompressBuffer();
        if ( result != eRW_Success ) {
            return result;
        }
    }

    count = min(m_BufferEnd - m_BufferPos, count);
    memcpy(buffer, m_Buffer.At(m_BufferPos), count);
    if ( bytes_read ) {
        *bytes_read = count;
    }
    m_BufferPos += count;
    return eRW_Success;
}


size_t CNlmZipReader::x_ReadZipHeader(char* buffer)
{
    const char* header = buffer;
    size_t header_read = 0;
    while ( header_read < size_t(kHeaderSize) ) {
        size_t cur_cnt = 1;
        ERW_Result result = m_Reader->Read(buffer, cur_cnt, &cur_cnt);
        if ( result != eRW_Success || cur_cnt == 0 ) {
            // not enough bytes
            x_StartPlain();
            return header_read;
        }
        buffer += cur_cnt;
        header_read += cur_cnt;
        if ( memcmp(header, "ZIP", header_read) != 0 ) {
            // header is not "ZIP"
            x_StartPlain();
            return header_read;
        }
    }
    // "ZIP" - skip it
    m_Header = eHeaderNone;
    // open decompressor
    x_StartDecompressor();
    return 0;
}


void CNlmZipReader::x_StartPlain(void)
{
    if ( m_Header == eHeaderAlways ) {
        NCBI_THROW(CCompressionException, eCompression,
                   "No 'ZIP' header in NLMZIP stream");
    }
    m_Header = eHeaderNone;
}


void CNlmZipReader::x_StartDecompressor(void)
{
    m_Decompressor.reset(new CZipCompression);
    m_Header = eHeaderNone;
}


ERW_Result CNlmZipReader::x_DecompressBuffer(void)
{
    static const size_t kLengthsLength = 8;
    static const size_t kMax_UncomprSize = 1024*1024;
    static const size_t kMax_ComprSize = 1024*1024;

    char header[kLengthsLength];
    size_t bytes;
    ERW_Result result = x_Read(header, kLengthsLength, &bytes);
    if ( (result == eRW_Success || result == eRW_Eof) && bytes == 0 ) {
        return eRW_Eof;
    }
    if ( result != eRW_Success || bytes != kLengthsLength ) {
        return eRW_Error;
    }
    size_t compr_size = 0;
    for ( size_t i = 0; i < 4; ++i ) {
        compr_size = (compr_size<<8) | (unsigned char)header[i];
    }
    size_t uncompr_size = 0;
    for ( size_t i = 4; i < 8; ++i ) {
        uncompr_size = (uncompr_size<<8) | (unsigned char)header[i];
    }

    if ( compr_size > kMax_ComprSize ) {
        return eRW_Error;
    }
    if ( uncompr_size > kMax_UncomprSize ) {
        return eRW_Error;
    }
    if ( x_Read(m_Compressed.Alloc(compr_size),
                compr_size, &bytes) != eRW_Success || bytes != compr_size ) {
        return eRW_Error;
    }
    if ( !m_Decompressor->DecompressBuffer(m_Compressed.At(0),
                                           compr_size,
                                           m_Buffer.Alloc(uncompr_size),
                                           uncompr_size,
                                           &uncompr_size) ) {
        return eRW_Error;
    }
    m_BufferEnd = uncompr_size;
    m_BufferPos = 0;
    return eRW_Success;
}


ERW_Result CNlmZipReader::x_Read(char* buffer,
                                 size_t count,
                                 size_t* bytes_read)
{
    *bytes_read = 0;
    while ( count ) {
        size_t cnt;
        ERW_Result result = m_Reader->Read(buffer, count, &cnt);
        *bytes_read += cnt;
        buffer += cnt;
        count -= cnt;
        if ( result != eRW_Success ) {
            return result;
        }
        if ( cnt == 0 ) {
            break;
        }
    }
    return eRW_Success;
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/04/25 19:05:24  ivanov
 * Fixed compilation warnings on 64-bit Worshop compiler
 *
 * Revision 1.7  2005/03/14 17:32:22  vasilche
 * Removed obsolete methods and data members.
 *
 * Revision 1.6  2005/03/10 20:51:49  vasilche
 * Implemented IReader filter for NlmZip format.
 *
 * Revision 1.5  2004/08/09 15:59:07  vasilche
 * Implemented CNlmZipBtRdr::Pushback() for uncompressed data.
 *
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
