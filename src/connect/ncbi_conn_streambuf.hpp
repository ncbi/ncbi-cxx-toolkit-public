#ifndef NCBI_CONN_STREAMBUF__HPP
#define NCBI_CONN_STREAMBUF__HPP

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
 * Authors:  Denis Vakatov, Anton Lavrentiev
 *
 * File Description:
 *   CONN-based C++ stream buffer
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.11  2002/02/05 16:05:26  lavr
 * List of included header files revised
 *
 * Revision 6.10  2002/02/04 20:19:10  lavr
 * +xsgetn() for MIPSPro compiler (buggy version supplied with std.library)
 *
 * Revision 6.9  2002/01/30 20:09:14  lavr
 * Define xsgetn() for WorkShop compiler also
 *
 * Revision 6.8  2002/01/28 20:21:11  lavr
 * Do not use auto_ptr in class body; auto_ptr moved to constructor
 *
 * Revision 6.7  2001/12/07 22:58:44  lavr
 * More comments added
 *
 * Revision 6.6  2001/05/29 19:35:21  grichenk
 * Fixed non-blocking stream reading for GCC
 *
 * Revision 6.5  2001/05/14 16:47:46  lavr
 * streambuf::xsgetn commented out as it badly interferes
 * with truly-blocking stream reading via istream::read.
 *
 * Revision 6.4  2001/05/11 14:04:08  grichenk
 * + CConn_Streambuf::xsgetn(), CConn_Streambuf::showmanyc()
 *
 * Revision 6.3  2001/01/12 23:49:20  lavr
 * Timeout and GetCONN method added
 *
 * Revision 6.2  2001/01/11 23:04:07  lavr
 * Bugfixes; tie is now done at streambuf level, not in iostream
 *
 * Revision 6.1  2001/01/09 23:34:51  vakatov
 * Initial revision (draft, not tested in run-time)
 *
 * ===========================================================================
 */

#include <corelib/ncbistre.hpp>
#include <connect/ncbi_connection.h>


BEGIN_NCBI_SCOPE


class CConn_Streambuf : public streambuf
{
public:
    CConn_Streambuf(CONNECTOR connector, const STimeout* timeout,
                    streamsize buf_size, bool tie);
    CONN GetCONN() const { return m_Conn; };
    virtual ~CConn_Streambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
#if defined(NCBI_COMPILER_GCC)      || \
    defined(NCBI_COMPILER_WORKSHOP) || \
    defined(NCBI_COMPILER_MIPSPRO)
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
#endif
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    CONN                m_Conn;      // underlying connection handle

    CT_CHAR_TYPE*       m_Buf;       // of size  2 * m_BufSize
    CT_CHAR_TYPE*       m_WriteBuf;  // m_Buf
    CT_CHAR_TYPE*       m_ReadBuf;   // m_Buf + m_BufSize
    streamsize          m_BufSize;   // of m_WriteBuf, m_ReadBuf

    bool                m_Tie;       // always flush before reading

    void x_CheckThrow(EIO_Status status, const string& msg);
};


END_NCBI_SCOPE

#endif  /* NCBI_CONN_STREAMBUF__HPP */
