#ifndef UTIL___READERWRITER__H
#define UTIL___READERWRITER__H

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

#include <corelib/ncbistl.hpp>
#include <stddef.h>


BEGIN_NCBI_SCOPE


class IReader
{
public:
    /// Read as many as buf_size bytes into a buffer pointed
    /// to by buf argument.  Return the number of bytes actually read,
    /// or 0 on EOF, or -1 in case of an error.
    /// Special case:  if buf_size is passed as 0, then the value of
    /// buf is ignored, and the return value is unspecified, but no
    /// change is done to the state of input device.
    virtual ssize_t  Read    (void* buf, size_t buf_size) = 0;

    /// Return the number of bytes ready to be read from input
    /// device without blocking.  Return 0 if no such number is
    /// available, -1 in case of EOF (or an error).
    virtual ssize_t  SHowMany(void) { return 0; };

    virtual         ~IReader (void) { };
};


class IWriter
{
public:
    /// Write buf_size bytes from the buffer pointed to by
    /// buf argument onto output device.  Return the number
    /// of bytes actually written, or 0 if buf_size was passed as 0
    /// (buf is ignored in this case), or -1 on error.
    virtual ssize_t  Write (const void* buf, size_t buf_size) = 0;

    virtual         ~IWriter(void) { };
};


class IReaderWriter : public IReader, IWriter
{
public:
    virtual         ~IReaderWriter (void) { };
};


END_NCBI_SCOPE


#endif /*UTIL___READERWRITER__H*/


/*
 * ---------------------------------------------------------------------------
 * $Log$
 * Revision 1.1  2003/09/22 20:26:23  lavr
 * Initial revision
 *
 * Revision 1.1  2003/09/22 17:57:10  lavr
 * Initial revision
 *
 * ---------------------------------------------------------------------------
 */
