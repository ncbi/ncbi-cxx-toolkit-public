#ifndef NCBICGIR__HPP
#define NCBICGIR__HPP

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
*  NCBI CGI response generator class
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  1998/12/09 20:18:10  vasilche
* Initial implementation of CGI response generator
*
* ===========================================================================
*/

#include <ncbistd.hpp>
#include <ncbistre.hpp>
#include <list>
#include <map>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE


class CCgiResponse {
public:
    // Constructor
    CCgiResponse();
    // Destructor
    ~CCgiResponse();

    // Copy constructor
    CCgiResponse(const CCgiResponse& response);
    // Copy assignement operator
    CCgiResponse &operator=(const CCgiResponse& response);

    // Attribute setters:

    // Set content type
    void SetContentType(const string &type);

    // Set document generation date
    void SetDate(const tm& date);

    // Set document modification date
    void SetLastModified(const tm& date);

    // Set expiration date
    void SetExpires(const tm& date);

    // Attribute getters:
    // return false if this attribute was not set (have default value)

    // Get content type
    bool GetContentType(string& type) const;

    // Get document generation date
    bool GetDate(tm& date) const;
    bool GetDate(string& date) const;

    // Get document modification date
    bool GetLastModified(tm& date) const;
    bool GetLastModified(string& date) const;

    // Get expiration date
    bool GetExpires(tm& date) const;
    bool GetExpires(string& date) const;

    // Writes generated HTTP response
    // throws
    void Write(CNcbiIstream& data, CNcbiOstream& out) const;
    void Write(CNcbiOstream& out) const;

protected:
    
    string m_ContentType;
    tm m_Date;
    tm m_LastModified;
    tm m_Expires;

    // helpers
    static bool s_GetString(string &out, const string& value);
    static bool s_GetDate(string &out, const tm& date);
    static bool s_GetDate(tm &out, const tm& date);
};


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <ncbicgir.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGIR__HPP */
