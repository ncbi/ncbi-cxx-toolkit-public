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
* Author: Eugene Vasilchenko
*
* File Description:
*  NCBI CGI response generator
*
* ---------------------------------------------------------------------------
* $Log$
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

#include <cgi/ncbicgir.hpp>
#include <time.h>


BEGIN_NCBI_SCOPE


const string CCgiResponse::sm_ContentTypeName    = "Content-Type";
const string CCgiResponse::sm_ContentTypeDefault = "text/html";
const string CCgiResponse::sm_HTTPStatusDefault  = "HTTP/1.1 200 OK";


inline bool s_ZeroTime(const tm& date)
{
    static const tm kZeroTime = { 0 };
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}


CCgiResponse::CCgiResponse(CNcbiOstream* os)
    : m_IsRawCgi(false),
      m_Output(os ? os : &NcbiCout)
{
    return;
}


CCgiResponse::~CCgiResponse(void)
{
    return;
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


void CCgiResponse::SetHeaderValue(const string& name, const tm& date)
{
    if ( s_ZeroTime(date) ) {
        RemoveHeaderValue(name);
        return;
    }

    char buff[64];
    if ( !::strftime(buff, sizeof(buff), "%a %b %d %H:%M:%S %Y", &date) ) {
        THROW1_TRACE(CErrnoException,
                     "CCgiResponse::SetHeaderValue() -- strftime() failed");
    }
    SetHeaderValue(name, buff);
}


CNcbiOstream& CCgiResponse::out(void) const
{
    if ( !m_Output ) {
        THROW1_TRACE(runtime_error, "CCgiResponse::out() on NULL out.stream");
    }
    return *m_Output;
}


CNcbiOstream& CCgiResponse::WriteHeader(CNcbiOstream& os) const
{
    // HTTP status line (if "raw CGI" response)
    if ( IsRawCgi() ) {
        os << sm_HTTPStatusDefault << HTTP_EOL;
    }

    // Default content type (if it's not specified by user already)
    if ( !HaveHeaderValue(sm_ContentTypeName) ) {
        os << sm_ContentTypeName << ": " << sm_ContentTypeDefault << HTTP_EOL;
    }

    // Cookies (if any)
    if ( !Cookies().Empty() ) {
        os << m_Cookies;
    }

    // All header lines (in alphabetical order)
    iterate (TMap, i, m_HeaderValues) {
        os << i->first << ": " << i->second << HTTP_EOL;
    }

    // End of header (empty line)
    return os << HTTP_EOL;
}


void CCgiResponse::Flush(void) const
{
    out() << NcbiFlush;
}


END_NCBI_SCOPE
