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
* Revision 1.9  2001/10/15 18:26:32  ucko
* Implement sync; fix a typo
*
* Revision 1.8  1999/10/26 18:11:57  vakatov
* CCgiObuffer::overflow() -- get rid of unused var "bumpCount" (warning)
*
* Revision 1.7  1999/10/21 14:50:49  sandomir
* optimization for overflow() (internal buffer added)
*
* Revision 1.6  1999/06/08 21:36:29  vakatov
* #HAVE_NO_CHAR_TRAITS::  use "CT_XXX_TYPE" instead of "xxx_type" for
* xxx = { "int", "char", "pos", "off" }
*
* Revision 1.5  1999/05/17 00:26:18  vakatov
* Use double-quote rather than angle-brackets for the private headers
*
* Revision 1.4  1999/05/06 23:18:10  vakatov
* <fcgibuf.hpp> became a local header file.
*
* Revision 1.3  1999/05/03 20:32:28  vakatov
* Use the (newly introduced) macro from <corelib/ncbidbg.h>:
*   RETHROW_TRACE,
*   THROW0_TRACE(exception_class),
*   THROW1_TRACE(exception_class, exception_arg),
*   THROW_TRACE(exception_class, exception_args)
* instead of the former (now obsolete) macro _TRACE_THROW.
*
* Revision 1.2  1999/04/28 16:54:41  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.1  1999/04/27 14:50:05  vasilche
* Added FastCGI interface.
* CNcbiContext renamed to CCgiContext.
*
* ===========================================================================
*/

// NCBI C++ headers
#include <corelib/ncbistd.hpp>
// 3rd-party headers
#include <fcgiapp.h>
// local headers
#include "fcgibuf.hpp"
// C++ headers
#include <algorithm>

BEGIN_NCBI_SCOPE

CCgiObuffer::CCgiObuffer(FCGX_Stream* out, int bufsize /*=512*/ )
    : m_out(out), m_bufsize( bufsize <= 0 ? 1 : bufsize )
{
    if ( !out || out->isReader ) {
        THROW1_TRACE(runtime_error, "CCgiObuffer: out is not writer");
    }

    m_buf = new char[ m_bufsize ];
    setp( m_buf, m_buf + m_bufsize );
}

CCgiObuffer::~CCgiObuffer(void)
{
    overflow( CT_EOF );
    delete[] m_buf;
}

CT_INT_TYPE CCgiObuffer::overflow(CT_INT_TYPE c)
{
    FCGX_Stream* out = m_out;
    const CT_CHAR_TYPE* from = pbase(), * to = pptr();
    streamsize count = to - from;

    while ( count > 0 ) {
        streamsize chunk = min(count, streamsize(out->stop - out->wrNext));
        if ( chunk == 0 ) {
            _TRACE("flushing FCGX_Stream");
            // output stream overflow - we need flush
            if ( out->isClosed || out->isReader ) {
                return CT_EOF;
            }
            out->emptyBuffProc(out, false);
            if (out->stop == out->wrNext  &&  !out->isClosed) {
                THROW1_TRACE(runtime_error,
                             "CCgiObuffer::overflow: error in emptyBuffProc");
            }
        }
        else {
            memcpy(out->wrNext, from, chunk);
            out->wrNext += chunk;
            from += chunk;
            count -= chunk;
        }
    }
   
    setp( m_buf, m_buf + m_bufsize );
   
    if ( c == CT_EOF ) {
        return 0;
    }

    if ( out->wrNext == out->stop ) {
        _TRACE("flushing FCGX_Stream");
        out->emptyBuffProc(out, false);
        if ( out->wrNext == out->stop ) {
            if ( !out->isClosed ) {
                THROW1_TRACE(runtime_error,
                             "CCgiObuffer::overflow: error in emptyBuffProc");
            }
            return CT_EOF;
        }
    }

    return CT_NOT_EOF(*out->wrNext++ = c);
}

int CCgiObuffer::sync(void)
{
    _TRACE("CCgiObuffer::sync");
    CT_INT_TYPE result1 = CT_EQ_INT_TYPE(overflow(CT_EOF), CT_EOF);
    int         result2 = FCGX_FFlush(m_out);
    if (result1 || result2) {
        return CT_EOF;
    } else {
        return 0;
    }
}


CCgiIbuffer::CCgiIbuffer(FCGX_Stream* in)
    : m_in(in)
{
    if ( !in || !in->isReader ) {
        THROW1_TRACE(runtime_error, "CCgiObuffer: in is not reader");
    }
}

CT_INT_TYPE CCgiIbuffer::uflow(void)
{
    FCGX_Stream* in = m_in;

    if ( in->rdNext == in->stop ) {
        if ( in->isClosed || !in->isReader ) {
            return CT_EOF;
        }

        in->fillBuffProc(in);
        in->stopUnget = in->rdNext;

        if ( in->rdNext == in->stop ) {
            _ASSERT(in->isClosed); /* bug in fillBufProc if not */
            return CT_EOF;
        }
    }

    return CT_NOT_EOF(*in->rdNext++);
}

CT_INT_TYPE CCgiIbuffer::underflow(void)
{
    FCGX_Stream* in = m_in;

    if ( in->rdNext == in->stop ) {
        if ( in->isClosed || !in->isReader ) {
            return CT_EOF;
        }

        in->fillBuffProc(in);
        in->stopUnget = in->rdNext;

        if ( in->rdNext == in->stop ) {
            _ASSERT(in->isClosed); /* bug in fillBufProc if not */
            return CT_EOF;
        }
    }

    return CT_NOT_EOF(*in->rdNext);
}

END_NCBI_SCOPE
