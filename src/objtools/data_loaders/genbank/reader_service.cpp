/*  $Id$
* ===========================================================================
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
* ===========================================================================
*
*  Author:  Eugene Vasilchenko
*
*  File Description: Common class to control ID1/ID2 service connections
*
*/

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/genbank/reader_service.hpp>
#include <connect/ncbi_conn_stream.hpp>
#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


struct ConnInfoDeleter2
{
    /// C Language deallocation function.
    static void Delete(SConnNetInfo* object)
    { ConnNetInfo_Destroy(object); }
};


struct SServerScanInfo
{
    typedef vector< AutoPtr<SSERV_Info, CDeleter<SSERV_Info> > > TSkipServers;
    SServerScanInfo(const TSkipServers& skip_servers)
        : m_TotalCount(0),
          m_SkippedCount(0),
          m_SkipServers(skip_servers),
          m_CurrentServer(0)
        {
        }
    int m_TotalCount;
    int m_SkippedCount;
    const TSkipServers& m_SkipServers;
    const SSERV_Info* m_CurrentServer;
    
    bool SkipServer(const SSERV_Info* server);
};


bool SServerScanInfo::SkipServer(const SSERV_Info* server)
{
    ++m_TotalCount;
    ITERATE ( TSkipServers, it, m_SkipServers ) {
        if ( SERV_EqualInfo(server, it->get()) ) {
            ++m_SkippedCount;
            return true;
        }
    }
    return false;
}


static const SSERV_Info* s_GetNextInfo(SERV_ITER iter, void* data)
{
    SServerScanInfo* scan_info = static_cast<SServerScanInfo*>(data);
    const SSERV_Info* info = SERV_GetNextInfo(iter);
    while ( info && scan_info->SkipServer(info) ) {
        info = SERV_GetNextInfo(iter);
    }
    scan_info->m_CurrentServer = info;
    return info;
}


CReaderServiceConnector::SConnInfo CReaderServiceConnector::Connect(void)
{
    SConnInfo info;
    
    STimeout tmout;
    tmout.sec = m_OpenTimeout;
    tmout.usec = 0;
    
    SServerScanInfo scan_info(m_SkipServers);
        
    SSERVICE_Extra params;
    memset(&params, 0, sizeof(params));
    params.data = &scan_info;
    params.get_next_info = s_GetNextInfo;
    params.flags = fHCC_NoAutoRetry;
        
    if ( NStr::StartsWith(m_ServiceName, "http://") ) {
        info.m_Stream.reset(new CConn_HttpStream(m_ServiceName));
    }
    else {
        info.m_Stream.reset(new CConn_ServiceStream(m_ServiceName, fSERV_Any,
                                                    0, &params, &tmout));
    }

    CConn_IOStream& stream = *info.m_Stream;
    // need to call CONN_Wait to force connection to open
    if ( !stream.bad() ) {
        CONN_Wait(stream.GetCONN(), eIO_Write, &tmout);
        info.m_ServerInfo = scan_info.m_CurrentServer;
    }
    if ( scan_info.m_TotalCount == scan_info.m_SkippedCount ) {
        // all servers are skipped, reset skip-list
        m_SkipServers.clear();
    }
    return info;
}


string CReaderServiceConnector::GetConnDescription(CConn_IOStream& stream) const
{
    string ret = m_ServiceName;
    CONN conn = stream.GetCONN();
    if ( conn ) {
        AutoPtr<char, CDeleter<char> > descr(CONN_Description(conn));
        if ( descr ) {
            ret += " -> ";
            ret += descr.get();
        }
    }
    return ret;
}


CReaderServiceConnector::CReaderServiceConnector(void)
    : m_OpenTimeout(0),
      m_Timeout(0)
{
}


CReaderServiceConnector::CReaderServiceConnector(const string& service_name,
                                                 int open_timeout,
                                                 int timeout)
    : m_ServiceName(service_name),
      m_OpenTimeout(open_timeout),
      m_Timeout(timeout)
{
}


CReaderServiceConnector::~CReaderServiceConnector(void)
{
}


void CReaderServiceConnector::SetServiceName(const string& service_name)
{
    m_ServiceName = service_name;
    m_SkipServers.clear();
}


void CReaderServiceConnector::SetTimeout(int timeout)
{
    m_Timeout = timeout;
    if ( !m_OpenTimeout ) {
        m_OpenTimeout = timeout;
    }
}


void CReaderServiceConnector::SetOpenTimeout(int open_timeout)
{
    m_OpenTimeout = open_timeout;
}


void CReaderServiceConnector::RememberIfBad(SConnInfo& conn_info)
{
    if ( conn_info.m_ServerInfo ) {
        // server failed without any reply, remember to skip it next time
        m_SkipServers.push_back(SERV_CopyInfo(conn_info.m_ServerInfo));
        conn_info.m_ServerInfo = 0;
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
