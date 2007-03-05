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
#include <corelib/ncbistr.hpp>


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

struct SCgiStatus;

class NCBI_XCGI_EXPORT CCgiException : EXCEPTION_VIRTUAL_BASE public CException
{
public:
    /// HTTP status codes
    enum EStatusCode {
        eStatusNotSet               = 0,   ///< Internal value - code not set

        e200_Ok                     = 200,
        e201_Created                = 201,
        e202_Accepted               = 202,
        e203_NonAuthInformation     = 203,
        e204_NoContent              = 204,
        e205_ResetContent           = 205,
        e206_PartialContent         = 206,

        e300_MultipleChoices        = 300,
        e301_MovedPermanently       = 301,
        e302_Found                  = 302,
        e303_SeeOther               = 303,
        e304_NotModified            = 304,
        e305_UseProxy               = 305,
        e307_TemporaryRedirect      = 307,

        e400_BadRequest             = 400,
        e401_Unauthorized           = 401,
        e402_PaymentRequired        = 402,
        e403_Forbidden              = 403,
        e404_NotFound               = 404,
        e405_MethodNotAllowed       = 405,
        e406_NotAcceptable          = 406,
        e407_ProxyAuthRequired      = 407,
        e408_RequestTimeout         = 408,
        e409_Conflict               = 409,
        e410_Gone                   = 410,
        e411_LengthRequired         = 411,
        e412_PreconditionFailed     = 412,
        e413_RequestEntityTooLarge  = 413,
        e414_RequestURITooLong      = 414,
        e415_UnsupportedMediaType   = 415,
        e416_RangeNotSatisfiable    = 416,
        e417_ExpectationFailed      = 417,

        e500_InternalServerError    = 500,
        e501_NotImplemented         = 501,
        e502_BadGateway             = 502,
        e503_ServiceUnavailable     = 503,
        e504_GatewayTimeout         = 504,
        e505_HTTPVerNotSupported    = 505
    };

    CCgiException& SetStatus(const SCgiStatus& status);

    EStatusCode GetStatusCode(void) const
        {
            return m_StatusCode;
        }
    string      GetStatusMessage(void) const
        {
            return m_StatusMessage.empty() ?
                sx_GetStdStatusMessage(m_StatusCode) : m_StatusMessage;
        }

    NCBI_EXCEPTION_DEFAULT(CCgiException, CException);

protected:
    /// Override method for initializing exception data.
    virtual void x_Init(const CDiagCompileInfo& info,
                        const string& message,
                        const CException* prev_exception,
                        EDiagSev severity);

    /// Override method for copying exception data.
    virtual void x_Assign(const CException& src);

private:
    static string sx_GetStdStatusMessage(EStatusCode code);

    EStatusCode m_StatusCode;
    string      m_StatusMessage;
};


struct SCgiStatus {
    SCgiStatus(CCgiException::EStatusCode code,
               const string& message = kEmptyStr)
        : m_Code(code), m_Message(message) {}
    CCgiException::EStatusCode m_Code;
    string                     m_Message;
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
        eData        //< Syntaxically correct but contains odd data (from the
                     //< point of view of particular CGI application)
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
        eSessionId,     ///< SessionId is not specified
        eImplNotSet,    ///< Session implementaion is not set
        eDeleted,       ///< Session has been deleted
        eSessionDoesnotExist, ///< Session does not exsit;
        eImplException, ///< Implementation exception
        eAttrNotFound,  ///< Attribute not found
        eNotLoaded      ///< Session is not loaded
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eSessionId:      return "SessionId is not specified";
        case eImplNotSet:     return "Session implementaion is not set";
        case eDeleted:        return "Session has been deleted";
        case eSessionDoesnotExist: return "Session does not exsit";
        case eImplException:  return "Implementaion exception";
        case eAttrNotFound:   return "Attribute not found";
        case eNotLoaded:      return "Session is not loaded";
        default:         return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCgiSessionException, CCgiException);
};


#define NCBI_CGI_THROW_WITH_STATUS(exception, err_code, message, status) \
    {                                                                    \
        NCBI_EXCEPTION_VAR(cgi_exception, exception, err_code, message); \
        cgi_exception.SetStatus( (status) );                             \
        NCBI_EXCEPTION_THROW(cgi_exception);                             \
    }

#define NCBI_CGI_THROW2_WITH_STATUS(exception, err_code,                  \
                                    message, extra, status)               \
    {                                                                     \
        NCBI_EXCEPTION2_VAR(cgi_exception, exception,                     \
                            err_code, message, extra);                    \
        cgi_exception.SetStatus( (status) );                              \
        NCBI_EXCEPTION_THROW(cgi_exception);                              \
    }


inline
CCgiException& CCgiException::SetStatus(const SCgiStatus& status)
{
    m_StatusCode = status.m_Code;
    m_StatusMessage = status.m_Message;
    return *this;
}


END_NCBI_SCOPE


/* @} */

#endif  // CGI___CGI_EXCEPTION__HPP
