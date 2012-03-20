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
* Author:  Aaron Ucko
*
* File Description:
*   Support classes for on-demand CGI input parsing.
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>
#include <cgi/impl/cgi_entry_reader.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_util.hpp>
#include <cgi/error_codes.hpp>

#define NCBI_USE_ERRCODE_X Cgi_API

BEGIN_NCBI_SCOPE

static const string kBoundaryTag        = "boundary=";
static const string kContentDisposition = "Content-Disposition";
static const string kContentType        = "Content-Type";

#define CCER "CCgiEntryReader: "

static bool s_MatchesBoundary(const string& l, const string& b)
{
    return (l == b  ||  (l.size() == b.size() + 2  &&  NStr::StartsWith(l, b)
                         &&  NStr::EndsWith(l, "--")));
}


CCgiEntryReader::~CCgiEntryReader()
{
    if ((m_State & fHitBoundary) == 0) {
        x_Flush();
        x_HitBoundary(false);
    }
}


ERW_Result CCgiEntryReader::Read(void* buf, size_t count, size_t* bytes_read)
{
    _ASSERT((m_State & fHitBoundary) != 0
            ||  m_Context.m_ContentType == TContext::eCT_Multipart);
    if (count > 0) {
        if (m_Buffer.empty()) {
            x_FillBuffer(count);
        }
        size_t n = min(m_Buffer.size(), count);
        memcpy(buf, m_Buffer.data(), n);
        m_Buffer.erase(0, n);
        if (bytes_read) {
            *bytes_read = n;
        }
        if ((m_State & fHitBoundary) != 0  &&  !n ) {
            return eRW_Eof;
        }
    } else if (bytes_read) {
        *bytes_read = 0; // for the record
    }
    return eRW_Success;
}


ERW_Result CCgiEntryReader::PendingCount(size_t* count)
{
    _ASSERT(count);
    if ( !m_Buffer.empty() ) {
        *count = m_Buffer.size();
        return eRW_Success;
    } else if ((m_State & fHitBoundary) != 0) {
        *count = 0;
        return eRW_Eof;
    } else if (m_Context.m_In.rdbuf()->in_avail() <= 0) {
        return eRW_NotImplemented;
    } else if ((m_State & fHitCRLF) == fHitCRLF
               && CT_EQ_INT_TYPE(m_Context.m_In.peek(), CT_TO_INT_TYPE('-'))) {
        return eRW_NotImplemented; // possible boundary
    } else {
        *count = 1;
        return eRW_Success;
    }
}


void CCgiEntryReader::x_FillBuffer(SIZE_TYPE count)
{
    if (count == 0  ||  (m_State & fHitBoundary) != 0) {
        return;
    }
    string    line;
    SIZE_TYPE n_min = count == NPOS ? count : m_Context.m_Boundary.size() + 3;
    while ((m_State & fHitBoundary) == 0  &&  count > m_Buffer.size()) {
        int prev_state = m_State;
        m_State &= ~fUnread;
        // Ensure that the boundary will actually register if present.
        SIZE_TYPE n = max(count - m_Buffer.size(), n_min);
        switch (m_Context.x_DelimitedRead(line, n)) {
        case TContext::eRT_EOF:
            // virtual boundary -- no more entries!
            x_HitBoundary(true);
            if ((m_State & fHitCRLF) == fHitCRLF
                &&  s_MatchesBoundary(line, m_Context.m_Boundary)) {
                return;
            }
            break;

        case TContext::eRT_Delimiter:
            if ((m_State & fHitCRLF) == fHitCRLF
                &&  s_MatchesBoundary(line, m_Context.m_Boundary)) {
                x_HitBoundary(line != m_Context.m_Boundary);
                return; // refrain from adding line to buffer
            }
            m_State |= fHitCRLF;
            break;

        case TContext::eRT_LengthBound:
            m_State &= ~fHitCRLF;
            break;

        case TContext::eRT_PartialDelimiter:
            m_State |= fHitCR;
            m_State &= ~fHitLF;
            break;
        }
        if (m_Buffer.size() + line.size() + 2 > m_Buffer.capacity()) {
            m_Buffer.reserve(min(m_Buffer.capacity() * 2,
                                 m_Buffer.size() + line.size() + 2));
        }
        if ((prev_state & (fUnread | fHitCR)) == fHitCR) {
            m_Buffer += '\r';
            if ((prev_state & fHitLF) != 0) {
                m_Buffer += '\n';
            }
        }
        m_Buffer += line;
    }
}


void CCgiEntryReader::x_HitBoundary(bool final)
{
    m_State |= fHitBoundary;
    if (m_Context.m_CurrentReader == this) {
        m_Context.m_CurrentReader = NULL;
    }
    if (final) {
        m_Context.m_ContentType = TContext::eCT_Null;
    }
}


