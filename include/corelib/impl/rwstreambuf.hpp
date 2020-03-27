#ifndef CORELIB___RWSTREAMBUF__HPP
#define CORELIB___RWSTREAMBUF__HPP

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
 * Authors:  Anton Lavrentiev
 *
 * File Description:
 *   Reader-writer-based stream buffer
 *
 */

/// @file rwstreambuf.hpp
/// Reader-writer-based stream buffer.
/// @sa IReader, IWriter, IReaderWriter, CRStream, CWStream, CRWStream

#include <corelib/ncbistre.hpp>
#include <corelib/reader_writer.hpp>

#ifdef NCBI_COMPILER_MIPSPRO
#  define CRWStreambufBase CMIPSPRO_ReadsomeTolerantStreambuf
#else
#  define CRWStreambufBase CNcbiStreambuf
#  ifdef NCBI_COMPILER_MSVC
#    pragma warning(push)
#    pragma warning(disable:4996)
#  endif //NCBI_COMPILER_MSVC
#endif //NCBI_COMPILER_MIPSPRO


BEGIN_NCBI_SCOPE


/// Reader-writer-based stream buffer.

class NCBI_XNCBI_EXPORT CRWStreambuf : public CRWStreambufBase
{
public:
    /// Which of the objects (passed in the constructor) must be deleted on
    /// this object's destruction, whether to tie I/O, and how to process
    /// exceptions thrown at lower levels...
    enum EFlags {
        fOwnReader      = 1 << 0,    ///< Own the underlying reader
        fOwnWriter      = 1 << 1,    ///< Own the underlying writer
        fOwnAll         = fOwnReader + fOwnWriter,
        fUntie          = 1 << 2,    ///< Do not flush before reading
        fNoStatusLog    = 1 << 3,    ///< Do not log unsuccessful I/O
        fLogExceptions  = 1 << 4,    ///< Exceptions logged only
        fLeakExceptions = 1 << 5     ///< Exceptions leaked out
    };
    typedef int TFlags;              ///< Bitwise OR of EFlags


    CRWStreambuf(IReaderWriter* rw       = 0,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf      = 0,
                 TFlags         flags    = 0);

    /// NOTE:  if both reader and writer have actually happened to be the same
    ///        object, then when owned, it will _not_ be deleted twice.
    CRWStreambuf(IReader*       r,
                 IWriter*       w,
                 streamsize     buf_size = 0,
                 CT_CHAR_TYPE*  buf      = 0,
                 TFlags         flags    = 0);

    virtual ~CRWStreambuf();

protected:
    virtual CT_INT_TYPE overflow(CT_INT_TYPE c);
    virtual streamsize  xsputn(const CT_CHAR_TYPE* buf, streamsize n);

    virtual CT_INT_TYPE underflow(void);
    virtual streamsize  xsgetn(CT_CHAR_TYPE* s, streamsize n);
    virtual streamsize  showmanyc(void);

    virtual int         sync(void);

    /// Per the standard, setbuf(0, 0) makes I/O unbuffered.  Other behavior is
    /// implementation-dependent:
    /// "buf_size" == 1 makes I/O unbuffered ("buf", if provided, may still be
    /// used internally as a one-char un-get location).
    /// Special case: setbuf(non-NULL, 0) creates an internal buffer of some
    /// predefined size, which will be automatically deallocated in dtor;  the
    /// value of the first argument is ignored (can be any non-NULL pointer).
    /// Otherwise, setbuf() sets I/O arena of size "buf_size" located at "buf",
    /// and halved between the I/O directions, if both are used.  If "buf" is
    /// provided as NULL, the buffer of the requested size gets allocated
    /// internally, and gets automatically freed upon CRWStreambuf destruction.
    /// Before replacing the buffer, this call first attempts to flush any
    /// pending output (sync) to the output device, and return any pending
    /// input sequence (internal read buffer) to the input device (pushback).
    virtual CNcbiStreambuf* setbuf(CT_CHAR_TYPE* buf, streamsize buf_size);

    /// Only seekoff(0, IOS_BASE::cur, *) to obtain current position, and input
    /// skip-forward are permitted:
    /// seekoff(off, IOS_BASE::cur or IOS_BASE::beg, IOS_BASE::in) when the
    /// requested stream position is past the current input position (so the
    /// stream can read forward internally to reach that position).
    virtual CT_POS_TYPE seekoff(CT_OFF_TYPE off, IOS_BASE::seekdir whence,
                                IOS_BASE::openmode which =
                                IOS_BASE::in | IOS_BASE::out);

#ifdef NCBI_OS_MSWIN
    // See comments in "connect/ncbi_conn_streambuf.hpp"
    virtual streamsize  _Xsgetn_s(CT_CHAR_TYPE* buf, size_t, streamsize n)
    { return xsgetn(buf, n); }
#endif /*NCBI_OS_MSWIN*/

protected:
    CT_POS_TYPE x_GetGPos(void)
    { return x_GPos - (CT_OFF_TYPE)(gptr()  ? egptr() - gptr() : 0); }

    CT_POS_TYPE x_GetPPos(void)
    { return x_PPos + (CT_OFF_TYPE)(pbase() ? pbase() - pptr() : 0); }

    int         x_Sync(void)
    { return pbase() < pptr() ? sync() : 0; }

    streamsize  x_Read(CT_CHAR_TYPE* s, streamsize n);

    ERW_Result  x_Pushback(void);

protected:
    TFlags            m_Flags;

    AutoPtr<IReader>  m_Reader;
    AutoPtr<IWriter>  m_Writer;

    size_t            m_BufSize;
    CT_CHAR_TYPE*     m_ReadBuf;
    CT_CHAR_TYPE*     m_WriteBuf;

    CT_CHAR_TYPE*     m_pBuf;
    CT_CHAR_TYPE      x_Buf;

    CT_POS_TYPE       x_GPos;    ///< get position [for istream::tellg()]
    CT_POS_TYPE       x_PPos;    ///< put position [for ostream::tellp()]

    bool              x_Eof;     ///< whether at EOF
    bool              x_Err;     ///< whether there was a _write_ error
    CT_POS_TYPE       x_ErrPos;  ///< position of the _write_ error (if x_Err)

private:
    CRWStreambuf(const CRWStreambuf&);
    CRWStreambuf operator=(const CRWStreambuf&);
};


END_NCBI_SCOPE


#ifdef NCBI_COMPILER_MSVC
#  pragma warning(pop)
#endif //NCBI_COMPILER_MSVC

#endif /* CORELIB___RWSTREAMBUF__HPP */
