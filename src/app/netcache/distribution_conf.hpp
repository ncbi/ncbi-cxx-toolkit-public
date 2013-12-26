#ifndef NETCACHE__DISTRIBUTION_CONF__HPP
#define NETCACHE__DISTRIBUTION_CONF__HPP

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
 * Authors: Denis Vakatov, Pavel Ivanov, Sergey Satskiy
 *
 * File Description: Data structures and API to support blobs mirroring.
 *
 */


#include "nc_utils.hpp"


BEGIN_NCBI_SCOPE


class CNCDistributionConf
{
public:
    // Reads settings from an ini file and fills data structures which are
    // frequently used by other dudes
    static bool Initialize(Uint2 control_port);
    static void Finalize(void);

    static void WriteSetup(CSrvSocketTask& task);

    // Provides the slot number for the given key
    static bool GetSlotByKey(const string& key, Uint2& slot, Uint2& time_bucket);
    static bool GetSlotByNetCacheKey(const string& key,
            Uint2& slot, Uint2& time_bucket);
    static void GetSlotByICacheKey(const string& key,
            Uint2& slot, Uint2& time_bucket);
    static void GetSlotByRnd(Uint4 key_rnd, Uint2& slot, Uint2& time_bucket);

    // Provides server IDs which serve the given slot
    static TServersList GetServersForSlot(Uint2 slot);
    static const TServersList& GetRawServersForSlot(Uint2 slot);

    // Provides common slots for the current and the given server
    static const vector<Uint2>& GetCommonSlots(Uint8 server);

    // Get the current server ID
    static Uint8 GetSelfID(void);

    static Uint4 GetMainSrvIP(const string& key);

    // Get all partners "host:port" strings
    static const TNCPeerList& GetPeers(void);
    static string GetPeerName(Uint8 srv_id);
    static string GetFullPeerName(Uint8 srv_id);
    static TServersList GetPeerServers(void);

    // Generates a blob key which is covered by the current server slots
    static void GenerateBlobKey(Uint2 local_port,
                                string& key, Uint2& slot, Uint2& time_bucket);

    // Tests if a slot is served by the local server
    static bool IsServedLocally(Uint2 slot);

    static Uint2 GetCntSlotBuckets(void);
    static Uint2 GetCntTimeBuckets(void);
    static const vector<Uint2>& GetSelfSlots(void);
    static Uint1 GetCntActiveSyncs(void);
    static Uint1 GetMaxSyncsOneServer(void);
    static Uint1 GetSyncPriority(void);
    static Uint2 GetMaxPeerTotalConns(void);
    static Uint2 GetMaxPeerBGConns(void);
    static Uint1 GetCntErrorsToThrottle(void);
    static Uint8 GetPeerThrottlePeriod(void);
    static Uint1 GetPeerTimeout(void);
    static Uint1 GetBlobListTimeout(void);
    static Uint8 GetSmallBlobBoundary(void);
    static Uint2 GetMaxMirrorQueueSize(void);
    static const string& GetSyncLogFileName(void);
    static Uint4 GetMaxSlotLogEvents(void);
    static Uint4 GetCleanLogReserve(void);
    static Uint4 GetMaxCleanLogBatch(void);
    static Uint8 GetMinForcedCleanPeriod(void);
    static Uint4 GetCleanAttemptInterval(void);
    static Uint8 GetPeriodicSyncInterval(void);
    static Uint8 GetPeriodicSyncHeadTime(void);
    static Uint8 GetPeriodicSyncTailTime(void);
    static Uint8 GetPeriodicSyncTimeout(void);
    static Uint8 GetFailedSyncRetryDelay(void);
    static Uint8 GetNetworkErrorTimeout(void);

    static const string& GetMirroringSizeFile(void);
    static const string& GetPeriodicLogFile(void);
    static void PrintBlobCopyStat(Uint8 create_time, Uint8 create_server, Uint8 write_server);
};


END_NCBI_SCOPE


#endif /* NETCACHE__DISTRIBUTION_CONF__HPP */

