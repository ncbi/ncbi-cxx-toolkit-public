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

#include <corelib/ncbistd.hpp>
#include <cgi/ncbicgir.hpp>
#include <list>
#include <map>
#include <time.h>


// This is to use the ANSI C++ standard templates without the "std::" prefix
// NCBI_USING_NAMESPACE_STD;

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
// USING_NCBI_SCOPE;

BEGIN_NCBI_SCOPE

static const tm kZeroTime = { 0 };

const string CCgiResponse::sm_ContentTypeName = "Content-Type";
const string CCgiResponse::sm_ContentTypeDefault = "text/html";
const string CCgiResponse::sm_HTTPStatusDefault = "HTTP/1.1 200 OK";

inline bool s_ZeroTime(const tm& date)
{
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}

CCgiResponse::CCgiResponse(CNcbiOstream* out)
    : m_RawCgi(false), m_Output(out)
{
}

CCgiResponse::CCgiResponse(const CCgiResponse& response)
{
    *this = response;
}

CCgiResponse::~CCgiResponse()
{
}

CCgiResponse& CCgiResponse::operator=(const CCgiResponse& response)
{
    m_RawCgi = response.m_RawCgi;
    m_HeaderValues = response.m_HeaderValues;
    m_Cookies.Clear();
    m_Cookies.Add(response.m_Cookies);
    return *this;
}

bool CCgiResponse::HaveHeaderValue(const string& name) const
{
    TMap& xx_HeaderValues = const_cast<TMap&>(m_HeaderValues); // BW_01
    return xx_HeaderValues.find(name) != xx_HeaderValues.end();
}

string CCgiResponse::GetHeaderValue(const string &name) const
{
    TMap& xx_HeaderValues = const_cast<TMap&>(m_HeaderValues); // BW_01
    TMap::const_iterator ptr = xx_HeaderValues.find(name);
    
    if ( ptr == xx_HeaderValues.end() )
        return string();

    return ptr->second;
}

void CCgiResponse::RemoveHeaderValue(const string& name)
{
    m_HeaderValues.erase(name);
}

void CCgiResponse::SetHeaderValue(const string &name, const string& value)
{
    if ( value.empty() ) {
        RemoveHeaderValue(name);
    } else {
        m_HeaderValues[name] = value;
    }
}

void CCgiResponse::SetHeaderValue(const string &name, const tm& date)
{
    if ( s_ZeroTime(date) ) {
        RemoveHeaderValue(name);
    } else {
        char buff[64];
        if ( !::strftime(buff, sizeof(buff), "%a %b %d %H:%M:%S %Y", &date) )
            throw runtime_error("CCgiResponse::SetHeaderValue() -- strftime() failed");
        SetHeaderValue(name, buff);
    }
}

CNcbiOstream& CCgiResponse::out(void) const
{
    if ( !m_Output ) {
        return NcbiCout;
    }
    return *m_Output;
}

CNcbiOstream& CCgiResponse::WriteHeader(CNcbiOstream& out) const
{
    if ( IsRawCgi() ) {
        // Write HTTP status line for raw CGI response
        out << sm_HTTPStatusDefault << NcbiEndl;
    }

    // write default content type (if not set by user)
    if ( !HaveHeaderValue(sm_ContentTypeName) ) {
        out << sm_ContentTypeName << ": " << sm_ContentTypeDefault << NcbiEndl;
    }

    // write cookies
    if ( HaveCookies() ) {
        out << m_Cookies;
    }

    // Write all header lines in alphabetic order
    TMap& xx_HeaderValues = const_cast<TMap&>(m_HeaderValues); // BW_01
    for ( TMap::const_iterator i = xx_HeaderValues.begin();
          i != xx_HeaderValues.end(); ++i ) {
        out << i->first << ": " << i->second << NcbiEndl;
    }

    // write empty line - end of header
    return out << NcbiEndl;
}

END_NCBI_SCOPE
