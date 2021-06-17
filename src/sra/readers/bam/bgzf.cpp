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

NCBI_PARAM_DECL(int, BGZF, DEBUG);
NCBI_PARAM_DEF_EX(int, BGZF, DEBUG, 0, eParam_NoThread, BGZF_DEBUG);


static int s_GetDebug(void)
{
    static int value = NCBI_PARAM_TYPE(BGZF, DEBUG)::GetDefault();
    return value;
}


enum EFileMode {
    eUseFileIO,
    eUseMemFile,
    eUseVDBFile
};
static const EFileMode kFileMode = eUseVDBFile;
static const bool kCheckBlockCRC32 = true;
#ifdef USE_RANGE_CACHE
static const size_t kSegmentSizePow2Max = 22; // 4 MB
static const size_t kSegmentSizePow2Min = 18; // 256 KB
static const double kMaxBufferSeconds = .5;
#endif

#ifndef USE_RANGE_CACHE
static const size_t kSegmentSizePow2 = 22; // 4 MB
static const size_t kSegmentSize = 1 << kSegmentSizePow2;

static inline
CPagedFile::TFilePos s_GetPagePos(CPagedFile::TFilePos file_pos)
{
    return file_pos - file_pos % kSegmentSize;
}
#endif


const char* CBGZFException::GetErrCodeString(void) const
{
    switch ( GetErrCode() ) {
    case eOtherError:   return "eOtherError";
    case eFormatError:  return "eFormatError";
    case eInvalidArg:   return "eInvalidArg";
    default:            return CException::GetErrCodeString();
    }
}


CPagedFilePage::CPagedFilePage()
    : m_FilePos(TFilePos(-1)),
      m_Size(0),
      m_Ptr(0),
      m_MemFile(0)
{
}


CPagedFilePage::~CPagedFilePage()
{
    if ( m_MemFile ) {
        m_MemFile->Unmap(const_cast<char*>(m_Ptr));
    }
}


CPagedFile::CPagedFile(const string& file_name)
    : m_PageCache(new TPageCache(10)),
      m_TotalReadBytes(0),
      m_TotalReadSeconds(0),
      m_PreviousReadBytes(0),
      m_PreviousReadSeconds(0)
{
    switch ( kFileMode ) {
    case eUseFileIO:
        m_File.Open(file_name, CFileIO::eOpen, CFileIO::eRead);
        break;
    case eUseMemFile:
        m_MemFile.reset(new CMemoryFileMap(file_name,
                                           CMemoryFile::eMMP_Read,
                                           CMemoryFile::eMMS_Shared));
        break;
    case eUseVDBFile:
        m_VDBFile = CBamVDBFile(file_name);
        break;
    }
}


CPagedFile::~CPagedFile()
{
    if ( s_GetDebug() >= 1 && m_TotalReadBytes ) {
        LOG_POST("BGZF: Total read "<<m_TotalReadBytes/double(1<<20)<<" MB"
                 " speed: "<<m_TotalReadBytes/(m_TotalReadSeconds*(1<<20))<<" MB/s"
                 " time: "<<m_TotalReadSeconds);
    }
}


CPagedFile::TPage CPagedFile::GetPage(TFilePos file_pos)
{
#ifdef USE_RANGE_CACHE
    size_t size_pow2 = GetNextPageSizePow2();
    TPage page = m_PageCache->get_lock(file_pos, size_pow2);
    TFilePos page_pos = page.get_range().GetFrom();
    size_t page_size = page.get_range().GetLength();
#else
    TFilePos page_pos = s_GetPagePos(file_pos);
    TPage page = m_PageCache->get_lock(page_pos);
    size_t page_size = kSegmentSize;
#endif
    if ( page->GetFilePos() != page_pos ) {
        CFastMutexGuard guard(page.GetValueMutex());
        if ( page->GetFilePos() != page_pos ) {
            x_ReadPage(*page, page_pos, page_size);
        }
    }
    if ( !page->Contains(file_pos) ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "BGZF read @ "<<file_pos<<" is beyond file size");
    }
    return page;
}


pair<Uint8, double> CPagedFile::GetReadStatistics() const
{
    Uint8 bytes;
    double seconds;
    do {
        bytes = m_TotalReadBytes;
        seconds = m_TotalReadSeconds;
    } while ( bytes != m_TotalReadBytes );
    return make_pair(bytes, seconds);
}


