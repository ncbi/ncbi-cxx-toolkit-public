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
 * Authors: Dmitri Dmitrienko
 *
 * File Description:
 *
 *  Casssandra connection factory class
 *
 */

#ifndef OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_FACTORY_HPP
#define OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_FACTORY_HPP

#include "cass_driver.hpp"
#include "IdCassScope.hpp"

#include <corelib/ncbiargs.hpp>
#include <corelib/ncbireg.hpp>

#include <memory>
#include <string>

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

class CCassConnectionFactory:
    public enable_shared_from_this<CCassConnectionFactory>
{
 public:
    CCassConnectionFactory(const CCassConnectionFactory&) = delete;
    CCassConnectionFactory& operator=(const CCassConnectionFactory&) = delete;
    CCassConnectionFactory(CCassConnectionFactory&&) = delete;
    CCassConnectionFactory& operator=(CCassConnectionFactory&&) = delete;

    CCassConnectionFactory();
    ~CCassConnectionFactory();
    void AppParseArgs(const CArgs &  args);
    void LoadConfig(const string &  cfg_name, const string &  section);
    void LoadConfig(const CNcbiRegistry &  registry, const string &  section);
    void LoadConfig(IRegistry const* registry, string const& section);
    void ReloadConfig();
    void ReloadConfig(const CNcbiRegistry &  registry);
    void ReloadConfig(IRegistry const* registry);
    shared_ptr<CCassConnection> CreateInstance();

    void GetHostPort(string & hostlist, short & port);
    string GetUserName() const;
    string GetPassword() const;

    void SetServiceName(string const& service);
    void SetDataNamespace(string const& data_namespace);

    static shared_ptr<CCassConnectionFactory> s_Create()
    {
        return make_shared<CCassConnectionFactory>();
    }

    void SetLogging(EDiagSev  severity)
    {
        m_LogSeverity = severity;
    }

 protected:
    void ProcessParams(void);

 private:
    void x_ValidateArgs(void);


    CFastMutex              m_RunTimeParams;
    string                  m_CfgName;
    string                  m_Section;
    string                  m_CassHosts;
    string                  m_CassUserName;
    string                  m_CassPassword;
    string                  m_CassDataNamespace;
    string                  m_PassFile;
    string                  m_PassSection;
    string                  m_LoadBalancingStr;
    string                  m_CassBlackList;
    unsigned int            m_CassConnTimeoutMs;
    unsigned int            m_CassQueryTimeoutMs;
    unsigned int            m_CassQueryRetryTimeoutMs{0};
    int                     m_MaxRetries;
    bool                    m_CassFallbackRdConsistency;
    unsigned int            m_CassFallbackWrConsistency;
    loadbalancing_policy_t  m_LoadBalancing;
    bool                    m_TokenAware;
    bool                    m_LatencyAware;
    unsigned int            m_NumThreadsIo;
    unsigned int            m_NumConnPerHost;
    unsigned int            m_Keepalive;

    EDiagSev                m_LogSeverity;
    bool                    m_LogEnabled;
};

END_IDBLOB_SCOPE

#endif  // OBJTOOLS__PUBSEQ_GATEWAY__IMPL__CASSANDRA__CASS_FACTORY_HPP
