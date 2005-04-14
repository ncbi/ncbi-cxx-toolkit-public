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
#include <util/transmissionrw.hpp>

BEGIN_NCBI_SCOPE

CTransmissionWriter::CTransmissionWriter(IWriter* wrt, EOwnership own_writer)
: m_Wrt(wrt),
  m_OwnWrt(own_writer)
{
    _ASSERT(wrt);

    Int4 start_word = 0x01020304;
    size_t written;
    ERW_Result res = m_Wrt->Write(&start_word, sizeof(start_word), &written);
    if (res != eRW_Success || written != sizeof(start_word)) {
        _ASSERT(0);
    }
}

CTransmissionWriter::~CTransmissionWriter()
{
    if (m_OwnWrt) {
        delete m_Wrt;
    }
}

ERW_Result CTransmissionWriter::Write(const void* buf,
                                      size_t      count,
                                      size_t*     bytes_written)
{
    ERW_Result res;
    size_t written;

    Int4 cnt = count;
    res = m_Wrt->Write(&cnt, sizeof(cnt), &written);
    if (res != eRW_Success) return res;
    if (written != sizeof(cnt)) {
        return eRW_Error;
    }
    size_t wrt_count = 0;
    for (const char* ptr = (char*)buf; count > 0; ptr += written) {
        res = m_Wrt->Write(ptr, count, &written);
        count -= written;
        wrt_count += written;
        if (res != eRW_Success) return res;
    }
    if (bytes_written)
        *bytes_written = wrt_count;
    return res;
}

ERW_Result CTransmissionWriter::Flush(void)
{
    return m_Wrt->Flush();
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

ERW_Result 
CTransmissionReader::Read(void*   buf,
                         size_t   count,
                         size_t*  bytes_read)
{
    ERW_Result res;
    if (!m_StartRead) {
        res = x_ReadStart();
        if (res != eRW_Success) return res;
    }

    // read packet header
    while (m_PacketBytesToRead == 0) {
        Int4 cnt;
        res = x_ReadRepeated(&cnt, sizeof(cnt));
        if (res != eRW_Success) { 
            if (bytes_read)
                *bytes_read = 0;
            return res;
        }
        if (m_ByteSwap) {
            m_PacketBytesToRead = CByteSwap::GetInt4((unsigned char*)&cnt);
        } else {
            m_PacketBytesToRead = cnt;
        }
    }

    size_t to_read = min(count, m_PacketBytesToRead);

    size_t read;
    res = m_Rdr->Read(buf, to_read, &read);
    m_PacketBytesToRead -= read;
    if (bytes_read)
        *bytes_read = read;
    return res;
}

ERW_Result 
CTransmissionReader::PendingCount(size_t* count)
{
    return m_Rdr->PendingCount(count);
}


ERW_Result 
CTransmissionReader::x_ReadStart()
{
    _ASSERT(!m_StartRead);

    m_StartRead = true;

    ERW_Result res;
    unsigned start_word = 0x01020304;
    unsigned start_word_coming;

    res = x_ReadRepeated(&start_word_coming, sizeof(start_word_coming));
    if (res != eRW_Success) return res;

    m_ByteSwap = (start_word_coming != start_word);

    _ASSERT(start_word_coming == 0x01020304 || 
            start_word_coming == 0x04020301);

    return res;
}

ERW_Result 
CTransmissionReader::x_ReadRepeated(void*   buf, size_t  count)
{
    ERW_Result res;
    char* ptr = (char*)buf;
    size_t read;
    
    for( ;count > 0; count -= read, ptr += read) {
        res = m_Rdr->Read(ptr, count, &read);
        if (res != eRW_Success) return res;
    } // for
    return res;
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2005/04/14 16:28:36  kuznets
 * Added ownership flags
 *
 * Revision 1.2  2005/04/14 14:12:26  ucko
 * +ncbidbg.hpp for _ASSERT
 *
 * Revision 1.1  2005/04/14 13:48:11  kuznets
 * Initial revision
 *
 *
 * ===========================================================================
 */