void CPagedFile::SetPreviousReadStatistics(const pair<Uint8, double>& stats)
{
    m_PreviousReadBytes = stats.first;
    m_PreviousReadSeconds = stats.second;
}


size_t CPagedFile::GetNextPageSizePow2() const
{
    const Uint8 add_read_bytes = 1000000; // 100KB
    const double add_read_bytes_per_second = 8e6; // 8 MBps
    
    pair<Uint8, double> stats = GetReadStatistics();
    stats.first += m_PreviousReadBytes;
    stats.second += m_PreviousReadSeconds;

    Uint8 read_bytes = stats.first + m_PreviousReadBytes + add_read_bytes;
    double read_seconds = stats.second + m_PreviousReadSeconds +
        add_read_bytes/add_read_bytes_per_second;
    double seconds_per_byte = read_seconds/read_bytes;

    size_t size_pow2 = kSegmentSizePow2Max;
    double seconds = (size_t(1)<<size_pow2)*seconds_per_byte;
    while ( seconds > kMaxBufferSeconds && size_pow2 > kSegmentSizePow2Min ) {
        size_pow2 -= 1;
        seconds *= .5;
    }
    return size_pow2;
}


void CPagedFile::x_AddReadStatistics(Uint8 bytes, double seconds)
{
    CFastMutexGuard guard(m_Mutex);
    m_TotalReadBytes += bytes;
    m_TotalReadSeconds += seconds;
}


void CPagedFile::x_ReadPage(CPagedFilePage& page, TFilePos file_pos, size_t size)
{
    if ( m_MemFile ) {
        page.m_Ptr = (const char*)m_MemFile->Map(file_pos, size);
        page.m_MemFile = m_MemFile.get();
    }
    else {
        page.m_Buffer.resize(size);
        char* dst = page.m_Buffer.data();
        size_t rem = size;
        {
            CStopWatch sw(CStopWatch::eStart);
            if ( m_VDBFile ) {
                size_t cnt = m_VDBFile.ReadAll(file_pos, dst, rem);
                rem -= cnt;
                dst += cnt;
            }
            else {
                CFastMutexGuard guard(m_Mutex);
                m_File.SetFilePos(file_pos);
                while (rem) {
                    size_t cnt = m_File.Read(dst, rem);
                    if (cnt == 0) {
                        // end of file or error
                        break;
                    }
                    rem -= cnt;
                    dst += cnt;
                }
            }
            Uint8 bytes = size-rem;
            double seconds = sw.Elapsed();
            x_AddReadStatistics(bytes, seconds);
            if ( s_GetDebug() >= 3 ) {
                LOG_POST(Info<<"BGZF: Read page @ "<<file_pos
                         <<" "<<bytes<<" bytes in "<<seconds<<" sec"
                         " speed: "<<bytes/(seconds*(1<<20))<<" MB/s");
            }
        }
        if (rem) {
            memset(dst, 0, min(rem, size_t(CBGZFBlock::kMaxFileBlockSize)));
            size -= rem;
        }
        page.m_Ptr = page.m_Buffer.data();
    }
    page.m_Size = size;
    page.m_FilePos = file_pos;
}

/*
void CPagedFile::Seek(CRef<CPagedFilePage>& page, Uint8 file_pos)
{
    if ( page && !page->Contains(file_pos) ) {
        Release(page);
        _ASSERT(!page);
    }
    if ( !page ) {
        page = Acquire(file_pos);
    }
    _ASSERT(page);
    _ASSERT(page->GetFilePos() == s_GetPagePos(file_pos));
}
*/

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


static
const char* s_Read(char* buffer, size_t len,
                   CPagedFile& file, CPagedFile::TPage& page, Uint8 file_pos)
{
    page = file.GetPage(file_pos);
    _ASSERT(page->Contains(file_pos));
    const char* ptr = page->GetPagePtr();
    _ASSERT(ptr);
    size_t off = size_t(file_pos - page->GetFilePos());
    size_t avail = page->GetPageSize() - off;
    if ( len <= avail ) {
        return ptr + off;
    }
    // current buffer doesn't have enough,
    // combine required data from several buffers
    char* dst = buffer;
    _ASSERT(avail < CBGZFBlock::kMaxFileBlockSize);
    memcpy(dst, ptr+off, avail);
    len -= avail;
    file_pos += avail;
    dst += avail;
    _ASSERT(len);
    do {
        page = file.GetPage(file_pos);
        _ASSERT(page->Contains(file_pos));
        ptr = page->GetPagePtr();
        _ASSERT(ptr);
        off = size_t(file_pos - page->GetFilePos());
        avail = page->GetPageSize() - off;
        size_t cnt = min(avail, len);
        _ASSERT(cnt < 0x10000);
        memcpy(dst, ptr+off, cnt);
        len -= cnt;
        file_pos += cnt;
        dst += cnt;
    } while ( len );
    return buffer;
}


