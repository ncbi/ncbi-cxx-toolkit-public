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
 * Revision 1.7  2002/02/05 16:05:43  lavr
 * List of included header files revised
 *
 * Revision 1.6  2002/02/04 20:22:34  lavr
 * Stream positioning added; more assert()'s to check standard compliance
 *
 * Revision 1.5  2002/01/28 20:26:39  lavr
 * Completely redesigned
 *
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

#include <corelib/ncbistd.hpp>
#include <util/stream_pushback.hpp>
#include <string.h>


#if defined(NCBI_COMPILER_GCC)  &&  NCBI_COMPILER_VERSION < 300
#  define PUBSYNC    sync
#  define PUBSEEKOFF seekoff
#  define PUBSEEKPOS seekpos
#else
#  define PUBSYNC    pubsync
#  define PUBSEEKOFF pubseekoff
#  define PUBSEEKPOS pubseekpos
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
    virtual CT_POS_TYPE  seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                 IOS_BASE::openmode which);
    virtual CT_POS_TYPE  seekpos(CT_POS_TYPE pos, IOS_BASE::openmode which);

    virtual CT_INT_TYPE  overflow(CT_INT_TYPE c);
    virtual streamsize   xsputn(const CT_CHAR_TYPE* buf, streamsize n);
    virtual CT_INT_TYPE  underflow(void);
    virtual streamsize   xsgetn(CT_CHAR_TYPE* buf, streamsize n);
    virtual streamsize   showmanyc(void);

    virtual CT_INT_TYPE  pbackfail(CT_INT_TYPE c = CT_EOF);

    virtual int          sync(void);

    // declared setbuf here to only throw an exception at run-time
    virtual streambuf*   setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    void                 x_FillBuffer(void);
    void                 x_DropBuffer(void);

    istream&             m_Is;      // i/o stream this streambuf is attached to
    streambuf*           m_Sb;      // original streambuf
    CT_CHAR_TYPE*        m_Buf;     // == 0 when the buffer has been emptied
    streamsize           m_BufSize;
    void*                m_DelPtr;

    static const streamsize k_MinBufSize;
};


const streamsize CPushback_Streambuf::k_MinBufSize = 4096;


CPushback_Streambuf::CPushback_Streambuf(istream&      is,
                                         CT_CHAR_TYPE* buf,
                                         streamsize    buf_size,
                                         void*         del_ptr) :
    m_Is(is), m_Sb(is.rdbuf()),
    m_Buf(buf), m_BufSize(buf_size), m_DelPtr(del_ptr)
{
    setp(0, 0); // unbuffered output at this level of streambuf's hierarchy
    setg(m_Buf, m_Buf, m_Buf + m_BufSize);
    m_Is.rdbuf(this);
}


CPushback_Streambuf::~CPushback_Streambuf()
{
    delete[] (CT_CHAR_TYPE*) m_DelPtr;
    if (dynamic_cast<CPushback_Streambuf*> (m_Sb))
        delete m_Sb;
}


CT_POS_TYPE CPushback_Streambuf::seekoff(CT_OFF_TYPE off,
                                         IOS_BASE::seekdir whence,
                                         IOS_BASE::openmode which)
{
    x_DropBuffer();
    if (whence == ios::cur  &&  (which & ios::in)) {
        return (CT_POS_TYPE)((CT_OFF_TYPE)(-1));
    }
    return m_Sb->PUBSEEKOFF(off, whence, which);
}


CT_POS_TYPE CPushback_Streambuf::seekpos(CT_POS_TYPE pos,
                                         IOS_BASE::openmode which)
{
    x_DropBuffer();
    return m_Sb->PUBSEEKPOS(pos, which);
}


CT_INT_TYPE CPushback_Streambuf::overflow(CT_INT_TYPE c)
{
    if ( !CT_EQ_INT_TYPE(c, CT_EOF) ) {
        return m_Sb->sputc(CT_TO_CHAR_TYPE(c));
    }
    if (m_Sb->PUBSYNC() == 0) {
        return CT_NOT_EOF(CT_EOF); // success
    }
    return CT_EOF; // failure
}


streamsize CPushback_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize n)
{
    // hope that this is an optimized copy operation (instead of overflow()s)
    return m_Sb->sputn(buf, n);
}


CT_INT_TYPE CPushback_Streambuf::underflow(void)
{
    // we are here because there is no more data in the pushback buffer
    _ASSERT(!gptr()  ||  gptr() >= egptr());

    x_FillBuffer();
    if (gptr() < egptr())
        return CT_TO_INT_TYPE(*gptr());
    return CT_EOF;
}


