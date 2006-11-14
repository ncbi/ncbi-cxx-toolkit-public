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
* Authors:  Eugene Vasilchenko, Denis Vakatov
*
* File Description:
*   CCgiResponse  -- CGI response generator class
*
*/


#include <ncbi_pch.hpp>
#include <cgi/ncbicgir.hpp>
#include <cgi/cgi_exception.hpp>
#include <cgi/cgi_session.hpp>
#include <time.h>

// Mac OS has unistd.h, but STDOUT_FILENO is not defined
#ifdef NCBI_OS_MAC 
#  define STDOUT_FILENO 1
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  define STDOUT_FILENO 1
#endif


BEGIN_NCBI_SCOPE


const string CCgiResponse::sm_ContentTypeName    = "Content-Type";
const string CCgiResponse::sm_LocationName       = "Location";
const string CCgiResponse::sm_ContentTypeDefault = "text/html";
const string CCgiResponse::sm_ContentTypeMixed   = "multipart/mixed";
const string CCgiResponse::sm_ContentTypeRelated = "multipart/related";
const string CCgiResponse::sm_ContentTypeXMR     = "multipart/x-mixed-replace";
const string CCgiResponse::sm_ContentDispoName   = "Content-Disposition";
const string CCgiResponse::sm_FilenamePrefix     = "attachment; filename=\"";
const string CCgiResponse::sm_HTTPStatusName     = "Status";
const string CCgiResponse::sm_HTTPStatusDefault  = "200 OK";
const string CCgiResponse::sm_BoundaryPrefix     = "NCBI_CGI_Boundary_";

NCBI_PARAM_DEF_IN_SCOPE(bool, CGI, ThrowOnBadOutput, true, CCgiResponse);


inline bool s_ZeroTime(const tm& date)
{
    static const tm kZeroTime = { 0 };
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}


CCgiResponse::CCgiResponse(CNcbiOstream* os, int ofd)
    : m_IsRawCgi(false),
      m_IsMultipart(eMultipart_none),
      m_BetweenParts(false),
      m_Output(NULL),
      m_OutputFD(0),
      m_Session(NULL)
{
    SetOutput(os ? os  : &NcbiCout,
              os ? ofd : STDOUT_FILENO  // "os" on this line is NOT a typo
              );
}


CCgiResponse::~CCgiResponse(void)
{
    x_RestoreOutputExceptions();
}


bool CCgiResponse::HaveHeaderValue(const string& name) const
{
    return m_HeaderValues.find(name) != m_HeaderValues.end();
}


string CCgiResponse::GetHeaderValue(const string &name) const
{
    TMap::const_iterator ptr = m_HeaderValues.find(name);

    return (ptr == m_HeaderValues.end()) ? kEmptyStr : ptr->second;
}


void CCgiResponse::RemoveHeaderValue(const string& name)
{
    m_HeaderValues.erase(name);
}


void CCgiResponse::SetHeaderValue(const string& name, const string& value)
{
    if ( value.empty() ) {
        RemoveHeaderValue(name);
    } else {
        m_HeaderValues[name] = value;
    }
}


void CCgiResponse::SetHeaderValue(const string& name, const struct tm& date)
{
    if ( s_ZeroTime(date) ) {
        RemoveHeaderValue(name);
        return;
    }

    char buff[64];
    if ( !::strftime(buff, sizeof(buff),
                     "%a, %d %b %Y %H:%M:%S GMT", &date) ) {
        NCBI_THROW(CCgiErrnoException, eErrno,
                   "CCgiResponse::SetHeaderValue() -- strftime() failed");
    }
    SetHeaderValue(name, buff);
}


void CCgiResponse::SetHeaderValue(const string& name, const CTime& date)
{
    if ( date.IsEmpty()) {
        RemoveHeaderValue(name);
        return;
    }
    SetHeaderValue(name,
                   date.GetGmtTime().AsString("w, D b Y h:m:s") + " GMT");
}


void CCgiResponse::SetStatus(unsigned int code, const string& reason)
{
    if (code < 100) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- code too small, below 100");
    }
    if (code > 999) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- code too big, exceeds 999");
    }
    if (reason.find_first_of("\r\n") != string::npos) {
        THROW1_TRACE(runtime_error,
                     "CCgiResponse::SetStatus() -- text contains CR or LF");
    }
    SetHeaderValue(sm_HTTPStatusName, NStr::UIntToString(code) + ' ' + reason);
}


void CCgiResponse::x_RestoreOutputExceptions(void)
{
    if (m_Output  &&  m_ThrowOnBadOutput.Get()) {
        m_Output->exceptions(m_OutputExpt);
    }
}


void CCgiResponse::SetOutput(CNcbiOstream* out, int fd)
{
    x_RestoreOutputExceptions();

    m_Output   = out;
    m_OutputFD = fd;

    // Make the output stream to throw on write if it's in a bad state
    if (m_Output  &&  m_ThrowOnBadOutput.Get()) {
        m_OutputExpt = m_Output->exceptions();
        m_Output->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    }
}

