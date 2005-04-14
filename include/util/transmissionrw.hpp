#ifndef UTIL___TRANSRWCHECK__HPP
#define UTIL___TRANSRWCHECK__HPP

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

#include <util/reader_writer.hpp>

BEGIN_NCBI_SCOPE

/// IReader implementation with transmission control
///
class NCBI_XUTIL_EXPORT CTransmissionReader : public IReader
{
public:
    /// Constructed on another IReader 
    /// (supposed to implement the actual transmission)
    ///
    CTransmissionReader(IReader* rdr);
    size_t GetPacketBytesToRead() const { return m_PacketBytesToRead; }

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);



private:

    ERW_Result x_ReadStart();
    ERW_Result x_ReadRepeated(void*   buf,
                              size_t  count);

private:
    CTransmissionReader(const CTransmissionReader&);
    CTransmissionReader& operator=(const CTransmissionReader&);

private:
    IReader*   m_Rdr;
    size_t     m_PacketBytesToRead;
    bool       m_ByteSwap;
    bool       m_StartRead;
};

/// IWriter with transmission control
///
class NCBI_XUTIL_EXPORT CTransmissionWriter : public IWriter
{
public:
    /// Constructed on another IWriter (channel level)
    ///
    CTransmissionWriter(IWriter* wrt);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void);

private:
    IWriter*     m_Wrt;
};

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2005/04/14 13:46:59  kuznets
 * Initial release
 *
 *
 * ===========================================================================
 */

#endif /* UTIL___TRANSRWCHECK__HPP */
