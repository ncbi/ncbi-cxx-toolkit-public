#ifndef UTIL___RWSTREAM__HPP
#define UTIL___RWSTREAM__HPP

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


#include <util/rwstreambuf.hpp>


BEGIN_NCBI_SCOPE


/// Reader-based stream; @sa IReader
///
/// @param buf_size
///     specifies the number bytes for internal I/O buffer,
///     with half used for reading and the other half for writing
///     by underlying stream buffer CRWStreambuf;
///     0 causes to create the buffer of some default size.
///
/// @param buf
///     may specify the buffer location (if 0, an internal storage gets
///     allocated and later freed upon stream destruction).
///
/// @param own
///     controls whether IReader is destroyed upon stream destruction.
///
/// Special case of "buf_size" == 1 and "buf" == 0 creates unbuffered stream.

class CRStream : public CNcbiIstream
{
public:
    CRStream(IReader*                 r,
             streamsize               buf_size = 0,
             CT_CHAR_TYPE*            buf = 0,
             CRWStreambuf::TOwnership own = 0) :
        CNcbiIstream(0), m_Sb(r, 0, buf_size, buf, own)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    ~CRStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


/// Writer-based stream; @sa IWriter
/// @sa IRStream

class CWStream : public CNcbiOstream
{
public:
    CWStream(IWriter*                 w,
             streamsize               buf_size = 0,
             CT_CHAR_TYPE*            buf = 0,
             CRWStreambuf::TOwnership own = 0) :
        CNcbiOstream(0), m_Sb(0, w, buf_size, buf, own)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    ~CWStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


/// Reader-writer based stream; @sa IReaderWriter
/// @sa IRStream

class CRWStream : public CNcbiIostream
{
public:
    CRWStream(IReaderWriter*           rw,
              streamsize               buf_size = 0,
              CT_CHAR_TYPE*            buf = 0,
              CRWStreambuf::TOwnership own = 0) :
        CNcbiIostream(0), m_Sb(rw, buf_size, buf, own)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    ~CRWStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2005/03/17 19:06:10  lavr
 * Document parameters
 *
 * Revision 1.7  2005/03/10 20:50:45  vasilche
 * Do not forget to pass ownership to CRWStreambuf.
 *
 * Revision 1.6  2004/05/24 19:54:35  lavr
 * Added stream dtors for AUTOMATIC_STREAMBUF_DESTRUCTION case
 *
 * Revision 1.5  2004/05/17 15:48:45  lavr
 * Ownership argument added to stream constructors
 *
 * Revision 1.4  2004/01/15 20:05:32  lavr
 * Use 0 as a default buffer size in stream constructors
 *
 * Revision 1.3  2003/11/12 17:44:26  lavr
 * Uniformed file layout
 *
 * Revision 1.2  2003/10/22 19:15:15  lavr
 * Explicit calls for iostream constructors added
 *
 * Revision 1.1  2003/10/22 18:14:31  lavr
 * Initial revision
 *
 * ===========================================================================
 */

#endif /* UTIL___RWSTREAM__HPP */
