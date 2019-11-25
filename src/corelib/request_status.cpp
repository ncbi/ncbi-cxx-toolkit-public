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
 * Author: Aleksey Grichenko
 *
 * File Description:
 *   CRequestStatus implementation.
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/request_status.hpp>


/** @addtogroup Utility
 *
 * @{
 */

BEGIN_NCBI_SCOPE


string CRequestStatus::GetStdStatusMessage(ECode code)
{
    switch ( code ) {
    case e100_Continue: return "Continue";
    case e101_SwitchingProtocols: return "Switching Protocols";
    case e200_Ok: return "OK";
    case e201_Created: return "Created";
    case e202_Accepted: return "Accepted";
    case e203_NonAuthInformation: return "Non-Authoritative Information";
    case e204_NoContent: return "No Content";
    case e205_ResetContent: return "Reset Content";
    case e206_PartialContent: return "Partial Content";
    case e299_PartialContentBrokenConnection: return "Partial Content Broken Connection";
    case e300_MultipleChoices: return "Multiple Choices";
    case e301_MovedPermanently: return "Moved Permanently";
    case e302_Found: return "Found";
    case e303_SeeOther: return "See Other";
    case e304_NotModified: return "Not Modified";
    case e305_UseProxy: return "Use Proxy";
    case e307_TemporaryRedirect: return "Temporary Redirect";
    case e400_BadRequest: return "Bad Request";
    case e401_Unauthorized: return "Unauthorized";
    case e402_PaymentRequired: return "Payment Required";
    case e403_Forbidden: return "Forbidden";
    case e404_NotFound: return "Not Found";
    case e405_MethodNotAllowed: return "Method Not Allowed";
    case e406_NotAcceptable: return "Not Acceptable";
    case e407_ProxyAuthRequired: return "Proxy Authentication Required";
    case e408_RequestTimeout: return "Request Timeout";
    case e409_Conflict: return "Conflict";
    case e410_Gone: return "Gone";
    case e411_LengthRequired: return "Length Required";
    case e412_PreconditionFailed: return "Precondition Failed";
    case e413_RequestEntityTooLarge: return "Request Entity Too Large";
    case e414_RequestURITooLong: return "Request-URI Too Long";
    case e415_UnsupportedMediaType: return "Unsupported Media Type";
    case e416_RangeNotSatisfiable: return "Requested Range Not Satisfiable";
    case e417_ExpectationFailed: return "Expectation Failed";
    case e422_UnprocessableEntity: return "Unprocessable Entity";
    case e499_BrokenConnection: return "Broken Connection";
    case e500_InternalServerError: return "Internal Server Error";
    case e501_NotImplemented: return "Not Implemented";
    case e502_BadGateway: return "Bad Gateway";
    case e503_ServiceUnavailable: return "Service Unavailable";
    case e504_GatewayTimeout: return "Gateway Timeout";
    case e505_HTTPVerNotSupported: return "HTTP Version Not Supported";
    }
    return "Unknown HTTP status code";
}


/* @} */

END_NCBI_SCOPE
