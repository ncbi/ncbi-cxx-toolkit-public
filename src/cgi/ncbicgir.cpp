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

#include <ncbistd.hpp>
#include <ncbicgir.hpp>
#include <list>
#include <map>
#include <time.h>


// This is to use the ANSI C++ standard templates without the "std::" prefix
// NCBI_USING_NAMESPACE_STD;

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
// USING_NCBI_SCOPE;

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

static const tm kZeroTime = { 0 };

const string CCgiResponse::sm_ContentTypeName = "Content-type";
const string CCgiResponse::sm_ContentTypeDefault = "text/html";

inline bool s_ZeroTime(const tm& date)
{
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}

CCgiResponse::CCgiResponse()
    : m_Output(0)
{
}

CCgiResponse::CCgiResponse(const CCgiResponse& response)
    : m_Output(0)
{
    m_HeaderValues = response.m_HeaderValues;
}

CCgiResponse::~CCgiResponse()
{
}

CCgiResponse& CCgiResponse::operator=(const CCgiResponse& response)
{
    m_HeaderValues = response.m_HeaderValues;
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
        m_HeaderValues.insert(TMap::value_type(name, value));
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
    if ( !m_Output )
        throw runtime_error("CCgiResponse: Null output stream");
    return *m_Output;
}

CNcbiOstream& CCgiResponse::WriteHeader(CNcbiOstream& out) const
{
    TMap& xx_HeaderValues = const_cast<TMap&>(m_HeaderValues); // BW_01
    for ( TMap::const_iterator i = xx_HeaderValues.begin();
          i != xx_HeaderValues.end(); ++i ) {
        out << i->first << ": " << i->second << NcbiEndl;
    }
    if ( !HaveHeaderValue(sm_ContentTypeName) ) {
        out << sm_ContentTypeName << ": " << sm_ContentTypeDefault << NcbiEndl;
    }
    if ( HaveCookies() ) {
        out << m_Cookies;
    }
    return out << NcbiEndl;
}

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
