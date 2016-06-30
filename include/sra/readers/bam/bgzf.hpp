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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_entry;

class NCBI_BAMREAD_EXPORT CBGZFException : public CException
{
public:
    enum EErrCode {
        eOtherError,
        eFormatError,      ///< includes decompression errors
        eInvalidArg        ///< invalid function argument
    };
    virtual const char* GetErrCodeString(void) const;
    NCBI_EXCEPTION_DEFAULT(CBGZFException,CException);
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

    bool operator<(const CBGZFPos& b) const
        {
            return m_VirtualPos < b.m_VirtualPos;
        }
    bool operator==(const CBGZFPos& b) const
        {
            return m_VirtualPos == b.m_VirtualPos;
        }
    bool operator!=(const CBGZFPos& b) const
        {
            return !(*this == b);
        }

private:    
    TVirtualPos m_VirtualPos;

};
NCBI_BAMREAD_EXPORT
ostream& operator<<(ostream& out, const CBGZFPos& p);

typedef pair<CBGZFPos, CBGZFPos> CBGZFRange;


class CBGZFBlockInfo
{
public:
    typedef Uint8 TFileBlockPos; // position of block start in a file
    typedef Uint4 TFileBlockSize; // size of block in a file
    typedef Uint4 TDataSize; // size of uncompressed data
    typedef Uint4 TCRC32;

    CBGZFBlockInfo()
        : m_FileBlockPos(0),
          m_FileBlockSize(0),
          m_CRC32(0),
          m_DataSize(0)
        {
        }


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
    TCRC32 GetCRC32() const
        {
            return m_CRC32;
        }
    TDataSize GetDataSize() const
        {
            return m_DataSize;
        }

    static const TFileBlockSize kHeaderSize = 18;
    static const TFileBlockSize kFooterSize = 8;
    static const TFileBlockSize kMaxFileBlockSize = 1<<16;
    static const TDataSize kMaxDataSize = 1<<16;

    TFileBlockSize GetCompressedSize() const
        {
            return m_FileBlockSize - (kHeaderSize+kFooterSize);
        }

protected:
    friend class CBGZFFile;

private:
    TFileBlockPos m_FileBlockPos;
    TFileBlockSize m_FileBlockSize;
    TCRC32 m_CRC32;
    TDataSize m_DataSize;
};


class NCBI_BAMREAD_EXPORT CBGZFFile : public CObject
{
public:
    explicit
    CBGZFFile(const string& file_name);
    ~CBGZFFile();

    // Read block info from a BGZF stream.
    // Optionally save whole block data for decompression
    // and return pointer to compressed data.
    const char* ReadBlock(CBGZFPos::TFileBlockPos file_pos,
                          CBGZFBlockInfo& block_info,
                          CSimpleBufferT<char>* buffer);

    // read using buffer pointer if necessary
    // the buffer should have at least size bytes
    const char* Read(CBGZFPos::TFileBlockPos file_pos, size_t size,
                     char* buffer);
    // read using buffer if necessary
    const char* Read(CBGZFPos::TFileBlockPos file_pos, size_t size,
                     CSimpleBufferT<char>& buffer);

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
    
protected:

private:
    CMemoryFile m_MemFile;
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

    CBGZFPos GetPos() const
        {
            return CBGZFPos(m_BlockInfo.GetFileBlockPos(), m_ReadPos);
        }
    void Seek(CBGZFPos pos);

    // return number of available bytes in current decompressed buffer
    size_t GetNextAvailableBytes();
    // return pointer to count bytes in current decompressed buffer
    // or null if current buffer has smaller number of remaining bytes
    //const char* GetReadPtr(size_t count);
    // read up to count bytes into a buffer, may return smaller number
    size_t Read(char* buf, size_t count);

    // read count bytes and return pointer to read data
    // the pointer is either into decompressed buffer or into temporary buffer
    // the returned pointer is guaranteed to be valid until next read or seek
    const char* Read(size_t count);
    
private:
    void x_ReadBlock(CBGZFPos::TFileBlockPos file_pos);

    CRef<CBGZFFile> m_File;
    CBGZFBlockInfo m_BlockInfo;
    CBGZFPos::TByteOffset m_ReadPos;
    AutoArray<char> m_Data;
    CSimpleBufferT<char> m_ReadBuffer;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BGZF__HPP
