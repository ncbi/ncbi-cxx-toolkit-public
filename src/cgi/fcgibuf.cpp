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
 *   FCGI streambufs
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <fcgiapp.h>
#include "fcgibuf.hpp"
#include <algorithm>


BEGIN_NCBI_SCOPE


CCgiObuffer::CCgiObuffer(FCGX_Stream* out)
    : m_out(out)
{
    if (!m_out  ||  m_out->isReader) {
        THROW1_TRACE(runtime_error, "CCgiObuffer: out is not writer");
    }

    setp((CT_CHAR_TYPE*) m_out->wrNext, (CT_CHAR_TYPE*) m_out->stop);
}


CCgiObuffer::~CCgiObuffer(void)
{
    (void) sync();
}


CT_INT_TYPE CCgiObuffer::overflow(CT_INT_TYPE c)
{
    _ASSERT(m_out->stop == (unsigned char*) epptr());
    m_out->wrNext = (unsigned char*) pptr();

    if (m_out->wrNext == m_out->stop) {
        if (m_out->isClosed  || !m_out->emptyBuffProc) {
            return CT_EOF;
        }

        m_out->emptyBuffProc(m_out, false);
        if (m_out->wrNext == m_out->stop) {
            if ( !m_out->isClosed ) {
                THROW1_TRACE(runtime_error,
                             "CCgiObuffer::overflow: error in emptyBuffProc");
            }
            return CT_EOF;
        }

        setp((CT_CHAR_TYPE*) m_out->wrNext, (CT_CHAR_TYPE*) m_out->stop);
    }

    return CT_EQ_INT_TYPE(c, CT_EOF)
        ? CT_NOT_EOF(CT_EOF) : sputc(CT_TO_CHAR_TYPE(c));
}


int CCgiObuffer::sync(void)
{
    CT_INT_TYPE oflow = overflow(CT_EOF);
    if ( CT_EQ_INT_TYPE(oflow, CT_EOF) )
        return -1;
    int flush = FCGX_FFlush(m_out);
    setp((CT_CHAR_TYPE*) m_out->wrNext, (CT_CHAR_TYPE*) m_out->stop);
    return flush ? -1 : 0;
}


CCgiIbuffer::CCgiIbuffer(FCGX_Stream* in)
    : m_in(in)
{
    if (!m_in  ||  !m_in->isReader) {
        THROW1_TRACE(runtime_error, "CCgiObuffer: in is not reader");
    }

    setg((CT_CHAR_TYPE*) m_in->stopUnget,
         (CT_CHAR_TYPE*) m_in->rdNext, (CT_CHAR_TYPE*) m_in->stop);
}


CT_INT_TYPE CCgiIbuffer::underflow(void)
{
    _ASSERT(m_in->stop == (unsigned char*) egptr());
    m_in->rdNext = (unsigned char*) gptr();

    if (m_in->rdNext == m_in->stop) {
        if (m_in->isClosed  ||  !m_in->fillBuffProc) {
            return CT_EOF;
        }

        m_in->fillBuffProc(m_in);
        if (m_in->rdNext == m_in->stop) {
            if ( !m_in->isClosed ) {
                THROW1_TRACE(runtime_error,
                             "CCgiIbuffer::underflow: error in fillBuffProc");
            }
            return CT_EOF;
        }

        m_in->stopUnget = m_in->rdNext;
        setg((CT_CHAR_TYPE*) m_in->stopUnget,
             (CT_CHAR_TYPE*) m_in->rdNext, (CT_CHAR_TYPE*) m_in->stop);
    }

    return sgetc();
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.16  2004/05/17 20:56:50  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.15  2003/10/16 15:18:55  lavr
 * Multiple flush bug fixed from R1.13
 *
 * Revision 1.13  2003/10/09 20:57:06  lavr
 * Bugfixes for the attempt to make I/O buffers fully buffered on top
 * of respective implementations of FCGI buffers in the FCGI library
 *
 * Revision 1.11  2003/10/07 14:40:28  lavr
 * CCgiIBuffer: made buffered on internal FGCX_Stream storage
 * CCgiOBuffer: buffering is done entirely on internal FGCX_Stream storage
 * Log moved to end; guard macro standardized
 *
 * Revision 1.10  2001/12/04 22:36:10  vakatov
 * Removed obnoxious TRACE in CCgiObuffer::sync()
 *
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
