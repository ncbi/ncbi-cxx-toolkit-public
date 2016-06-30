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
 * Authors:  Eugene Vasilchenko
 *
 * File Description:
 *   Access to BGZF files (block GZip file)
 *
 */

#include <ncbi_pch.hpp>
#include <sra/readers/bam/bgzf.hpp>
#include <util/util_exception.hpp>
#include <util/checksum.hpp>
#include <util/compress/zlib/zlib.h>

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   BAM2Graph
//NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;

static const bool kCheckBlockCRC32 = true;
static const Uint8 kSegmentSize = 4<<20;

const char* CBGZFException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eOtherError:   return "eOtherError";
    case eFormatError:  return "eFormatError";
    case eInvalidArg:   return "eInvalidArg";
    default:            return CException::GetErrCodeString();
    }
}


static inline
char* s_Reserve(size_t size, CSimpleBufferT<char>& buffer)
{
    if ( buffer.size() < size ) {
        if ( size < buffer.size()*2 ) {
            size = max(buffer.size()*2, size_t(1024));
        }
        buffer.resize(size);
    }
    return buffer.data();
}


static inline
const char* s_Read(CMemoryFile& in,
                   Uint8 file_pos, size_t len,
                   char* buffer)
{
    char* dst = buffer;
    while ( len ) {
        Uint8 off = file_pos % kSegmentSize;
        off_t seg_start = off_t(file_pos - off);
        const void* ptr = in.GetPtr();
        if ( !ptr || in.GetOffset() != seg_start ) {
            ptr = in.Map(seg_start, kSegmentSize);
        }
        _ASSERT(ptr && in.GetOffset() == seg_start);
        size_t cnt = size_t(min(kSegmentSize - off, Uint8(len)));
        _ASSERT(cnt < 0x10000);
        memcpy(dst, static_cast<const char*>(ptr)+off, cnt);
        len -= cnt;
        file_pos += cnt;
        dst += cnt;
    }
    return buffer;
}


static inline
const char* s_Read(CMemoryFile& in,
                   Uint8 file_pos, size_t len,
                   CSimpleBufferT<char>& buffer)
{
    Uint8 off = file_pos % kSegmentSize;
    off_t seg_start = off_t(file_pos - off);
    const void* ptr = in.GetPtr();
    if ( !ptr || in.GetOffset() != seg_start ) {
        ptr = in.Map(seg_start, kSegmentSize);
    }
    _ASSERT(ptr && in.GetOffset() == seg_start);
    Uint8 avail = kSegmentSize - off;
    if ( len <= avail ) {
        return static_cast<const char*>(ptr) + off;
    }
    char* dst = s_Reserve(len, buffer);
    _ASSERT(avail < 0x10000);
    memcpy(dst, static_cast<const char*>(ptr)+off, avail);
    len -= avail;
    file_pos += avail;
    dst += avail;
    while ( len ) {
        off = file_pos % kSegmentSize;
        seg_start = off_t(file_pos - off);
        ptr = in.Map(seg_start, kSegmentSize);
        _ASSERT(ptr && in.GetOffset() == seg_start);
        avail = kSegmentSize - off;
        size_t cnt = size_t(min(avail, Uint8(len)));
        _ASSERT(cnt < 0x10000);
        memcpy(dst, static_cast<const char*>(ptr)+off, cnt);
        len -= cnt;
        file_pos += cnt;
        dst += cnt;
    }
    return buffer.data();
}


ostream& operator<<(ostream& out, const CBGZFPos& p)
{
    return out << p.GetFileBlockPos() << '.' << p.GetByteOffset();
}


CBGZFFile::CBGZFFile(const string& file_name)
    : m_MemFile(file_name,
                CMemoryFile::eMMP_Read,
                CMemoryFile::eMMS_Shared,
                0, kSegmentSize) // to prevent initial mapping
{
}


CBGZFFile::~CBGZFFile()
{
}


