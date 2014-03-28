#ifndef CONN___NETSCHEDULE_API_EXPT__HPP
#define CONN___NETSCHEDULE_API_EXPT__HPP

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
 * Authors:  Anatoliy Kuznetsov, Maxim Didenko
 *
 * File Description:
 *   NetSchedule client API.
 *
 */


/// @file netschedule_client.hpp
/// NetSchedule client specs.
///

#include <connect/services/netservice_api_expt.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetScheduleClient
 *
 * @{
 */

/// Map from exception names to codes
/// @internal
class NCBI_XCONNECT_EXPORT CNetScheduleExceptionMap
{
public:
    static CException::TErrCode GetCode(const string& name);
};

/// NetSchedule internal exception
///
class NCBI_XCONNECT_EXPORT CNetScheduleException : public CNetServiceException
{
public:
    // NB: if you update this enum, update constructor for
    // CNetScheduleExceptionMap to include new mapping.
    enum EErrCode {
        eInternalError,
        eProtocolSyntaxError,
        eAuthenticationError,
        eKeyFormatError,
        eJobNotFound,
        eGroupNotFound,
        eAffinityNotFound,
        eInvalidJobStatus,
        eUnknownQueue,
        eUnknownQueueClass,
        eUnknownService,
        eTooManyPendingJobs,
        eDataTooLong,
        eInvalidClient,
        eClientDataVersionMismatch,
        eAccessDenied,
        eSubmitsDisabled,
        eShuttingDown,
        eDuplicateName,
        eObsoleteCommand,
        eInvalidParameter,
        eInvalidAuthToken,
        eTooManyPreferredAffinities,
        ePrefAffExpired,
        eTryAgain,
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode()) {
        case eInternalError:       return "eInternalError";
        case eProtocolSyntaxError: return "eProtocolSyntaxError";
        case eAuthenticationError: return "eAuthenticationError";
        case eKeyFormatError:      return "eKeyFormatError";
        case eJobNotFound:         return "eJobNotFound";
        case eGroupNotFound:       return "eGroupNotFound";
        case eAffinityNotFound:    return "eAffinityNotFound";
        case eInvalidJobStatus:    return "eInvalidJobStatus";
        case eUnknownQueue:        return "eUnknownQueue";
        case eUnknownQueueClass:   return "eUnknownQueueClass";
        case eUnknownService:      return "eUnknownService";
        case eTooManyPendingJobs:  return "eTooManyPendingJobs";
        case eDataTooLong:         return "eDataTooLong";
        case eInvalidClient:       return "eInvalidClient";
        case eClientDataVersionMismatch:
                                   return "eClientDataVersionMismatch";
        case eAccessDenied:        return "eAccessDenied";
        case eSubmitsDisabled:     return "eSubmitsDisabled";
        case eShuttingDown:        return "eShuttingDown";
        case eDuplicateName:       return "eDuplicateName";
        case eObsoleteCommand:     return "eObsoleteCommand";
        case eInvalidParameter:    return "eInvalidParameter";
        case eInvalidAuthToken:    return "eInvalidAuthToken";
        case eTooManyPreferredAffinities:
            return "eTooManyPreferredAffinities";
        case ePrefAffExpired:      return "ePrefAffExpired";
        case eTryAgain:            return "eTryAgain";
        default:                   return CNetServiceException::GetErrCodeString();
        }
    }

    unsigned ErrCodeToHTTPStatusCode() const;

    NCBI_EXCEPTION_DEFAULT(CNetScheduleException, CNetServiceException);
};

/* @} */


END_NCBI_SCOPE


#endif  /* CONN___NETSCHEDULE_API_EXPT__HPP */