streamsize CPushback_Streambuf::xsgetn(CT_CHAR_TYPE* buf, streamsize m)
{
    size_t n_total = 0;
    while (m) {
        if (gptr() < egptr()) {
            size_t n       = (size_t) m;
            size_t n_avail = (size_t) (egptr() - gptr());
            size_t n_read  = (n <= n_avail) ? n : n_avail;
            memcpy(buf, gptr(), n_read*sizeof(CT_CHAR_TYPE));
            gbump((int) n_read);
            m       -= (streamsize) n_read;
            buf     += (streamsize) n_read;
            n_total += (streamsize) n_read;
        } else {
            x_FillBuffer();
            if (gptr() >= egptr())
                break;
        }
    }
    return n_total;
}


streamsize CPushback_Streambuf::showmanyc(void)
{
    // we are here because (according to the standard) gptr() >= egptr()
    _ASSERT(!gptr()  ||  gptr() >= egptr());

    return m_Sb->in_avail();
}


CT_INT_TYPE CPushback_Streambuf::pbackfail(CT_INT_TYPE /*c*/)
{
    /* We always maintain "usual backup condition" (27.5.2.4.3.13) after
     * underflow(), i.e. 1 byte backup after a good read is always possible.
     * That is, this function gets called only if the user tries to
     * back up more than once (although, some attempts may be successful,
     * this function call denotes that the backup area is full).
     */
    return CT_EOF; // always fail
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


void CPushback_Streambuf::x_FillBuffer()
{
    _ASSERT(m_Sb);
    CPushback_Streambuf* sb = dynamic_cast<CPushback_Streambuf*> (m_Sb);
    if ( sb ) {
        _ASSERT(&m_Is == &sb->m_Is);
        m_Sb     = sb->m_Sb;
        sb->m_Sb = 0;
        if (sb->gptr() >= sb->egptr()) {
            delete sb;
            x_FillBuffer();
            return;
        }
        delete[] (CT_CHAR_TYPE*) m_DelPtr;
        m_Buf        = sb->m_Buf;
        m_BufSize    = sb->m_BufSize;
        m_DelPtr     = sb->m_DelPtr;
        sb->m_DelPtr = 0;
        setg(sb->gptr(), sb->gptr(), sb->egptr());
        delete sb;
    } else {
        CT_CHAR_TYPE* bp = 0;
        streamsize buf_size = m_DelPtr
            ? (streamsize)(m_Buf - (CT_CHAR_TYPE*) m_DelPtr) + m_BufSize : 0;
        if (buf_size < k_MinBufSize) {
            buf_size = k_MinBufSize;
            bp = new CT_CHAR_TYPE[buf_size];
        }
        streamsize n = m_Sb->sgetn(bp? bp : (CT_CHAR_TYPE*)m_DelPtr, buf_size);
        if (n <= 0) {
            // NB: For unknown reasons WorkShop6 can return -1 from sgetn :-/
            delete bp;
            return;
        }
        if (bp) {
            delete[] (CT_CHAR_TYPE*) m_DelPtr;
            m_DelPtr = bp;
        }
        m_Buf = (CT_CHAR_TYPE*) m_DelPtr;
        m_BufSize = buf_size;
        setg(m_Buf, m_Buf, m_Buf + n);
    }
}


void CPushback_Streambuf::x_DropBuffer()
{
    CPushback_Streambuf* sb = dynamic_cast<CPushback_Streambuf*> (m_Sb);
    if (sb) {
        m_Sb     = sb->m_Sb;
        sb->m_Sb = 0;
        delete sb;
        x_DropBuffer();
        return;
    }
    // nothing in the buffer; no putback area as well
    setg(m_Buf, m_Buf, m_Buf);
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
        // We may not need to create another streambuf,
        //     just recycle the existing one here...
        _ASSERT(del_ptr < (sb->m_DelPtr ? sb->m_DelPtr : sb->m_Buf)  ||
                sb->m_Buf + sb->m_BufSize <= del_ptr);
        // 1/ points to a (adjacent) part of the internal buffer we just read?
        if (sb->m_Buf <= buf  &&  buf + buf_size == sb->gptr()) {
            _ASSERT(!del_ptr  ||  del_ptr == sb->m_DelPtr);
            sb->setg(buf, buf, sb->m_Buf + sb->m_BufSize);
            return;
        }
        // 2/ equal to a (adjacent) part of the internal buffer we just read?
        if ((streamsize)(sb->gptr() - sb->m_Buf) >= buf_size) {
            CT_CHAR_TYPE* bp = sb->gptr() - buf_size;
            if (memcmp(buf, bp, buf_size) == 0) {
                sb->setg(bp, bp, sb->egptr());
                if ( del_ptr ) {
                    delete[] (CT_CHAR_TYPE*) del_ptr;
                }
                return;
            }
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