ostream& operator<<(ostream& out, const CBGZFPos& p)
{
    return out << p.GetFileBlockPos() << '.' << p.GetByteOffset();
}


ostream& operator<<(ostream& out, const CBGZFRange& r)
{
    return out << r.first << '-' << r.second;
}


CBGZFBlock::CBGZFBlock()
    : m_FileBlockPos(TFileBlockPos(-1)),
      m_FileBlockSize(0),
      m_DataSize(0),
      m_Data(new char[kMaxDataSize])
{
}


CBGZFBlock::~CBGZFBlock()
{
}


CBGZFFile::CBGZFFile(const string& file_name)
    : m_File(new CPagedFile(file_name)),
      m_BlockCache(new TBlockCache(10)),
      m_TotalUncompressBytes(0),
      m_TotalUncompressSeconds(0)
{
}


CBGZFFile::~CBGZFFile()
{
    if ( s_GetDebug() >= 1 && m_TotalUncompressBytes ) {
        LOG_POST("BGZF: Total decompressed "<<m_TotalUncompressBytes/double(1<<20)<<" MB"
                 " speed: "<<m_TotalUncompressBytes/(m_TotalUncompressSeconds*(1<<20))<<" MB/s"
                 " time: "<<m_TotalUncompressSeconds);
    }
}


CBGZFFile::TBlock CBGZFFile::GetBlock(TFileBlockPos file_pos,
                                      CPagedFile::TPage& page,
                                      CSimpleBufferT<char>& buffer)
{
    TBlock block = m_BlockCache->get_lock(file_pos);
    if ( block->GetFileBlockPos() != file_pos ) {
        CFastMutexGuard guard(block.GetValueMutex());
        if ( block->GetFileBlockPos() != file_pos ) {
            if ( !x_ReadBlock(*block, file_pos, page, buffer) ) {
                block.Reset();
            }
        }
    }
    return block;
}


pair<Uint8, double> CBGZFFile::GetUncompressStatistics() const
{
    Uint8 bytes;
    double seconds;
    do {
        bytes = m_TotalUncompressBytes;
        seconds = m_TotalUncompressSeconds;
    } while ( bytes != m_TotalUncompressBytes );
    return make_pair(bytes, seconds);
}


void CBGZFFile::x_AddUncompressStatistics(Uint8 bytes, double seconds)
{
    CFastMutexGuard guard(m_Mutex);
    m_TotalUncompressBytes += bytes;
    m_TotalUncompressSeconds += seconds;
}


CBGZFStream::CBGZFStream()
    : m_ReadPos(0),
      m_EndPos(CBGZFPos::GetInvalid())
{
}


CBGZFStream::CBGZFStream(CBGZFFile& file)
    : m_ReadPos(0),
      m_InReadBuffer(CBGZFBlock::kMaxFileBlockSize),
      m_EndPos(CBGZFPos::GetInvalid())
{
    Open(file);
}


CBGZFStream::~CBGZFStream()
{
}


void CBGZFStream::Close()
{
    m_Block.Reset();
    m_Page.Reset();
    m_File.Reset();
}


void CBGZFStream::Open(CBGZFFile& file)
{
    Close();
    m_File.Reset(&file);
    m_EndPos = CBGZFPos::GetInvalid();
}


bool CBGZFStream::x_NextBlock()
{
    m_Block = m_File->GetBlock(GetNextBlockFilePos(), m_Page, m_InReadBuffer);
    m_ReadPos = 0;
    return m_Block;
}


