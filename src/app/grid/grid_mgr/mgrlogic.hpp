#ifndef NCBI_GRID_MGR_LOGIC__HPP
#define NCBI_GRID_MGR_LOGIC__HPP

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
 * Author:  Maxim Didenko
 *
 *     
 */

#include <corelib/ncbistd.hpp>
#include <corelib/ncbimisc.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbistr.hpp>

#include <list>

USING_NCBI_SCOPE;

class CServiceInfo;
class CNSServices
{
public:
    typedef list<AutoPtr<CServiceInfo> > TServiceInfos;
    typedef TServiceInfos::const_iterator const_iterator;
    typedef CServiceInfo value_type;

    CNSServices();
    CNSServices(const string& lbsurl);
    ~CNSServices();

    const_iterator begin() const { return m_Services.begin(); }

    const_iterator end() const { return m_Services.end(); }

private:
    TServiceInfos m_Services;
};
//
// class CServiceInfo
//
class CHostInfo;
class CServiceInfo
{
public: 
    typedef list<AutoPtr<CHostInfo> > THostInfos;
    typedef THostInfos::const_iterator const_iterator;
    typedef CHostInfo value_type;

    CServiceInfo(const string& name, const string& value);
    ~CServiceInfo();

    const string& GetName() const { return m_Name; }

    const_iterator begin() const { return m_Hosts.begin(); }
    const_iterator end() const { return m_Hosts.end(); }

private:    
    string m_Name;
    THostInfos m_Hosts;
};

//
// class CHostInfo
//

class CQueueInfo;
class CHostInfo
{
public:

    typedef list<AutoPtr<CQueueInfo> > TQueueInfos;
    typedef TQueueInfos::const_iterator const_iterator;
    typedef CQueueInfo value_type;

    CHostInfo(const string& host, unsigned int port);
    ~CHostInfo();

    void CollectInfo();

    const string& GetHost() const { return m_Host; }
    unsigned int GetPort() const { return m_Port; }

    bool IsActive() const { return m_Active; }
    const string& GetVersion() const { return m_Version; }

    const_iterator begin() const { return m_Queues.begin(); }
    const_iterator end() const { return m_Queues.end(); }

private:
    string m_Host;
    unsigned int m_Port;
    TQueueInfos m_Queues;
    bool m_Active;
    string m_Version;
    
};

//
// class CQueueInfo
//

class CWorkerNodeInfo;
class CQueueInfo
{
public:
    typedef list<AutoPtr<CWorkerNodeInfo> > TWorkerNodeInfos;
    typedef TWorkerNodeInfos::const_iterator const_iterator;
    typedef CWorkerNodeInfo value_type;

    CQueueInfo(const string& name, const string& host, unsigned int port);
    ~CQueueInfo();

    void CollectInfo();

    const string& GetName() const { return m_Name; }
    const string& GetHost() const { return m_Host; }
    unsigned int GetPort() const { return m_Port; }

    const string& GetInfo() const { return m_Info; }

    const_iterator begin() const { return m_WorkerNodes.begin(); }
    const_iterator end() const { return m_WorkerNodes.end(); }

private:
    string m_Name;
    string m_Host;
    unsigned int m_Port;
    string m_Info;
    TWorkerNodeInfos m_WorkerNodes;
};

//
// class CWorkerNodeInfo
//

class CWorkerNodeInfo
{
public:
    CWorkerNodeInfo(const string& host, unsigned int port);
    ~CWorkerNodeInfo();

    void CollectInfo();

    const string& GetHost() const { return m_Host; }
    unsigned int GetPort() const { return m_Port; }

    void SetClientName(const string& name) { m_ClientName = name; }
    const string& GetClientName() const { return m_ClientName; }

    void SetVersion(const string& version) { m_Version = version;}
    const string GetVersion() const { return m_Version; }

    void SetLastAccess(const CTime& time);
    const CTime& GetLastAccess() const { return m_LastAccess; }

    bool IsActive() const { return m_Active; }
    const CTime& GetBuildTime() const { return m_BuildTime; }

    string GetStatistics() const;

private:
    string m_Host;
    unsigned int m_Port;
    string m_ClientName;
    CTime m_LastAccess;
    CTime m_BuildTime;
    bool m_Active;
    string m_Version;
    
};


//
// class CNetCacheStatInfo
//

class CNetCacheStatInfo
{
public:
    CNetCacheStatInfo(const string& host, unsigned int port);
	
    const string& GetHost() const { return m_Host; }
    unsigned int GetPort() const { return m_Port; }

    string GetStatistics() const;
	string GetVersion() const { return m_Version; }

private:
    string       m_Host;
    unsigned int m_Port;
    string       m_Version;
};


template<class Info>
inline string GetSPort(const Info& info)
{
    return NStr::UIntToString(info.GetPort());
}


#endif /* NCBI_GRID_MGR_LOGIC__HPP */


