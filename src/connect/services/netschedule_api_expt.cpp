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
 * Author: Maxim Didenko
 *
 * File Description:
 *   Implementation of NetSchedule API.
 *
 */

#include <ncbi_pch.hpp>

#include <util/static_map.hpp>
#include <connect/services/netschedule_api_expt.hpp>


BEGIN_NCBI_SCOPE

typedef SStaticPair<const char*, CException::TErrCode> TExceptionMapElem;
static const TExceptionMapElem s_NSExceptionArray[] = {
    {"eAccessDenied", CNetScheduleException::eAccessDenied},
    {"eAffinityNotFound", CNetScheduleException::eAffinityNotFound},
    {"eAuthenticationError", CNetScheduleException::eAuthenticationError},
    {"eCommandIsNotAllowed", CNetScheduleException::eCommandIsNotAllowed},
    {"eDataTooLong", CNetScheduleException::eDataTooLong},
    {"eDuplicateName", CNetScheduleException::eDuplicateName},
    {"eGroupNotFound", CNetScheduleException::eGroupNotFound},
    {"eInternalError", CNetScheduleException::eInternalError},
    {"eInvalidAuthToken", CNetScheduleException::eInvalidAuthToken},
    {"eInvalidClient", CNetScheduleException::eInvalidClient},
    {"eInvalidJobStatus", CNetScheduleException::eInvalidJobStatus},
    {"eInvalidParameter", CNetScheduleException::eInvalidParameter},
    {"eJobNotFound", CNetScheduleException::eJobNotFound},
    {"eKeyFormatError", CNetScheduleException::eKeyFormatError},
    {"eObsoleteCommand", CNetScheduleException::eObsoleteCommand},
    {"ePrefAffExpired", CNetScheduleException::ePrefAffExpired},
    {"eProtocolSyntaxError", CNetScheduleException::eProtocolSyntaxError},
    {"eShuttingDown", CNetScheduleException::eShuttingDown},
    {"eSubmitsDisabled", CNetScheduleException::eSubmitsDisabled},
    {"eTooManyPendingJobs", CNetScheduleException::eTooManyPendingJobs},
    {"eTooManyPreferredAffinities", CNetScheduleException::eTooManyPreferredAffinities},
    {"eTryAgain", CNetScheduleException::eTryAgain},
    {"eUnknownQueue", CNetScheduleException::eUnknownQueue},
    {"eUnknownQueueClass", CNetScheduleException::eUnknownQueueClass},
    {"eUnknownService", CNetScheduleException::eUnknownService},
};
typedef CStaticArrayMap<const char*, CException::TErrCode, PNocase_CStr> TNSExceptionMap;
DEFINE_STATIC_ARRAY_MAP(TNSExceptionMap, s_NSExceptionMap, s_NSExceptionArray);

CException::TErrCode CNetScheduleExceptionMap::GetCode(const string& name)
{
    TNSExceptionMap::const_iterator it = s_NSExceptionMap.find(name.c_str());
    if (it == s_NSExceptionMap.end())
        return CException::eInvalid;
    return it->second;
}

const char* CNetScheduleException::GetErrCodeString() const
{
    return GetErrCodeString(GetErrCode());
}

const char* CNetScheduleException::GetErrCodeString(
        CException::TErrCode err_code)
{
    switch (err_code) {
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
    default:                   return "eInvalid";
    }
}

unsigned CNetScheduleException::ErrCodeToHTTPStatusCode() const
{
    switch (GetErrCode()) {
    default: /* Including eInternalError */     return 500;
    case eProtocolSyntaxError:                  return 400;
    case eAuthenticationError:                  return 401;
    case eKeyFormatError:                       return 400;
    case eJobNotFound:                          return 404;
    case eGroupNotFound:                        return 404;
    case eAffinityNotFound:                     return 404;
    case eInvalidJobStatus:                     return 409;
    case eUnknownQueue:                         return 404;
    case eUnknownQueueClass:                    return 404;
    case eUnknownService:                       return 404;
    case eTooManyPendingJobs:                   return 503;
    case eDataTooLong:                          return 413;
    case eInvalidClient:                        return 400;
    case eClientDataVersionMismatch:            return 304;
    case eAccessDenied:                         return 401;
    case eSubmitsDisabled:                      return 503;
    case eShuttingDown:                         return 400;
    case eDuplicateName:                        return 409;
    case eObsoleteCommand:                      return 501;
    case eInvalidParameter:                     return 400;
    case eInvalidAuthToken:                     return 401;
    case eTooManyPreferredAffinities:           return 503;
    case ePrefAffExpired:                       return 404;
    case eTryAgain:                             return 503;
    }
}

const char* CNetScheduleException::GetErrCodeDescription(
        CException::TErrCode err_code)
{
    switch (err_code) {
    case eInternalError:
        return "NetSchedule server internal error";
    case eProtocolSyntaxError:
        return "NetSchedule server cannot parse the client command";
    case eAuthenticationError:
        return "NetSchedule server received incomplete client authentication";
    case eJobNotFound:
        return "The job is not found";
    case eGroupNotFound:
        return "The job group is not found";
    case eAffinityNotFound:
        return "The job affinity is not found";
    case eInvalidJobStatus:
        return "The job status does not support the requested operation";
    case eUnknownQueue:
        return "The queue is not found";
    case eUnknownQueueClass:
        return "The queue class is not found";
    case eUnknownService:
        return "The service is not found";
    case eDataTooLong:
        return "The provided data are too long";
    case eInvalidClient:
        return "The command requires a non-anonymous client";
    case eClientDataVersionMismatch:
        return "The client data cannot be set because the data "
                "version doesn't match";
    case eAccessDenied:
        return "Not enough privileges to perform the requested operation";
    case eSubmitsDisabled:
        return "Cannot submit a job because submits are disabled";
    case eShuttingDown:
        return "NetSchedule refuses command execution because "
                "it is shutting down";
    case eDuplicateName:
        return "A dynamic queue cannot be created because "
                "another queue with the same name already exists";
    case eObsoleteCommand:
        return "The command is obsolete and will be ignored";
    case eInvalidParameter:
        return "Invalid value for a command argument";
    case eInvalidAuthToken:
        return "The requested job operation is rejected "
            "because the provided authorization token is invalid";
    case eTooManyPreferredAffinities:
        return "There is no room for a new preferred affinity";
    case ePrefAffExpired:
        return "The preferred affinities expired and were reset "
                "because the worker node did not communicate within "
                "the timeout. The command execution is refused.";
    case eTryAgain:
        return "BerkleyDB has too many incomplete transactions at the moment. "
                "Try again later.";
    default:
        return GetErrCodeString(err_code);
    }
}

END_NCBI_SCOPE
