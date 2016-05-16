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
 * Author:  Aaron Ucko
 *
 */

#include <ncbi_pch.hpp>
#include <html/indentstream.hpp>
#include <string.h>


BEGIN_NCBI_SCOPE


class CIndentingStreambuf : public CNcbiStreambuf
{
public:
    CIndentingStreambuf(CNcbiStreambuf* real_buf, SIZE_TYPE indent);
    ~CIndentingStreambuf(void);

protected:
    CNcbiStreambuf* setbuf   (CT_CHAR_TYPE* p, streamsize n);
    CT_POS_TYPE     seekoff  (CT_OFF_TYPE off, IOS_BASE::seekdir way,
                              IOS_BASE::openmode m);
    CT_POS_TYPE     seekpos  (CT_POS_TYPE pos, IOS_BASE::openmode m);
    int             sync     (void);
    streamsize      showmanyc(void);
    streamsize      xsgetn   (CT_CHAR_TYPE* p, streamsize n);
    CT_INT_TYPE     underflow(void);
    CT_INT_TYPE     uflow    (void);
    CT_INT_TYPE     pbackfail(CT_INT_TYPE c = CT_EOF);
    CT_INT_TYPE     overflow (CT_INT_TYPE c = CT_EOF);

private:
    CNcbiStreambuf*      m_Buf;
    CIndentingStreambuf* m_ISB;
    string               m_Indent;
    char                 m_PutArea[1024];
    bool                 m_OutputActive; // avoid double-indenting
    bool                 m_NeedIndent;
};


CIndentingOstream::CIndentingOstream(CNcbiOstream& real_stream,
                                     SIZE_TYPE indent)
    : CNcbiOstream(new CIndentingStreambuf(real_stream.rdbuf(), indent))
{
    return;
}


CIndentingStreambuf::CIndentingStreambuf(CNcbiStreambuf* real_buf,
                                         SIZE_TYPE indent)
    : m_OutputActive(false)
{
    m_ISB = dynamic_cast<CIndentingStreambuf*>(real_buf);
    if (m_ISB != 0) { // optimize
        m_ISB->overflow();
        m_Buf        = m_ISB->m_Buf;
        m_Indent     = m_ISB->m_Indent;
        m_NeedIndent = m_ISB->m_NeedIndent;
    } else {
        m_Buf        = real_buf;
        m_NeedIndent = true;
    }
    m_Indent.append(indent, ' ');
    setp(m_PutArea, m_PutArea + sizeof(m_PutArea));
}


CIndentingStreambuf::~CIndentingStreambuf()
{
    overflow();
    if (m_ISB) {
        // Make sure not to lose the information
        m_ISB->m_NeedIndent = m_NeedIndent;
    }
}


CT_INT_TYPE CIndentingStreambuf::overflow(CT_INT_TYPE c)
{
    if (m_NeedIndent  &&  pptr() != pbase()) {
        m_Buf->sputn(m_Indent.data(), m_Indent.size());
        m_NeedIndent = false;
    }
    if ( !m_OutputActive ) { // avoid double-indenting
        m_OutputActive = true;
        const char *p, *oldp = m_PutArea;
        while (oldp < pptr()
               &&  (p = (const char*)memchr(oldp, '\n', pptr() - oldp)) != 0) {
            m_Buf->sputn(oldp, p - oldp + 1);
            if (p == pptr() - 1) {
                m_NeedIndent = true; // defer
            } else {
                m_Buf->sputn(m_Indent.data(), m_Indent.size());
            }
            oldp = p + 1;
        }
        m_Buf->sputn(oldp, pptr() - oldp);
        m_OutputActive = false;
        setp(m_PutArea, m_PutArea + sizeof(m_PutArea));
    }
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        sputc(CT_TO_CHAR_TYPE(c));
    }
    return CT_NOT_EOF(CT_EOF);
}


int CIndentingStreambuf::sync(void)
{
    overflow();
    return m_Buf->PUBSYNC();
}


// Wrappers

CNcbiStreambuf* CIndentingStreambuf::setbuf(CT_CHAR_TYPE* p, streamsize n)
{
#ifdef NO_PUBSYNC
    return m_Buf->setbuf(p, n);
#else
    return m_Buf->pubsetbuf(p, n);
#endif
}


CT_POS_TYPE CIndentingStreambuf::seekoff(CT_OFF_TYPE off,
                                         IOS_BASE::seekdir way,
                                         IOS_BASE::openmode m)
{
    return m_Buf->PUBSEEKOFF(off, way, m);
}


CT_POS_TYPE CIndentingStreambuf::seekpos(CT_POS_TYPE pos, IOS_BASE::openmode m)
{
    return m_Buf->PUBSEEKPOS(pos, m);
}


streamsize CIndentingStreambuf::showmanyc(void)
{
    return m_Buf->in_avail();
}


streamsize CIndentingStreambuf::xsgetn(CT_CHAR_TYPE* p, streamsize n)
{
    return m_Buf->sgetn(p, n);
}


CT_INT_TYPE CIndentingStreambuf::underflow(void)
{
    return m_Buf->sgetc();
}


CT_INT_TYPE CIndentingStreambuf::uflow(void)
{
    return m_Buf->sbumpc();
}


CT_INT_TYPE CIndentingStreambuf::pbackfail(CT_INT_TYPE c)
{
    return (CT_EQ_INT_TYPE(c, CT_EOF) ? CT_EOF : m_Buf->sputbackc((char)c));
}


END_NCBI_SCOPE
