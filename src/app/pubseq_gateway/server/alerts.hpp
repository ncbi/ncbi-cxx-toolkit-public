#ifndef PUBSEQ_GATEWAY_ALERTS__HPP
#define PUBSEQ_GATEWAY_ALERTS__HPP

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
 * Authors:  Sergey Satskiy
 *
 * File Description:
 *   PSG server alerts
 *
 */


#include <map>
#include <chrono>
#include <mutex>
using namespace std;

#include <connect/services/json_over_uttp.hpp>
USING_NCBI_SCOPE;


enum EPSGAlertType {
    eUnknown = -1,
    eConfigAuthDecrypt = 0,
    eConfigNoSi2csiCache = 1,
    eConfigNoBioseqInfoCache = 2,
    eConfigNoBlobPropCache = 3,
    eConfigHttpWorkers = 4,
    eConfigListenerBacklog = 5,
    eConfigMaxConnections = 6,
    eConfigTimeout = 7,
    eConfigRetries = 8,
    eConfigExcludeCacheSize = 9,
    eConfigExcludeCachePurgeSize = 10,
    eConfigExcludeCacheInactivity = 11,
    eConfigStatScaleType = 12,
    eConfigStatMinMaxVal = 13,
    eConfigStatNBins = 14,
    eOpenCassandra = 15,
    eNoValidCassandraMapping = 16,      // PSG has no valid cassandra mapping at hand
    eInvalidCassandraMapping = 17,      // PSG has detected an invalid mapping in cassandra
    eNewCassandraMappingAccepted = 18,  // PSG accepted an updated mapping from cassandra
    eNewCassandraSatNamesMapping = 19,  // PSG has detected new sat names mapping in cassandra
                                        // however it can be accepted only
                                        // after restart
    eOpenCache = 20                     // PSG cannot create or open the LMDB cache
};

enum EPSGAlertAckResult {
    eAlertNotFound = 0,
    eAlertAlreadyAcknowledged = 1,
    eAlertAcknowledged = 2
};


struct SPSGAlertAttributes
{
    chrono::system_clock::time_point    m_LastDetectedTimestamp;
    chrono::system_clock::time_point    m_AcknowledgedTimestamp;
    bool                                m_On;
    size_t                              m_Count;            // total, i.e. since startup
    size_t                              m_CountSinceAck;    // since acknowledge
    string                              m_User;
    string                              m_Message;

    SPSGAlertAttributes() :
        m_LastDetectedTimestamp(chrono::system_clock::now()),
        m_AcknowledgedTimestamp(),
        m_On(true), m_Count(1), m_CountSinceAck(1)
    {}

    CJsonNode Serialize(void) const;
};




class CPSGAlerts
{
    public:
        void Register(enum EPSGAlertType alert_type, const string &  message);
        enum EPSGAlertAckResult Acknowledge(const string &  alert_id,
                                            const string &  user);
        enum EPSGAlertAckResult Acknowledge(enum EPSGAlertType alert_type,
                                            const string &  user);
        CJsonNode Serialize(void) const;

    private:
        mutable mutex                                   m_Lock;
        map<enum EPSGAlertType, SPSGAlertAttributes>    m_Alerts;

        enum EPSGAlertType x_IdToType(const string &  alert_id) const;
        string x_TypeToId(enum EPSGAlertType  type) const;
};


#endif /* PUBSEQ_GATEWAY_ALERTS__HPP */

