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
 *   Stream utilities:
 *   1. Push an arbitrary block of data back to a C++ input stream.
 *   2. Non-blocking read.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <util/util_exception.hpp>
#include <util/stream_utils.hpp>
#include <string.h>


#ifdef NCBI_COMPILER_MIPSPRO
#  define CPushback_StreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#else
#  define CPushback_StreambufBase CNcbiStreambuf
#endif //NCBI_COMPILER_MIPSPRO

#ifdef HAVE_GOOD_IOS_CALLBACKS
#  undef  HAVE_GOOD_IOS_CALLBACKS
#endif
#if defined(HAVE_IOS_REGISTER_CALLBACK)  &&  \
  (!defined(NCBI_COMPILER_WORKSHOP)  ||  !defined(_MT))
#  define HAVE_GOOD_IOS_CALLBACKS 1
#endif


BEGIN_NCBI_SCOPE


/*****************************************************************************
 *  Helper class: internal streambuf to be substituted instead of
 *  the original one in the stream, when the data are pushed back.
 */

class CPushback_Streambuf : public CPushback_StreambufBase
{
    friend struct CStreamUtils;

public:
    CPushback_Streambuf(istream& istream, CT_CHAR_TYPE* buf,
                        streamsize buf_size, void* del_ptr);
    virtual ~CPushback_Streambuf();

protected:
    virtual CT_POS_TYPE seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                IOS_BASE::openmode which);
    virtual CT_POS_TYPE seekpos(CT_POS_TYPE pos, IOS_BASE::openmode which);

    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n);
    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* buf, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual CT_INT_TYPE pbackfail(CT_INT_TYPE c = CT_EOF);

    virtual int         sync(void);

    // declared setbuf here to only throw an exception at run-time
    virtual streambuf*  setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

private:
    void                x_FillBuffer(streamsize max_size);
    void                x_DropBuffer(void);

    istream&            m_Is;      // I/O stream this streambuf is attached to
    streambuf*          m_Sb;      // original streambuf
    CT_CHAR_TYPE*       m_Buf;     // == 0 when the buffer has been emptied
    streamsize          m_BufSize;
    void*               m_DelPtr;

#ifdef HAVE_GOOD_IOS_CALLBACKS
    int                 m_Index;
    static void         x_Callback(IOS_BASE::event, IOS_BASE&, int);
#endif //HAVE_GOOD_IOS_CALLBACKS

    static const streamsize k_MinBufSize;
};


const streamsize CPushback_Streambuf::k_MinBufSize = 4096;


#ifdef HAVE_GOOD_IOS_CALLBACKS
void CPushback_Streambuf::x_Callback(IOS_BASE::event event,
                                     IOS_BASE&       ios,
                                     int             index)
{
    if (event == IOS_BASE::erase_event)
        delete static_cast<streambuf*> (ios.pword(index));
}
#endif //HAVE_GOOD_IOS_CALLBACKS


CPushback_Streambuf::CPushback_Streambuf(istream&      is,
                                         CT_CHAR_TYPE* buf,
                                         streamsize    buf_size,
                                         void*         del_ptr) :
    m_Is(is), m_Buf(buf), m_BufSize(buf_size), m_DelPtr(del_ptr)
{
    setp(0, 0); // unbuffered output at this level of streambuf's hierarchy
    setg(m_Buf, m_Buf, m_Buf + m_BufSize);
    m_Sb = m_Is.rdbuf(this);
#ifdef HAVE_GOOD_IOS_CALLBACKS
    try {
        m_Index             = m_Is.xalloc();
        m_Is.pword(m_Index) = this;
        m_Is.register_callback(x_Callback, m_Index);
    }
    STD_CATCH_ALL("CPushback_Streambuf::CPushback_Streambuf");
#endif //HAVE_GOOD_IOS_CALLBACKS
}


CPushback_Streambuf::~CPushback_Streambuf()
{
#ifdef HAVE_GOOD_IOS_CALLBACKS
    m_Is.pword(m_Index) = 0;
#endif //HAVE_GOOD_IOS_CALLBACKS
    delete[] (CT_CHAR_TYPE*) m_DelPtr;
    if (m_Sb) {
        m_Is.rdbuf(m_Sb);
        if (dynamic_cast<CPushback_Streambuf*> (m_Sb))
            delete m_Sb;
    }
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

    return m_Sb->PUBSYNC() == 0 ? CT_NOT_EOF(CT_EOF) : CT_EOF;
}


