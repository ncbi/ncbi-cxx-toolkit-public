#ifndef CONNECT___NCBI_CONN_READER_WRITER__HPP
#define CONNECT___NCBI_CONN_READER_WRITER__HPP

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
 * Author:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer implementations for connect library objects
 *
 */

#include <corelib/ncbimisc.hpp>
#include <util/reader_writer.hpp>


/** @addtogroup ReaderWriter
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/* CSocket must live longer than CSocketReader created
 * on top of that socket.  CSocketReader does NOT own CSocket.
 */


class NCBI_XCONNECT_EXPORT CSocketReader
{
public:
    CSocketReader(CSocket* sock, EOwnership if_to_own = eNoOwnership);
    virtual ~CSocketReader();

    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0);

    virtual ERW_Result PendingCount(size_t* count);

protected:
    CSocket*   m_Sock;
    EOwnership m_Owned;
};


CSocketReader::CSocketReader(CSocket* sock, EOwnership if_to_own)
    : m_Sock(sock), m_Owned(if_to_own)
{
}


CSocketReader::~CSocketReader()
{
    if (m_Owned) {
        delete m_Sock;
    }
}


ERW_Result CSocketReader::PendingCount(size_t* count)
{
    if (!m_Sock) {
        return eRW_Error;
    }
    STimeout zero = {0, 0}, tmo;
    if (m_Sock->GetTimeout(eIO_Read, &tmo) != eIO_Success) {
        return eRW_Error;
    }
    if (m_Sock->SetTimeout(eIO_Read, &zero) != eIO_Success) {
        return eRW_Error;
    }
    char c;
    EIO_Status status = m_Sock->Read(&c, sizeof(c), count, eIO_ReadPeek);
    if (m_Sock->SetTimeout(eIO_Read, &tmo) != eIO_Success) {
        return eRW_Error;
    }
    return status == eIO_Success ? eRW_Success : eIO_Error;
}


ERW_Result CSocketReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    if (!m_Sock)
        return eRW_Error;
    switch (m_Sock->Read(buf, count, bytes_read, eIO_ReadPlain)) {
    case eIO_Timeout:
        return eRW_Timeout;
    case eIO_Success:
        return eRW_Success;
    case eIO_Closed:
        return eRW_Eof;
    default:
        break;
    }
    return eRW_Error;
}



END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2004/10/01 18:27:41  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif  /* CONNECT___NCBI_CONN_READER_WRITER__HPP */
