#ifndef FCGIBUF__HPP
#define FCGIBUF__HPP

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
* ---------------------------------------------------------------------------
* $Log$
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

#include <corelib/ncbistd.hpp>

class FCGX_Stream;

BEGIN_NCBI_SCOPE

class CCgiObuffer : public IO_PREFIX::streambuf
{
public:
    CCgiObuffer(FCGX_Stream* out);

    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);

private:

    FCGX_Stream* m_out;
};

class CCgiIbuffer : public IO_PREFIX::streambuf
{
public:
    CCgiIbuffer(FCGX_Stream* in);

    virtual CT_INT_TYPE uflow(void);
    virtual CT_INT_TYPE underflow(void);

private:

    FCGX_Stream* m_in;
};

END_NCBI_SCOPE

#endif  /* FCGIBUF__HPP */
