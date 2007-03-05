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
    : m_out(out), m_cnt(0)
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
    m_cnt += ((unsigned char*) pptr()) - m_out->wrNext;

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
    : m_in(in), m_cnt(0)
{
    if (!m_in  ||  !m_in->isReader) {
        THROW1_TRACE(runtime_error, "CCgiObuffer: in is not reader");
    }

    x_Setg();
}


void CCgiIbuffer::x_Setg(void)
{
    m_cnt += m_in->stop - m_in->rdNext;
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
        x_Setg();
    }

    return sgetc();
}


END_NCBI_SCOPE