void CBGZFStream::Seek(CBGZFPos pos, CBGZFPos end_pos)
{
    m_EndPos = end_pos;
    if ( pos == GetPos() ) {
        return;
    }
    m_Block = m_File->GetBlock(pos.GetFileBlockPos(), m_Page, m_InReadBuffer);
    m_ReadPos = pos.GetByteOffset();
    if ( m_ReadPos && !HaveBytesInBlock() ) {
        NCBI_THROW_FMT(CBGZFException, eInvalidArg,
                       "Bad BGZF("<<pos.GetFileBlockPos()<<") offset: "<<
                       m_ReadPos<<" vs "<<GetBlockDataSize());
    }
}


static const size_t kFixedHeaderSize = 12; // up to and including XLEN
static const size_t kExtraHeaderSize = 4; // type and size
static const size_t kRequiredExtraSize = kExtraHeaderSize + 2; // BSIZE block
static const size_t kInitialExtraSize = kRequiredExtraSize;
static const size_t kFooterSize = 8; // CRC & ISIZE

bool CBGZFFile::x_ReadBlock(CBGZFBlock& block,
                            TFileBlockPos file_pos0,
                            CPagedFile::TPage& page,
                            CSimpleBufferT<char>& buffer)
{
    try {
        page = m_File->GetPage(file_pos0);
    }
    catch ( CBGZFException& exc ) {
        if ( exc.GetErrCode() == exc.eFormatError &&
             (page->GetFilePos()+page->GetPageSize() == file_pos0) ) {
            // read past of the file
            return false;
        }
        throw;
    }
    
    CBGZFPos::TFileBlockPos file_pos = file_pos0;

    // parse header
    char header_tmp[kFixedHeaderSize + kInitialExtraSize];
    const char* header =
        s_Read(header_tmp, kFixedHeaderSize + kInitialExtraSize,
               *m_File, page, file_pos);
    file_pos += kFixedHeaderSize + kInitialExtraSize;
    
    // parse and check GZip header
    if (header[0] != 31 || header[1] != '\x8b' ||
        header[2] != 8 || header[3] != 4) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos0<<") MAGIC: "<<
                       hex<<SBamUtil::MakeUint4(header));
    }
    // 4-7 - modification time - ignored
    // 8 - extra flags - ignored
    // 9 - OS - ignored

    // prepare extra data
    size_t extra_size = SBamUtil::MakeUint2(header + 10);
    if ( extra_size < kRequiredExtraSize ) {
        // too small
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos0<<") XLEN: "<<extra_size);
    }
    const char* extra = header + kFixedHeaderSize;
    if ( extra_size >= kInitialExtraSize ) {
        // read remainder of extra data
        memcpy(buffer.data(), extra, kInitialExtraSize);
        size_t more_extra_size = extra_size - kInitialExtraSize;
        char* more_extra_dst = buffer.data() + kInitialExtraSize;
        const char* more_extra =
            s_Read(more_extra_dst, more_extra_size,
                   *m_File, page, file_pos);
        file_pos += more_extra_size;
        if ( more_extra != more_extra_dst ) {
            memcpy(more_extra_dst, more_extra, more_extra_size);
        }
        extra = buffer.data();
    }
    size_t real_header_size = kFixedHeaderSize + extra_size;

    // parse extra data to determine BGZF block size
    CBGZFBlock::TFileBlockSize block_size = 0;
    while ( extra_size >= kExtraHeaderSize ) {
        size_t extra_data_size = SBamUtil::MakeUint2(extra + 2);
        size_t extra_block_size = extra_data_size + kExtraHeaderSize;
        if ( extra[0] == 'B' && extra[1] == 'C' &&
             extra_data_size == 2 &&
             extra_block_size <= extra_size ) {
            block_size = SBamUtil::MakeUint2(extra + 4) + 1;
        }
        if ( extra_block_size >= extra_size ) {
            break;
        }
        extra_size -= extra_block_size;
        extra += extra_block_size;
    }
    if ( block_size <= real_header_size + kFooterSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos0<<") SIZE: "<<block_size);
    }
    
    // read compressed data and footer
    _ASSERT(block_size <= CBGZFBlock::kMaxFileBlockSize);
    const char* compressed_data =
        s_Read(buffer.data(), block_size - real_header_size,
               *m_File, page, file_pos);
    size_t compressed_size = block_size - (real_header_size + kFooterSize);
    
    // decompress data
    size_t decompressed_size = 0;
    {
        CStopWatch sw(CStopWatch::eStart);
        
        const char* src = compressed_data;
        size_t src_size = compressed_size;
        char* dst = block.m_Data.get();
        size_t dst_size = CBGZFBlock::kMaxDataSize;

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
                           "Bad BGZF("<<file_pos0<<") compression sizes: "<<
                           src_size<<" -> "<<dst_size);
        }
        
        int err = inflateInit2(&stream, -15);
        if ( err == Z_OK ) {
            err = inflate(&stream, Z_FINISH);
            if ( err == Z_STREAM_END ) {
                decompressed_size = stream.total_out;
            }
            inflateEnd(&stream);
        }

        double seconds = sw.Elapsed();
        x_AddUncompressStatistics(compressed_size, seconds);
        if ( s_GetDebug() >= 5 ) {
            LOG_POST(Info<<"BGZF: Decompressed block"
                     " @ "<<file_pos0<<" in "<<seconds<<" sec"
                     " speed: "<<compressed_size/(seconds*(1<<20))<<" MB/s");
        }
    }
    
    // parse footer
    const char* footer = compressed_data + compressed_size;
    CBGZFBlock::TCRC32 crc32 = SBamUtil::MakeUint4(footer);
    CBGZFBlock::TDataSize data_size = SBamUtil::MakeUint4(footer + 4);
    if ( /*data_size == 0 || */data_size > CBGZFBlock::kMaxDataSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos0<<") DATA SIZE: " << data_size);
    }
    if ( data_size == 0 && s_GetDebug() >= 2 ) {
        LOG_POST(Warning<<"Zero BGZF("<<file_pos0<<")");
    }

    // check decompression results
    if ( decompressed_size != data_size ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad BGZF("<<file_pos0<<") decompressed data size: "<<
                       decompressed_size<<" vs "<<data_size);
    }
    if ( kCheckBlockCRC32 ) {
        CChecksum checksum(CChecksum::eCRC32ZIP);
        checksum.AddChars(block.m_Data.get(), decompressed_size);
        if ( checksum.GetChecksum() != crc32 ) {
            NCBI_THROW_FMT(CBGZFException, eFormatError,
                           "Bad BGZF("<<file_pos0<<") CRC32: "<<
                           hex<<Uint4(checksum.GetChecksum())<<" vs "<<crc32);
        }
    }

    // save block parameters
    block.m_FileBlockSize = block_size;
    block.m_DataSize = data_size;
    block.m_FileBlockPos = file_pos0;
    return true;
}


