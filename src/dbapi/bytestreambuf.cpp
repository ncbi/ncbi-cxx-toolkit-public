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
* Author: Michael Kholodov
*
* File Description: streambuf implementation for BLOBs
*/

#include <ncbi_pch.hpp>
#include <exception>
#include <algorithm>
#include "bytestreambuf.hpp"
#include <dbapi/driver/public.hpp>
#include <dbapi/error_codes.hpp>
#include "rs_impl.hpp"


#define NCBI_USE_ERRCODE_X   Dbapi_BlobStream

BEGIN_NCBI_SCOPE

enum { DEF_BUFSIZE = 2048 };

CByteStreamBuf::CByteStreamBuf(streamsize bufsize, TBlobOStreamFlags flags,
                               CDB_Connection* conn)
    : m_buf(0), 
    m_size(bufsize > 0 ? size_t(bufsize) : DEF_BUFSIZE), 
    /* m_len(0),*/ m_rs(0), m_cmd(0) //, m_column(-1)
{ 
    m_buf = new CT_CHAR_TYPE[m_size * 2]; // read and write buffer in one
    setg(0, 0, 0); // call underflow on the first read

    setp(getPBuf(), getPBuf() + m_size);
    _TRACE("I/O buffer size: " << m_size);
    if ((flags & fBOS_UseTransaction) != 0) {
        _ASSERT(conn != NULL);
        m_AutoTrans.reset(new CAutoTrans(*conn));
    }
}

CByteStreamBuf::~CByteStreamBuf()
{
    try {
#if 0 // misbehaves
        if( m_rs != 0 && m_len > 0 )
            m_rs->SkipItem();
#endif

        delete[] m_buf;
        delete m_cmd;
    }
    NCBI_CATCH_ALL_X( 3, kEmptyStr )
}

CT_CHAR_TYPE* CByteStreamBuf::getGBuf()
{
    return m_buf;
}

CT_CHAR_TYPE* CByteStreamBuf::getPBuf() 
{
    return m_buf + m_size;
}

void CByteStreamBuf::SetCmd(CDB_SendDataCmd* cmd) {
    delete m_cmd;
    m_cmd = cmd;
}

void CByteStreamBuf::SetRs(CResultSet* rs) {
    //delete m_rs;
    m_rs = rs;
    //m_column = m_rs->CurrentItemNo();
}

CT_INT_TYPE CByteStreamBuf::underflow()
{
    if( m_rs == 0 )
        throw runtime_error("CByteStreamBuf::underflow(): CResultSet* is null");
  
#if 0
    static size_t total = 0;

    if( m_column < 0 || m_column != m_rs->CurrentItemNo() ) {
        if( m_column < 0 ) {
            _TRACE("Column for ReadItem not set, current column: "
                   << m_rs->CurrentItemNo());
#ifdef _DEBUG
            _ASSERT(0);
#endif
        }
        else
            _TRACE("Total read from ReadItem: " << total);
        total = 0;
        m_column = m_rs->CurrentItemNo();
        return CT_EOF;
    }
    else {
#endif
        size_t len = m_rs->Read(getGBuf(), m_size);
        _TRACE("Column: " << m_rs->GetColumnNo() << ", Bytes read to buffer: " << len);
        if( len == 0 )
            return CT_EOF;
        //total += len;
        setg(getGBuf(), getGBuf(), getGBuf() + len);
        return CT_TO_INT_TYPE(*getGBuf());
#if 0
    }
#endif
    
}

CT_INT_TYPE CByteStreamBuf::overflow(CT_INT_TYPE c)
{
    if( m_cmd == 0 ) {
        throw runtime_error
            ("CByteStreamBuf::overflow(): CDB_SendDataCmd* is null");
    }

    static size_t total = 0;
    size_t put = m_cmd->SendChunk(pbase(), pptr() - pbase());
    total += put;
    if (m_AutoTrans.get()  &&  m_cmd->HasMoreResults()) {
        m_AutoTrans->Finish();
        m_AutoTrans.reset();
    }
    if( put > 0 ) {
        setp(getPBuf(), getPBuf() + m_size );

        if( ! CT_EQ_INT_TYPE(c, CT_EOF) )
            sputc(CT_TO_CHAR_TYPE(c));

        return c;
    }
    else {
        _TRACE("Total sent: " << total);
        total = 0;
        m_AutoTrans.reset();
        return CT_EOF;
    }
    
}

int CByteStreamBuf::sync()
{
    overflow(CT_EOF);
    return 0;
}

streambuf* 
CByteStreamBuf::setbuf(CT_CHAR_TYPE* /*p*/, streamsize /*n*/)
{
    throw runtime_error("CByteStreamBuf::setbuf(): not allowed");
}


streamsize CByteStreamBuf::showmanyc() 
{
    streamsize left = egptr() - gptr();
    return min(left, (streamsize)1);
}
//======================================================
END_NCBI_SCOPE