const char* CBGZFFile::Read(CBGZFPos::TFileBlockPos file_pos, size_t size,
                            char* buffer)
{
    return s_Read(m_MemFile, file_pos, size, buffer);
}


const char* CBGZFFile::Read(CBGZFPos::TFileBlockPos file_pos, size_t size,
                            CSimpleBufferT<char>& buffer)
{
    return s_Read(m_MemFile, file_pos, size, buffer);
}


const char* CBGZFFile::ReadBlock(CBGZFPos::TFileBlockPos file_pos,
                                 CBGZFBlockInfo& block_info,
                                 CSimpleBufferT<char>* buffer)
{
    const size_t kHeaderSize = CBGZFBlockInfo::kHeaderSize;
    const size_t kFooterSize = CBGZFBlockInfo::kFooterSize;
    const size_t kMaxDataSize = CBGZFBlockInfo::kMaxDataSize;

    char tmp[kHeaderSize];
    const char* header = Read(file_pos, kHeaderSize, tmp);

    // parse and check GZip header
    if ( header[0] != 31 || header[1] != '\x8b' ||
         header[2] != 8 || header[3] != 4 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") MAGIC: ");
    }
    // 4-7 - modification time
    // 8 - extra flags
    // 9 - OS
    if ( CBGZFFile::MakeUint2(header+10) != 6 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") XLEN: ");
    }
    // extra data
    if ( header[12] != 'B' || header[13] != 'C' ||
         header[14] != 2 || header[15] != 0 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") EXTRA: ");
    }

    CBGZFBlockInfo::TFileBlockSize block_size = CBGZFFile::MakeUint2(header+16)+1;

    if ( block_size <= kHeaderSize + kFooterSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") FILE SIZE: "<<block_size);
    }

    const char* data;
    const char* footer;
    if ( buffer ) {
        data = Read(file_pos + kHeaderSize, block_size - kHeaderSize, *buffer);
        footer = data + block_size - kHeaderSize - kFooterSize;
    }
    else {
        data = 0;
        footer = Read(file_pos + block_size - kFooterSize, kFooterSize, tmp);
    }

    CBGZFBlockInfo::TDataSize data_size = CBGZFFile::MakeUint4(footer+4);

    if ( data_size == 0 || data_size > kMaxDataSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") DATA SIZE: "<<data_size);
    }

    block_info.m_FileBlockPos = file_pos;
    block_info.m_FileBlockSize = block_size;
    block_info.m_CRC32 = CBGZFFile::MakeUint4(footer);
    block_info.m_DataSize = data_size;

    return data;
}


CBGZFStream::CBGZFStream()
    : m_ReadPos(0)
{
}


CBGZFStream::CBGZFStream(CBGZFFile& file)
    : m_ReadPos(0)
{
    Open(file);
}


CBGZFStream::~CBGZFStream()
{
}


void CBGZFStream::Close()
{
    m_File.Reset();
    m_ReadPos = m_BlockInfo.GetDataSize();
}


void CBGZFStream::Open(CBGZFFile& file)
{
    Close();
    m_File.Reset(&file);
    if ( !m_Data.get() ) {
        m_Data.reset(new char[CBGZFBlockInfo::kMaxDataSize]);
    }
}


void CBGZFStream::Seek(CBGZFPos pos)
{
    CBGZFPos::TFileBlockPos file_pos = pos.GetFileBlockPos();
    if ( file_pos != m_BlockInfo.GetFileBlockPos() ||
         m_BlockInfo.GetDataSize() == 0 ) {
        x_ReadBlock(pos.GetFileBlockPos());
    }
    m_ReadPos = pos.GetByteOffset();
    if ( m_ReadPos >= m_BlockInfo.GetDataSize() ) {
        NCBI_THROW_FMT(CBGZFException, eInvalidArg,
                       "Bad BGZF("<<file_pos<<") offset: "<<
                       m_ReadPos<<" vs "<<m_BlockInfo.GetDataSize());
    }
}


