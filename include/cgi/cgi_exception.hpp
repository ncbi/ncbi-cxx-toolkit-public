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
#include <corelib/request_status.hpp>
#include <util/ncbi_url.hpp>


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

        e200_Ok                     = CRequestStatus::e200_Ok,
        e201_Created                = CRequestStatus::e201_Created,
        e202_Accepted               = CRequestStatus::e202_Accepted,
        e203_NonAuthInformation     = CRequestStatus::e203_NonAuthInformation,
        e204_NoContent              = CRequestStatus::e204_NoContent,
        e205_ResetContent           = CRequestStatus::e205_ResetContent,
        e206_PartialContent         = CRequestStatus::e206_PartialContent,

        e300_MultipleChoices        = CRequestStatus::e300_MultipleChoices,
        e301_MovedPermanently       = CRequestStatus::e301_MovedPermanently,
        e302_Found                  = CRequestStatus::e302_Found,
        e303_SeeOther               = CRequestStatus::e303_SeeOther,
        e304_NotModified            = CRequestStatus::e304_NotModified,
        e305_UseProxy               = CRequestStatus::e305_UseProxy,
        e307_TemporaryRedirect      = CRequestStatus::e307_TemporaryRedirect,

        e400_BadRequest             = CRequestStatus::e400_BadRequest,
        e401_Unauthorized           = CRequestStatus::e401_Unauthorized,
        e402_PaymentRequired        = CRequestStatus::e402_PaymentRequired,
        e403_Forbidden              = CRequestStatus::e403_Forbidden,
        e404_NotFound               = CRequestStatus::e404_NotFound,
        e405_MethodNotAllowed       = CRequestStatus::e405_MethodNotAllowed,
        e406_NotAcceptable          = CRequestStatus::e406_NotAcceptable,
        e407_ProxyAuthRequired      = CRequestStatus::e407_ProxyAuthRequired,
        e408_RequestTimeout         = CRequestStatus::e408_RequestTimeout,
        e409_Conflict               = CRequestStatus::e409_Conflict,
        e410_Gone                   = CRequestStatus::e410_Gone,
        e411_LengthRequired         = CRequestStatus::e411_LengthRequired,
        e412_PreconditionFailed     = CRequestStatus::e412_PreconditionFailed,
        e413_RequestEntityTooLarge  = CRequestStatus::e413_RequestEntityTooLarge,
        e414_RequestURITooLong      = CRequestStatus::e414_RequestURITooLong,
        e415_UnsupportedMediaType   = CRequestStatus::e415_UnsupportedMediaType,
        e416_RangeNotSatisfiable    = CRequestStatus::e416_RangeNotSatisfiable,
        e417_ExpectationFailed      = CRequestStatus::e417_ExpectationFailed,

        e500_InternalServerError    = CRequestStatus::e500_InternalServerError,
        e501_NotImplemented         = CRequestStatus::e501_NotImplemented,
        e502_BadGateway             = CRequestStatus::e502_BadGateway,
        e503_ServiceUnavailable     = CRequestStatus::e503_ServiceUnavailable,
        e504_GatewayTimeout         = CRequestStatus::e504_GatewayTimeout,
        e505_HTTPVerNotSupported    = CRequestStatus::e505_HTTPVerNotSupported
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
        case eValue:   return "Bad cookie";
        case eString:  return "Bad cookie string format";
        default:       return CException::GetErrCodeString();
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
        eIndex     = CCgiRequestException::eIndex,
        eEntry     = CCgiRequestException::eEntry,
        eAttribute = CCgiRequestException::eAttribute,
        eRead      = CCgiRequestException::eRead,
        eFormat    = CCgiRequestException::eFormat
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
        case eErrno:    return "System error";
        case eModTime:  return "File system error";
        default:        return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT
    (CCgiErrnoException, CErrnoTemplException<CCgiException>);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiResponseException --
///
///   Exceptions used by CGI response

class CCgiResponseException : public CCgiException
{
public:
    enum EErrCode {
        eDoubleHeader,          ///< Header has already been written
        eBadHeaderValue         ///< Invalid header value
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eDoubleHeader:   return "Header has already been written";
        case eBadHeaderValue: return "Invalid header value";
        default:              return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCgiResponseException, CCgiException);
};


/////////////////////////////////////////////////////////////////////////////
///
/// CCgiHeadException --
///
///   Exceptions used to stop processing request after sending header when
///   the request method is HEAD. This does not indicate an error, it's
///   just a way to exit from ProcessRequest.

class CCgiHeadException : public CCgiException
{
public:
    enum EErrCode {
        eHeaderSent          ///< Header has been written
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eHeaderSent:  return "Header has been written";
        default:           return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CCgiHeadException, CCgiException);
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
        eSessionId,            ///< SessionId not specified
        eImplNotSet,           ///< Session implementation not set
        eDeleted,              ///< Session has been deleted
        eSessionDoesnotExist,  ///< Session does not exist
        eImplException,        ///< Implementation exception
        eAttrNotFound,         ///< Attribute not found
        eNotLoaded             ///< Session not loaded
    };
    virtual const char* GetErrCodeString(void) const
    {
        switch ( GetErrCode() ) {
        case eSessionId:            return "SessionId not specified";
        case eImplNotSet:           return "Session implementation not set";
        case eDeleted:              return "Session has been deleted";
        case eSessionDoesnotExist:  return "Session does not exist";
        case eImplException:        return "Implementation exception";
        case eAttrNotFound:         return "Attribute not found";
        case eNotLoaded:            return "Session not loaded";
        default:                    return CException::GetErrCodeString();
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


/// @deprecated Use CUrlException
NCBI_DEPRECATED
typedef CUrlException CCgiArgsException;

/// @deprecated Use CUrlParserException
NCBI_DEPRECATED
typedef CUrlParserException CCgiArgsParserException;


END_NCBI_SCOPE


/* @} */

#endif  // CGI___CGI_EXCEPTION__HPP
