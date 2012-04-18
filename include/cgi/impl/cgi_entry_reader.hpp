#ifndef CGI_IMPL___CGI_ENTRY_READER__HPP
#define CGI_IMPL___CGI_ENTRY_READER__HPP

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
 * Authors:  Aaron Ucko
 *
 */

/// @file cgi_entry_reader.hpp
/// Support classes for on-demand CGI input parsing.


#include <cgi/ncbicgi.hpp>


/** @addtogroup CGIReqRes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class CCgiEntryReader : public IReader
{
public:
    typedef CCgiEntryReaderContext TContext;

    ERW_Result Read(void* buf, size_t count, size_t* bytes_read);
    ERW_Result PendingCount(size_t* count);

private:
    enum EState {
        fUnread      = 0x1,
        fHitCR       = 0x2,
        fHitLF       = 0x4,
        fHitCRLF     = fHitCR | fHitLF,
        fHitBoundary = 0x8
    };
    typedef int TState;

    CCgiEntryReader(TContext& context)
        : m_Context(context), m_State(fUnread | fHitCRLF)
        { }
    ~CCgiEntryReader();

    void x_FillBuffer(SIZE_TYPE count);
    void x_Flush(void) { x_FillBuffer(NPOS); }
    void x_HitBoundary(bool final);

    TContext& m_Context;
    string    m_Buffer;
    TState    m_State;

    friend class CCgiEntryReaderContext;
};


class CCgiEntryReaderContext
{
public:
    CCgiEntryReaderContext(CNcbiIstream& in, TCgiEntries& out,
                           const string& content_type,
                           size_t content_length
                               = CCgiRequest::kContentLengthUnknown,
                           string* content_log = NULL);
    ~CCgiEntryReaderContext();

    TCgiEntriesI GetNextEntry(void);

private:
    typedef CCgiEntryReader TReader;
    enum EContentType {
        eCT_Null, // at end of input
        eCT_URLEncoded,
        eCT_Multipart
    };
    enum EReadTerminator {
        eRT_Delimiter,
        eRT_EOF,
        eRT_LengthBound,
        eRT_PartialDelimiter
    };

    void            x_FlushCurrentEntry(void);
    EReadTerminator x_DelimitedRead(string& s, SIZE_TYPE n = NPOS);
    void            x_ReadURLEncodedEntry(string& name, string& value);
    void            x_ReadMultipartHeaders(string& name, string& filename,
                                           string& content_type);

    CNcbiIstream& m_In;
    TCgiEntries&  m_Out;
    EContentType  m_ContentType;
    bool          m_ContentTypeDeclared;
    size_t        m_ContentLength;
    string        m_Boundary;
    string*       m_ContentLog;
    unsigned int  m_Position;
    SIZE_TYPE     m_BytePos;
    CCgiEntry*    m_CurrentEntry;
    TReader*      m_CurrentReader;

    friend class CCgiEntryReader;
};


END_NCBI_SCOPE


/* @} */

#endif  /* CGI_IMPL___CGI_ENTRY_READER__HPP */
