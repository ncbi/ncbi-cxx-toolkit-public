#ifndef CGI___CGI_EXCEPTION__HPP
#define CGI___CGI_EXCEPTION__HPP

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
* Authors:  Andrei Gourianov, Denis Vakatov
*
*/

/// @file cgi_exception.hpp
/// Exception classes used by the NCBI CGI framework
///


#include <corelib/ncbiexpt.hpp>


/** @addtogroup CGIExcep
 *
 * @{
 */


BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiException --
///
///   Base class for the exceptions used by CGI framework

class CCgiException : EXCEPTION_VIRTUAL_BASE public CException
{
    NCBI_EXCEPTION_DEFAULT(CCgiException, CException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCgiCookieException --
///
///   Exceptions used by CCgiCookie and CCgiCookies classes

class CCgiCookieException : public CParseTemplException<CCgiException>
{
public:
    enum EErrCode {
        eValue,     //< Bad cookie value
        eString     //< Bad cookie string (Set-Cookie:) format
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eValue:    return "Bad cookie value";
        case eString:   return "Bad cookie string format";
        default:        return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT2
    (CCgiCookieException, CParseTemplException<CCgiException>,
     std::string::size_type);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCgiRequestException --
///
///
///   Exceptions to be used by CGI framework itself (see CCgiParseException)
///   or the CGI application's request processing code in the cases when there
///   is a problem is in the HTTP request itself  (its header and/or body).
///   The problem can be in the syntax as well as in the content.

class CCgiRequestException : public CCgiException
{
public:
    /// Bad (malformed or missing) HTTP request components
    enum EErrCode {
        eCookie,     //< Cookie
        eRead,       //< Error in reading raw content of HTTP request
        eIndex,      //< ISINDEX
        eEntry,      //< Entry value
        eAttribute,  //< Entry attribute
        eFormat,     //< Format or encoding
        eData,       //< Syntaxically correct but contains odd data (from the
                     //< point of view of particular CGI application)
        eSessionImpNotRegistred,
        eSessionDoesnotExist
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eCookie:    return "Malformed HTTP Cookie";
        case eRead:      return "Error in receiving HTTP request";
        case eIndex:     return "Error in parsing ISINDEX-type CGI arguments";
        case eEntry:     return "Error in parsing CGI arguments";
        case eAttribute: return "Bad part attribute in multipart HTTP request";
        case eFormat:    return "Misformatted data in HTTP request";
        case eData:      return "Unexpected or inconsistent HTTP request";
        default:         return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CCgiRequestException, CCgiException);
};



/////////////////////////////////////////////////////////////////////////////
///
/// CCgiParseException --
///
///   Exceptions used by CGI framework when the error has occured while
///   parsing the contents (header and/or body) of the HTTP request

class CCgiParseException : public CParseTemplException<CCgiRequestException>
{
public:
    /// @sa CCgiRequestException
    enum EErrCode {
        eIndex      = CCgiRequestException::eIndex,
        eEntry      = CCgiRequestException::eEntry,
        eAttribute  = CCgiRequestException::eAttribute,
        eRead       = CCgiRequestException::eRead,
        eFormat     = CCgiRequestException::eFormat
        // WARNING:  no enums not listed in "CCgiRequestException::EErrCode"
        //           can be here -- unless you re-implement GetErrCodeString()
    };

    NCBI_EXCEPTION_DEFAULT2
    (CCgiParseException, CParseTemplException<CCgiRequestException>,
     std::string::size_type);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiErrnoException --
///
///   Exceptions used by CGI framework when the error is more system-related
///   and there is an "errno" status from the system call that can be obtained

class CCgiErrnoException : public CErrnoTemplException<CCgiException>
{
public:
    enum EErrCode {
        eErrno,   //< Generic system call failure
        eModTime  //< File modification time cannot be obtained
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eErrno:   return "System error";
        case eModTime: return "File system error";
        default:       return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT
    (CCgiErrnoException, CErrnoTemplException<CCgiException>);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiArgsException --
///
///   Exceptions to be used by CGI arguments parser.
///

class CCgiArgsException : public CCgiException
{
public:
    /// Bad (malformed or missing) HTTP request components
    enum EErrCode {
        eFormat,     //< Format of arguments
        eValue,      //< Invalid argument value
        eName,       //< Argument does not exist
        eNoArgs      //< CUrl contains no arguments
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eFormat:    return "Misformatted CGI query string";
        case eValue:     return "Invalid argument value";
        case eName:      return "Unknown argument name";
        case eNoArgs:    return "Arguments list is empty";
        default:         return CException::GetErrCodeString();
        }
    }
    NCBI_EXCEPTION_DEFAULT(CCgiArgsException, CCgiException);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiArgsParserException --
///
///   Exceptions used by CGI arguments parser when the error has occured while
///   parsing the arguments.
///

class CCgiArgsParserException : public CParseTemplException<CCgiArgsException>
{
public:
    /// @sa CCgiArgsException
    enum EErrCode {
        eFormat     = CCgiArgsException::eFormat,
        eValue      = CCgiArgsException::eValue
        // WARNING:  no enums not listed in "CCgiArgsException::EErrCode"
        //           can be here -- unless you re-implement GetErrCodeString()
    };

    NCBI_EXCEPTION_DEFAULT2
    (CCgiArgsParserException, CParseTemplException<CCgiArgsException>,
     std::string::size_type);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiSessionException --
///
///   Exceptions used by CGI session

class CCgiSessionException : public CCgiException
{
public:
    enum EErrCode {
        eSessionId,     ///< SessionId is not spesified
        eImplNotSet,    ///< Session implementaion is not set
        eDeleted,       ///< Session has been deleted
        eSessionDoesnotExist, ///< Session does not exsit;
        eImplException  ///< Implementation exception
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eSessionId:      return "SessionId is not spesified";
        case eImplNotSet:     return "Session implementaion is not set";
        case eDeleted:        return "Session has been deleted";
        case eSessionDoesnotExist: return "Session does not exsit";
        case eImplException:  return "Implementaion exception";
        default:         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCgiSessionException, CCgiException);
};

END_NCBI_SCOPE


/* @} */


/*
 * ===========================================================================
 * $Log$
 * Revision 1.10  2005/12/19 16:55:03  didenko
 * Improved CGI Session implementation
 *
 * Revision 1.9  2005/12/15 21:53:38  grichenk
 * Check if ArgsList is initialized (initialize in non-const GetArgs())
 *
 * Revision 1.8  2005/12/15 18:21:15  didenko
 * Added CGI session support
 *
 * Revision 1.7  2005/12/08 21:33:20  grichenk
 * Added CCgiArgsException and CCgiArgsParserException
 *
 * Revision 1.6  2005/10/17 16:46:40  grichenk
 * Added CCgiArgs_Parser base class.
 * Redesigned CCgiRequest to use CCgiArgs_Parser.
 * Replaced CUrlException with CCgiParseException.
 *
 * Revision 1.5  2005/10/13 18:30:15  grichenk
 * Added cgi_util with CCgiArgs and CUrl.
 * Moved URL encoding/decoding functions to cgi_util.
 * Added CUrlException.
 *
 * Revision 1.4  2004/09/07 19:14:09  vakatov
 * Better structurize (and use) CGI exceptions to distinguish between user-
 * and server- errors
 *
 * Revision 1.3  2004/08/04 15:52:57  vakatov
 * CCgiParseException::EErrCode += eRead.
 * Also, commented and Doxynen'ized.
 *
 * Revision 1.2  2003/04/10 19:01:39  siyan
 * Added doxygen support
 *
 * Revision 1.1  2003/02/24 19:59:58  gouriano
 * use template-based exceptions instead of errno and parse exceptions
 *
 * ===========================================================================
 */

#endif  // CGI___CGI_EXCEPTION__HPP
