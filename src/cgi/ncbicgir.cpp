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
* Revision 1.1  1998/12/09 20:18:18  vasilche
* Initial implementation of CGI response generator
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbicgir.hpp>
#include <list>
#include <map>


// This is to use the ANSI C++ standard templates without the "std::" prefix
// NCBI_USING_NAMESPACE_STD;

// This is to use the ANSI C++ standard templates without the "std::" prefix
// and to use NCBI C++ entities without the "ncbi::" prefix
// USING_NCBI_SCOPE;

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

static const tm kZeroTime = { 0 };

inline bool s_ZeroTime(const tm& date)
{
    return ::memcmp(&date, &kZeroTime, sizeof(tm)) == 0;
}

CCgiResponse::CCgiResponse() :
    m_Date(kZeroTime),
    m_LastModified(kZeroTime),
    m_Expires(kZeroTime)
{
}

CCgiResponse::CCgiResponse(const CCgiResponse& response) :
    m_ContentType(response.m_ContentType),
    m_Date(response.m_Date),
    m_LastModified(response.m_LastModified),
    m_Expires(response.m_Expires)
{
}

CCgiResponse::~CCgiResponse()
{
}

CCgiResponse& CCgiResponse::operator=(const CCgiResponse& response)
{
    m_ContentType = response.m_ContentType;
    m_Date = response.m_Date;
    m_LastModified = response.m_LastModified;
    m_Expires = response.m_Expires;
    return *this;
}

bool CCgiResponse::s_GetString(string &out, const string& value)
{
    if ( value.empty() ) {
        return false;
    }
    
    out = value;
    return true;
}

bool CCgiResponse::s_GetDate(string &out, const tm& date)
{
    if ( s_ZeroTime(date) )
        return false;

    char buff[64];
    if ( !::strftime(buff, sizeof(buff), "%a %b %d %H:%M:%S %Y", &date) )
        throw runtime_error("CCgiResponse::s_GetDate() -- strftime() failed");
    out = buff;
    return true;
}

bool CCgiResponse::s_GetDate(tm &out, const tm& date)
{
    if ( s_ZeroTime(date) )
        return false;

    out = date;
    return true;
}

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE
