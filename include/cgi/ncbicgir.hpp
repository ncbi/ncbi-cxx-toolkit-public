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
* Revision 1.2  1998/12/10 19:58:18  vasilche
* Header option made more generic
*
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

    // Header setters
    void SetHeaderValue(const string& name, const string& value);
    void SetHeaderValue(const string& name, const tm& value);
    void RemoveHeaderValue(const string& name);

    // Header getter
    string GetHeaderValue(const string& name) const;
    bool HaveHeader(const string& name) const;

    // Specific header setters:
    // Set content type
    void SetContentType(const string &type);

    // Specific header getters:
    // Get content type
    string GetContentType() const;


    // Write HTTP response header
    virtual CNcbiOstream& WriteHeader(CNcbiOstream& out) const;

    // Write HTTP response body
    virtual CNcbiOstream& WriteBody(CNcbiOstream& out) const;

    // Writes generated HTTP response
    // throws
    void Write(CNcbiIstream& data, CNcbiOstream& out) const;
    void Write(CNcbiOstream& out) const;

protected:

    static const string sm_ContentTypeName;
    static const string sm_ContentTypeDefault;
    
    typedef map<string, string> TMap;
    TMap m_HeaderValues;

};


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <ncbicgir.inl>


// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGIR__HPP */
