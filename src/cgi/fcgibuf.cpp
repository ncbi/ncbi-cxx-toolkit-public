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
*   !!! PUT YOUR DESCRIPTION HERE !!!
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1999/04/27 14:50:05  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <corelib/fcgibuf.hpp>
#include <fcgiapp.h>

BEGIN_NCBI_SCOPE

CCgiObuffer::CCgiObuffer(FCGX_Stream* out)
    : m_out(out)
{
}

CCgiObuffer::int_type CCgiObuffer::overflow(int_type c)
{
    const char* from = pbase(), * to = pptr();
    for ( const char_type* ptr = from; ptr != to; ++ptr ) {
        FCGX_PutChar(*ptr, m_out);
    }

    pbump(from - to);

    if ( c == traits_type::eof() ) {
        return traits_type::eof();
    }
    else {
        FCGX_PutChar(c, m_out);
        return traits_type::not_eof(c);
    }
}

END_NCBI_SCOPE