CCgiEntryReaderContext::CCgiEntryReaderContext(CNcbiIstream& in,
                                               TCgiEntries& out,
                                               const string& content_type,
                                               size_t content_length,
                                               string* content_log)
    : m_In(in), m_Out(out), m_ContentLength(content_length),
      m_ContentLog(content_log), m_Position(0), m_BytePos(0),
      m_CurrentEntry(NULL), m_CurrentReader(NULL)
{
    if (NStr::StartsWith(content_type, "multipart/form-data")) {
        SIZE_TYPE pos = content_type.find(kBoundaryTag);
        if (pos == NPOS) {
            NCBI_THROW(CCgiRequestException, eFormat,
                       CCER "no boundary field in " + content_type);
        }
        m_ContentType = eCT_Multipart;
        m_Boundary = "--" + content_type.substr(pos + kBoundaryTag.size());
        string      line;
        CT_INT_TYPE next = (x_DelimitedRead(line) == eRT_EOF ? CT_EOF
                            : m_In.peek());
        // work around a bug in IE 8 null submission handling
        if ( line.empty()  &&  !CT_EQ_INT_TYPE(next, CT_EOF) ) {
            next = (x_DelimitedRead(line) == eRT_EOF ? CT_EOF : m_In.peek());
        }
        if ( !s_MatchesBoundary(line, m_Boundary)
            ||  (line == m_Boundary  &&  CT_EQ_INT_TYPE(next, CT_EOF))) {
            NCBI_THROW(CCgiRequestException, eFormat,
                       CCER "multipart opening line " + line
                       + " differs from declared boundary " + m_Boundary);
        }
        if (line != m_Boundary) { // null submission(!)
            m_ContentType = eCT_Null;
        }
    } else {
        m_ContentType = eCT_URLEncoded;
        m_Boundary = "&"; // ";" never really caught on
    }
}


CCgiEntryReaderContext::~CCgiEntryReaderContext()
{
    x_FlushCurrentEntry();
}


TCgiEntriesI CCgiEntryReaderContext::GetNextEntry(void)
{
    string name, value, filename, content_type;

    x_FlushCurrentEntry();

    switch (m_ContentType) {
    case eCT_Null:
        return m_Out.end();

    case eCT_URLEncoded:
        x_ReadURLEncodedEntry(name, value);
        break;

    case eCT_Multipart:
        x_ReadMultipartHeaders(name, filename, content_type);
        break;
    }

    if (name.empty()  &&  m_ContentType == eCT_Null) {
        return m_Out.end();
    }

    CCgiEntry    entry(value, filename, ++m_Position, content_type);
    TCgiEntriesI it = m_Out.insert(TCgiEntries::value_type(name, entry));
    if (m_ContentType == eCT_Multipart) {
        m_CurrentEntry = &it->second;
        it->second.SetValue(m_CurrentReader = new CCgiEntryReader(*this));
    }
    return it;
}


void CCgiEntryReaderContext::x_FlushCurrentEntry(void)
{
    if (m_CurrentReader) {
        m_CurrentReader->x_Flush();
        _ASSERT(m_CurrentReader == NULL);
        m_CurrentEntry  = NULL;
    }
}


CCgiEntryReaderContext::EReadTerminator
CCgiEntryReaderContext::x_DelimitedRead(string& s, SIZE_TYPE n)
{
    char            delim      = '\r';
    CT_INT_TYPE     delim_read = CT_EOF;
    EReadTerminator reason     = eRT_Delimiter;

    switch (m_ContentType) {
    case eCT_URLEncoded:
        _ASSERT(n == NPOS);
        delim = m_Boundary[0];
        break;

    case eCT_Multipart:
        break;

    default:
        _TROUBLE;
    }

    // Add 1 to n when not up against the content length to compensate
    // for get()'s insistence on producing (and counting) a trailing
    // NUL.  (When up against the content length, the last byte may
    // require more finesse.)
    if (n != NPOS) {
        ++n;
    }
    if (m_ContentLength != CCgiRequest::kContentLengthUnknown) {
        n = min(n, m_ContentLength - m_BytePos);
    }

    if (n == NPOS) {
        NcbiGetline(m_In, s, delim);
        m_BytePos += s.size();
        if (m_In.eof()) {
            reason = eRT_EOF;
        } else {
            m_In.unget();
            delim_read = m_In.get();
            _ASSERT(CT_EQ_INT_TYPE(delim_read, CT_TO_INT_TYPE(delim)));
            ++m_BytePos;
        }
    } else {
        if (n != 1) {
            AutoArray<char> buffer(n);
            m_In.get(buffer.get(), n, delim);
            s.assign(buffer.get(), (size_t)m_In.gcount());
            m_BytePos += (size_t)m_In.gcount();
        }
        if (m_ContentLength != CCgiRequest::kContentLengthUnknown
            &&  m_BytePos == m_ContentLength - 1  &&  !m_In.eof() ) {
            CT_INT_TYPE next = m_In.peek();
            if ( !CT_EQ_INT_TYPE(next, CT_EOF)
                &&  !CT_EQ_INT_TYPE(next, CT_TO_INT_TYPE(delim))) {
                _VERIFY(next == m_In.get());
                s += CT_TO_CHAR_TYPE(next);
                ++m_BytePos;
            }
        }
        if (m_In.eof()  ||  m_BytePos >= m_ContentLength) {
            reason = eRT_EOF;
        } else {
            // NB: this is an ugly workaround for a buggy STL behavior that
            //     lets short reads (e.g. originating from reading pipes) get
            //     through to the user level, causing istream::read() to
            //     wrongly assert EOF...
            m_In.clear();
            delim_read = m_In.get();
            _ASSERT( !CT_EQ_INT_TYPE(delim_read, CT_EOF) );
            if (CT_EQ_INT_TYPE(delim_read, CT_TO_INT_TYPE(delim))) {
                ++m_BytePos;
            } else {
                reason = eRT_LengthBound;
                m_In.unget();
            }
        }
    }

    if (m_ContentLog) {
        *m_ContentLog += s;
        if (reason == eRT_Delimiter) {
            *m_ContentLog += delim;
        }
    }

    if (m_ContentType == eCT_Multipart  &&  reason == eRT_Delimiter) {
        delim_read = m_In.get();
        if (CT_EQ_INT_TYPE(delim_read, CT_TO_INT_TYPE('\n'))) {
            ++m_BytePos;
            if (m_ContentLog) {
                *m_ContentLog += '\n';
            }
        } else {
            m_In.unget();
            reason = eRT_PartialDelimiter;
        }
    }

    if (m_ContentType == eCT_URLEncoded  &&  NStr::EndsWith(s, HTTP_EOL)
        &&  reason == eRT_EOF) {
        // discard terminal CRLF
        s.resize(s.size() - 2);
    }

    return reason;
}


