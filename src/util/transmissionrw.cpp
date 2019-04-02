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
 * Author: Anatoliy Kuznetsov
 *
 * File Description:  Transmission control.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbidbg.hpp>
#include <corelib/ncbi_bswap.hpp>
#include <corelib/ncbistr.hpp>
#include <util/util_exception.hpp>
#include <util/transmissionrw.hpp>

BEGIN_NCBI_SCOPE

static const Uint4 sStartWord = 0x01020304;
static const Uint4 sEndPacket = 0xFFFFFFFF;
const size_t kLengthSize = sizeof(Uint4);

CTransmissionWriter::CTransmissionWriter(IWriter* wrt, 
                                         EOwnership own_writer,
                                         ESendEofPacket send_eof)
    : m_Wrt(wrt),
      m_OwnWrt(own_writer),
      m_SendEof(send_eof),
      m_PacketBytesToWrite(0)
{
    _ASSERT(wrt);

    ERW_Result res = WriteUint4(sStartWord);
    if (res != eRW_Success) {
        NCBI_THROW(CIOException, eWrite,  "Cannot write the byte order");
    }
}


namespace {
class CIOBytesCountGuard 
{
public:
    size_t count;
    CIOBytesCountGuard(size_t* ret)
        : count(0), m_Ret(ret)
    {}

    ~CIOBytesCountGuard() { if (m_Ret) *m_Ret = count; }
private:
    size_t* m_Ret;
};
} // namespace

ERW_Result CTransmissionWriter::Write(const void* buf,
                                      size_t      count,
                                      size_t*     bytes_written)
{
    CIOBytesCountGuard wrt(bytes_written);

    if (count >= sEndPacket) {
        // The buffer cannot fit in one packet -- split it.
        count = (size_t) 0x80008000LU;
    }

    if (!m_PacketBytesToWrite) {
        Uint4 cnt = (Uint4) count;
        ERW_Result res = WriteUint4(cnt);
        if (res != eRW_Success) 
            return res;
        m_PacketBytesToWrite = cnt;
    }

    ERW_Result res = m_Wrt->Write(buf, m_PacketBytesToWrite, &wrt.count);
    if (res == eRW_Success) m_PacketBytesToWrite -= wrt.count;
    return res;
}

ERW_Result CTransmissionWriter::Flush(void)
{
    return m_Wrt->Flush();
}

ERW_Result CTransmissionWriter::Close(void)
{
    if (m_PacketBytesToWrite) {
        return eRW_Error;
    }

    if (m_SendEof != eSendEofPacket)
        return eRW_Success;

    m_SendEof = eDontSendEofPacket;

    return WriteUint4(sEndPacket);
}

CTransmissionWriter::~CTransmissionWriter()
{
    Close();

    if (m_OwnWrt)
        delete m_Wrt;
}

ERW_Result CTransmissionWriter::WriteUint4(const Uint4& value)
{
    auto data = reinterpret_cast<const char*>(&value);
    size_t written;
    ERW_Result rv = eRW_Success;

    for (auto to_write = kLengthSize; (rv == eRW_Success) && to_write; to_write -= written) {
        rv = m_Wrt->Write(data, to_write, &written);
        data += written;
    }

    return rv;
}

template <typename TType>
inline const TType* CTransmissionReader::SReadData::Data() const
{
    return reinterpret_cast<const TType*>(data() + m_Start);
}

inline size_t CTransmissionReader::SReadData::DataSize() const
{
    return m_End - m_Start;
}

inline char* CTransmissionReader::SReadData::Space()
{
    return data() + m_End;
}

inline size_t CTransmissionReader::SReadData::SpaceSize() const
{
    return size() - m_End;
}

inline void CTransmissionReader::SReadData::Add(size_t count)
{
    m_End += count;
    _ASSERT(m_End <= size());
}

inline void CTransmissionReader::SReadData::Remove(size_t count)
{
    m_Start += count;
    if (DataSize() == 0) m_Start = m_End = 0;
    _ASSERT(m_Start < size());
}

CTransmissionReader::CTransmissionReader(IReader* rdr, EOwnership own_reader)
    : m_Rdr(rdr),
      m_OwnRdr(own_reader),
      m_PacketBytesToRead(0),
      m_ByteSwap(false),
      m_StartRead(false)
{
}


CTransmissionReader::~CTransmissionReader()
{
    if (m_OwnRdr) {
        delete m_Rdr;
    }
}


ERW_Result CTransmissionReader::ReadData()
{
    size_t read;

    auto res = m_Rdr->Read(m_ReadData.Space(), m_ReadData.SpaceSize(), &read);
    if (res == eRW_Success) m_ReadData.Add(read);

    return res;
}


ERW_Result CTransmissionReader::Read(void*    buf,
                                     size_t   count,
                                     size_t*  bytes_read)
{
    const size_t kMinReadSize = 32 * 1024;

    CIOBytesCountGuard read(bytes_read);

    if (!m_StartRead) {
        auto res = x_ReadStart();
        if (res != eRW_Success) return res;
    }

    // read packet header
    while (m_PacketBytesToRead == 0) {
        auto res = ReadLength(m_PacketBytesToRead);
        if (res != eRW_Success) return res;
    }

    if (m_PacketBytesToRead == sEndPacket) return eRW_Eof;

    size_t to_read = count < m_PacketBytesToRead ? count : m_PacketBytesToRead;
    size_t already_read = m_ReadData.DataSize();

    if (!already_read) {
        // If chunk to read is big enough, read directly in the buf
        if (to_read >= kMinReadSize) {
            auto res = m_Rdr->Read(buf, to_read, &read.count);
            if (res == eRW_Success) m_PacketBytesToRead -= static_cast<Uint4>(read.count);
            return res;
        }

        auto res = ReadData();
        if (res != eRW_Success) return res;

        already_read = m_ReadData.DataSize();
    }

    if (already_read) {
        read.count = min(to_read, already_read);
        copy_n(m_ReadData.Data(), read.count, static_cast<char*>(buf));
        m_ReadData.Remove(read.count);
    }

    m_PacketBytesToRead -= static_cast<Uint4>(read.count);
    return eRW_Success;
}


ERW_Result CTransmissionReader::PendingCount(size_t* count)
{
    return m_Rdr->PendingCount(count);
}


ERW_Result CTransmissionReader::x_ReadStart()
{
    _ASSERT(!m_StartRead);

    m_StartRead = true;

    Uint4 start_word_coming;

    auto res = ReadLength(start_word_coming);
    if (res != eRW_Success) return res;

    m_ByteSwap = (start_word_coming != sStartWord);

    if (start_word_coming != sStartWord &&
        start_word_coming != 0x04030201)
        NCBI_THROW(CUtilException, eWrongData,
                   "Cannot determine the byte order. Got: " 
                   + NStr::UIntToString(start_word_coming, 0, 16));

    return eRW_Success;
}


ERW_Result CTransmissionReader::ReadLength(Uint4& length)
{
    while (m_ReadData.DataSize() < kLengthSize) {
        auto res = ReadData();
        if (res != eRW_Success) return res;
    }

    if (m_ByteSwap) {
        length = CByteSwap::GetInt4(m_ReadData.Data<unsigned char>());
    } else {
        length = *m_ReadData.Data<Uint4>();
    }

    m_ReadData.Remove(kLengthSize);
    return eRW_Success;
}


END_NCBI_SCOPE
