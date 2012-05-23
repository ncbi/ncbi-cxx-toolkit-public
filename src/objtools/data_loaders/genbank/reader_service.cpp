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
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_config.hpp>
#include <objtools/data_loaders/genbank/reader_service.hpp>
#include <objtools/data_loaders/genbank/reader_service_params.h>
#include <objtools/data_loaders/genbank/reader_id2_base.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbithr.hpp>
#include <connect/ncbi_socket.hpp>
#include <cmath>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CReader;

#define DEFAULT_TIMEOUT  20
#define DEFAULT_OPEN_TIMEOUT  5
#define DEFAULT_OPEN_TIMEOUT_MAX  30
#define DEFAULT_OPEN_TIMEOUT_MULTIPLIER  1.5
#define DEFAULT_OPEN_TIMEOUT_INCREMENT  0

static CIncreasingTime::SAllParams s_OpenTimeoutParams = {
    {
        NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT,
        0,
        DEFAULT_OPEN_TIMEOUT
    },
    {
        NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_MAX,
        NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_MAX,
        DEFAULT_OPEN_TIMEOUT_MAX
    },
    {
        NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_MULTIPLIER,
        NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_MULTIPLIER,
        DEFAULT_OPEN_TIMEOUT_MULTIPLIER
    },
    {
        NCBI_GBLOADER_READER_PARAM_OPEN_TIMEOUT_INCREMENT,
        NCBI_GBLOADER_READER_PARAM2_OPEN_TIMEOUT_INCREMENT,
        DEFAULT_OPEN_TIMEOUT_INCREMENT
    }
};

typedef CId2ReaderBase::CDebugPrinter CDebugPrinter;


NCBI_PARAM_DECL(int, GENBANK, CONN_DEBUG);
NCBI_PARAM_DEF_EX(int, GENBANK, CONN_DEBUG, 0,
                  eParam_NoThread, GENBANK_CONN_DEBUG);

static int s_GetDebugLevel(void)
{
    static NCBI_PARAM_TYPE(GENBANK, CONN_DEBUG) s_Value;
    return s_Value.Get();
}


struct ConnInfoDeleter2
{
    /// C Language deallocation function.
    static void Delete(SConnNetInfo* object)
    { ConnNetInfo_Destroy(object); }
};


struct SServerScanInfo : public CObject
{
    typedef vector< AutoPtr<SSERV_Info, CDeleter<SSERV_Info> > > TSkipServers;
    SServerScanInfo(const TSkipServers& skip_servers)
        : m_TotalCount(0),
          m_SkippedCount(0),
          m_CurrentServer(0),
          m_SkipServers(skip_servers)
        {
        }
    int m_TotalCount;
    int m_SkippedCount;
    const SSERV_Info* m_CurrentServer;
    const TSkipServers& m_SkipServers;

