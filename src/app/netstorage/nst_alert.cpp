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
 * File Description: NetStorage server alerts
 *
 */
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>

#include "nst_alert.hpp"


BEGIN_NCBI_SCOPE

struct AlertToId
{
    enum EAlertType     type;
    string              id;
};


const AlertToId     alertToIdMap[] = {
                        { eStartupConfig,          "StartupConfig" },
                        { eReconfigure,            "Reconfigure" },
                        { ePidFile,                "PidFile" },
                        { eDB,                     "Database" },
                        { eAccess,                 "AccessDenied" },
                        { eConfigOutOfSync,        "ConfigOutOfSync" },
                        { eDecryptDBPass,          "DecryptDBPassword" },
                        { eDecryptAdminNames,      "DecryptAdminNames" },
                        { eDBConnect,              "DatabaseConnect" },
                        { eDecryptInNetStorageAPI, "DecryptInNetStorageAPI" }
                                     };
const size_t        alertToIdMapSize = sizeof(alertToIdMap) / sizeof(AlertToId);




CJsonNode SNSTAlertAttributes::Serialize(void) const
{
    CJsonNode       alert(CJsonNode::NewObjectNode());

    alert.SetBoolean("Acknowledged", !m_On);
    alert.SetInteger("Count", m_Count);
    alert.SetInteger("CountSinceAcknowledge", m_CountSinceAck);
    alert.SetString("LastOccured",
                    NST_FormatPreciseTime(m_LastDetectedTimestamp));
    if (double(m_AcknowledgedTimestamp) == 0.0)
        alert.SetString("LastAcknowledged", "n/a");
    else
        alert.SetString("LastAcknowledged",
                        NST_FormatPreciseTime(m_AcknowledgedTimestamp));
    alert.SetString("User", m_User);
    alert.SetString("Message", m_Message);

    return alert;
}



void CNSTAlerts::Register(enum EAlertType alert_type,
                          const string &  message)
{
    map< enum EAlertType,
         SNSTAlertAttributes >::iterator    found;
    CFastMutexGuard                         guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found != m_Alerts.end()) {
        // Alert has already been there
        found->second.m_LastDetectedTimestamp = CNSTPreciseTime::Current();
        found->second.m_On = true;
        found->second.m_Message = message;
        ++found->second.m_Count;
        ++found->second.m_CountSinceAck;
        return;
    }

    // Brand new alert
    SNSTAlertAttributes     attrs;
    attrs.m_Message = message;
    m_Alerts[alert_type] = attrs;
}


enum EAlertAckResult CNSTAlerts::Acknowledge(const string &  alert_id,
                                             const string &  user)
{
    EAlertType  type = x_IdToType(alert_id);
    if (type == eUnknown)
        return eNotFound;

    return Acknowledge(type, user);
}


enum EAlertAckResult CNSTAlerts::Acknowledge(enum EAlertType alert_type,
                                             const string &  user)
{
    map< enum EAlertType,
         SNSTAlertAttributes >::iterator    found;
    CFastMutexGuard                         guard(m_Lock);

    found = m_Alerts.find(alert_type);
    if (found == m_Alerts.end())
        return eNotFound;

    if (!found->second.m_On)
        return eAlreadyAcknowledged;

    found->second.m_AcknowledgedTimestamp = CNSTPreciseTime::Current();
    found->second.m_On = false;
    found->second.m_User = user;
    found->second.m_CountSinceAck = 0;
    return eAcknowledged;
}


enum EAlertType CNSTAlerts::x_IdToType(const string &  alert_id) const
{
    for (size_t  k = 0; k < alertToIdMapSize; ++k) {
        if (NStr::CompareNocase(alert_id, alertToIdMap[k].id) == 0)
            return alertToIdMap[k].type;
    }
    return eUnknown;
}


string CNSTAlerts::x_TypeToId(enum EAlertType  type) const
{
    for (size_t  k = 0; k < alertToIdMapSize; ++k) {
        if (alertToIdMap[k].type == type)
            return alertToIdMap[k].id;
    }
    return "unknown";
}


CJsonNode CNSTAlerts::Serialize(void) const
{
    CJsonNode           alerts(CJsonNode::NewArrayNode());
    CFastMutexGuard     guard(m_Lock);

    for (map<enum EAlertType,
             SNSTAlertAttributes>::const_iterator  k = m_Alerts.begin();
         k != m_Alerts.end(); ++k) {
        CJsonNode   single_alert(k->second.Serialize());

        single_alert.SetString("Name", x_TypeToId(k->first));
        alerts.Append(single_alert);
    }
    return alerts;
}


END_NCBI_SCOPE