void CCgiEntryReaderContext::x_ReadURLEncodedEntry(string& name, string& value)
{
    if (x_DelimitedRead(name) == eRT_EOF  ||  m_In.eof()) {
        m_ContentType = eCT_Null;
    }
    SIZE_TYPE name_len = name.find('=');
    if (name_len != NPOS) {
        value = name.substr(name_len + 1);
        name.resize(name_len);
    }
    NStr::URLDecodeInPlace(name);
    NStr::URLDecodeInPlace(value);
}


static CTempString s_FindAttribute(const CTempString& str, const string& name,
                                   CT_POS_TYPE input_pos, bool required)
{
    SIZE_TYPE att_pos = str.find("; " + name + "=\"");
    if (att_pos == NPOS) {
        if (required) {
            NCBI_THROW2(CCgiParseException, eAttribute, CCER
                    "part header lacks required attribute " + name + ": " + str,
                    (std::string::size_type) NcbiStreamposToInt8(input_pos));
        } else {
            return kEmptyStr;
        }
    }
    SIZE_TYPE att_start = att_pos + name.size() + 4;
    SIZE_TYPE att_end   = str.find('\"', att_start);
    if (att_end == NPOS) {
        NCBI_THROW2(CCgiParseException, eAttribute,
                CCER "part header contains unterminated attribute " + name +
                ": " + str.substr(att_pos),
                (std::string::size_type) NcbiStreamposToInt8(input_pos) +
                    att_start);
    }
    return str.substr(att_start, att_end - att_start);
}


void CCgiEntryReaderContext::x_ReadMultipartHeaders(string& name,
                                                    string& filename,
                                                    string& content_type)
{
    string line;
    for (;;) {
        SIZE_TYPE input_pos = m_BytePos;
        switch (x_DelimitedRead(line)) {
        case eRT_Delimiter:
            break;

        case eRT_EOF:
            NCBI_THROW2(CCgiParseException, eEntry,
                        CCER "Hit end of input while reading part headers",
                        input_pos);

        case eRT_LengthBound:
            _TROUBLE;

        case eRT_PartialDelimiter:
            NCBI_THROW2(CCgiParseException, eEntry,
                        CCER "CR in part header not followed by LF", input_pos);
        }

        if (line.empty()) {
            break;
        }

        SIZE_TYPE pos = line.find(':');
        if (pos == NPOS) {
            NCBI_THROW2(CCgiParseException, eEntry,
                        CCER "part header lacks colon: " + line, input_pos);
        }
        CTempString field_name(line, 0, pos);
        if (field_name == kContentDisposition) {
            if (NStr::CompareNocase(line, pos, 13, ": form-data; ") != 0) {
                NCBI_THROW2(CCgiParseException, eEntry,
                            CCER "malformatted Content-Disposition header: "
                            + line,
                            input_pos);
            }
            name     = s_FindAttribute(line, "name",     input_pos, true);
            filename = s_FindAttribute(line, "filename", input_pos, false);
        } else if (field_name == kContentType) {
            content_type = line.substr(pos + 2);
        } else {
            ERR_POST_X(4, Warning << CCER "ignoring unrecognized part header: "
                       + line);
        }
    }
}


END_NCBI_SCOPE
