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
 * File Description: PSG server alerts
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "alerts.hpp"
#include "pubseq_gateway_convert_utils.hpp"


struct SAlertToId
{
    EPSGS_AlertType     type;
    string              id;
};


const SAlertToId     kAlertToIdMap[] = {
    { ePSGS_ConfigAuthDecrypt,                "ConfigAuthDecrypt" },
    { ePSGS_ConfigHttpWorkers,                "ConfigHttpWorkers" },
    { ePSGS_ConfigListenerBacklog,            "ConfigListenerBacklog" },
    { ePSGS_ConfigMaxConnections,             "ConfigMaxConnections" },
    { ePSGS_ConfigTimeout,                    "ConfigTimeout" },
    { ePSGS_ConfigRetries,                    "ConfigRetries" },
    { ePSGS_ConfigExcludeCacheSize,           "ConfigExcludeCacheSize" },
    { ePSGS_ConfigExcludeCachePurgeSize,      "ConfigExcludeCachePurgeSize" },
    { ePSGS_ConfigExcludeCacheInactivity,     "ConfigExcludeCacheInactivity" },
    { ePSGS_ConfigStatScaleType,              "ConfigStatScaleType" },
    { ePSGS_ConfigStatMinMaxVal,              "ConfigStatMinMaxVal" },
    { ePSGS_ConfigStatNBins,                  "ConfigStatNBins" },
    { ePSGS_OpenCassandra,                    "OpenCassandra" },
    { ePSGS_NoValidCassandraMapping,          "NoValidCassandraMapping" },
    { ePSGS_InvalidCassandraMapping,          "InvalidCassandraMapping" },
    { ePSGS_NewCassandraMappingAccepted,      "NewCassandraMappingAccepted"},
    { ePSGS_NewCassandraSatNamesMapping,      "NewCassandraSatNamesMapping" },
    { ePSGS_OpenCache,                        "OpenCache" },
    { ePSGS_NoCassandraPublicCommentsMapping, "NoCassandraPublicCommentsMapping" },
};
const size_t        kAlertToIdMapSize = sizeof(kAlertToIdMap) / sizeof(SAlertToId);



CJsonNode SPSGAlertAttributes::Serialize(void) const
{
    CJsonNode       result = CJsonNode::NewObjectNode();

    if (m_AcknowledgedTimestamp.time_since_epoch().count() == 0)
        result.SetString("AcknowledgedTime", "n/a");
    else
        result.SetString("AcknowledgedTime",
                         FormatPreciseTime(m_AcknowledgedTimestamp));

    result.SetString("LastDetectedTime",
                     FormatPreciseTime(m_LastDetectedTimestamp));
    result.SetBoolean("On", m_On);
    result.SetInteger("Count", m_Count);
    result.SetInteger("CountSinceAcknowledge", m_CountSinceAck);
    result.SetString("User", m_User);
    result.SetString("Message", m_Message);
    return result;
}


void CPSGAlerts::Register(enum EPSGS_AlertType  alert_type,
                          const string &   message)
{
    map<EPSGS_AlertType,
        SPSGAlertAttributes>::iterator      found;
    lock_guard<mutex>                       guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found != m_Alerts.end()) {
        // Alert has already been there
        found->second.m_LastDetectedTimestamp = chrono::system_clock::now();
        found->second.m_On = true;
        ++found->second.m_Count;
        ++found->second.m_CountSinceAck;
        found->second.m_Message = message;
        return;
    }

    // Brand new alert
    SPSGAlertAttributes     attrs;
    attrs.m_Message = message;
    m_Alerts[alert_type] = attrs;
}


EPSGS_AlertAckResult CPSGAlerts::Acknowledge(const string &  alert_id,
                                             const string &  user)
{
    EPSGS_AlertType     type = x_IdToType(alert_id);
    if (type == EPSGS_AlertType::ePSGS_Unknown)
        return ePSGS_AlertNotFound;

    return Acknowledge(type, user);
}


EPSGS_AlertAckResult CPSGAlerts::Acknowledge(EPSGS_AlertType alert_type,
                                             const string &  user)
{
    map<EPSGS_AlertType,
        SPSGAlertAttributes>::iterator      found;
    lock_guard<mutex>                       guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found == m_Alerts.end())
        return ePSGS_AlertNotFound;

    if (!found->second.m_On)
        return ePSGS_AlertAlreadyAcknowledged;

    found->second.m_AcknowledgedTimestamp = chrono::system_clock::now();
    found->second.m_On = false;
    found->second.m_User = user;
    found->second.m_CountSinceAck = 0;
    return ePSGS_AlertAcknowledged;
}


EPSGS_AlertType CPSGAlerts::x_IdToType(const string &  alert_id) const
{
    for (size_t  k = 0; k < kAlertToIdMapSize; ++k) {
        if (NStr::CompareNocase(alert_id, kAlertToIdMap[k].id) == 0)
            return kAlertToIdMap[k].type;
    }
    return EPSGS_AlertType::ePSGS_Unknown;
}


string CPSGAlerts::x_TypeToId(EPSGS_AlertType  type) const
{
    for (size_t  k = 0; k < kAlertToIdMapSize; ++k) {
        if (kAlertToIdMap[k].type == type)
            return kAlertToIdMap[k].id;
    }
    return "unknown";
}


// Provides the alerts
CJsonNode CPSGAlerts::Serialize(void) const
{
    CJsonNode                                   result = CJsonNode::NewObjectNode();
    map<EPSGS_AlertType,
        SPSGAlertAttributes>::const_iterator    k;
    lock_guard<mutex>                           guard(m_Lock);

    for (k = m_Alerts.begin(); k != m_Alerts.end(); ++k) {
        result.SetByKey(x_TypeToId(k->first), k->second.Serialize());
    }
    return result;
}


// Provides only the active alerts
CJsonNode CPSGAlerts::SerializeActive(void) const
{
    CJsonNode                                   result = CJsonNode::NewObjectNode();
    map<EPSGS_AlertType,
        SPSGAlertAttributes>::const_iterator    k;
    lock_guard<mutex>                           guard(m_Lock);

    for (k = m_Alerts.begin(); k != m_Alerts.end(); ++k) {
        if (k->second.m_On) {
            result.SetByKey(x_TypeToId(k->first), k->second.Serialize());
        }
    }
    return result;
}

