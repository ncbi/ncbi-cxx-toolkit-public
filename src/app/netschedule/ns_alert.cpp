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
 * File Description: NeSchedule affinity registry
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "ns_alert.hpp"


BEGIN_NCBI_SCOPE

struct AlertToId
{
    enum EAlertType     type;
    string              id;
};


const AlertToId     alertToIdMap[] = { { eConfig,          "config" },
                                       { eReconfigure,     "reconfigure" },
                                       { ePidFile,         "pidfile" },
                                       { eStartAfterCrash, "startaftercrash" } };
const size_t        alertToIdMapSize = sizeof(alertToIdMap) / sizeof(AlertToId);



string SNSAlertAttributes::Serialize(void) const
{
    string      acknowledged;

    if (m_AcknowledgedTimestamp.tv_sec == 0 &&
        m_AcknowledgedTimestamp.tv_nsec == 0)
        acknowledged = "n/a";
    else
        acknowledged = NS_FormatPreciseTime(m_AcknowledgedTimestamp);

    return "OK:last_detected_time: " +
                        NS_FormatPreciseTime(m_LastDetectedTimestamp) + "\n"
           "OK:acknowledged_time: " + acknowledged + "\n"
           "OK:on: " + NStr::BoolToString(m_On) + "\n"
           "OK:count: " + NStr::NumericToString(m_Count);
}


void CNSAlerts::Register(enum EAlertType alert_type)
{
    map< enum EAlertType,
         SNSAlertAttributes >::iterator     found;
    CFastMutexGuard                         guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found != m_Alerts.end()) {
        // Alert has already been there
        found->second.m_LastDetectedTimestamp = CNSPreciseTime::Current();
        found->second.m_On = true;
        ++found->second.m_Count;
        return;
    }

    // Brand new alert
    m_Alerts[alert_type] = SNSAlertAttributes();
}


enum EAlertAckResult CNSAlerts::Acknowledge(const string &  alert_id)
{
    EAlertType  type = x_IdToType(alert_id);
    if (type == eUnknown)
        return eNotFound;

    return Acknowledge(type);
}


enum EAlertAckResult CNSAlerts::Acknowledge(enum EAlertType alert_type)
{
    map< enum EAlertType,
         SNSAlertAttributes >::iterator     found;
    CFastMutexGuard                         guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found == m_Alerts.end())
        return eNotFound;

    if (!found->second.m_On)
        return eAlreadyAcknowledged;

    found->second.m_AcknowledgedTimestamp = CNSPreciseTime::Current();
    found->second.m_On = false;
    return eAcknowledged;
}


enum EAlertType CNSAlerts::x_IdToType(const string &  alert_id) const
{
    for (size_t  k = 0; k < alertToIdMapSize; ++k) {
        if (NStr::CompareNocase(alert_id, alertToIdMap[k].id) == 0)
            return alertToIdMap[k].type;
    }
    return eUnknown;
}


string CNSAlerts::x_TypeToId(enum EAlertType  type) const
{
    for (size_t  k = 0; k < alertToIdMapSize; ++k) {
        if (alertToIdMap[k].type == type)
            return alertToIdMap[k].id;
    }
    return "unknown";
}


// Provides the alerts which are on at the moment.
// It is a url encoded format
string CNSAlerts::GetURLEncoded(void) const
{
    string                                      result;
    map< enum EAlertType,
         SNSAlertAttributes >::const_iterator   k;
    CFastMutexGuard                             guard(m_Lock);

    for (k = m_Alerts.begin(); k != m_Alerts.end(); ++k) {
        if (!result.empty())
            result += "&";
        result += "alert_" + x_TypeToId(k->first) + "=";
        if (!k->second.m_On)
            result += "-";
        result += NStr::NumericToString(k->second.m_Count);
    }
    return result;
}


// Provides the alerts for the STAT ALERTS command
string CNSAlerts::Serialize(void) const
{
    string                                      result;
    map< enum EAlertType,
         SNSAlertAttributes >::const_iterator   k;
    CFastMutexGuard                             guard(m_Lock);

    for (k = m_Alerts.begin(); k != m_Alerts.end(); ++k) {
        if (!result.empty())
            result += "\n";
        result += "OK:[alert " + x_TypeToId(k->first) + "]\n" +
                  k->second.Serialize();
    }
    return result;
}


END_NCBI_SCOPE

