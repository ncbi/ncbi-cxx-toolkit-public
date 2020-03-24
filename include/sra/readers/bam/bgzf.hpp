#ifndef SRA__READER__BAM__BGZF__HPP
#define SRA__READER__BAM__BGZF__HPP
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

#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <util/simple_buffer.hpp>
#include <sra/readers/bam/vdbfile.hpp>
#include <sra/readers/bam/cache_with_lock.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;
class CPagedFile;
class CPagedFilePage;
class CBGZFFile;
class CBGZFStream;

class CPagedFilePage : public CObject
{
public:
    typedef Uint8 TFilePos;

    CPagedFilePage();
    ~CPagedFilePage();
    
    TFilePos GetFilePos() const
    {
        return m_FilePos;
    }
    size_t GetPageSize() const
    {
        return m_Size;
    }
    const char* GetPagePtr() const
    {
        return m_Ptr;
    }

    bool Contains(TFilePos file_pos) const
    {
        return (file_pos - GetFilePos()) < GetPageSize();
    }

protected:
    friend class CPagedFile;
    
private:
    volatile TFilePos m_FilePos;
    size_t m_Size;
    const char* m_Ptr;
    CSimpleBufferT<char> m_Buffer;
    CMemoryFileMap* m_MemFile;
};


class NCBI_BAMREAD_EXPORT CPagedFile : public CObject
{
public:
    typedef CPagedFilePage::TFilePos TFilePos;

    explicit
    CPagedFile(const string& file_name);
    ~CPagedFile();

#define USE_RANGE_CACHE 1
#ifdef USE_RANGE_CACHE
    typedef CBinaryRangeCacheWithLock<TFilePos, CPagedFilePage> TPageCache;
#else
    typedef CCacheWithLock<TFilePos, CPagedFilePage> TPageCache;
#endif
    typedef TPageCache::CLock TPage;

    // return page that contains the file position
    TPage GetPage(TFilePos pos);

    pair<Uint8, double> GetReadStatistics() const;
    void SetPreviousReadStatistics(const pair<Uint8, double>& stats);
    // estimate best next page size to read using collected statistics
    size_t GetNextPageSizePow2() const;

private:
    void x_AddReadStatistics(Uint8 bytes, double seconds);
    
    void x_ReadPage(CPagedFilePage& page, TFilePos file_pos, size_t size);

    CFastMutex m_Mutex;
    
    // three variants: direct file IO, memory mapped file, or VDB KFile
    CFileIO m_File;
    AutoPtr<CMemoryFileMap> m_MemFile;
    CBamVDBFile m_VDBFile;

    // cache for loaded pages
    CRef<TPageCache> m_PageCache;

    volatile Uint8 m_TotalReadBytes;
    volatile double m_TotalReadSeconds;
    Uint8 m_PreviousReadBytes;
    double m_PreviousReadSeconds;
};


class NCBI_BAMREAD_EXPORT CBGZFException : public CException
{
public:
    enum EErrCode {
        eOtherError,
        eFormatError,      ///< includes decompression errors
        eInvalidArg        ///< invalid function argument
    };
    virtual const char* GetErrCodeString(void) const override;
    NCBI_EXCEPTION_DEFAULT(CBGZFException,CException);
};


struct SBamUtil {
    // conversion of BAM bytes into larger values - ints and floats
    // the source data have any alignment
    
    static Uint2 MakeUint2(const char* buf)
        {
            return Uint2(Uint1(buf[0]))|
                (Uint2(Uint1(buf[1]))<<8);
        }
    
    static Uint4 MakeUint4(const char* buf)
        {
            return Uint4(Uint1(buf[0]))|
                (Uint4(Uint1(buf[1]))<<8)|
                (Uint4(Uint1(buf[2]))<<16)|
                (Uint4(Uint1(buf[3]))<<24);
        }

    static Uint8 MakeUint8(const char* buf)
        {
            return Uint8(Uint1(buf[0]))|
                (Uint8(Uint1(buf[1]))<<8)|
                (Uint8(Uint1(buf[2]))<<16)|
                (Uint8(Uint1(buf[3]))<<24)|
                (Uint8(Uint1(buf[4]))<<32)|
                (Uint8(Uint1(buf[5]))<<40)|
                (Uint8(Uint1(buf[6]))<<48)|
                (Uint8(Uint1(buf[7]))<<56);
        }

