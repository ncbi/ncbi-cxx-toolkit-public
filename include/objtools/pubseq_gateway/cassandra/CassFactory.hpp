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

#ifndef _CASS_FACTORY_H_
#define _CASS_FACTORY_H_

#include <corelib/ncbiargs.hpp>

#include <objtools/pubseq_gateway/diag/IdLogUtl.hpp>
#include <objtools/pubseq_gateway/diag/AppLog.hpp>
#include "CassDriver.hpp"
#include "IdCassScope.hpp"

BEGIN_IDBLOB_SCOPE
USING_NCBI_SCOPE;

#define DFLT_C_TIMEOUT_MS  30000
#define DFLT_Q_TIMEOUT_MS  5000

class CCassConnectionFactory: public enable_shared_from_this<CCassConnectionFactory> {
private:
	DISALLOW_COPY_AND_ASSIGN(CCassConnectionFactory);
	CFastMutex m_RunTimeParams;
	string  m_CfgName;
	string  m_Section;
	string  m_BigDataHost;
    short   m_BigDataPort;
    string  m_BigDataUser;
    string  m_BigDataPassword;
    string  m_BigDataNameSpace;
    string  m_BigDataLog;
	string  m_PassFile;
	string  m_PassSection;
	string  m_LoadBalancingStr;
	string  m_tokenAwareStr;
	string  m_latencyAwareStr;
    unsigned int m_BigDataCTimeoutMS;
    unsigned int m_BigDataQTimeoutMS;
	bool m_BigDataFallBackRdConsistency;
    loadbalancing_policy_t m_LoadBalancing;
    bool m_tokenAware;
    bool m_latencyAware;
    unsigned int m_numThreadsIo;
    unsigned int m_numConnPerHost;
    unsigned int m_maxConnPerHost;
    unsigned int m_keepalive;
protected:
	CCassConnectionFactory() : 
        m_BigDataPort(0),
		m_BigDataCTimeoutMS(DFLT_C_TIMEOUT_MS), 
		m_BigDataQTimeoutMS(DFLT_Q_TIMEOUT_MS), 
		m_BigDataFallBackRdConsistency(false), 
		m_LoadBalancing(LB_DCAWARE), 
		m_tokenAware(true), 
		m_latencyAware(true), 
		m_numThreadsIo(0), 
		m_numConnPerHost(0), 
		m_maxConnPerHost(0),
		m_keepalive(0) 
	{}
	void ProcessParams();
    void GetHostPort(string &host, short &port);
public:
	~CCassConnectionFactory();
	void AppInit(CArgDescriptions* argdesc);
	void AppParseArgs(const CArgs& args);
	void LoadConfig(string CfgName, string section);
    void LoadConfig(const CNcbiRegistry &Registry, string section);
	void ReloadConfig();
    void ReloadConfig(const CNcbiRegistry &Registry);
	shared_ptr<CCassConnection> CreateInstance();
	static shared_ptr<CCassConnectionFactory> Create() {
		return shared_ptr<CCassConnectionFactory>(new CCassConnectionFactory());
	}
};

END_IDBLOB_SCOPE

#endif
