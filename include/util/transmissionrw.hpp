#ifndef UTIL___TRANSMISSIONRW__HPP
#define UTIL___TRANSMISSIONRW__HPP

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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Reader writer with transmission checking
 *
 */

/// @file transmissionrw.hpp
/// Reader writer with transmission checking
/// @sa IReader, IWriter

#include <corelib/ncbimisc.hpp>
#include <corelib/reader_writer.hpp>
#include <corelib/ncbidbg.hpp> // for _ASSERT


BEGIN_NCBI_SCOPE


/// IReader implementation with transmission control
///
class NCBI_XUTIL_EXPORT CTransmissionReader : public IReader
{
public:
    /// Constructed on another IReader 
    /// (supposed to implement the actual transmission)
    ///
    /// @param rdr 
    ///    Reader to transmit data (comm. level)
    ///
    explicit CTransmissionReader(IReader* rdr, 
                                 EOwnership own_reader = eNoOwnership);
    virtual ~CTransmissionReader();

    size_t GetPacketBytesToRead() const { return m_PacketBytesToRead; }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);


    /// Get underlying reader
    IReader& GetReader() { _ASSERT(m_Rdr); return *m_Rdr; }

private:

    ERW_Result x_ReadStart();
    ERW_Result x_ReadRepeated(void*   buf,
                              size_t  count);

private:
    CTransmissionReader(const CTransmissionReader&);
    CTransmissionReader& operator=(const CTransmissionReader&);

private:
    IReader*   m_Rdr;
    EOwnership m_OwnRdr;
    size_t     m_PacketBytesToRead;
    bool       m_ByteSwap;
    bool       m_StartRead;
};


/// IWriter with transmission control
///
class NCBI_XUTIL_EXPORT CTransmissionWriter : public IWriter
{
public:
    enum ESendEofPacket {
        eSendEofPacket,     ///< Writer will send EOF packet in the destructor
        eDontSendEofPacket  ///< Writer will not send EOF packet in the destructor
    };
    /// Constructed on another IWriter (comm. level)
    ///
    explicit CTransmissionWriter(IWriter* wrt, 
                                 EOwnership own_writer = eNoOwnership,
                                 ESendEofPacket send_eof = eDontSendEofPacket );

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

    ERW_Result Close(void);

    void SetSendEof(ESendEofPacket flag) { m_SendEof = flag; }

    virtual ~CTransmissionWriter();

    /// Get underlying writer
    IWriter& GetWriter() { _ASSERT(m_Wrt); return *m_Wrt; }

private:
    ERW_Result x_WritePacket(const void* buf,
                             size_t      count,
                             size_t&     bytes_written);

private:
    CTransmissionWriter(const CTransmissionWriter&);
    CTransmissionWriter& operator=(CTransmissionWriter&);
private:
    IWriter*     m_Wrt;
    EOwnership   m_OwnWrt;
    ESendEofPacket m_SendEof;
    size_t m_ToWrite;
};


END_NCBI_SCOPE

#endif /* UTIL___TRANSMISSIONRW__HPP */
