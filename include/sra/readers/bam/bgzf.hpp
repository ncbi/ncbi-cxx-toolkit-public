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
#include <util/compress/zlib.hpp>

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
    TCRC32 GetCRC32() const
        {
            return m_CRC32;
        }
    TDataSize GetDataSize() const
        {
            return m_DataSize;
        }


    // read block info from a BGZF stream
    // optionally save whole block data for decompression
    NCBI_BAMREAD_EXPORT void Read(CNcbiIstream& in, char* data = 0);

    static const TFileBlockSize kHeaderSize = 18;
    static const TFileBlockSize kFooterSize = 8;
    static const TFileBlockSize kMaxFileBlockSize = 1<<16;
    static const TDataSize kMaxDataSize = 1<<16;

    TFileBlockSize GetCompressedSize() const
        {
            return m_FileBlockSize - (kHeaderSize+kFooterSize);
        }

private:
    TFileBlockPos m_FileBlockPos;
    TFileBlockSize m_FileBlockSize;
    TCRC32 m_CRC32;
    TDataSize m_DataSize;
};
NCBI_BAMREAD_EXPORT
ostream& operator<<(ostream& out, const CBGZFBlockInfo& block);


class NCBI_BAMREAD_EXPORT CBGZFStream
{
public:
    explicit
    CBGZFStream(const string& file_name);
    ~CBGZFStream();

    void Open(const string& file_name);
    
    void Seek(CBGZFPos pos);

    CBGZFPos GetPos() const
        {
            return CBGZFPos(m_BlockInfo.GetFileBlockPos(), m_ReadPos);
        }

    size_t Read(char* buf, size_t count);
    
private:
    void x_ReadBlock();

    CNcbiIfstream m_File;
    CZipCompression m_Zip;
    CBGZFBlockInfo m_BlockInfo;
    CBGZFPos::TByteOffset m_ReadPos;
    AutoArray<char> m_Data;
    AutoArray<char> m_FileData;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // SRA__READER__BAM__BGZF__HPP
