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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer implementations for connect library objects
 *
 */

#include <ncbi_pch.hpp>
#include <connect/ncbi_conn_reader_writer.hpp>


BEGIN_NCBI_SCOPE


ERW_Result CSocketReaderWriter::Pushback(const void* buf,
                                         size_t      count,
                                         void*       del_ptr)
{
    if (!m_Sock.IsOwned()) {
        ERW_Result result = x_Result(m_Sock->Pushback(buf, count));
        if (result != eRW_Success)
            return result;
    }
    delete[] (CT_CHAR_TYPE*) del_ptr;
    return eRW_Success;
}


ERW_Result CSocketReaderWriter::PendingCount(size_t* count)
{
    static const STimeout kZero = {0, 0};
    if (!m_Sock) {
        return eRW_Error;
    }
    const STimeout* tmp = m_Sock->GetTimeout(eIO_Read);
    STimeout tmo;
    if (tmp) {
        tmo = *tmp;
        tmp = &tmo;
    }
    if (m_Sock->SetTimeout(eIO_Read, &kZero) != eIO_Success) {
        return eRW_Error;
    }
    EIO_Status status = m_Sock->Read(0, 1, count, eIO_ReadPeek);
    if (m_Sock->SetTimeout(eIO_Read, tmp)    != eIO_Success) {
        return eRW_Error;
    }
    return status == eIO_Success  ||  status == eIO_Timeout
        ? eRW_Success : eRW_Error;
}


ERW_Result CSocketReaderWriter::x_Result(EIO_Status status)
{
    switch (status) {
    case eIO_Success:
        return eRW_Success;
    case eIO_Timeout:
        return eRW_Timeout;
    case eIO_Closed:
        return eRW_Eof;
    default:
        break;
    }
    return eRW_Error;
}


END_NCBI_SCOPE