    void Reset(void) {
        m_TotalCount = 0;
        m_SkippedCount = 0;
        m_CurrentServer = 0;
    }
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


struct ConnNetInfoDeleter
{
    /// C Language deallocation function.
    static void Delete(SConnNetInfo* object) {
        ConnNetInfo_Destroy(object);
    }
};


static void s_ScanInfoReset(void* data)
{
    SServerScanInfo* scan_info = static_cast<SServerScanInfo*>(data);
    scan_info->Reset();
}


static void s_ScanInfoCleanup(void* data)
{
    SServerScanInfo* scan_info = static_cast<SServerScanInfo*>(data);
    scan_info->RemoveReference();
}


static const SSERV_Info* s_ScanInfoGetNextInfo(void* data, SERV_ITER iter)
{
    SServerScanInfo* scan_info = static_cast<SServerScanInfo*>(data);
    const SSERV_Info* info = SERV_GetNextInfo(iter);
    while ( info && scan_info->SkipServer(info) ) {
        info = SERV_GetNextInfo(iter);
    }
    scan_info->m_CurrentServer = info;
    return info;
}


CReaderServiceConnector::SConnInfo
CReaderServiceConnector::Connect(int error_count)
{
    SConnInfo info;
    
    STimeout tmout;
    SetOpenTimeoutTo(&tmout, error_count);
    
    CRef<SServerScanInfo> scan_info;

    if ( NStr::StartsWith(m_ServiceName, "http://") ) {
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Opening HTTP connection to " << m_ServiceName;
        }
        info.m_Stream.reset(new CConn_HttpStream(m_ServiceName));
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Opened HTTP connection to "<<m_ServiceName;
        }
    }
    else {
        AutoPtr<SConnNetInfo, ConnNetInfoDeleter> net_info
            (ConnNetInfo_Create(m_ServiceName.c_str()));
        net_info->max_try = 1;
        
        if ( !m_SkipServers.empty() && s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "skip:";
            ITERATE ( TSkipServers, it, m_SkipServers ) {
                s << " " << CSocketAPI::ntoa(it->get()->host);
            }
        }
        CRef<SServerScanInfo> scan_ptr(new SServerScanInfo(m_SkipServers));
        SSERVICE_Extra params;
        memset(&params, 0, sizeof(params));
        params.reset = s_ScanInfoReset;
        params.cleanup = s_ScanInfoCleanup;
        params.get_next_info = s_ScanInfoGetNextInfo;
        params.flags = fHTTP_NoAutoRetry;
        
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Opening service connection to " << m_ServiceName;
        }
        params.data = scan_ptr;
        scan_ptr->AddReference();
        info.m_Stream.reset(new CConn_ServiceStream(m_ServiceName, fSERV_Any,
                                                    net_info.get(),
                                                    &params,
                                                    &tmout));
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Opened service connection to " << m_ServiceName;
        }
        scan_info = scan_ptr;
    }

    CConn_IOStream& stream = *info.m_Stream;
    // need to call CONN_Wait to force connection to open
    if ( !stream.bad() ) {
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Waiting for connector...";
        }
        CONN_Wait(stream.GetCONN(), eIO_Write, &tmout);
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Got connector.";
        }
        if ( scan_info ) {
            info.m_ServerInfo = scan_info->m_CurrentServer;
        }
    }
    if ( scan_info && s_GetDebugLevel() > 0 ) {
        CDebugPrinter s("CReaderConnector");
        s << "servers:";
        s << " total: "<<scan_info->m_TotalCount;
        s << " skipped: "<<scan_info->m_SkippedCount;
    }
    if ( scan_info && !m_SkipServers.empty() &&
         scan_info->m_TotalCount == scan_info->m_SkippedCount ) {
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "Clearing skip servers.";
        }
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
    : m_Timeout(DEFAULT_TIMEOUT),
      m_OpenTimeout(s_OpenTimeoutParams)
{
}


CReaderServiceConnector::CReaderServiceConnector(const string& service_name)
    : m_ServiceName(service_name),
      m_Timeout(DEFAULT_TIMEOUT),
      m_OpenTimeout(s_OpenTimeoutParams)
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


void CReaderServiceConnector::InitTimeouts(CConfig& conf,
                                           const string& driver_name)
{
    m_Timeout = conf.GetInt(driver_name,
                            NCBI_GBLOADER_READER_PARAM_TIMEOUT,
                            CConfig::eErr_NoThrow,
                            DEFAULT_TIMEOUT);
    m_OpenTimeout.Init(conf, driver_name, s_OpenTimeoutParams);
}


void CReaderServiceConnector::RememberIfBad(SConnInfo& conn_info)
{
    if ( conn_info.m_ServerInfo ) {
        // server failed without any reply, remember to skip it next time
        m_SkipServers.push_back(SERV_CopyInfo(conn_info.m_ServerInfo));
        if ( s_GetDebugLevel() > 0 ) {
            CDebugPrinter s("CReaderConnector");
            s << "added skip: "<<
                CSocketAPI::ntoa(m_SkipServers.back().get()->host);
        }
        conn_info.m_ServerInfo = 0;
    }
}


void CReaderServiceConnector::x_SetTimeoutTo(STimeout* tmout,
                                             double timeout)
{
    tmout->sec = unsigned(timeout);
    tmout->usec = unsigned((timeout-tmout->sec)*1e9);
}


END_SCOPE(objects)
END_NCBI_SCOPE
