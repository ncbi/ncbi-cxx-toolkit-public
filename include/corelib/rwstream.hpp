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


BEGIN_NCBI_SCOPE


/** @addtogroup Stream
 *
 * @{
 */


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
/// @param flags
///     controls whether IReader is destroyed upon stream destruction,
///     and whether excpetions cause logging (or caught silently).
///
/// Special case of "buf_size" == 1 and "buf" == 0 creates unbuffered stream.

class NCBI_XNCBI_EXPORT CRStream : public CNcbiIstream
{
public:
    CRStream(IReader*             r,
             streamsize           buf_size = 0,
             CT_CHAR_TYPE*        buf      = 0,
             CRWStreambuf::TFlags flags    = 0) :
        CNcbiIstream(0), m_Sb(r, 0, buf_size, buf, flags)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    virtual ~CRStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


/// Writer-based stream; @sa IWriter
/// @sa IRStream

class NCBI_XNCBI_EXPORT CWStream : public CNcbiOstream
{
public:
    CWStream(IWriter*             w,
             streamsize           buf_size = 0,
             CT_CHAR_TYPE*        buf      = 0,
             CRWStreambuf::TFlags flags    = 0) :
        CNcbiOstream(0), m_Sb(0, w, buf_size, buf, flags)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    virtual ~CWStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


/// Reader-writer based stream; @sa IReaderWriter
/// @sa IRStream

class NCBI_XNCBI_EXPORT CRWStream : public CNcbiIostream
{
public:
    CRWStream(IReaderWriter*       rw,
              streamsize           buf_size = 0,
              CT_CHAR_TYPE*        buf      = 0,
              CRWStreambuf::TFlags flags    = 0) :
        CNcbiIostream(0), m_Sb(rw, buf_size, buf, flags)
    {
        init(&m_Sb);
    }

#ifdef AUTOMATIC_STREAMBUF_DESTRUCTION
    virtual ~CRWStream()
    {
        rdbuf(0);
    }
#endif

private:
    CRWStreambuf m_Sb;
};


/// istream based IReader
class NCBI_XNCBI_EXPORT CStreamReader : public IReader
{
public:
    CStreamReader(CNcbiIstream& is, EOwnership own = eNoOwnership)
        : m_Stream(&is, own)
        {
        }
    ~CStreamReader();

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read = 0);
    virtual ERW_Result PendingCount(size_t* count);

private:
    AutoPtr<CNcbiIstream> m_Stream;

private: // prevent copy
    CStreamReader(const CStreamReader&);
    void operator=(const CStreamReader&);
};


/* @} */


END_NCBI_SCOPE

#endif /* CORELIB___RWSTREAM__HPP */