streamsize CPushback_Streambuf::xsputn(const CT_CHAR_TYPE* buf, streamsize n)
{
    // hope that this is an optimized copy operation (instead of overflow()s)
    return m_Sb->sputn(buf, n);
}


CT_INT_TYPE CPushback_Streambuf::underflow(void)
{
    // we are here because there is no more data in the pushback buffer
    _ASSERT(gptr()  &&  gptr() >= egptr());

#ifdef NCBI_COMPILER_MIPSPRO
    if (m_MIPSPRO_ReadsomeGptrSetLevel  &&  m_MIPSPRO_ReadsomeGptr != gptr())
        return CT_EOF;
    m_MIPSPRO_ReadsomeGptr = (CT_CHAR_TYPE*)(-1L);
#endif //NCBI_COMPILER_MIPSPRO

    x_FillBuffer(m_Sb->in_avail());
    return gptr() < egptr() ? CT_TO_INT_TYPE(*gptr()) : CT_EOF;
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
            x_FillBuffer(m);
            if (gptr() >= egptr())
                break;
        }
    }
    return n_total;
}


streamsize CPushback_Streambuf::showmanyc(void)
{
    // we are here because (according to the standard) gptr() >= egptr()
    _ASSERT(gptr()  &&  gptr() >= egptr());

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
    NCBI_THROW(CUtilException, eWrongCommand,
               "CPushback_Streambuf::setbuf: not allowed");
    /*NOTREACHED*/
    return this;
}


