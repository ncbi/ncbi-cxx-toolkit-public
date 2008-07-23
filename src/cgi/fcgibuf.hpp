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


struct FCGX_Stream;


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

#endif  /* CGI___FCGIBUF__HPP */
