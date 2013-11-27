#ifndef CONN___NETCACHE_ADMIN__HPP
#define CONN___NETCACHE_ADMIN__HPP

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
 * Authors:  Dmitry Kazimirov
 *
 * File Description:
 *   Administrative API for NetCache.
 *
 */

/// @file netcache_admin.hpp
/// Administrative API for NetCache.
///

#include "netcomponent.hpp"


BEGIN_NCBI_SCOPE


/** @addtogroup NetCacheClient
 *
 * @{
 */


struct SNetCacheAdminImpl;

class NCBI_XCONNECT_EXPORT CNetCacheAdmin
{
    NCBI_NET_COMPONENT(NetCacheAdmin);

    /// Shutdown options
    ///
    enum EShutdownOption {
        eNormalShutdown,    ///< Normal shutdown is requested.
        eDrain,             ///< Wait for all blobs to expire.
    };

    /// Shutdown the server daemon.
    ///
    /// @note
    ///  Protected to avoid a temptation to call it from time to time. :)
    void ShutdownServer(EShutdownOption shutdown_option = eNormalShutdown);

    /// Reload configuration parameters from the same source.
    void ReloadServerConfig();

    /// Remove all blobs from an ICache database.
    void Purge(const string& cache_name);

    /// Print contents of the configuration file
    void PrintConfig(CNcbiOstream& output_stream);

    /// Enumeration for use with PrintStat(). Indicates
    /// whether to return the current (incomplete) aggregation
    /// period or the previous one (which is complete).
    enum EStatPeriodCompleteness {
        eReturnCurrentPeriod,
        eReturnCompletePeriod
    };

    /// Print server statistics
    /// @param output_stream Stream for printing
    ///        the GETSTAT command output.
    /// @param aggregation_period Statistics aggregation
    ///        interval to print ('1min', '1h', 'life', and so on).
    /// @param period_completeness Whether to print statistics
    ///        for the previous (complete) aggregation interval
    ///        (as opposed to printing the current but incomplete
    ///        statistics).
    void PrintStat(CNcbiOstream& output_stream,
            const string& aggregation_period = kEmptyStr,
            EStatPeriodCompleteness period_completeness = eReturnCurrentPeriod);

    // Print server health information
    void PrintHealth(CNcbiOstream& output_stream);

    void GetServerVersion(CNcbiOstream& output_stream);
};

/* @} */


END_NCBI_SCOPE

#endif  /* CONN___NETCACHE_ADMIN__HPP */
