#ifndef CGI___NCBICGIR__HPP
#define CGI___NCBICGIR__HPP

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

#include <cgi/ncbicgi.hpp>
#include <map>


/** @addtogroup CGIReqRes
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XCGI_EXPORT CCgiResponse
{
public:
    // 'ctors
    CCgiResponse(CNcbiOstream* os = 0, int ofd = -1);
    // (default sets out.stream to "cout", ofd to STDOUT_FILENO)
    ~CCgiResponse(void);

    // Set/get the "raw CGI" response type
    void SetRawCgi(bool is_raw);
    bool IsRawCgi(void) const;

    // Set response status.
    // NOTE:  use this method rather than SetHeaderValue("Status", ...)
    //        as it is safer and it works for the "raw CGI" response type, too.
    void SetStatus(unsigned int code, const string& reason = kEmptyStr);

    // Header setters
    void SetHeaderValue   (const string& name, const string& value);
    void SetHeaderValue   (const string& name, const tm&     value);
    void RemoveHeaderValue(const string& name);

    // Header getter
    string GetHeaderValue (const string& name) const;
    bool   HaveHeaderValue(const string& name) const;

    // Set/get content type [text/html by default if not provided]
    void   SetContentType(const string& type);
    string GetContentType(void) const;

    // Get cookies set
    const CCgiCookies& Cookies(void) const;
    CCgiCookies&       Cookies(void);

    // Set/get output stream (NULL here means "no output stream")
    void          SetOutput(CNcbiOstream* os, int fd = -1);
    CNcbiOstream* GetOutput(void) const;
    int           GetOutputFD(void) const;

    // Get output stream.  Throw exception if GetOutput() is NULL
    CNcbiOstream& out(void) const;

    // Flush output stream
    void Flush(void) const;

    // Write HTTP response header to the output stream
    CNcbiOstream& WriteHeader(void) const;
    CNcbiOstream& WriteHeader(CNcbiOstream& os) const;

protected:
    static const string sm_ContentTypeName;     // Content type header name
    static const string sm_ContentTypeDefault;  // Dflt content type: text/html
    static const string sm_HTTPStatusName;      // Status header name:   Status
    static const string sm_HTTPStatusDefault;   // Default HTTP status:  200 OK
    
    typedef map<string, string, PNocase> TMap;

    bool          m_IsRawCgi;      // The "raw CGI" flag
    TMap          m_HeaderValues;  // Header lines in alphabetical order
    CCgiCookies   m_Cookies;       // Cookies
    CNcbiOstream* m_Output;        // Default output stream
    int           m_OutputFD;      // Output file descriptor, if available.

    // Prohibit copy constructor and assignment operator
    CCgiResponse(const CCgiResponse&);
    CCgiResponse& operator= (const CCgiResponse&);
};


/* @} */


/////////////////////////////////////////////////////////////////////////////
//  IMPLEMENTATION of INLINE functions
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
//  CCgiResponse::
//

inline void CCgiResponse::SetRawCgi(bool is_raw)
{
    m_IsRawCgi = is_raw;
}

inline bool CCgiResponse::IsRawCgi(void) const
{
    return m_IsRawCgi;
}

inline void CCgiResponse::SetContentType(const string& type)
{
    SetHeaderValue(sm_ContentTypeName, type);
}

inline string CCgiResponse::GetContentType(void) const
{
    return GetHeaderValue(sm_ContentTypeName);
}

inline const CCgiCookies& CCgiResponse::Cookies(void) const
{
    return m_Cookies;
}

inline CCgiCookies& CCgiResponse::Cookies(void)
{
    return m_Cookies;
}

inline void CCgiResponse::SetOutput(CNcbiOstream* out, int fd)
{
    m_Output   = out;
    m_OutputFD = fd;
}

inline CNcbiOstream* CCgiResponse::GetOutput(void) const
{
    return m_Output;
}

inline int CCgiResponse::GetOutputFD(void) const
{
    return m_OutputFD;
}

inline CNcbiOstream& CCgiResponse::WriteHeader(void) const
{
    return WriteHeader(out());
}


END_NCBI_SCOPE



/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.16  2004/06/22 16:50:51  lavr
 * Note default content type (if explicit one is not provided)
 *
 * Revision 1.15  2003/11/05 18:40:55  dicuccio
 * Added export specifiers
 *
 * Revision 1.14  2003/04/10 19:01:44  siyan
 * Added doxygen support
 *
 * Revision 1.13  2002/03/19 00:34:54  vakatov
 * Added convenience method CCgiResponse::SetStatus().
 * Treat the status right in WriteHeader() for both plain and "raw CGI" cases.
 * Made all header values to be case-insensitive.
 *
 * Revision 1.12  2001/10/04 18:17:52  ucko
 * Accept additional query parameters for more flexible diagnostics.
 * Support checking the readiness of CGI input and output streams.
 *
 * Revision 1.11  2001/07/17 22:39:53  vakatov
 * CCgiResponse:: Made GetOutput() fully consistent with out().
 * Prohibit copy constructor and assignment operator.
 * Retired "ncbicgir.inl", moved inline functions to the header itself, made
 * some non-inline. Improved comments, got rid of some unused STL headers.
 * Well-groomed the code.
 *
 * Revision 1.10  2001/05/17 14:49:37  lavr
 * Typos corrected
 *
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

#endif  /* CGI___NCBICGIR__HPP */
