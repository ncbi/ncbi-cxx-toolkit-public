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

#include <corelib/ncbistd.hpp>
#include <corelib/fcgibuf.hpp>
#include <fcgiapp.h>
#include <algorithm>

BEGIN_NCBI_SCOPE

CCgiObuffer::CCgiObuffer(FCGX_Stream* out)
    : m_out(out)
{
    if ( !out || out->isReader ) {
        _TRACE_THROW();
        throw runtime_error("CCgiObuffer: out is not writer");
    }
}

CCgiObuffer::int_type CCgiObuffer::overflow(int_type c)
{
    FCGX_Stream* out = m_out;
    const char_type* from = pbase(), * to = pptr();
    streamsize count = to - from;
    streamsize bumpCount = count;
    while ( count > 0 ) {
        streamsize chunk = min(count, streamsize(out->stop - out->wrNext));
        if ( chunk == 0 ) {
            // output stream overflow - we need flush
            if ( out->isClosed || out->isReader ) {
                return traits_type::eof();
            }
            out->emptyBuffProc(out, false);
            if ( out->stop == out->wrNext ) {
                if ( !out->isClosed ) {
                    _TRACE_THROW();
                    throw runtime_error("CCgiObuffer::overflow: error in emptyBuffProc");
                }
            }
        }
        else {
            memcpy(out->wrNext, from, chunk);
            out->wrNext += chunk;
            from += chunk;
            count -= chunk;
        }
    }
    pbump(bumpCount);

    if ( c == traits_type::eof() ) {
        return traits_type::eof();
    }

    if ( out->wrNext == out->stop ) {
        out->emptyBuffProc(out, false);
        if ( out->wrNext == out->stop ) {
            if ( !out->isClosed ) {
                _TRACE_THROW();
                throw runtime_error("CCgiObuffer::overflow: error in emptyBuffProc");
            }
            return traits_type::eof();
        }
    }

    return traits_type::not_eof(*out->wrNext++ = c);
}

CCgiIbuffer::CCgiIbuffer(FCGX_Stream* in)
    : m_in(in)
{
    if ( !in || !in->isReader ) {
        _TRACE_THROW();
        throw runtime_error("CCgiObuffer: out is not reader");
    }
}

CCgiObuffer::int_type CCgiIbuffer::uflow(void)
{
    FCGX_Stream* in = m_in;

    if ( in->rdNext == in->stop ) {
        if ( in->isClosed || !in->isReader ) {
            return traits_type::eof();
        }

        in->fillBuffProc(in);
        in->stopUnget = in->rdNext;

        if ( in->rdNext == in->stop ) {
            _ASSERT(in->isClosed); /* bug in fillBufProc if not */
            return traits_type::eof();
        }
    }

    return traits_type::not_eof(*in->rdNext++);
}

CCgiObuffer::int_type CCgiIbuffer::underflow(void)
{
    FCGX_Stream* in = m_in;

    if ( in->rdNext == in->stop ) {
        if ( in->isClosed || !in->isReader ) {
            return traits_type::eof();
        }

        in->fillBuffProc(in);
        in->stopUnget = in->rdNext;

        if ( in->rdNext == in->stop ) {
            _ASSERT(in->isClosed); /* bug in fillBufProc if not */
            return traits_type::eof();
        }
    }

    return traits_type::not_eof(*in->rdNext);
}

END_NCBI_SCOPE
