#ifndef CORELIB___READER_WRITER__HPP
#define CORELIB___READER_WRITER__HPP

/* $Id$
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
 * Author:  Anton Lavrentiev
 *
 * File Description:
 *   Abstract reader-writer interface classes
 *
 */

/// @file reader_writer.hpp
/// Abstract reader-writer interface classes
///
/// Slightly adapted, however, to build std::streambuf on top of them.


#include <corelib/ncbistl.hpp>
#include <stddef.h>

/** @addtogroup Stream
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/// Result codes for I/O operations
/// @sa IReader, IWriter, IReaderWriter
enum ERW_Result {
    eRW_NotImplemented = -1,
    eRW_Success = 0,
    eRW_Timeout,
    eRW_Error,
    eRW_Eof
};

NCBI_XNCBI_EXPORT const char* g_RW_ResultToString(ERW_Result res);


/// A very basic data-read interface.
/// @sa
///  IWriter, IReaderWriter, CRStream
class NCBI_XNCBI_EXPORT IReader
{
public:
    /// Read as many as "count" bytes into a buffer pointed to by the "buf"
    /// argument.  Always store the number of bytes actually read (0 if read
    /// none) via the pointer "bytes_read", if provided non-NULL.
    /// Return non-eRW_Success code if EOF / error condition encountered
    /// during the operation (some data may have been read, nevertheless).
    /// Special case:  if "count" passed as 0, then the value of
    /// "buf" is ignored, and no change should be made to the state
    /// of the input device (but may return non-eRW_Success to indicate
    /// that the input device has already been in an error condition).
    /// @attention
    ///     It is implementation-specific whether the call blocks until
    ///     the entire buffer is read or the call returns when at least
    ///     some data are available.  In general, it is advised that this
    ///     call is made within a loop that checks for EOF condition and
    ///     proceeds with the reading until the requested amount of data
    ///     has been retrieved.
    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0) = 0;

    /// Via parameter "count" (which is guaranteed to be supplied non-NULL)
    /// return the number of bytes that are ready to be read from input
    /// device without blocking.  Return eRW_Success if the number of
    /// pending bytes has been stored at the location pointed to by "count".
    /// Return eRW_NotImplemented if the number cannot be determined.
    /// Otherwise, return other eRW_... condition to reflect the problem
    /// ("*count" does not need to be updated in the case of non-eRW_Success).
    /// Note that if reporting 0 bytes ready, the method may return either
    /// both eRW_Success and zero "*count", or return eRW_NotImplemented alone.
    virtual ERW_Result PendingCount(size_t* count) = 0;


    /// This method gets called by RStream buffer destructor to return buffered
    /// yet still unread (from the stream) portion of data  back to the device.
    /// It's semantically equivalent to CStreamUtils::Pushback() with the only
    /// difference that IReader can only assume the ownership of "buf" when
    /// "del_ptr" is passed non-NULL.
    /// Return eRW_Success when data have been successfully pushed back and the
    /// ownership of the pointers has been assumed as described above.
    /// Any other error code results in pointers remained and handled within
    /// the stream buffer being deleted, with a error messages suppressed for
    /// eRW_NotImplemented (default implementation).
    virtual ERW_Result Pushback(const void* buf, size_t count,
                                void* del_ptr = 0);

    virtual ~IReader();
};


/// A very basic data-write interface.
/// @sa
///  IReader, IReaderWriter, CWStream
class NCBI_XNCBI_EXPORT IWriter
{
public:
    /// Write up to "count" bytes from the buffer pointed to by the "buf"
    /// argument onto the output device.  Always store the number of bytes
    /// actually written, or 0 if "count" has been passed as 0
    /// ("buf" is ignored in this case), via the "bytes_written" pointer,
    /// if provided non-NULL.  Note that the method can return non-eRW_Success
    /// in case of an I/O error along with indicating (some) data delivered
    /// to the output device.
    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0) = 0;

    /// Flush pending data (if any) down to the output device.
    virtual ERW_Result Flush(void) = 0;

    virtual ~IWriter();
};


/// A very basic data-read/write interface.
/// @sa
///  IReader, IWriter, CRWStream
class NCBI_XNCBI_EXPORT IReaderWriter : public virtual IReader,
                                        public virtual IWriter
{
public:
    virtual ~IReaderWriter();
};


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___READER_WRITER__HPP */
