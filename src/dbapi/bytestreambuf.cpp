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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.9  2004/05/17 21:10:28  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.8  2002/07/12 13:34:11  kholodov
* Fixed: incorrect length reported for NULL BLOB
*
* Revision 1.7  2002/06/21 14:41:32  kholodov
* Fixed: reset total bytes written for debug output
*
* Revision 1.6  2002/05/16 22:09:19  kholodov
* Fixed: incorrect start of BLOB column
*
* Revision 1.5  2002/05/14 19:51:47  kholodov
* Fixed: incorrect column no handling for detecting end of column
*
* Revision 1.4  2002/05/13 19:11:53  kholodov
* Modified: added proper handling of EOFs while reading column data using CDB_Result::CurrentItemNo().
*
* Revision 1.3  2002/04/03 20:06:25  ucko
* Always return >= 1 from showmanyc() to avoid spurious "EOF"s.
* Pass status messages to _TRACE rather than writing them to cout.
* #if out buggy code in destructor.
*
* Revision 1.2  2002/02/14 00:59:40  vakatov
* Clean-up: warning elimination, fool-proofing, fine-tuning, identation, etc.
*
* Revision 1.1  2002/01/30 14:51:21  kholodov
* User DBAPI implementation, first commit
*
*
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <exception>
#include <algorithm>
#include "bytestreambuf.hpp"
#include <dbapi/driver/exception.hpp>
#include <dbapi/driver/interfaces.hpp>
#include <dbapi/driver/public.hpp>

BEGIN_NCBI_SCOPE

static const streamsize DEF_BUFSIZE = 2048;

CByteStreamBuf::CByteStreamBuf(streamsize bufsize)
    : m_buf(0), 
    m_size(bufsize > 0 ? bufsize : DEF_BUFSIZE), 
    m_len(0), m_rs(0), m_cmd(0), m_column(-1)
{ 
    m_buf = new CT_CHAR_TYPE[m_size * 2]; // read and write buffer in one
    setg(0, 0, 0); // call underflow on the first read

    setp(getPBuf(), getPBuf() + m_size);
    _TRACE("I/O buffer size: " << m_size);
}

CByteStreamBuf::~CByteStreamBuf()
{
#if 0 // misbehaves
    if( m_rs != 0 && m_len > 0 )
        m_rs->SkipItem();
#endif

    delete[] m_buf;
    delete m_cmd;
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

void CByteStreamBuf::SetRs(CDB_Result* rs) {
    delete m_rs;
    m_rs = rs;
    m_column = m_rs->CurrentItemNo();
}

CT_INT_TYPE CByteStreamBuf::underflow()
{
    if( m_rs == 0 )
        throw runtime_error("CByteStreamBuf::underflow(): CDB_Result* is null");
  
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
        m_len = m_rs->ReadItem(getGBuf(), m_size);
        _TRACE("Column: " << m_column << ", Bytes read to buffer: " << m_len);
        if( m_len == 0 )
            return CT_EOF;
        total += m_len;
        setg(getGBuf(), getGBuf(), getGBuf() + m_len);
        return CT_TO_INT_TYPE(*getGBuf());
    }
    
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
    if( put > 0 ) {
        setp(getPBuf(), getPBuf() + m_size );

        if( ! CT_EQ_INT_TYPE(c, CT_EOF) )
            sputc(CT_TO_CHAR_TYPE(c));

        return c;
    }
    else {
        _TRACE("Total sent: " << total);
        total = 0;
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
