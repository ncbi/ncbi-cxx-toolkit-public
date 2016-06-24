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

BEGIN_NCBI_SCOPE

//#define NCBI_USE_ERRCODE_X   BAM2Graph
//NCBI_DEFINE_ERR_SUBCODE_X(6);

BEGIN_SCOPE(objects)

class CSeq_entry;

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
void s_Read(CNcbiIstream& in, char* dst, size_t len)
{
    while ( len ) {
        in.read(dst, len);
        if ( !in ) {
            NCBI_THROW(CIOException, eRead, "Read failure");
        }
        size_t cnt = in.gcount();
        len -= cnt;
        dst += cnt;
    }
}


static inline
string s_ReadString(CNcbiIstream& in, size_t len)
{
    string ret(len, ' ');
    s_Read(in, &ret[0], len);
    return ret;
}


static inline
void s_ReadMagic(CNcbiIstream& in, const char* magic)
{
    _ASSERT(strlen(magic) == 4);
    char buf[4];
    s_Read(in, buf, 4);
    if ( memcmp(buf, magic, 4) != 0 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad file magic: "<<NStr::PrintableString(string(buf, buf+4)));
    }
}


static inline
uint16_t s_MakeUint16(const char* buf)
{
    return uint16_t(uint8_t(buf[0]))|
        (uint16_t(uint8_t(buf[1]))<<8);
}


static inline
uint32_t s_MakeUint32(const char* buf)
{
    return uint32_t(uint8_t(buf[0]))|
        (uint32_t(uint8_t(buf[1]))<<8)|
        (uint32_t(uint8_t(buf[2]))<<16)|
        (uint32_t(uint8_t(buf[3]))<<24);
}


static inline
uint64_t s_MakeUint64(const char* buf)
{
    return uint64_t(uint8_t(buf[0]))|
        (uint64_t(uint8_t(buf[1]))<<8)|
        (uint64_t(uint8_t(buf[2]))<<16)|
        (uint64_t(uint8_t(buf[3]))<<24)|
        (uint64_t(uint8_t(buf[4]))<<32)|
        (uint64_t(uint8_t(buf[5]))<<40)|
        (uint64_t(uint8_t(buf[6]))<<48)|
        (uint64_t(uint8_t(buf[7]))<<56);
}


static inline
void s_Read(CBGZFStream& in, char* dst, size_t len)
{
    while ( len ) {
        size_t cnt = in.Read(dst, len);
        len -= cnt;
        dst += cnt;
    }
}


static inline
uint32_t s_ReadUInt32(CBGZFStream& in)
{
    char buf[4];
    s_Read(in, buf, 4);
    return s_MakeUint32(buf);
}


static inline
uint64_t s_ReadUInt64(CBGZFStream& in)
{
    char buf[8];
    s_Read(in, buf, 8);
    return s_MakeUint64(buf);
}


static inline
int32_t s_ReadInt32(CBGZFStream& in)
{
    return int32_t(s_ReadUInt32(in));
}


ostream& operator<<(ostream& out, const CBGZFPos& p)
{
    return out << p.GetFileBlockPos() << '.' << p.GetByteOffset();
}

ostream& operator<<(ostream& out, const CBGZFBlockInfo& b)
{
    return out << "BGZF("<<b.GetFileBlockPos()<<')';
}

void CBGZFBlockInfo::Read(CNcbiIstream& in, char* data)
{
    m_FileBlockPos = in.tellg();
    char buf[kHeaderSize];
    s_Read(in, buf, 18);
    // parse and check GZip header
    if ( buf[0] != 31 || buf[1] != '\x8b' || buf[2] != 8 || buf[3] != 4 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<*this<<" MAGIC: ");
    }
    // 4-7 - modification time
    // 8 - extra flags
    // 9 - OS
    if ( s_MakeUint16(buf+10) != 6 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<*this<<" XLEN: ");
    }
    // extra data
    if ( buf[12] != 'B' || buf[13] != 'C' || buf[14] != 2 || buf[15] != 0 ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<*this<<" EXTRA: ");
    }
    m_FileBlockSize = s_MakeUint16(buf+16)+1;
    if ( GetFileBlockSize() <= kHeaderSize+kFooterSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<*this<<" FILE SIZE: "<<GetFileBlockSize());
    }
    const char* footer;
    if ( data ) {
        memcpy(data, buf, kHeaderSize);
        s_Read(in, data+kHeaderSize, GetCompressedSize()+kFooterSize);
        footer = data+kHeaderSize+GetCompressedSize();
    }
    else {
        in.seekg(GetCompressedSize(), ios::cur);
        s_Read(in, buf, kFooterSize);
        footer = buf;
    }
    m_CRC32 = s_MakeUint32(footer);
    m_DataSize = s_MakeUint32(footer+4);
    if ( GetDataSize() == 0 || GetDataSize() > kMaxDataSize ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<*this<<" DATA SIZE: "<<GetDataSize());
    }
}


CBGZFStream::CBGZFStream(const string& file_name)
    : m_File(file_name.c_str(), ios::binary),
      m_ReadPos(0),
      m_Data(CBGZFBlockInfo::kMaxDataSize),
      m_FileData(CBGZFBlockInfo::kMaxFileBlockSize)
{
    m_Zip.SetFlags(m_Zip.fGZip);
}


CBGZFStream::~CBGZFStream()
{
}


void CBGZFStream::Open(const string& file_name)
{
    m_File.open(file_name.c_str(), ios::binary);
    m_BlockInfo = CBGZFBlockInfo();
    m_ReadPos = 0;
}


void CBGZFStream::Seek(CBGZFPos pos)
{
    if ( pos.GetFileBlockPos() != m_BlockInfo.GetFileBlockPos() ||
         m_BlockInfo.GetDataSize() == 0 ) {
        m_File.seekg(pos.GetFileBlockPos());
        x_ReadBlock();
    }
    m_ReadPos = pos.GetByteOffset();
    if ( m_ReadPos >= m_BlockInfo.GetDataSize() ) {
        NCBI_THROW_FMT(CBGZFException, eInvalidArg,
                       "Bad "<<m_BlockInfo<<" offset: "<<
                       m_ReadPos<<" vs "<<m_BlockInfo.GetDataSize());
    }
}


void CBGZFStream::x_ReadBlock()
{
    m_BlockInfo.Read(m_File, m_FileData.get());
    size_t size;
    m_Zip.DecompressBuffer(m_FileData.get(), m_BlockInfo.GetFileBlockSize(),
                           m_Data.get(), CBGZFBlockInfo::kMaxDataSize,
                           &size);
    if ( size != m_BlockInfo.GetDataSize() ) {
        NCBI_THROW_FMT(CBGZFException, eFormatError,
                       "Bad "<<m_BlockInfo<<" decompressed data size: "<<
                       size<<" vs "<<m_BlockInfo.GetDataSize());
    }
    m_ReadPos = 0;
}


size_t CBGZFStream::Read(char* buf, size_t count)
{
    while ( m_ReadPos >= m_BlockInfo.GetDataSize() ) {
        x_ReadBlock();
    }
    CBGZFBlockInfo::TDataSize avail = m_BlockInfo.GetDataSize() - m_ReadPos;
    CBGZFBlockInfo::TDataSize cnt = CBGZFBlockInfo::TDataSize(min(size_t(avail), count));
    memcpy(buf, m_Data.get() + m_ReadPos, cnt);
    m_ReadPos += cnt;
    return cnt;
}

    
END_SCOPE(objects)
END_NCBI_SCOPE