CNcbiOstream* CCgiResponse::GetOutput(void) const
{
    if (m_Output  &&
        (m_Output->rdstate()  &  (IOS_BASE::badbit | IOS_BASE::failbit))
        != 0  &&
        m_ThrowOnBadOutput.Get()) {
        ERR_POST(Critical <<
                 "CCgiResponse::GetOutput() -- output stream is in bad state");
        const_cast<CCgiResponse*>(this)->SetThrowOnBadOutput(false);
    }
    return m_Output;
}


CNcbiOstream& CCgiResponse::out(void) const
{
    if ( !m_Output ) {
        THROW1_TRACE(runtime_error, "CCgiResponse::out() on NULL out.stream");
    }
    return *GetOutput();
}


CNcbiOstream& CCgiResponse::WriteHeader(CNcbiOstream& os) const
{
    // HTTP status line (if "raw CGI" response)
    bool skip_status = false;
    if ( IsRawCgi() ) {
        string status = GetHeaderValue(sm_HTTPStatusName);
        if ( status.empty() ) {
            status = sm_HTTPStatusDefault;
        } else {
            skip_status = true;  // filter out the status from the HTTP header
        }
        os << "HTTP/1.1 " << status << HTTP_EOL;
    }

    if (m_IsMultipart != eMultipart_none
        &&  CCgiUserAgent().GetEngine() == CCgiUserAgent::eEngine_IE) {
        // MSIE requires multipart responses to start with these extra
        // headers, which confuse other browsers. :-/
        os << sm_ContentTypeName << ": message/rfc822" << HTTP_EOL << HTTP_EOL
           << "Mime-Version: 1.0" << HTTP_EOL;
    }

    // Default content type (if it's not specified by user already)
    switch (m_IsMultipart) {
    case eMultipart_none:
        if ( !HaveHeaderValue(sm_ContentTypeName) ) {
            os << sm_ContentTypeName << ": " << sm_ContentTypeDefault
               << HTTP_EOL;
        }
        break;
    case eMultipart_mixed:
        os << sm_ContentTypeName << ": " << sm_ContentTypeMixed
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    case eMultipart_related:
        os << sm_ContentTypeName << ": " << sm_ContentTypeRelated
           << "; type=" << (HaveHeaderValue(sm_ContentTypeName)
                            ? sm_ContentTypeName : sm_ContentTypeDefault)
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    case eMultipart_replace:
        os << sm_ContentTypeName << ": " << sm_ContentTypeXMR
           << "; boundary=" << m_Boundary << HTTP_EOL;
        break;
    }
    
    if (m_Session) {
        const CCgiCookie * const scookie = m_Session->GetSessionCookie();
        if (scookie) {
            const_cast<CCgiResponse*>(this)->Cookies().Add(*scookie);
        }
    }
    if (m_TrackingCookie.get()) {
        const_cast<CCgiResponse*>(this)->Cookies().Add(*m_TrackingCookie);
    }
    
    // Cookies (if any)
    if ( !Cookies().Empty() ) {
        os << m_Cookies;
    }

    // All header lines (in alphabetical order)
    ITERATE (TMap, i, m_HeaderValues) {
        if (skip_status  &&  NStr::EqualNocase(i->first, sm_HTTPStatusName)) {
            break;
        } else if (m_IsMultipart != eMultipart_none
                   &&  NStr::StartsWith(i->first, "Content-", NStr::eNocase)) {
            break;
        }
        os << i->first << ": " << i->second << HTTP_EOL;
    }

    if (m_IsMultipart != eMultipart_none) { // proceed with first part
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL;
        if ( !HaveHeaderValue(sm_ContentTypeName) ) {
            os << sm_ContentTypeName << ": " << sm_ContentTypeDefault
               << HTTP_EOL;
        }
        for (TMap::const_iterator it = m_HeaderValues.lower_bound("Content-");
             it != m_HeaderValues.end()
             &&  NStr::StartsWith(it->first, "Content-", NStr::eNocase);
             ++it) {
            os << it->first << ": " << it->second << HTTP_EOL;
        }
    }

    // End of header (empty line)
    return os << HTTP_EOL;
}

void CCgiResponse::BeginPart(const string& name, const string& type_in,
                             CNcbiOstream& os)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    if ( !m_BetweenParts ) {
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL;
    }

    string type = type_in;
    if (type.empty() /* &&  m_IsMultipart == eMultipart_replace */) {
        type = GetHeaderValue(sm_ContentTypeName);
    }
    os << sm_ContentTypeName << ": "
       << (type.empty() ? sm_ContentTypeDefault : type) << HTTP_EOL;

    if ( !name.empty() ) {
        os << sm_ContentDispoName << ": " << sm_FilenamePrefix
           << NStr::PrintableString(name) << '"' << HTTP_EOL;
    } else if (m_IsMultipart != eMultipart_replace) {
        ERR_POST(Warning << "multipart content contains anonymous part");
    }

    os << HTTP_EOL;
}

void CCgiResponse::EndPart(CNcbiOstream& os)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    if ( !m_BetweenParts ) {
        os << HTTP_EOL << "--" << m_Boundary << HTTP_EOL << NcbiFlush;
    }
    m_BetweenParts = true;
}

