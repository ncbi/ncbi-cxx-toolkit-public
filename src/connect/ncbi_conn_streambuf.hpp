#ifndef CONNECT___NCBI_CONN_STREAMBUF__HPP
#define CONNECT___NCBI_CONN_STREAMBUF__HPP

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
 */

#include <corelib/ncbistre.hpp>
#include <connect/ncbi_connection.h>


#ifdef NCBI_COMPILER_MIPSPRO
#  include <util/stream_utils.hpp>
#  define CConn_StreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#elif defined(NCBI_COMPILER_GCC) && defined(NO_PUBSYNC)
#  include <util/stream_utils.hpp>
#  define CConn_StreambufBase CShowmanycStreambuf
#else
#  if defined(NCBI_COMPILER_WORKSHOP) && defined(_MT)
#    ifdef HAVE_IOS_XALLOC
#      undef  HAVE_BUGGY_IOS_CALLBACKS
#      define HAVE_BUGGY_IOS_CALLBACKS 1
#    endif
#  endif
#  define CConn_StreambufBase CNcbiStreambuf
#endif/*NCBI_COMPILER_MIPSPRO*/


BEGIN_NCBI_SCOPE


class CConn_Streambuf : public CConn_StreambufBase
{
public:
    CConn_Streambuf(CONNECTOR connector, const STimeout* timeout,
                    streamsize buf_size, bool tie);
    virtual ~CConn_Streambuf();
    CONN    GetCONN(void) const { return m_Conn; };

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual CNcbiStreambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    CONN                m_Conn;      // underlying connection handle

    CT_CHAR_TYPE*       m_Buf;       // of size  2 * m_BufSize
    CT_CHAR_TYPE*       m_ReadBuf;   // m_Buf
    CT_CHAR_TYPE*       m_WriteBuf;  // m_Buf + m_BufSize
    streamsize          m_BufSize;   // of m_WriteBuf, m_ReadBuf

    bool                m_Tie;       // always flush before reading

    EIO_Status          x_LogIfError(const char* file, int line,
                                     EIO_Status status, const string& msg);
};


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 6.27  2003/12/18 13:25:19  ucko
 * Renamed CGCC_ShowmanycStreambuf to CShowmanycStreambuf.
 *
 * Revision 6.26  2003/12/18 03:45:06  ucko
 * Add CGCC_ShowmanycStreambuf as a layer between streambuf and
 * CConn_Streambuf for the benefit of Readsome.
 *
 * Revision 6.25  2003/11/12 17:45:38  lavr
 * Minor rearrangement
 *
 * Revision 6.24  2003/10/22 18:15:34  lavr
 * CConn_Streambuf base class changed into CNcbiStreambuf
 *
 * Revision 6.23  2003/09/23 21:06:07  lavr
 * Rearranged included headers
 *
 * Revision 6.22  2003/09/23 01:43:03  ucko
 * Make sure to #include <corelib/ncbistre.hpp> unconditionally.
 * (The indirect include is now conditional on MIPSpro...)
 * Move BEGIN_NCBI_SCOPE to after all #includes.
 *
 * Revision 6.21  2003/09/22 20:45:27  lavr
 * Define HAVE_BUGGY_IOS_CALLBACKS locally (not to depend on xutil, excl IRIX)
 *
 * Revision 6.20  2003/05/20 18:05:53  lavr
 * x_LogIfError() to accept and print approproate file location
 *
 * Revision 6.19  2003/05/20 16:45:49  lavr
 * IsOkay() removed (GetCONN() should be enough)
 *
 * Revision 6.18  2003/05/12 18:32:27  lavr
 * Modified not to throw exceptions from stream buffer; few more improvements
 *
 * Revision 6.17  2003/04/22 18:29:38  lavr
 * Conditionally define CConn_Streambuf's base class (to help Doc++ SB)
 *
 * Revision 6.16  2003/04/11 17:57:11  lavr
 * Define xsgetn() unconditionally
 *
 * Revision 6.15  2003/03/30 07:00:09  lavr
 * MIPS-specific workaround for lamely-designed stream read ops
 *
 * Revision 6.14  2003/03/28 03:30:36  lavr
 * Define CConn_Streambuf::xsgetn() unconditionally of compiler
 *
 * Revision 6.13  2002/06/12 19:20:50  lavr
 * Guard macro name standardized
 *
 * Revision 6.12  2002/06/06 19:02:01  lavr
 * Take advantage of CConn_Exception class
 * Some housekeeping: guard macro name changed, log moved to the end
 *
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

#endif  /* CONNECT___NCBI_CONN_STREAMBUF__HPP */
