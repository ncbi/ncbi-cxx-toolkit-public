#ifndef NETSTORAGE_SERVER__HPP
#define NETSTORAGE_SERVER__HPP

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
 * Authors:  Denis Vakatov
 *
 * File Description: NetStorage threaded server
 *
 */

#include <string>
#include <connect/services/compound_id.hpp>
#include <connect/server.hpp>


#include "nst_server_parameters.hpp"
#include "nst_precise_time.hpp"
#include "nst_alert.hpp"
#include "nst_database.hpp"


BEGIN_NCBI_SCOPE



class CNetStorageServer : public CServer
{
public:
    CNetStorageServer();
    virtual ~CNetStorageServer();

    void AddDefaultListener(IServer_ConnectionFactory *  factory);
    CJsonNode SetParameters(const SNetStorageServerParameters &  new_params,
                            bool                                 reconfigure);

    virtual bool ShutdownRequested(void);
    void SetShutdownFlag(int signum = 0);

    const bool &  IsLog() const
    { return m_Log; }

    unsigned int  GetNetworkTimeout(void) const
    { return m_NetworkTimeout; }
    unsigned short  GetPort() const
    { return m_Port; }
    unsigned int  GetHostNetAddr() const
    { return m_HostNetAddr; }
    const CNSTPreciseTime &  GetStartTime(void) const
    { return m_StartTime; }
    string  GetSessionID(void) const
    { return m_SessionID; }
    void SaveCommandLine(const string &  cmd_line)
    { m_CommandLine = cmd_line; }
    string GetCommandLine(void) const
    { return m_CommandLine; }
    CCompoundIDPool GetCompoundIDPool(void) const
    { return m_CompoundIDPool; }

    bool IsAdminClientName(const string &  name) const;

    enum EAlertAckResult AcknowledgeAlert(const string &  id);
    enum EAlertAckResult AcknowledgeAlert(EAlertType  alert_type);
    void RegisterAlert(EAlertType  alert_type);
    CJsonNode SerializeAlerts(void) const;
    CNSTDatabase &  GetDb(void);

    // Metainfo support
    bool NeedMetadata(const string &  service) const;
    CJsonNode ReadMetadataConfiguration(const IRegistry &  reg);
    CJsonNode serializeMetadataInfo(void) const;

    static CNetStorageServer *  GetInstance(void);

protected:
    virtual void Exit();

private:
    unsigned short              m_Port;
    unsigned int                m_HostNetAddr;
    mutable bool                m_Shutdown;
    int                         m_SigNum;
    bool                        m_Log;
    string                      m_SessionID;
    unsigned int                m_NetworkTimeout;
    CNSTPreciseTime             m_StartTime;
    string                      m_CommandLine;
    CCompoundIDPool             m_CompoundIDPool;
    CNSTAlerts                  m_Alerts;
    auto_ptr<CNSTDatabase>      m_Db;   // Access to NST attributes DB

    static CNetStorageServer *  sm_netstorage_server;

    // Administrators
    mutable CFastMutex          m_AdminClientNamesLock;
    set<string>                 m_AdminClientNames;

    // Metadata services support
    mutable CFastMutex          m_MetadataServicesLock;
    set<string>                 m_MetadataServices;

private:
    string  x_GenerateGUID(void) const;
    set<string> x_GetAdminClientNames(const string &  client_names);
    CJsonNode  x_diffInJson(const vector<string> &  added,
                            const vector<string> &  deleted) const;
};


END_NCBI_SCOPE

#endif

