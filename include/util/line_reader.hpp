#ifndef UTIL___LINE_READER__HPP
#define UTIL___LINE_READER__HPP

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
 * Author:  Aaron Ucko, Anatoliy Kuznetsov
 *
 */

/// @file line_reader.hpp
/// Lightweight interface for getting lines of data with minimal
/// memory copying.
///
/// Any implementation must always keep its current line in memory so
/// that callers may harvest data from it in place.

#include <corelib/ncbiobj.hpp>
#include <corelib/ncbifile.hpp>
#include <corelib/reader_writer.hpp>

#include <memory>

/** @addtogroup Miscellaneous
 *
 * @{
 */


BEGIN_NCBI_SCOPE

/// Abstract base class for lightweight line-by-line reading.
class NCBI_XUTIL_EXPORT ILineReader : public CObject
{
public:
    /// Return a new ILineReader object corresponding to the given
    /// filename, taking "-" (but not "./-") to mean standard input.
    static ILineReader* New(const string& filename);

    /// Indicates (negatively) whether there is any more input.
    virtual bool AtEOF(void) const = 0;

    /// Return the next character to be read without consuming it.
    virtual char PeekChar(void) const = 0;

    /// Advance to the next line.  Must also be called prior to
    /// obtaining the first line.
    virtual ILineReader& operator++(void) = 0;

    /// Return the current line, minus its terminator.
    virtual CTempString operator*(void) const = 0;

    /// Return the current (absolute) position.
    virtual CT_POS_TYPE GetPosition(void) const = 0;
};


/// Simple implementation of ILineReader for i(o)streams.
class NCBI_XUTIL_EXPORT CStreamLineReader : public ILineReader
{
public:
    CStreamLineReader(CNcbiIstream& is, EOwnership own = eNoOwnership)
        : m_Stream(is), m_OwnStream(own) { }
    ~CStreamLineReader();

    bool               AtEOF(void) const;
    char               PeekChar(void) const;
    CStreamLineReader& operator++(void);
    CTempString        operator*(void) const;
    CT_POS_TYPE        GetPosition(void) const;

private:
    CNcbiIstream& m_Stream;
    EOwnership    m_OwnStream;
    string        m_Line;
};


/// Simple implementation of ILineReader for regions of memory
/// (such as memory-mapped files).
class NCBI_XUTIL_EXPORT CMemoryLineReader : public ILineReader
{
public:
    /// Work with the half-open range [start, end).
    CMemoryLineReader(const char* start, const char* end)
        : m_Start(start), m_End(end), m_Pos(start) { }
    CMemoryLineReader(const char* start, SIZE_TYPE length)
        : m_Start(start), m_End(start + length), m_Pos(start) { }
    CMemoryLineReader(CMemoryFile* mem_file, EOwnership ownership);

    bool               AtEOF(void) const;
    char               PeekChar(void) const;
    CMemoryLineReader& operator++(void);
    CTempString        operator*(void) const;
    CT_POS_TYPE        GetPosition(void) const;

private:
    const char*           m_Start;
    const char*           m_End;
    const char*           m_Pos;
    CTempString           m_Line;
    auto_ptr<CMemoryFile> m_MemFile;
};

/// Implementation of ILineReader for IReader
///
class NCBI_XUTIL_EXPORT CIReaderLineReader : public ILineReader
{
public:
    /// Work with the half-open range [start, end).
    CIReaderLineReader(IReader* reader, EOwnership ownership);
    virtual ~CIReaderLineReader();

    /// In this implementation buffer size MUST be larger than line 
    /// length we expect
    void SetBufferSize(size_t buf_size);

    bool                AtEOF(void) const;
    char                PeekChar(void) const;
    CIReaderLineReader& operator++(void);
    CTempString         operator*(void) const;
    CT_POS_TYPE         GetPosition(void) const;
private:
    CIReaderLineReader(const CIReaderLineReader&);
    CIReaderLineReader& operator=(const CIReaderLineReader&);
private:
    ERW_Result x_ReadBuffer();
private:
    IReader*      m_Reader;
    EOwnership    m_OwnReader;
    vector<char>  m_Buffer;
    size_t        m_BufferDataSize;
    ERW_Result    m_RW_Result;
    bool          m_Eof;
    size_t        m_BufferReadSize;
    CTempString   m_Line;
};



END_NCBI_SCOPE


/* @} */

#endif  /* UTIL___LINE_READER__HPP */
