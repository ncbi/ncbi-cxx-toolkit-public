#ifndef CORELIB___REQUEST_STATUS__HPP
#define CORELIB___REQUEST_STATUS__HPP

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
 * Authors:  Aleksey Grichenko, Denis Vakatov
 *
 * File Description:
 *   HTTP status codes.
 *
 */

/// @file request_status.hpp
///
///   Defines CRequestStatus class for NCBI C++ diagnostic API.
///


#include <corelib/ncbistl.hpp>


/** @addtogroup Diagnostics
 *
 * @{
 */


BEGIN_NCBI_SCOPE


class NCBI_XNCBI_EXPORT CRequestStatus
{
public:
    enum ECode {
        e200_Ok                     = 200,
        e201_Created                = 201,
        e202_Accepted               = 202,
        e203_NonAuthInformation     = 203,
        e204_NoContent              = 204,
        e205_ResetContent           = 205,
        e206_PartialContent         = 206,

        /// Non-standard status code - used to indicate broken connection
        /// while serving partial-content request.
        e299_PartialContentBrokenConnection = 299,

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

        /// Non-standard status code - used to indicate broken connection
        /// while serving normal request.
        e499_BrokenConnection       = 499,

        e500_InternalServerError    = 500,
        e501_NotImplemented         = 501,
        e502_BadGateway             = 502,
        e503_ServiceUnavailable     = 503,
        e504_GatewayTimeout         = 504,
        e505_HTTPVerNotSupported    = 505
    };
};


END_NCBI_SCOPE


#endif  /* CORELIB___REQUEST_STATUS__HPP */
