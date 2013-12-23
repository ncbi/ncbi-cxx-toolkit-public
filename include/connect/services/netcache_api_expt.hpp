#ifndef CONN___NETCACHE_API_EXPT__HPP
#define CONN___NETCACHE_API_EXPT__HPP

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
 * Authors:  Anatoliy Kuznetsov, Dmitry Kazimirov
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_api_expt.hpp
/// NetCache API exception declarations.
///

#include <connect/services/netservice_api_expt.hpp>

BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


/// NetCache internal exception
///
class CNetCacheException : public CNetServiceException
{
public:
    typedef CNetServiceException TParent;
    enum EErrCode {
        ///< If client is not allowed to run this operation
        eAuthenticationError,
        ///< BLOB key corruption or version mismatch
        eKeyFormatError,
        ///< Server side error
        eServerError,
        ///< Blob is not found
        eBlobNotFound,
        ///< Access denied
        eAccessDenied,
        ///< Blob could not be read completely
        eBlobClipped,
        ///< The requested command is (yet) unknown
        eUnknownCommand,
        ///< The requested command is not implemented
        eNotImplemented,
        ///< Invalid response from the server
        eInvalidServerResponse,
    };

    virtual const char* GetErrCodeString() const
    {
        switch (GetErrCode()) {
        case eAuthenticationError:      return "eAuthenticationError";
        case eKeyFormatError:           return "eKeyFormatError";
        case eServerError:              return "eServerError";
        case eBlobNotFound:             return "eBlobNotFound";
        case eAccessDenied:             return "eAccessDenied";
        case eUnknownCommand:           return "eUnknownCommand";
        case eNotImplemented:           return "eNotImplemented";
        case eInvalidServerResponse:    return "eInvalidServerResponse";
        default:                        return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheException, CNetServiceException);
};


/// Exception thrown when the requested blob is
/// older than the requested age.
///
class CNetCacheBlobTooOldException : public CNetCacheException
{
public:
    typedef CNetCacheException TParent;
    enum EErrCode {
        ///< The blob is older than the requested age.
        eBlobTooOld,
    };

    virtual const char* GetErrCodeString() const
    {
        switch (GetErrCode()) {
        case eBlobTooOld:               return "eBlobTooOld";
        default:                        return CException::GetErrCodeString();
        }
    }

    unsigned GetAge() const
    {
        const char* age = strstr(GetMsg().c_str(), "AGE=");
        if (age == NULL)
            return (unsigned) -1;
        return (unsigned) atoi(age + sizeof("AGE=") - 1);
    }

    int GetVersion() const
    {
        const char* ver = strstr(GetMsg().c_str(), "VER=");
        if (ver == NULL)
            return 0;
        return atoi(ver + sizeof("VER=") - 1);
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheBlobTooOldException, CNetCacheException);
};


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_API_EXPT__HPP */
