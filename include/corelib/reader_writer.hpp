#ifndef UTIL___READER_WRITER__HPP
#define UTIL___READER_WRITER__HPP

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
 *   Abstract reader-writer interface classes
 *
 */

/// @file reader_writer.hpp
/// Abstract reader-writer interface classes


#include <corelib/ncbistl.hpp>
#include <stddef.h>


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


/// A very basic data-read interface.
///
/// It is however slightly adapted to build std::istreambuf on top of it.

class IReader
{
public:
    /// Read as many as count bytes into a buffer pointed
    /// to by buf argument.  Store the number of bytes actually read,
    /// or 0 on EOF or error, via the pointer "bytes_read", if provided.
    /// Special case:  if count passed as 0, then the value of
    /// buf is ignored, and the return value is always eRW_Success, but
    /// no change is actually done to the state of input device.
    virtual ERW_Result Read(void*   buf,
                            size_t  count,
                            size_t* bytes_read = 0) = 0;

    /// Return the number of bytes ready to be read from input
    /// device without blocking.  Return 0 if no such number is
    /// available (in case of an error or EOF).
    virtual ERW_Result PendingCount(size_t* count) = 0;

    virtual ~IReader() {}
};



/// A very basic data-write interface.

class IWriter
{
public:
    /// Write up to count bytes from the buffer pointed to by
    /// buf argument onto output device.  Store the number
    /// of bytes actually written, or 0 if either count was
    /// passed as 0 (buf is ignored in this case) or an error occured,
    /// via the "bytes_written" pointer, if provided.
    virtual ERW_Result Write(const void* buf,
                             size_t      count,
                             size_t*     bytes_written = 0) = 0;

    /// Flush pending data (if any) down to output device.
    virtual ERW_Result Flush(void) = 0;

    virtual ~IWriter() {}
};



/// A very basic data-read/write interface.

class IReaderWriter : public IReader, public IWriter
{
public:
    virtual ~IReaderWriter() {}
};


END_NCBI_SCOPE



/*
 * ===========================================================================
 * $Log$
 * Revision 1.9  2003/11/03 20:01:52  lavr
 * Add ERW_Result::eRW_Timeout
 *
 * Revision 1.8  2003/10/22 18:14:47  lavr
 * IWriter::Flush() added
 *
 * Revision 1.7  2003/09/29 16:06:59  lavr
 * Change ERW_Result enumeration and members names
 *
 * Revision 1.6  2003/09/25 13:35:50  kuznets
 * Minor syntax issue corrected.
 *
 * Revision 1.5  2003/09/24 21:12:39  lavr
 * Doxygenification
 *
 * Revision 1.4  2003/09/24 15:56:24  kuznets
 * Minor syntax error fixed in ERW_Result declaration
 *
 * Revision 1.3  2003/09/24 15:45:36  lavr
 * Changed to use ERW_Result in return codes; pointers to store I/O counts
 *
 * Revision 1.2  2003/09/22 22:38:21  vakatov
 * Minor (mostly style) fixes;  renaming
 *
 * Revision 1.1  2003/09/22 20:26:23  lavr
 * Initial revision
 * ===========================================================================
 */

#endif  /* UTIL___READER_WRITER__HPP */
