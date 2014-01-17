#ifndef CORELIB___STREAM_UTILS__HPP
#define CORELIB___STREAM_UTILS__HPP

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
 * Authors:  Anton Lavrentiev, Denis Vakatov
 *
 * File Description:
 *
 *   Stream utilities:
 *   1. Push an arbitrary block of data back to C++ input stream;
 *   2. Non-blocking read from a stream.
 *
 *   Reader-writer utilities:
 *   1. Construct an IReader object from an istream;
 *   2. Construct an IWriter object from an ostream;
 *   3. Construct an IReader object from a string;
 *   4. Append entire IReader's contents to a string.
 *
 */

#include <corelib/ncbistr.hpp>
#include <corelib/ncbistre.hpp>
#include <corelib/reader_writer.hpp>

/** @addtogroup Stream
 *
 * @{
 */


BEGIN_NCBI_SCOPE


struct NCBI_XNCBI_EXPORT CStreamUtils
{
// Push the block of data [buf, buf+buf_size) back to the input stream "is".
// If "del_ptr" is not NULL, then `delete[] (CT_CHAR_TYPE*) del_ptr' is called
// some time between the moment you call this function (perhaps, prior to when
// it returns) and when the passed block is no longer needed (i.e. its content
// is stored elsewhere, or its all read out from the stream, or if the stream
// is destructed).  Since this function may, at its own discretion, use the
// passed "buf" directly in the input stream as long as it needs to, for the
// sake of data integrity and consistency the block should not be modified
// from the outside until all of the passed data is read out from the stream.
// NOTE 1:  It's okay to pushback arbitrary data (i.e. not necessarily
//          just what has been last read from the stream).
// NOTE 2:  Data does not actually go to the original streambuf of "is".
// NOTE 3:  It's okay if "is" is a full-duplex stream (iostream).
// NOTE 4:  Data pushed back regardless of the current stream state, so it is
//          the caller's responsibility to check and possibly clear stream
//          state that can prevent further reading (e.g. an EOF condition
//          reached previously).
// NOTE 5:  After a pushback, the stream is allowed to do limited seeks
//          relative to current position (ios::cur);  only direct-access
//          (ios::beg, ios::end) seeks are fully okay (if permitted by "is").
// NOTE 6:  Stream re-positioning made after pushback clears all pushback data.
// NOTE 7:  The standard specifically says that putbacks must be interleaved
//          with reads in order to work properly.  That is especially
//          important to keep in mind, when using both the standard
//          putbacks and the pushbacks offered by this API.
// NOTE 8:  Implementation is incomplete (but safe and stable) and may leak
//          memory in Solaris WorkShop MT builds (due to a bug in C++ RTL).
    static void       Pushback(CNcbiIstream&       is,
                               CT_CHAR_TYPE*       buf,
                               streamsize          buf_size,
                               void*               del_ptr)
    { x_Pushback(is, buf, (size_t) buf_size, del_ptr, ePushback_NoCopy); }

// Acts just like its counterpart with 4 args (above), but this variant
// always copies (if necessary) the "pushback data" into an internal buffer,
// so "buf" is not required to remain read-only in the outer code.
    static void       Pushback(CNcbiIstream&       is,
                               const CT_CHAR_TYPE* buf,
                               streamsize          buf_size)
    { x_Pushback(is, const_cast<CT_CHAR_TYPE*> (buf), (size_t) buf_size); }

// Unsafe but fast API that tries to backup "buf_size" bytes in the
// internal stream buffer, but if that is not possible, will use
// (perhaps, partially) "buf" and "del_ptr" to pushback as described above.
// This call relies upon that the data in "buf" is exactly as it has been
// read from the stream previously (otherwise, behavior is undefined).
    static void       Stepback(CNcbiIstream&       is,
                               CT_CHAR_TYPE*       buf,
                               streamsize          buf_size,
                               void*               del_ptr = 0)
    { x_Pushback(is, buf, (size_t) buf_size, del_ptr, ePushback_Stepback); }

// Read at most "buf_size" bytes from the stream "is" into a buffer pointed
// to by "buf".  This call tries its best to be non-blocking.
// Return the number of bytes actually read, or 0 if nothing has been read
// (in case of either an error or EOF).
    static streamsize Readsome(CNcbiIstream&       is,
                               CT_CHAR_TYPE*       buf,
                               streamsize          buf_size);

private:
    // implementation
    typedef enum {
        ePushback_Copy,
        ePushback_NoCopy,
        ePushback_Stepback
    } EPushback_How;

    static void x_Pushback(CNcbiIstream& is,
                           CT_CHAR_TYPE* buf,
                           streamsize    buf_size,
                           void*         del_ptr = 0,
                           EPushback_How how = ePushback_Copy);
};


/// istream-based IReader
///
/// Besides other uses, this class can help unite two uni-derectional streams
/// into a single bi-directional one, e.g.:
///     CRWStream rw(new CStreamReader(cin), new CStreamWriter(cout),
///                  0, 0, CRWStreambuf::fOwnAll);
/// @sa
///   CStreamWriter
class NCBI_XNCBI_EXPORT CStreamReader : public IReader
{
public:
    CStreamReader(CNcbiIstream& is, EOwnership own = eNoOwnership)
        : m_Stream(&is, own)
    { }

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read);
    virtual ERW_Result PendingCount(size_t* count);

protected:
    AutoPtr<CNcbiIstream> m_Stream;

private:
    // prevent copy
    CStreamReader(const CStreamReader&);
    void operator=(const CStreamReader&);
};


/// ostream-based IWriter
///
/// Can help create single bi-directional stream from two uni-derectional ones:
///     CRWStream rw(new CStreamReader(cin), new CStreamWriter(cout),
///                  0, 0, CRWStreambuf::fOwnAll);
/// @sa
///   CStreamReader
class NCBI_XNCBI_EXPORT CStreamWriter : public IWriter
{
public:
    CStreamWriter(CNcbiOstream& os, EOwnership own = eNoOwnership)
        : m_Stream(&os, own)
    { }

    virtual ERW_Result Write(const void* buf,
                             size_t count, size_t* bytes_written);
    virtual ERW_Result Flush(void);

protected:
    AutoPtr<CNcbiOstream> m_Stream;

private:
    // Prevent copy
    CStreamWriter(const CStreamWriter&);
    void operator=(const CStreamWriter&);
};


/// string-based IReader
class NCBI_XNCBI_EXPORT CStringReader : public IReader
{
public:
    explicit CStringReader(const string& s)
        : m_String(s), m_Position(0)
    { }

    virtual ERW_Result Read(void* buf, size_t count, size_t* bytes_read);
    virtual ERW_Result PendingCount(size_t* count);

private:
    string    m_String;
    SIZE_TYPE m_Position;
};


/// Append all IReader contents to a given string.
void NCBI_XNCBI_EXPORT g_ExtractReaderContents(IReader& reader, string& s);


END_NCBI_SCOPE


/* @} */

#endif  /* CORELIB___STREAM_UTILS__HPP */
