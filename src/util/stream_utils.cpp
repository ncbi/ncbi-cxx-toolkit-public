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
 *   Push an arbitrary block of data back to a C++ input stream.
 *
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.4  2001/12/17 22:18:21  ucko
 * Overload xsgetn unconditionally.
 *
 * Revision 1.3  2001/12/13 17:21:27  vakatov
 * [GCC, NCBI_COMPILER_VERSION > 2.95.X]  Use pubsync() rather than sync()
 *
 * Revision 1.2  2001/12/09 06:31:52  vakatov
 * UTIL_StreamPushback() to return VOID rather than STREAMBUF*.
 * Got rid of a warning;  added comments;  use _ASSERT(), not assert().
 *
 * Revision 1.1  2001/12/07 22:59:36  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#include <util/stream_pushback.hpp>
#include <string.h>


#ifdef NCBI_COMPILER_GCC
#  if NCBI_COMPILER_VERSION < 300
#    define PUBSYNC sync
#  else
#    define PUBSYNC pubsync
#  endif
#else
#  define PUBSYNC pubsync
#endif


BEGIN_NCBI_SCOPE


/*****************************************************************************
 *  Helper class: internal streambuf to be substituted instead of
 *  the original one in the stream, when the data are pushed back.
 */

class CPushback_Streambuf : public streambuf
{
    friend void UTIL_StreamPushback(istream& is, CT_CHAR_TYPE* buf,
                                    streamsize buf_size, void* del_ptr);
public:
    CPushback_Streambuf(istream& istream, CT_CHAR_TYPE* buf,
                        streamsize buf_size, void* del_ptr);
    virtual ~CPushback_Streambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
    virtual streamsize  showmanyc(void);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n);

    virtual int         sync(void);

    // this method is declared here to be disabled (exception) at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    streambuf* x_SelfDestroy(void);

    bool          m_SelfDestroy;  // if x_SelfDestroy() is already in progress
    istream&      m_Is;           // i/o stream this streambuf is attached to
    streambuf*    m_Sb;           // original streambuf
    CT_CHAR_TYPE* m_Buf;
    streamsize    m_BufSize;
    void*         m_DelPtr;
};



CPushback_Streambuf::CPushback_Streambuf(istream&      is,
                                         CT_CHAR_TYPE* buf,
                                         streamsize    buf_size,
                                         void*         del_ptr) :
    m_SelfDestroy(false),
    m_Is(is), m_Sb(is.rdbuf()),
    m_Buf(buf), m_BufSize(buf_size), m_DelPtr(del_ptr)
{
    setp(0, 0); // unbuffered output at this level of streambuf's hierarchy
    setg(m_Buf, m_Buf, m_Buf + m_BufSize);
    m_Is.rdbuf(this);
}


CPushback_Streambuf::~CPushback_Streambuf()
{
    if ( m_DelPtr ) {
        delete[] (char*) m_DelPtr;
    }

    if ( m_SelfDestroy ) {
        m_Is.rdbuf(m_Sb);
    } else {
        delete m_Sb;
    }
}


CT_INT_TYPE CPushback_Streambuf::overflow(CT_INT_TYPE c)
{
    if ( CT_EQ_INT_TYPE(c, CT_EOF) ) {
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


streamsize CPushback_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    if (gptr() >= egptr()) {
        streambuf* sb = x_SelfDestroy();
        CT_INT_TYPE c = sb->sbumpc();
        if ( CT_EQ_INT_TYPE(c, CT_EOF) ) {
            return 0;
        }
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


// NB: Never use class data members after calling this beast!
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

extern void UTIL_StreamPushback(istream&      is,
                                CT_CHAR_TYPE* buf,
                                streamsize    buf_size,
                                void*         del_ptr)
{
    CPushback_Streambuf* sb = dynamic_cast<CPushback_Streambuf*> (is.rdbuf());

    _ASSERT(del_ptr <= buf);
    if ( sb ) {
        // We may not need to create another streambuf, just recycle the
        // existing one here...
        _ASSERT(del_ptr < (sb->m_DelPtr ? sb->m_DelPtr : sb->m_Buf)  ||
                sb->m_Buf + sb->m_BufSize <= del_ptr);

        // Points to a (adjacent) part of the internal buffer we just read?
        if (sb->m_Buf <= buf  &&  buf + buf_size == sb->gptr()) {
            _ASSERT(!del_ptr  ||  del_ptr == sb->m_DelPtr);
            sb->setg(buf, buf, sb->m_Buf + sb->m_BufSize);
            return;
        }
        // Equal to a (adjacent) part of the internal buffer we just read?
        if ((streamsize)(sb->gptr() - sb->m_Buf) >= buf_size  &&
            memcmp(buf, sb->gptr() - buf_size, buf_size) == 0) {
            sb->gbump(-int(buf_size));
            if ( del_ptr ) {
                delete[] (char*) del_ptr;
            }
            return;
        }
    }

    (void) new CPushback_Streambuf(is, buf, buf_size, del_ptr);
}


extern void UTIL_StreamPushback(istream&      is,
                                CT_CHAR_TYPE* buf,
                                streamsize    buf_size)
{
    CT_CHAR_TYPE* buf_copy = new CT_CHAR_TYPE[buf_size];
    memcpy(buf_copy, buf, buf_size);
    UTIL_StreamPushback(is, buf_copy, buf_size, buf_copy);
}


END_NCBI_SCOPE
