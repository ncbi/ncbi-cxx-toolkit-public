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
 * Authors:  Anatoliy Kuznetsov
 *
 * File Description:
 *   Net cache client API.
 *
 */

/// @file netcache_client.hpp
/// NetCache client specs. 
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
        ///< BLOB is locked by another client
        eBlobLocked,
        ///< Cache name unknown
        eUnknnownCache,
        ///< Blob is not found
        eBlobNotFound,
        eCommandIsNotAllowed
    };

    virtual const char* GetErrCodeString(void) const
    {
        switch (GetErrCode())
        {
        case eAuthenticationError: return "eAuthenticationError";
        case eKeyFormatError:      return "eKeyFormatError";
        case eServerError:         return "eServerError";
        case eBlobLocked:          return "eBlobLocked";
        case eUnknnownCache:       return "eUnknnownCache";
        case eBlobNotFound:        return "eBlobNotFound";
        case eCommandIsNotAllowed: return "eCommandIsNotAllowed";
        default:                   return CException::GetErrCodeString();
        }
    }

    NCBI_EXCEPTION_DEFAULT(CNetCacheException, CNetServiceException);
};


/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_API_EXPT__HPP */
