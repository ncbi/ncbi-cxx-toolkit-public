/* $Id$
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
 * Author:  Sergey Sikorskiy
 *
 * File Description:
 *   Database service name to server name mapping policy implementation.
 *
 */

#include <ncbi_pch.hpp>

#include "../ncbi_lbsm.h"
#include "../ncbi_server_infop.h"
#include <connect/ext/ncbi_dblb_svcmapper.hpp>
#include <connect/ext/ncbi_dblb.h>
#include <connect/ncbi_socket.hpp>
#include <connect/ncbi_service.h>
#include <corelib/ncbiapp.hpp>
#include <algorithm>
#include <math.h>


BEGIN_NCBI_SCOPE


///////////////////////////////////////////////////////////////////////////////
class CCharInserter
{
public:
    CCharInserter(vector<const char*>& names)
    : m_Names(&names)
    {
    }

public:
    void operator()(const TSvrRef& server)
    {
        m_Names->push_back(server->GetName().c_str());
    }

private:
    vector<const char*>* m_Names;
};



///////////////////////////////////////////////////////////////////////////////
CDBLB_ServiceMapper::CDBLB_ServiceMapper(const IRegistry* registry)
{
    ConfigureFromRegistry(registry);
}


CDBLB_ServiceMapper::~CDBLB_ServiceMapper(void)
{
}


string CDBLB_ServiceMapper::GetName(void) const
{
    return CDBServiceMapperTraits<CDBLB_ServiceMapper>::GetName();
}


void
CDBLB_ServiceMapper::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    ConfigureFromRegistry(registry);
}


void
CDBLB_ServiceMapper::ConfigureFromRegistry(const IRegistry* registry)
{
    // Get current registry ...
    CNcbiApplicationGuard app = CNcbiApplication::InstanceGuard();
    if (!registry && app) {
        registry = &app->GetConfig();
    }
    if (registry)
        m_EmptyTTL = registry->GetInt("dblb", "cached_empty_service_ttl", 1);
    else
        m_EmptyTTL = 1;
#if 0
    // test code
    string pref_section = "dblb/preferences";
    list<string> pref_keys;
    registry->EnumerateEntries(pref_section, &pref_keys);
    for (const string& service : pref_keys) {
        const string& preference_info = registry->Get(pref_section, service);
        CTempString endpoint, preference, host_str, port_str;
        Uint4 host;
        Uint2 port;

        NStr::SplitInTwo(preference_info, " \t", endpoint, preference,
                         NStr::fSplit_MergeDelimiters);
        NStr::SplitInTwo(endpoint, ":", host_str, port_str);
        host = CSocketAPI::gethostbyname(host_str);
        if ( !port_str.empty() ) {
            NStr::StringToNumeric(port_str, &port);
        }
        SetPreference(service, TSvrRef(new CDBServer(endpoint, host, port)),
                      NStr::StringToDouble(preference));
    }
#endif
}


bool CDBLB_ServiceMapper::x_IsEmpty(const string& service,
                                    TSERV_Type promiscuity, time_t now)
{
    TSrvSet& exclude_list = m_ExcludeMap[service];
    ERASE_ITERATE(TSrvSet, it, exclude_list) {
        if ((*it)->GetExpireTime() <= now) {
            _TRACE("For " << service << ": erasing from excluded server '"
                   << (*it)->GetName() << "', host " << (*it)->GetHost()
                   << ", port " << (*it)->GetPort());
            exclude_list.erase(it);
            m_LBEmptyMap.erase(service);
        }
    }

    TLBEmptyMap::iterator it = m_LBEmptyMap.find(service);
    if (it != m_LBEmptyMap.end()) {
        if (it->second.second >= now
            &&  (promiscuity & ~it->second.first) == 0) {
            // We've tried this service already. It is not served by load
            // balancer. There is no reason to try it again.
            _TRACE("Service " << service << " is known dead, bypassing LBSM.");
            return true;
        }
        else {
            m_LBEmptyMap.erase(it);
        }
    }

    return false;
}


TSvrRef
CDBLB_ServiceMapper::GetServer(const string& service)
{
    TSvrRef svr = x_GetLiteral(service);
    if (svr.NotEmpty()) {
        return svr;
    }
    CFastMutexGuard mg(m_Mtx);
    return x_GetServer(service);
}