void CPushback_Streambuf::x_FillBuffer(streamsize max_size)
{
    _ASSERT(m_Sb);
    if ( !max_size )
        ++max_size;
    CPushback_Streambuf* sb = dynamic_cast<CPushback_Streambuf*> (m_Sb);
    if ( sb ) {
        _ASSERT(&m_Is == &sb->m_Is);
        m_Sb     = sb->m_Sb;
        sb->m_Sb = 0;
        if (sb->gptr() >= sb->egptr()) {
            delete sb;
            x_FillBuffer(max_size);
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
        streamsize n = m_Sb->sgetn(bp ? bp : (CT_CHAR_TYPE*) m_DelPtr,
                                   min(buf_size, max_size));
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


void CStreamUtils::Pushback(CNcbiIstream& is,
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


void CStreamUtils::Pushback(CNcbiIstream&       is,
                            const CT_CHAR_TYPE* buf,
                            streamsize          buf_size)
{
    CT_CHAR_TYPE* buf_copy = new CT_CHAR_TYPE[buf_size];
    memcpy(buf_copy, buf, buf_size);
    Pushback(is, buf_copy, buf_size, buf_copy);
}


#ifdef NCBI_NO_READSOME
#  undef NCBI_NO_READSOME
#endif /*NCBI_NO_READSOME*/

#if defined(NCBI_COMPILER_GCC)
#  if NCBI_COMPILER_VERSION < 300
#    define NCBI_NO_READSOME 1
#  endif /*NCBI_COMPILER_VERSION*/
#elif defined(NCBI_COMPILER_MSVC)
    /* MSVC's readsome() is buggy [causes 1 byte reads] and is thus avoided.
     * Now when we have worked around the issue by implementing fastopen()
     * in CNcbiFstreams and thus making them fully buffered, we can go back
     * to the use of readsome() in MSVC... */
    //#  define NCBI_NO_READSOME 1
#elif defined(NCBI_COMPILER_MIPSPRO)
    /* MIPSPro does not comply with the standard and always checks for EOF
     * doing one extra read from the stream [which might be a killing idea
     * for network connections]. We introduced an ugly workaround here...
     * Don't use istream::readsome() but istream::read() instead in order to be
     * able to clear fake EOF caused by the unnecessary underflow() upcall.*/
#  define NCBI_NO_READSOME 1
#endif /*NCBI_COMPILER*/


#ifndef NCBI_NO_READSOME
static inline streamsize s_Readsome(CNcbiIstream& is,
                                    CT_CHAR_TYPE* buf,
                                    streamsize    buf_size)
{
#  ifdef NCBI_COMPILER_WORKSHOP
    /* Rogue Wave does not always return correct value from is.readsome() :-/
     * In particular, when streambuf::showmanyc() returns 1 followed by
     * a failed read() [which implements extraction from the stream], which
     * encounters the EOF, the readsome() will blindly return 1, and in
     * general, always exactly the number of bytes showmanyc() reported,
     * regardless of actually extracted by subsequent read operation.  Bug!
     * NOTE that showmanyc() does not guarantee the number of bytes that can
     * be read, but returns a best guess estimate [C++ Standard, footnote 275].
     */
    streamsize n = is.readsome(buf, buf_size);
    return n ? is.gcount() : 0;
#  else
    return is.readsome(buf, buf_size);
#  endif /*NCBI_COMPILER_WORKSHOP*/
}
#endif /*NCBI_NO_READSOME*/


static streamsize s_DoReadsome(CNcbiIstream& is,
                               CT_CHAR_TYPE* buf,
                               streamsize    buf_size)
{
    _ASSERT(buf  &&  buf_size);
#ifdef NCBI_NO_READSOME
#  undef NCBI_NO_READSOME
    // Special case: GCC had no readsome() prior to ver 3.0;
    // read() will set "eof" (and "fail") flag if gcount() < buf_size
    streamsize avail = is.rdbuf()->in_avail();
    if (avail == 0)
        avail++; // we still must read
    else if (buf_size < avail)
        avail = buf_size;
    is.read(buf, avail);
    streamsize count = is.gcount();
    // Reset "eof" flag if some data have been read
    if (count  &&  is.eof()  &&  !is.bad())
        is.clear();
    return count;
#else
    // Try to read data
    streamsize n = s_Readsome(is, buf, buf_size);
    if (n != 0  ||  !is.good())
        return n;
    // No buffered data found, try to read from the real source [still good]
    is.read(buf, 1);
    if ( !is.good() )
        return 0;
    if (buf_size == 1)
        return 1; // do not need more data
    // Read more data (up to "buf_size" bytes)
    return s_Readsome(is, buf + 1, buf_size - 1) + 1;
#endif /*NCBI_NO_READSOME*/
}


streamsize CStreamUtils::Readsome(CNcbiIstream& is,
                                  CT_CHAR_TYPE* buf,
                                  streamsize    buf_size)
{
#  ifdef NCBI_COMPILER_MIPSPRO
    CMIPSPRO_ReadsomeTolerantStreambuf* sb =
        dynamic_cast<CMIPSPRO_ReadsomeTolerantStreambuf*> (is.rdbuf());
    if (sb)
        sb->MIPSPRO_ReadsomeBegin();
#  endif /*NCBI_COMPILER_MIPSPRO*/
    streamsize result = s_DoReadsome(is, buf, buf_size);
#  ifdef NCBI_COMPILER_MIPSPRO
    if (sb)
        sb->MIPSPRO_ReadsomeEnd();
#  endif /*NCBI_COMPILER_MIPSPRO*/
    return result;
}


END_NCBI_SCOPE


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.42  2004/05/17 21:06:02  gorelenk
 * Added include of PCH ncbi_pch.hpp
 *
 * Revision 1.41  2004/02/03 13:13:46  lavr
 * ios::xalloc() is universal; HAVE_XALLOC is deprecated and removed
 *
 * Revision 1.40  2004/01/21 15:34:14  lavr
 * Use proper macro HAVE_IOS_REGISTER_CALLBACK
 *
 * Revision 1.39  2004/01/20 20:39:59  lavr
 * Replace HAVE_BUGGY_IOS_CALLBACKS with (better) HAVE_GOOD_IOS_CALLBACKS
 *
 * Revision 1.38  2003/12/31 16:24:54  lavr
 * Fix istream::readsome() return counter use for Rogue Wave implementations
 *
 * Revision 1.37  2003/12/31 15:31:20  lavr
 * Fix wrong use of istream::readsome() on platforms that lack it
 *
 * Revision 1.36  2003/12/30 17:28:39  lavr
 * Work around buggy implementation of istream::readsome() in Rogue Wave STL
 *
 * Revision 1.35  2003/12/29 15:18:40  lavr
 * Modified CStreamUtils::Readsome() to avoid blocking on GCC2.95
 *
 * Revision 1.34  2003/12/18 13:24:46  ucko
 * It seems MIPSpro also lacks showmanyc, so extend our
 * streambuf-with-showmanyc class to cover it as well and make it
 * CMIPSPRO_ReadsomeTolerantStreambuf's base class; also use it as
 * CPushback_StreambufBase's base class with GCC 2.9x.
 *
 * Revision 1.33  2003/12/18 03:44:29  ucko
 * s_Readsome(): If in_avail() and showmanyc() both return 0, do likewise
 * rather than potentially blocking.
 *
 * Revision 1.32  2003/11/21 19:58:47  lavr
 * x_FillBuffer() not to have default parameter: crucial in underflow()
 *
 * Revision 1.31  2003/11/19 16:49:35  vasilche
 * Fix previous commit to maintain backup condition.
 *
 * Revision 1.30  2003/11/19 15:41:50  vasilche
 * Temporary fix for wrong Readsome() after Pushback().
 *
 * Revision 1.29  2003/11/04 13:39:01  lavr
 * CPushback_Streambuf is conditionally based on MIPSPro
 *
 * Revision 1.28  2003/11/03 20:05:11  lavr
 * CStreamUtils::Readsome() reorganized: s_Readsome() introduced.
 * Add and elaborate notes about I/O on MSVC and MIPSPro.
 * Allow to use stream readsome() on these two compilers.
 *
 * Revision 1.27  2003/10/22 18:11:24  lavr
 * Change base class of CPushback_Streambuf into CNcbiStreambuf
 *
 * Revision 1.26  2003/10/16 19:37:36  lavr
 * Add comments and indentation of conditionals in Readsome()
 *
 * Revision 1.25  2003/05/21 19:34:34  lavr
 * Note that insufficient ios::read() also sets failbit (as per the standard)
 *
 * Revision 1.24  2003/05/20 16:44:59  lavr
 * CStreamUtils::Readsome() not to clear() in a bad() stream
 *
 * Revision 1.23  2003/04/14 21:08:39  lavr
 * Take advantage of HAVE_BUGGY_IOS_CALLBACKS
 *
 * Revision 1.22  2003/04/11 17:57:52  lavr
 * Take advantage of HAVE_IOS_XALLOC
 *
 * Revision 1.21  2003/03/30 07:00:36  lavr
 * MIPS-specific workaround for lame-designed stream read ops
 *
 * Revision 1.20  2003/03/28 03:26:15  lavr
 * Reinstate NCBI_NO_READSOME for MSVC
 *
 * Revision 1.19  2003/03/27 18:40:14  lavr
 * Temporarily remove NCBI_NO_READSOME for MSVC
 *
 * Revision 1.18  2003/03/27 16:50:14  lavr
 * #define NCBI_NO_READSOME for MSVC to prevent 1-byte at a time file input
 * caused by lame unbuffered implementation of basic_filebuf, that defines
 * showmanyc() to return 0 and doing byte-by-byte file input via fgetc()
 *
 * Revision 1.17  2003/03/25 22:14:03  lavr
 * CStreamUtils::Readsome(): Return not only on EOF but on any !good()
 *
 * Revision 1.16  2003/02/27 15:36:23  lavr
 * Preper indentation for NCBI_THROW
 *
 * Revision 1.15  2003/02/26 21:32:00  gouriano
 * modify C++ exceptions thrown by this library
 *
 * Revision 1.14  2002/12/19 14:50:42  dicuccio
 * Changed a friend class -> friend struct (relieves a warning for MSVC)
 *
 * Revision 1.13  2002/11/28 03:28:01  lavr
 * Comments updated
 *
 * Revision 1.12  2002/11/27 21:08:01  lavr
 * Rename "stream_pushback" -> "stream_utils" and enclose utils in a class
 * Add new utility method Readsome() for non-blocking read
 *
 * Revision 1.11  2002/10/29 22:06:27  ucko
 * Make *buf const in the copying version of UTIL_StreamPushback.
 *
 * Revision 1.10  2002/09/17 20:47:05  lavr
 * Add a comment to unreachable return from CPushback_Streambuf::setbuf()
 *
 * Revision 1.9  2002/08/16 17:56:44  lavr
 * PUBSYNC, PUBSEEK* moved to <corelib/ncbistre.hpp>
 *
 * Revision 1.8  2002/02/06 14:38:23  ucko
 * Split up conditional test for GCC2.x to work around KCC preprocessor lossage
 *
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