size_t CBGZFStream::GetNextAvailableBytes()
{
    size_t avail;
    while ( !(avail = (GetBlockDataSize() - m_ReadPos)) ) {
        x_NextBlock();
    }
    return avail;
}


bool CBGZFStream::HaveNextDataBlock()
{
    _ASSERT(!HaveBytesInBlock());
    do {
        if ( !(CBGZFPos(GetNextBlockFilePos(), 0) < m_EndPos) ) {
            return false;
        }
        if ( !x_NextBlock() ) {
            return false;
        }
    } while ( !HaveBytesInBlock() );
    return true;
}


size_t CBGZFStream::Read(char* buf, size_t count)
{
    count = min(GetNextAvailableBytes(), count);
    _ASSERT(count <= 0x10000);
    _ASSERT(m_Block);
    _ASSERT(m_Block->m_Data);
    memcpy(buf, m_Block->m_Data.get() + m_ReadPos, count);
    m_ReadPos += CBGZFPos::TByteOffset(count);
    return count;
}


const char* CBGZFStream::Read(size_t count)
{
    size_t avail = GetNextAvailableBytes();
    if ( count <= avail ) {
        _ASSERT(m_Block);
        _ASSERT(m_Block->m_Data);
        const char* ret = m_Block->m_Data.get() + m_ReadPos;
        m_ReadPos += CBGZFPos::TByteOffset(count);
        return ret;
    }
    char* dst = s_Reserve(count, m_OutReadBuffer);
    while ( count ) {
        CBGZFPos::TByteOffset cnt =
            CBGZFPos::TByteOffset(min(GetNextAvailableBytes(), count));
        _ASSERT(cnt <= CBGZFBlock::kMaxDataSize);
        _ASSERT(m_Block);
        _ASSERT(m_Block->m_Data);
        memcpy(dst, m_Block->m_Data.get() + m_ReadPos, cnt);
        m_ReadPos += cnt;
        dst += cnt;
        count -= cnt;
    }
    return m_OutReadBuffer.data();
}


END_SCOPE(objects)
END_NCBI_SCOPE