CRef<CDBServerOption>
CDBLB_ServiceMapper::x_GetLiteral(CTempString server)
{
    CTempString orig_server = server;
    CRef<CDBServerOption> svr;
    SIZE_TYPE pos = server.find_last_of(".:");
    if (pos == NPOS) {
        return svr;
    }
    Uint2 port = 0;
    if (server[pos] == ':'  &&  pos > 0
        &&  (server[0] == '['  ||  server.find(':') == pos)) {
        if (NStr::StringToNumeric(server.substr(pos+1), &port,
                                  NStr::fConvErr_NoThrow)) {
            if (server[0] == '['  &&  server[pos-1] == ']') {
                server = server.substr(1, pos - 2);
            } else {
                server = server.substr(0, pos);
            }
        } else {
            return svr;
        }
    }
    auto host = CSocketAPI::gethostbyname(server);
    if (host.IsEmpty()  &&  server[pos] == '.') {
        return svr;
    }
    time_t expiration = CDBServer::kMaxExpireTime;
    if ((Uint4)host != 0
        &&  !CSocketAPI::isip(server, CSocketAPI::eIP_Any) ) {
        expiration = time(nullptr) + 10;
    }
    svr.Reset(new CDBServerOption((Uint4)host ? orig_server : server,
                                  host, port, 1.0 /* ranking */,
                                  CDBServerOption::fState_Normal, expiration));
    return svr;
}

TSvrRef
CDBLB_ServiceMapper::x_GetServer(const string& service)
{
    time_t cur_time = time(NULL);
    if (x_IsEmpty(service, fSERV_IncludeInactive, cur_time)) {
        return TSvrRef();
    }

    const TSrvSet& exclude_list = m_ExcludeMap[service];
    vector<const char*> skip_names;
    std::for_each(exclude_list.begin(),
                  exclude_list.end(),
                  CCharInserter(skip_names));
    skip_names.push_back(NULL);

    SDBLB_Preference preference;
    TSvrRef preferred_svr = m_PreferenceMap[service].second;
    if (!preferred_svr.Empty()) {
        preference.host = preferred_svr->GetHost();
        preference.port = preferred_svr->GetPort();
        preference.pref = m_PreferenceMap[service].first;
    }

    SDBLB_ConnPoint cp;
    char name_buff[256];
    EDBLB_Status status;

    const char* svr_name = ::DBLB_GetServer(service.c_str(),
                                            fDBLB_AllowFallbackToStandby,
                                            preferred_svr.Empty()
                                                            ? 0
                                                            : &preference,
                                            &skip_names.front(),
                                            &cp,
                                            name_buff,
                                            sizeof(name_buff),
                                            &status);

    if (cp.time == 0) {
        cp.time = TNCBI_Time(cur_time) + 10;
    }

    if (svr_name  &&  *svr_name  &&  status != eDBLB_NoDNSEntry) {
        return TSvrRef(new CDBServer(svr_name,  cp.host, cp.port, cp.time));
    } else if (cp.host) {
        return TSvrRef(new CDBServer(kEmptyStr, cp.host, cp.port, cp.time));
    }

    _TRACE("Remembering: service " << service << " is dead.");
    m_LBEmptyMap[service] = make_pair(fSERV_IncludeInactive,
                                      cur_time + m_EmptyTTL);
    return TSvrRef();
}

void
CDBLB_ServiceMapper::GetServersList(const string& service, list<string>* serv_list) const
{
    serv_list->clear();
    TSvrRef svr = x_GetLiteral(service);
    if (svr.NotEmpty()) {
        string s;
        if (svr->GetHost() == 0) {
            s = service; // or svr->GetName(), equivalently
        } else {
            s = CSocketAPI::HostPortToString(svr->GetHost(), svr->GetPort());
        }
        serv_list->push_back(s);
        return;
    }
    SConnNetInfo* net_info = ConnNetInfo_Create(service.c_str());
    SERV_ITER srv_it = SERV_Open(service.c_str(),
                                 fSERV_Standalone
                                 | (TSERV_Type) fSERV_IncludeDown,
                                 0, net_info);
    ConnNetInfo_Destroy(net_info);
    const SSERV_Info* sinfo;
    while ((sinfo = SERV_GetNextInfo(srv_it)) != NULL) {
        if (sinfo->time > 0  &&  sinfo->time != NCBI_TIME_INFINITE) {
            string server_name(CSocketAPI::ntoa(sinfo->host));
            if (sinfo->port != 0) {
                server_name.append(1, ':');
                server_name.append(NStr::UIntToString(sinfo->port));
            }
            serv_list->push_back(server_name);
        }
    }
    SERV_Close(srv_it);
}

struct SRawOption : public CObject
{
    typedef CDBServerOption::TState TState;
    
    SRawOption(const SSERV_Info& si, TState st, const SLBSM_HostLoad& l)
        : sinfo(SERV_CopyInfoEx(&si, SERV_NameOfInfo(&si))), state(st), load(l)
        { }

    ~SRawOption()
        { free(sinfo); }
    
    SSERV_Info*    sinfo;
    TState         state;
    SLBSM_HostLoad load;
};

