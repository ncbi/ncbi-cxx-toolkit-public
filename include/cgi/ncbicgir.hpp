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
* Revision 1.9  1999/11/02 20:35:39  vakatov
* Redesigned of CCgiCookie and CCgiCookies to make them closer to the
* cookie standard, smarter, and easier in use
*
* Revision 1.8  1999/05/11 03:11:46  vakatov
* Moved the CGI API(along with the relevant tests) from "corelib/" to "cgi/"
*
* Revision 1.7  1999/04/28 16:54:19  vasilche
* Implemented stream input processing for FastCGI applications.
*
* Revision 1.6  1998/12/30 21:50:27  vakatov
* Got rid of some redundant headers
*
* Revision 1.5  1998/12/28 17:56:26  vakatov
* New CVS and development tree structure for the NCBI C++ projects
*
* Revision 1.4  1998/12/11 22:00:32  vasilche
* Added raw CGI response
*
* Revision 1.3  1998/12/11 18:00:52  vasilche
* Added cookies and output stream
*
* Revision 1.2  1998/12/10 19:58:18  vasilche
* Header option made more generic
*
* Revision 1.1  1998/12/09 20:18:10  vasilche
* Initial implementation of CGI response generator
*
* ===========================================================================
*/

#include <cgi/ncbicgi.hpp>
#include <list>
#include <map>


// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
BEGIN_NCBI_SCOPE

class CCgiResponse {
public:
    // Constructor
    CCgiResponse(CNcbiOstream* out = 0);
    // Destructor
    ~CCgiResponse();

    // Copy constructor
    CCgiResponse(const CCgiResponse& response);
    // Copy assignement operator
    CCgiResponse &operator=(const CCgiResponse& response);

    // Set/query raw type of response
    void SetRawCgi(bool raw);
    bool IsRawCgi(void) const;

    // Header setters
    void SetHeaderValue(const string& name, const string& value);
    void SetHeaderValue(const string& name, const tm& value);
    void RemoveHeaderValue(const string& name);

    // Header getter
    string GetHeaderValue(const string& name) const;
    bool HaveHeaderValue(const string& name) const;

    // Specific header setters:
    // Set content type
    void SetContentType(const string &type);

    // Specific header getters:
    // Get content type
    string GetContentType(void) const;

    // Get cookies set
    const CCgiCookies& Cookies(void) const;
    CCgiCookies& Cookies(void);

    // Set default output stream
    // returns previous output stream
    CNcbiOstream* SetOutput(CNcbiOstream* out);

    // Query output stream
    CNcbiOstream* GetOutput(void) const;

    // Conversion to ostream so that it's possible to write:
    //    response.out() << something << flush;
    CNcbiOstream& out(void) const;

    // Flushes output stream
    void Flush() const;

    // Write HTTP response header
    CNcbiOstream& WriteHeader(void) const;
    CNcbiOstream& WriteHeader(CNcbiOstream& out) const;

protected:
    typedef map<string, string> TMap;

    // Content type header name
    static const string sm_ContentTypeName;
    // Default content type
    static const string sm_ContentTypeDefault;
    // Default HTTP status line
    static const string sm_HTTPStatusDefault;
    
    // Raw CGI flag
    bool m_RawCgi;

    // Header lines in alphabetical order
    TMap m_HeaderValues;

    // Cookies
    CCgiCookies m_Cookies;

    // Defalut output stream
    CNcbiOstream* m_Output;
};


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <cgi/ncbicgir.inl>

// (END_NCBI_SCOPE must be preceeded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* NCBICGIR__HPP */
