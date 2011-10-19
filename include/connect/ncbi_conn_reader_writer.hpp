#ifndef CONNECT___NCBI_CONN_READER_WRITER__HPP
#define CONNECT___NCBI_CONN_READER_WRITER__HPP

/* $Id$
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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer implementations for connect library objects
 *
 */

#include <corelib/reader_writer.hpp>
#include <connect/ncbi_core_cxx.hpp>
#include <connect/ncbi_socket.hpp>


/** @addtogroup ReaderWriter
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCONNECT_EXPORT CSocketReaderWriter : public    IReaderWriter,
                                                 protected CConnIniter
{
public:
    CSocketReaderWriter(CSocket* sock, EOwnership if_to_own = eNoOwnership);
    virtual ~CSocketReaderWriter();

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0);

    virtual ERW_Result Flush(void) { return eRW_NotImplemented; };

    const STimeout*    GetTimeout(EIO_Event event) const;

    ERW_Result         SetTimeout(EIO_Event event, const STimeout* timeout);

protected:
    ERW_Result x_Result(EIO_Status status);

    CSocket*   m_Sock;
    EOwnership m_IsOwned;

private:
    CSocketReaderWriter(const CSocketReaderWriter&);
    CSocketReaderWriter operator=(const CSocketReaderWriter&);
}; 



inline CSocketReaderWriter::CSocketReaderWriter(CSocket*   sock,
                                                EOwnership if_to_own)
    : m_Sock(sock), m_IsOwned(if_to_own)
{
}


inline CSocketReaderWriter::~CSocketReaderWriter()
{
    if (m_IsOwned) {
        delete m_Sock;
    }
}


inline ERW_Result CSocketReaderWriter::Read(void*   buf,
                                            size_t  count,
                                            size_t* n_read)
{
    return m_Sock
        ? x_Result(m_Sock->Read(buf, count, n_read, eIO_ReadPlain))
        : eRW_Error;
}


inline ERW_Result CSocketReaderWriter::Write(const void* buf,
                                             size_t      count,
                                             size_t*     n_written)
{
    return m_Sock
        ? x_Result(m_Sock->Write(buf, count, n_written))
        : eRW_Error;
}


inline const STimeout* CSocketReaderWriter::GetTimeout(EIO_Event event) const
{
    return m_Sock ? m_Sock->GetTimeout(event) : 0;
}


inline ERW_Result CSocketReaderWriter::SetTimeout(EIO_Event       event,
                                                  const STimeout* timeout)
{
    return m_Sock
        ? x_Result(m_Sock->SetTimeout(event, timeout))
        : eRW_Error;
}


END_NCBI_SCOPE


/* @} */

#endif  /* CONNECT___NCBI_CONN_READER_WRITER__HPP */