void
CDBLB_ServiceMapper::GetServerOptions(const string& service, TOptions* options)
{
    auto svr = x_GetLiteral(service);
    if (svr.NotEmpty()) {
        options->clear();
        options->push_back(svr);
        return;
    }
    CFastMutexGuard mg(m_Mtx);
    options->clear();
    time_t cur_time = time(NULL);
    if (x_IsEmpty(service, fSERV_Promiscuous, cur_time)) {
        return;
    }

    const auto& preferred_svr = m_PreferenceMap.find(service);
    if (preferred_svr != m_PreferenceMap.end()
        &&  preferred_svr->second.first >= 100.0
        &&  preferred_svr->second.second.NotEmpty()) {
        TSvrRef server     = x_GetServer(service),
                requested  = preferred_svr->second.second;
        if (server.NotEmpty()
            &&  (requested->GetHost() == server->GetHost()
                 ||  requested->GetHost() == 0)
            &&  (requested->GetPort() == server->GetPort()
                 ||  requested->GetPort() == 0)) {
            _TRACE(service << ": latching to " << server->GetName() << '@'
                   << CSocketAPI::HostPortToString(server->GetHost(),
                                                   server->GetPort()));
            options->emplace_back
                (new CDBServerOption(server->GetName(), server->GetHost(),
                                     server->GetPort(), 1.0,
                                     CDBServerOption::fState_Normal,
                                     (TNCBI_Time) server->GetExpireTime()));
            return;
        }
    }

    TSERV_Type     type      = ((TSERV_Type) fSERV_Standalone
                                | fSERV_ReverseDns | fSERV_Promiscuous);
    SConnNetInfo*  net_info  = ConnNetInfo_Create(service.c_str());
    SERV_ITER      srv_it    = SERV_Open(service.c_str(), type, 0, net_info);
    ConnNetInfo_Destroy(net_info);
    // Getting final ratings involves non-trivial postprocessing based on
    // logic from s_GetNextInfo from ncbi_lbsmd.c and s_Preference and
    // LB_Select from ncbi_lb.c.  There would be no benefit to using
    // SERV_OpenP here; we need to account for any preference explicitly.
    const SSERV_Info*            sinfo;
    HOST_INFO                    hinfo;
    SLBSM_HostLoad               load            = { 0.0 };
    double                       total           = 0.0;
    TOptions::iterator           most_preferred  = options->end();
    TExcludeMap::const_iterator  exclusions      = m_ExcludeMap.find(service);
    size_t                       n               = 0;
    size_t                       missing_hinfo   = 0;
    list<CRef<SRawOption>>       raw_options;
    while ((sinfo = SERV_GetNextInfoEx(srv_it, &hinfo)) != NULL) {
        CDBServerOption::TState state = CDBServerOption::fState_Normal;
        if (sinfo->time <= 0  ||  sinfo->time == NCBI_TIME_INFINITE
            ||  sinfo->rate < SERV_MINIMAL_RATE) {
            state |= CDBServerOption::fState_Penalized;
        }
        if (exclusions != m_ExcludeMap.end()) {
            for (const auto& it : exclusions->second) {
                if (it->GetHost() == sinfo->host
                    &&  (it->GetPort() == sinfo->port
                         ||  it->GetPort() == 0)) {
                    state |= CDBServerOption::fState_Excluded;
                }
            }
        }
        if (hinfo == NULL) {
            load.status = load.statusBLAST = 0.0;
            missing_hinfo = true;
        } else {
            HINFO_Status(hinfo, &load.status);
            free(hinfo);
        }
        raw_options.emplace_back(new SRawOption(*sinfo, state, load));
        ++n;
    }
    if (missing_hinfo > 0) {
        SLBSM_HostLoad default_load = { 0.0 };
        if (missing_hinfo == n) {
            // Arbitrary, just needs to be non-zero.
            default_load.status = default_load.statusBLAST = 1.0;
        } else {
            // Use the other entries' average status values.
            double scale = 1.0 / (n - missing_hinfo);
            for (const auto& it : raw_options) {
                default_load.status      += it->load.status      * scale;
                default_load.statusBLAST += it->load.statusBLAST * scale;
            }
        }
        for (auto& it : raw_options) {
            if (it->load.status == 0.0) {
                it->load = default_load;
            }
        }
    }
    for (const auto& it : raw_options) {
        string                  name  = SERV_NameOfInfo(it->sinfo);
        CDBServerOption::TState state =  it->state;
        const SLBSM_HostLoad*   pload = &it->load;
        sinfo                         =  it->sinfo;
        double ranking = LBSM_CalculateStatus(sinfo->rate,
                                              0.0 /* fine, unavailable here */,
                                              (ESERV_Algo) sinfo->algo, pload);
        bool update_most_preferred = false;
        if (ranking > 0.0  &&  state == CDBServerOption::fState_Normal
            &&  preferred_svr != m_PreferenceMap.end()
            &&  preferred_svr->second.first > 0.0
            &&  preferred_svr->second.second.NotEmpty()
            &&  (preferred_svr->second.second->GetHost() == sinfo->host
                 ||  preferred_svr->second.second->GetHost() == 0)
            &&  (preferred_svr->second.second->GetPort() == sinfo->port
                 ||  preferred_svr->second.second->GetPort() == 0)) {
            ranking *= 1.1; // initial bonus, more adjustments next pass
            if (most_preferred == options->end()
                ||  (*most_preferred)->GetRanking() < ranking) {
                update_most_preferred = true;
            }
        }
        total += ranking;
        _TRACE(service << ": " << name << '@'
               << CSocketAPI::HostPortToString(sinfo->host, sinfo->port)
               << " -- initial ranking " << ranking << ", state " << state
               << ", expires " << CTime(sinfo->time));
        options->emplace_back
            (new CDBServerOption(name, sinfo->host, sinfo->port, ranking,
                                 state, sinfo->time));
        if (update_most_preferred) {
            TOptions::reverse_iterator ri = options->rbegin();
            most_preferred = (++ri).base();
        }
    }
    SERV_Close(srv_it);
    if (n >= 2  &&  most_preferred != options->end()) {
        double  pref    = preferred_svr->second.first * 0.01,
                access  = (*most_preferred)->GetRanking(),
                gap     = access / total,
                p0;
        if (gap >= pref) {
            p0 = gap;
        } else {
            double spread = 14.0 / (double(n) + 12.0);
            if (gap >= spread / double(n)) {
                p0 = pref;
            } else {
                p0 = 2.0 / spread * gap * pref;
            }
        }
        // No need to preserve the total here, so it's simplest just to
        // adjust a single ranking (albeit with special-case logic to
        // avoid the possibility of dividing by zero).
        double new_ranking;
        if (p0 == 1.0) {
            new_ranking = numeric_limits<double>::max();
        } else {
            new_ranking = (total - access) * p0 / (1.0 - p0);
        }
        const string&  name  = (*most_preferred)->GetName();
        Uint4          host  = (*most_preferred)->GetHost();
        Uint2          port  = (*most_preferred)->GetPort();
        _TRACE(service << ": " << name << '@'
               << CSocketAPI::HostPortToString(host, port)
               << " -- adjusting ranking to " << new_ranking);
        most_preferred->Reset
            (new CDBServerOption(name, host, port, new_ranking,
                                 (*most_preferred)->GetState(),
                                 (TNCBI_Time) (*most_preferred)->GetExpireTime()));
        
    }
    if (options->empty()) {
        _TRACE("Remembering: service " << service << " is dead.");
        m_LBEmptyMap[service] = make_pair(fSERV_Promiscuous,
                                          cur_time + m_EmptyTTL);
    }
}

