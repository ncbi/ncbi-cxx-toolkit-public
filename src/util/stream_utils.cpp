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
 * Author:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *   Perform stream pushback
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2001/12/07 22:59:36  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#include <util/stream_pushback.hpp>
#include <assert.h>
#include <string.h>

#ifdef NCBI_COMPILER_GCC
#define PUBSYNC sync
#else
#define PUBSYNC pubsync
#endif


BEGIN_NCBI_SCOPE


/*****************************************************************************
 *  Helper class: internal streambuf to be substituted instead of
 *  the original one in the stream, when the data are pushed back.
 */

class CPushback_Streambuf : public streambuf
{
    friend streambuf* UTIL_StreamPushback(istream& is, CT_CHAR_TYPE* buf,
                                          streamsize size, void* del_ptr);
public:
    CPushback_Streambuf(istream& istream, CT_CHAR_TYPE* buf,
                        streamsize size, void* del_ptr);
    virtual ~CPushback_Streambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
#ifdef NCBI_COMPILER_GCC
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
#endif
    virtual streamsize  showmanyc(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n);

    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    streambuf*    x_SelfDestroy(void);

    bool          m_SelfDestroy;

    istream&      m_Is;
    streambuf*    m_Sb;
    CT_CHAR_TYPE* m_Buf;
    streamsize    m_Size;
    void*         m_DelPtr;
};


CPushback_Streambuf::CPushback_Streambuf(istream&      is,
                                         CT_CHAR_TYPE* buf,
                                         streamsize    size,
                                         void*         del_ptr) :
    m_SelfDestroy(false),
    m_Is(is), m_Sb(is.rdbuf()), m_Buf(buf), m_Size(size), m_DelPtr(del_ptr)
{
    setp(0, 0); // unbuffered output at this level of streambuf's hierachy
    setg(m_Buf, m_Buf, m_Buf + m_Size);
    m_Is.rdbuf(this);
}


CPushback_Streambuf::~CPushback_Streambuf()
{
    if ( m_DelPtr ) {
        delete[] (char*) m_DelPtr;
    }

    if (m_SelfDestroy) {
        m_Is.rdbuf(m_Sb);
    } else {
        delete m_Sb;
    }
}


CT_INT_TYPE CPushback_Streambuf::overflow(CT_INT_TYPE c)
{
    if (CT_EQ_INT_TYPE(c, CT_EOF)) {
        return m_Sb->sputc(CT_TO_CHAR_TYPE(c));
    }
    if (m_Sb->PUBSYNC() == 0) {
        return CT_NOT_EOF(CT_EOF); // success
    }
    return CT_EOF; // failure
}


CT_INT_TYPE CPushback_Streambuf::underflow(void)
{
    // we are here because there is no more data in the pushback buffer
    streambuf* sb = x_SelfDestroy();
    return sb->sgetc();
}


#ifdef NCBI_COMPILER_GCC
streamsize CPushback_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if (gptr() >= egptr()) {
        streambuf* sb = x_SelfDestroy();
        CT_INT_TYPE c = sb->sbumpc();
        if (CT_EQ_INT_TYPE(c, CT_EOF))
            return 0;
        *buf = CT_TO_CHAR_TYPE(c);
        return 1;
    }

    // return whatever we have in the buffer; do not look into m_Sb this time
    size_t n = (size_t) m;
    size_t n_avail = egptr() - gptr();
    size_t n_read = (n <= n_avail) ? n : n_avail;
    memcpy(buf, gptr(), n_read);
    gbump((int) n_read);
    return n_read;
}
#endif


streamsize CPushback_Streambuf::showmanyc(void)
{
    // we are here because (according to the standard) gptr() >= egptr()
    streambuf* sb = x_SelfDestroy();
    return sb->in_avail();
}


streamsize CPushback_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize n)
{
    // hope that this is an optimized copy operation (instead of overflow()s)
    return m_Sb->sputn(buf, n);
}


int CPushback_Streambuf::sync(void)
{
    return m_Sb->PUBSYNC();
}


streambuf* CPushback_Streambuf::setbuf(CT_CHAR_TYPE* /*buf*/,
                                       streamsize    /*buf_size*/)
{
    THROW1_TRACE(runtime_error, "CPushback_Streambuf::setbuf() not allowed");
    return this;
}


// NB: Never use class data members after calling this beast
streambuf* CPushback_Streambuf::x_SelfDestroy()
{
    streambuf* sb = m_Sb;
    m_SelfDestroy = true;
    delete this;
    return sb;
}


/*****************************************************************************
 *  Public interface
 */

streambuf* UTIL_StreamPushback(istream&      is,
                               CT_CHAR_TYPE* buf,
                               streamsize    size,
                               void*         del_ptr)
{
    CPushback_Streambuf* sb = dynamic_cast<CPushback_Streambuf*>(is.rdbuf());

    assert(del_ptr <= buf);
    if ( sb ) {
#if 0
        {{
            int i = 0;
            CPushback_Streambuf* s = sb;
            do {
                i++;
                s = dynamic_cast<CPushback_Streambuf*>(s->m_Sb);
            } while (s);
            if (i > 1)
                cerr << "Current sb depth is " << i << endl;
        }}
#endif
        assert(del_ptr < (sb->m_DelPtr ? sb->m_DelPtr : sb->m_Buf)  ||
               sb->m_Buf + sb->m_Size <= del_ptr);
        if (sb->m_Buf <= buf  &&  buf + size == sb->gptr()) {
            assert(!del_ptr  ||  del_ptr == sb->m_DelPtr);
            sb->setg(buf, buf, sb->m_Buf + sb->m_Size);
            return sb;
        }
        if ((size_t)(sb->gptr() - sb->m_Buf) >= size  &&
            memcmp(buf, sb->gptr() - size, size) == 0) {
            sb->gbump(-int(size));
            if (del_ptr)
                delete[] (char*) del_ptr;
            return sb;
        }
    }

    return new CPushback_Streambuf(is, buf, size, del_ptr);
}


streambuf* UTIL_StreamPushback(istream&      is,
                               CT_CHAR_TYPE* buf,
                               streamsize    size)
{
    CT_CHAR_TYPE* b = new CT_CHAR_TYPE[size];
    memcpy(b, buf, size);
    return UTIL_StreamPushback(is, b, size, b);
}


END_NCBI_SCOPE
