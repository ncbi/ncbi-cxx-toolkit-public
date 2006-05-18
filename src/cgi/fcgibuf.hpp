#ifndef CGI___FCGIBUF__HPP
#define CGI___FCGIBUF__HPP

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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Adapter class between C++ and FastCGI streams.
 *
 */

#include <corelib/ncbistd.hpp>


class FCGX_Stream;


BEGIN_NCBI_SCOPE


class CCgiObuffer : public IO_PREFIX::streambuf
{
public:
    CCgiObuffer(FCGX_Stream* out);
    virtual ~CCgiObuffer(void);

    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual int         sync(void);

    unsigned int GetCount(void) const { return m_cnt; }

private:
    FCGX_Stream* m_out;
    unsigned int m_cnt;
};


class CCgiIbuffer : public IO_PREFIX::streambuf
{
public:
    CCgiIbuffer(FCGX_Stream* in);

    virtual CT_INT_TYPE underflow(void);

    unsigned int GetCount(void) const { return m_cnt; }

private:
    void x_Setg(void);

    FCGX_Stream* m_in;
    unsigned int m_cnt;
};


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.11  2006/05/18 19:05:26  grichenk
 * Added byte counter
 *
 * Revision 1.10  2003/10/16 15:18:55  lavr
 * Multiple flush bug fixed from R1.13
 *
 * Revision 1.8  2003/10/09 20:57:06  lavr
 * Bugfixes for the attempt to make I/O buffers fully buffered on top
 * of respective implementations of FCGI buffers in the FCGI library
 *
 * Revision 1.6  2003/10/07 14:40:28  lavr
 * CCgiIBuffer: made buffered on internal FGCX_Stream storage
 * CCgiOBuffer: buffering is done entirely on internal FGCX_Stream storage
 * Log moved to end; guard macro standardized
 *
 * Revision 1.5  2001/10/15 18:26:32  ucko
 * Implement sync; fix a typo
 *
 * Revision 1.4  1999/10/21 14:50:49  sandomir
 * optimization for overflow() (internal buffer added)
 *
 * Revision 1.3  1999/06/08 21:36:29  vakatov
 * #HAVE_NO_CHAR_TRAITS::  use "CT_XXX_TYPE" instead of "xxx_type" for
 * xxx = { "int", "char", "pos", "off" }
 *
 * Revision 1.2  1999/04/28 16:54:19  vasilche
 * Implemented stream input processing for FastCGI applications.
 *
 * Revision 1.1  1999/04/27 14:49:49  vasilche
 * Added FastCGI interface.
 * CNcbiContext renamed to CCgiContext.
 *
 * ===========================================================================
 */

#endif  /* CGI___FCGIBUF__HPP */