void CCgiResponse::EndLastPart(CNcbiOstream& os)
{
    _ASSERT(m_IsMultipart != eMultipart_none);
    os << HTTP_EOL << "--" << m_Boundary << "--" << HTTP_EOL << NcbiFlush;
    m_IsMultipart = eMultipart_none; // forbid adding more parts
}


void CCgiResponse::Flush(void) const
{
    out() << NcbiFlush;
}


void CCgiResponse::SetTrackingCookie(const string& name, const string& value,
                                     const string& domain, const string& path,
                                     const CTime& exp_time)
{
    m_TrackingCookie.reset(new CCgiCookie(name,value,domain,path));
    if (!exp_time.IsEmpty())
        m_TrackingCookie->SetExpTime(exp_time);
}


void CCgiResponse::SetThrowOnBadOutput(bool throw_on_bad_output)
{
    m_ThrowOnBadOutput.Set(throw_on_bad_output);
    if (m_Output  &&  throw_on_bad_output) {
        m_OutputExpt = m_Output->exceptions();
        m_Output->exceptions(IOS_BASE::badbit | IOS_BASE::failbit);
    }
}


END_NCBI_SCOPE


/*
* ===========================================================================
* $Log$
* Revision 1.30  2006/11/14 15:31:03  grichenk
* Restore output stream exceptions (fix for MIPS bug).
*
* Revision 1.29  2006/11/07 19:19:56  vakatov
* By default, throw an exception if the output CGI stream goes bad
*
* Revision 1.28  2006/10/18 18:33:09  vakatov
* Allow to throw an exception in case the output becomes bad or closed
* ([CGI.ThrowOnBadOutput] parameter)
*
* Revision 1.27  2006/09/12 14:27:52  ucko
* Add a SetFilename method and an API for producing multipart responses.
*
* Revision 1.26  2006/06/29 14:32:43  didenko
* Added tracking cookie
*
* Revision 1.25  2005/12/19 16:55:04  didenko
* Improved CGI Session implementation
*
* Revision 1.24  2005/12/15 18:21:15  didenko
* Added CGI session support
*
* Revision 1.23  2005/11/08 20:30:49  grichenk
* Added SetLocation(CUrl)
*
* Revision 1.22  2005/09/01 17:41:24  lavr
* +SetHeaderValue(CTime&)
*
* Revision 1.21  2004/05/17 20:56:50  gorelenk
* Added include of PCH ncbi_pch.hpp
*
* Revision 1.20  2003/07/14 20:26:39  vakatov
* CCgiResponse::SetHeaderValue() -- date format to conform with RFC1123
*
* Revision 1.19  2003/03/12 16:10:23  kuznets
* iterate -> ITERATE
*
* Revision 1.18  2003/02/24 20:01:55  gouriano
* use template-based exceptions instead of errno and parse exceptions
*
* Revision 1.17  2002/07/18 20:18:36  lebedev
* NCBI_OS_MAC: STDOUT_FILENO define added
*
* Revision 1.16  2002/07/11 14:22:59  gouriano
* exceptions replaced by CNcbiException-type ones
*
* Revision 1.15  2002/04/15 01:18:42  lavr
* Check if status is less than 100
*
* Revision 1.14  2002/03/19 00:34:56  vakatov
* Added convenience method CCgiResponse::SetStatus().
* Treat the status right in WriteHeader() for both plain and "raw CGI" cases.
* Made all header values to be case-insensitive.
*
* Revision 1.13  2001/10/04 18:17:53  ucko
* Accept additional query parameters for more flexible diagnostics.
* Support checking the readiness of CGI input and output streams.
*
* Revision 1.12  2001/07/17 22:39:54  vakatov
* CCgiResponse:: Made GetOutput() fully consistent with out().
* Prohibit copy constructor and assignment operator.
* Retired "ncbicgir.inl", moved inline functions to the header itself, made
* some non-inline. Improved comments, got rid of some unused STL headers.
* Well-groomed the code.
*
* Revision 1.11  2000/11/01 20:36:31  vasilche
* Added HTTP_EOL string macro.
*
* Revision 1.10  1999/11/02 20:35:43  vakatov
* Redesigned of CCgiCookie and CCgiCookies to make them closer to the
* cookie standard, smarter, and easier in use
*
* Revision 1.9  1999/05/11 03:11:52  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.8  1999/04/28 16:54:43  vasilche
* Implemented stream input processing for FastCGI applications.
* Fixed POST request parsing
*
* Revision 1.7  1999/02/22 22:45:39  vasilche
* Fixed map::insert(value_type) usage.
*
* Revision 1.6  1998/12/28 17:56:36  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.5  1998/12/23 13:57:55  vasilche
* Default output stream will be NcbiCout.
*
* Revision 1.4  1998/12/11 22:00:34  vasilche
* Added raw CGI response
*
* Revision 1.3  1998/12/11 18:00:54  vasilche
* Added cookies and output stream
*
* Revision 1.2  1998/12/10 19:58:22  vasilche
* Header option made more generic
*
* Revision 1.1  1998/12/09 20:18:18  vasilche
* Initial implementation of CGI response generator
*
* ===========================================================================
*/