    union UFloatUint4 {
        float f;
        Uint4 i;
    };
    static float MakeFloat(const char* buf)
        {
            UFloatUint4 u;
            u.i = MakeUint4(buf);
            return u.f;
        }
};


class CBGZFPos
{
public:
    typedef Uint8 TFileBlockPos; // position of block start in a file
    typedef Uint4 TByteOffset; // position of byte within block
    typedef Uint8 TVirtualPos; // virtual position, ordered

    static const Uint4 kMaxBlockSize = 1<<16;

    CBGZFPos()
        : m_VirtualPos(0)
        {
        }
    explicit
    CBGZFPos(TVirtualPos pos)
        : m_VirtualPos(pos)
        {
        }
    CBGZFPos(TFileBlockPos block_pos, TByteOffset byte_offset)
        : m_VirtualPos((block_pos<<16)+byte_offset)
        {
        }

    TVirtualPos GetVirtualPos() const
        {
            return m_VirtualPos;
        }
    
    TFileBlockPos GetFileBlockPos() const
        {
            return m_VirtualPos >> 16;
        }
    TByteOffset GetByteOffset() const
        {
            return TByteOffset(m_VirtualPos&(0xffff));
        }

    bool operator==(const CBGZFPos& b) const
        {
            return m_VirtualPos == b.m_VirtualPos;
        }
    bool operator!=(const CBGZFPos& b) const
        {
            return m_VirtualPos != b.m_VirtualPos;
        }
    bool operator<(const CBGZFPos& b) const
        {
            return m_VirtualPos < b.m_VirtualPos;
        }
    bool operator>(const CBGZFPos& b) const
        {
            return m_VirtualPos > b.m_VirtualPos;
        }
    bool operator<=(const CBGZFPos& b) const
        {
            return m_VirtualPos <= b.m_VirtualPos;
        }
    bool operator>=(const CBGZFPos& b) const
        {
            return m_VirtualPos >= b.m_VirtualPos;
        }

    static CBGZFPos GetInvalid()
        {
            return CBGZFPos(TVirtualPos(-1));
        }
    bool IsInvalid() const
        {
            return GetVirtualPos() == TVirtualPos(-1);
        }

    DECLARE_OPERATOR_BOOL(m_VirtualPos != 0);

private:    
    TVirtualPos m_VirtualPos;

};
NCBI_BAMREAD_EXPORT
ostream& operator<<(ostream& out, const CBGZFPos& p);

typedef pair<CBGZFPos, CBGZFPos> CBGZFRange;
NCBI_BAMREAD_EXPORT
ostream& operator<<(ostream& out, const CBGZFRange& r);

class CBGZFBlock
{
public:
    typedef Uint8 TFileBlockPos; // position of block start in a file
    typedef Uint4 TFileBlockSize; // size of block in a file
    typedef Uint4 TDataSize; // size of uncompressed data
    typedef Uint4 TCRC32;

    CBGZFBlock();
    ~CBGZFBlock();


    TFileBlockPos GetFileBlockPos() const
        {
            return m_FileBlockPos;
        }
    TFileBlockSize GetFileBlockSize() const
        {
            return m_FileBlockSize;
        }
    TFileBlockPos GetNextFileBlockPos() const
        {
            return GetFileBlockPos() + GetFileBlockSize();
        }
    TDataSize GetDataSize() const
        {
            return m_DataSize;
        }

    static const TFileBlockSize kMaxFileBlockSize = 1<<16;
    static const TDataSize kMaxDataSize = 1<<16;

protected:
    friend class CBGZFFile;
    friend class CBGZFStream;

private:
    volatile TFileBlockPos m_FileBlockPos;
    TFileBlockSize m_FileBlockSize;
    TDataSize m_DataSize;
    AutoArray<char> m_Data;
};


class NCBI_BAMREAD_EXPORT CBGZFFile : public CObject
{
public:
    explicit
    CBGZFFile(const string& file_name);
    ~CBGZFFile();