void
CDBLB_ServiceMapper::SetPreference(const string&  service,
                                   const TSvrRef& preferred_server,
                                   double         preference)
{
    CFastMutexGuard mg(m_Mtx);

    m_PreferenceMap[service] = make_pair(preference, preferred_server);
}


bool CDBLB_ServiceMapper::RecordServer(I_ConnectionExtra& extra) const
{
    I_ConnectionExtra::TSockHandle handle;

    try {
        handle = extra.GetLowLevelHandle();
    } catch (CException&) {
        return false;
    }
    if ( !handle ) {
        return false;
    }

    SOCK sock = NULL;
    try {
        SOCK_CreateOnTopEx(&handle, sizeof(handle), &sock, NULL, 0,
                           fSOCK_KeepOnClose);
        unsigned short port = SOCK_GetRemotePort(sock, eNH_HostByteOrder);
        unsigned int   host;
        SOCK_GetPeerAddress(sock, &host, NULL, eNH_NetworkByteOrder);
        // Could perhaps try to look name up, but this should suffice.
        string name = CSocketAPI::HostPortToString(host, port);
        CDBServer server(name, host, port);
        x_RecordServer(extra, server);
        SOCK_Close(sock);
        return true;
    } catch (exception&) {
        SOCK_Close(sock);
        // Log it?
        return false;
    } catch (...) {
        SOCK_Close(sock);
        throw;
    }
}


IDBServiceMapper*
CDBLB_ServiceMapper::Factory(const IRegistry* registry)
{
    return new CDBLB_ServiceMapper(registry);
}

///////////////////////////////////////////////////////////////////////////////
string
CDBServiceMapperTraits<CDBLB_ServiceMapper>::GetName(void)
{
    return "DBLB_NAME_MAPPER";
}


END_NCBI_SCOPE
