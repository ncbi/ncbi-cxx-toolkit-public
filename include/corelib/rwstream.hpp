#ifndef CORELIB___RWSTREAM__HPP
#define CORELIB___RWSTREAM__HPP

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
 *   Reader-writer based streams
 *
 */

/// @file rwstream.hpp
/// Reader-writer based streams
/// @sa IReader, IWriter, IReaderWriter, CRWStreambuf

#include <corelib/ncbimisc.hpp>
#include <corelib/impl/rwstreambuf.hpp>

/** @addtogroup Stream
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Note about the "buf_size" parameter for streams in this API.
///
/// CRWStream implementation is targeted at minimizing in-memory data copy
/// operations associated with I/O for intermediate buffering.  For that, the
/// following policies apply:
///
/// 1.  No read operation from the input device shall request less than the
///     internal stream buffer size.  In cases when the user code requests more
///     than the internal stream buffer size, the request may be passed through
///     to the input device to read directly into the buffer provided by user.
///     In that case, the read request can only be larger than the the size of
///     the internal stream buffer.
///
/// 2.  Write operations to the output device are done in full buffers, unless:
///  a. An incoming user write request is larger than the internal buffer,
///     then the contents of the internal buffer gets flushed first (which may
///     comprise of fewer bytes than the size of the internal buffer) followed
///     by a direct write request of the user's block (larger than the internal
///     stream buffer);
///  b. Flushing of the internal buffer (including 2a above) resulted in a
///     short write to the device (fewer bytes actually written).  Then, the
///     successive write attempt may contain fewer bytes than the size of the
///     internal stream buffer (namely, the remainder of what has been left
///     behind by the preceding [short] write).  This case also applies when
///     the flushing is done internally in a tied stream prior to reading.
///
/// However, any portable implementation should *not* rely on how data chunks
/// are being flushed or requested by the stream implementations.  If further
/// factoring into blocks (e.g. specially-sized) is necessary for the I/O
/// device to operate correctly, that should be implemented at the level of
/// the respective IReader/IWriter API explicitly.


/// Reader-based input stream.
/// @sa IReader
/// @attention
///     The underlying IReader is expected to block in Read() if unable to
///     extract at least one byte from the input device.  If unable to read
///     anything, it must never return eRW_Success.  In order for applications
///     to be able to use istream::readsome() (or CStreamUtils::Readsome()) on
///     CRstream, the underlying IReader must fully implement the
///     PendingCount() method.
/// @warning
///     The eRW_Error return code from any method of the underlying IReader may
///     get converted to an exception thrown if the requested I/O operation
///     cannot advance any data in stream.  That would make the stream to catch
///     it, and to set ios_base::badbit (ios_base::bad() returns true), then
///     perhaps to re-throw the exception (if so allowed by the stream).  Note
///     that accessing streambuf's methods directly won't shield from throwing
///     the exception, so it will always be delivered to user's code.
///
/// @param buf_size
///     specifies the number of bytes for internal I/O buffer, entirely used
///     for reading by the underlying stream buffer object CRWStreambuf;
///     0 causes to create a buffer of some default size.
///
/// @param buf
///     may specify the buffer location (if 0, an internal storage gets
///     allocated and later freed upon stream destruction).
///
/// @param stm_flags
///     controls whether IReader is destroyed upon stream destruction,
///     whether exceptions get logged (or leaked, or caught silently), etc.
///
/// Special case of "buf_size" == 1 creates an unbuffered stream ("buf", if
/// provided, is still used internally as a one-char un-get location).
///
/// @sa CRWStreambuf::TFlags, IWStream, IRWStream, CStreamUtils

class NCBI_XNCBI_EXPORT CRStream : public CNcbiIstream
{
public:
    CRStream(IReader*             r,
             streamsize           buf_size  = 0,
             CT_CHAR_TYPE*        buf       = 0,
             CRWStreambuf::TFlags stm_flags = 0)
        : CNcbiIstream(0), m_Sb(r, 0, buf_size, buf, stm_flags)
    {
        init(r ? &m_Sb : 0);
    }

private:
    CRWStreambuf m_Sb;
};


/// Writer-based output stream.
/// @sa IWriter
/// @attention
///     Underlying IWriter is expected to block in Write() if unable to output
///     at least one byte to the output device.  If unable to output anything,
///     it must never return eRW_Success.
/// @warning
///     The eRW_Error return code from any method of the underlying IWriter may
///     get converted to an exception thrown if the requested I/O operation
///     cannot advance any data in stream.  That would make the stream to catch
///     it, and to set ios_base::badbit (ios_base::bad() returns true), then
///     perhaps to re-throw the exception (if so allowed by the stream).  Note
///     that accessing streambuf's methods directly won't shield from throwing
///     the exception, so it will always be delivered to user's code.
///
/// @param buf_size
///     specifies the number of bytes for internal I/O buffer, entirely used
///     for writing by the underlying stream buffer object CRWStreambuf;
///     0 causes to create a buffer of some default size.
///
/// @param buf
///     may specify the buffer location (if 0, an internal storage gets
///     allocated and later freed upon stream destruction).
///
/// @param stm_flags
///     controls whether IWriter is destroyed upon stream destruction,
///     whether exceptions get logged (or leaked, or caught silently), etc.
///
/// Special case of "buf_size" == 1 creates an unbuffered stream ("buf", if
/// provided, is ignored).
///
/// @sa CRWStreambuf::TFlags, IRStream, IRWStream

class NCBI_XNCBI_EXPORT CWStream : public CNcbiOstream
{
public:
    CWStream(IWriter*             w,
             streamsize           buf_size  = 0,
             CT_CHAR_TYPE*        buf       = 0,
             CRWStreambuf::TFlags stm_flags = 0)
        : CNcbiOstream(0), m_Sb(0, w, buf_size, buf, stm_flags)
    {
        init(w ? &m_Sb : 0);
    }

private:
    CRWStreambuf m_Sb;
};


/// Reader-writer based input-output stream.
/// @sa IReaderWriter, IReader, IWriter
///
/// @param buf_size
///     specifies the number of bytes for internal I/O buffer, half of which is
///     to be used for reading and the other half -- for writing, by the
///     underlying stream buffer object CRWStreambuf;
///     0 causes to create a buffer of some default size.
///
/// @param buf
///     may specify the buffer location (if 0, an internal storage gets
///     allocated and later freed upon stream destruction).
///
/// @param stm_flags
///     controls whether IReaderWriter is destroyed upon stream destruction,
///     whether exceptions get logged (or leaked, or caught silently), etc.
///
/// Special case of "buf_size" == 1 creates an unbuffered stream ("buf", if
/// provided, may still be used internally as a one-char un-get location).
///
/// @sa CRWStreambuf::TFlags, IRStream, IWStream

class NCBI_XNCBI_EXPORT CRWStream : public CNcbiIostream
{
public:
    CRWStream(IReaderWriter*       rw,
              streamsize           buf_size  = 0,
              CT_CHAR_TYPE*        buf       = 0,
              CRWStreambuf::TFlags stm_flags = 0)
        : CNcbiIostream(0), m_Sb(rw, buf_size, buf, stm_flags)
    {
        init(rw ? &m_Sb : 0);
    }

    CRWStream(IReader*             r,
              IWriter*             w,
              streamsize           buf_size  = 0,
              CT_CHAR_TYPE*        buf       = 0,
              CRWStreambuf::TFlags stm_flags = 0)
        : CNcbiIostream(0), m_Sb(r, w, buf_size, buf, stm_flags)
    {
        init(r  ||  w ? &m_Sb : 0);
    }

private:
    CRWStreambuf m_Sb;
};


END_NCBI_SCOPE


/* @} */

#endif /* CORELIB___RWSTREAM__HPP */