    pair<Uint8, double> GetReadStatistics() const
        {
            return m_File->GetReadStatistics();
        }
    void SetPreviousReadStatistics(const pair<Uint8, double>& stats)
        {
            m_File->SetPreviousReadStatistics(stats);
        }

    pair<Uint8, double> GetUncompressStatistics() const;

protected:
    friend class CBGZFStream;

    void x_AddUncompressStatistics(Uint8 bytes, double seconds);

    typedef CBGZFPos::TFileBlockPos TFileBlockPos;
    typedef CCacheWithLock<TFileBlockPos, CBGZFBlock> TBlockCache;
    typedef TBlockCache::CLock TBlock;

    TBlock GetBlock(TFileBlockPos file_pos,
                    CPagedFile::TPage& page,
                    CSimpleBufferT<char>& buffer);
    
    bool x_ReadBlock(CBGZFBlock& block,
                     TFileBlockPos file_pos,
                     CPagedFile::TPage& page,
                     CSimpleBufferT<char>& buffer);
    
private:
    CRef<CPagedFile> m_File;
    CRef<TBlockCache> m_BlockCache;

    CFastMutex m_Mutex;
    
    volatile Uint8 m_TotalUncompressBytes;
    volatile double m_TotalUncompressSeconds;
};


class NCBI_BAMREAD_EXPORT CBGZFStream
{
public:
    CBGZFStream();
    explicit
    CBGZFStream(CBGZFFile& file);
    ~CBGZFStream();

    void Close();
    void Open(CBGZFFile& file);

    CBGZFBlock::TDataSize GetBlockDataSize() const
        {
            return m_Block? m_Block->GetDataSize(): 0;
        }
    CBGZFBlock::TFileBlockPos GetBlockFilePos() const
        {
            return m_Block? m_Block->GetFileBlockPos(): 0;
        }
    CBGZFBlock::TFileBlockPos GetNextBlockFilePos() const
        {
            return m_Block? m_Block->GetNextFileBlockPos(): 0;
        }
    bool HaveBytesInBlock() const
        {
            return m_ReadPos < GetBlockDataSize();
        }

    CBGZFPos GetPos() const
        {
            return CBGZFPos(GetBlockFilePos(), m_ReadPos);
        }
    CBGZFPos GetNextBlockPos() const
        {
            return CBGZFPos(GetNextBlockFilePos(), 0);
        }
    CBGZFPos GetSeekPos() const
        {
            if ( HaveBytesInBlock() ) {
                return GetPos();
            }
            else {
                return GetNextBlockPos();
            }
        }
    CBGZFPos GetEndPos() const
        {
            return m_EndPos;
        }
    // seek to position to read till end_pos, or EOF if end_pos is invalid
    void Seek(CBGZFPos pos, CBGZFPos end_pos = CBGZFPos::GetInvalid());

    // return non-zero number of available bytes in current decompressed buffer
    size_t GetNextAvailableBytes();
    // return true if there are more bytes before this position
    bool HaveNextAvailableBytes()
        {
            if ( HaveBytesInBlock() ) {
                return GetPos() < m_EndPos;
            }
            return HaveNextDataBlock();
        }
    // return true if there are more data blocks before this position
    // current buffer must be read till the end
    bool HaveNextDataBlock();

    // read up to count bytes into a buffer, may return smaller number
    size_t Read(char* buf, size_t count);

    // read count bytes and return pointer to read data
    // the pointer is either into decompressed buffer or into temporary buffer
    // the returned pointer is guaranteed to be valid until next read or seek
    const char* Read(size_t count);
    
private:
    bool x_NextBlock();
    
    const char* x_Read(CBGZFPos::TFileBlockPos file_pos, size_t size, char* buffer);
    
    // returns false if m_EndPos is invalid and EOF happened
    bool x_ReadBlock(CBGZFPos::TFileBlockPos file_pos);

    CRef<CBGZFFile> m_File;
    CPagedFile::TPage m_Page;
    CBGZFFile::TBlock m_Block;
    CBGZFPos::TByteOffset m_ReadPos;
    CSimpleBufferT<char> m_InReadBuffer;
    CSimpleBufferT<char> m_OutReadBuffer;
    CBGZFPos m_EndPos;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BGZF__HPP