void CBGZFStream::x_ReadBlock(CBGZFPos::TFileBlockPos file_pos)
{
    const char* data = m_File->ReadBlock(file_pos, m_BlockInfo, &m_ReadBuffer);
    size_t size = 0;

    {
        const char* src = data;
        size_t src_size = m_BlockInfo.GetCompressedSize();
        char* dst = m_Data.get();
        size_t dst_size = CBGZFBlockInfo::kMaxDataSize;

        z_stream stream;
        stream.next_in = (Bytef*)src;
        stream.avail_in = (uInt)src_size;
        stream.next_out = (Bytef*)dst;
        stream.avail_out = (uInt)dst_size;
        stream.zalloc = (alloc_func)0;
        stream.zfree = (free_func)0;
        
        // Check for source > 64K on 16-bit machine:
        if ( stream.avail_in != src_size ||
             stream.avail_out != dst_size ) {
            NCBI_THROW_FMT(CBGZFException, eInvalidArg,
                           "Bad BGZF("<<file_pos<<") compression sizes: "<<
                           src_size<<" -> "<<dst_size);
        }
        
        int err = inflateInit2(&stream, -15);
        if ( err == Z_OK ) {
            err = inflate(&stream, Z_FINISH);
            if ( err == Z_STREAM_END ) {
                size = stream.total_out;
            }
        }
    }

    if ( size != m_BlockInfo.GetDataSize() ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos<<") decompressed data size: "<<
                       size<<" vs "<<m_BlockInfo.GetDataSize());
    }
    if ( kCheckBlockCRC32 ) {
        CChecksum checksum(CChecksum::eCRC32ZIP);
        checksum.AddChars(m_Data.get(), size);
        if ( checksum.GetChecksum() != m_BlockInfo.GetCRC32() ) {
            NCBI_THROW_FMT(CBGZFException, eFormatError,
                           "Bad BGZF("<<file_pos<<") CRC32: "<<
                           hex<<Uint4(checksum.GetChecksum())<<" vs "<<
                           Uint4(m_BlockInfo.GetCRC32())<<dec);
        }
    }
    m_ReadPos = 0;
}


size_t CBGZFStream::GetNextAvailableBytes()
{
    size_t avail;
    while ( !(avail = (m_BlockInfo.GetDataSize() - m_ReadPos)) ) {
        x_ReadBlock(m_BlockInfo.GetNextFileBlockPos());
    }
    return avail;
}

/*
const char* CBGZFStream::GetReadPtr(size_t count)
{
    if ( count > GetNextAvailableBytes() ) {
        return 0;
    }
    const char* ret = m_Data.get() + m_ReadPos;
    m_ReadPos += count;
    return ret;
}
*/

size_t CBGZFStream::Read(char* buf, size_t count)
{
    count = min(GetNextAvailableBytes(), count);
    _ASSERT(count < 0x10000);
    memcpy(buf, m_Data.get() + m_ReadPos, count);
    m_ReadPos += CBGZFPos::TByteOffset(count);
    return count;
}


const char* CBGZFStream::Read(size_t count)
{
    size_t avail = GetNextAvailableBytes();
    if ( count <= avail ) {
        const char* ret = m_Data.get() + m_ReadPos;
        m_ReadPos += CBGZFPos::TByteOffset(count);
        return ret;
    }
    char* dst = s_Reserve(count, m_ReadBuffer);
    while ( count ) {
        CBGZFPos::TByteOffset cnt =
            CBGZFPos::TByteOffset(min(GetNextAvailableBytes(), count));
        _ASSERT(cnt < 0x10000);
        memcpy(dst, m_Data.get() + m_ReadPos, cnt);
        m_ReadPos += cnt;
        dst += cnt;
        count -= cnt;
    }
    return m_ReadBuffer.data();
}


END_SCOPE(objects)
END_NCBI_SCOPE
