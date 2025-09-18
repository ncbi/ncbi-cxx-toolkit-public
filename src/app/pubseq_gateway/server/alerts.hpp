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


enum EPSGS_AlertType {
    ePSGS_Unknown = -1,
    ePSGS_ConfigProblems = 0,
    ePSGS_OpenCassandra = 1,
    ePSGS_NoValidCassandraMapping = 2,              // PSG has no valid cassandra mapping at hand
    ePSGS_InvalidCassandraMapping = 3,              // PSG has detected an invalid mapping in cassandra
    ePSGS_NewCassandraMappingAccepted = 4,          // PSG accepted an updated mapping from cassandra
    ePSGS_NewCassandraSatNamesMapping = 5,          // PSG has detected new mapping in cassandra
    ePSGS_NewCassandraPublicCommentMapping = 6,
    ePSGS_OpenCache = 7,                            // PSG cannot create or open the LMDB cache
    ePSGS_NoCassandraPublicCommentsMapping = 8,     // PSG has no public comments mapping
    ePSGS_NoIPGKeyspace = 9,
    ePSGS_MyNCBIResolveDNS = 10,
    ePSGS_MyNCBITest = 11,
    ePSGS_MyNCBIUvLoop = 12,
    ePSGS_TcpConnHardLimitExceeded = 13,
    ePSGS_TcpConnSoftLimitExceeded = 14,
    ePSGS_TcpConnAlertLimitExceeded = 15,
    ePSGS_Throttling = 16,
    ePSGS_SeqIdClassificationConfigFilesRefreshSuccess = 17,
    ePSGS_SeqIdClassificationConfigFilesRefreshFailure = 18
};

enum EPSGS_AlertAckResult {
    ePSGS_AlertNotFound = 0,
    ePSGS_AlertAlreadyAcknowledged = 1,
    ePSGS_AlertAcknowledged = 2
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
        void Register(EPSGS_AlertType alert_type, const string &  message);
        EPSGS_AlertAckResult Acknowledge(const string &  alert_id,
                                         const string &  user);
        EPSGS_AlertAckResult Acknowledge(EPSGS_AlertType alert_type,
                                         const string &  user);
        CJsonNode Serialize(void) const;

    private:
        mutable mutex                               m_Lock;
        map<EPSGS_AlertType, SPSGAlertAttributes>   m_Alerts;

        EPSGS_AlertType x_IdToType(const string &  alert_id) const;
        string x_TypeToId(EPSGS_AlertType  type) const;
};


#endif /* PUBSEQ_GATEWAY_ALERTS__HPP */

