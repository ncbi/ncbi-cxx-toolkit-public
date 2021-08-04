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
* Author:  Sergey Sikorskiy
*
*/

#include <ncbi_pch.hpp>

#include <dbapi/driver/dbapi_svc_mapper.hpp>
#include <dbapi/driver/exception.hpp>
#include <corelib/ncbiapp.hpp>
// #include <connect/ncbi_socket.h>
#include <algorithm>


BEGIN_NCBI_SCOPE


//////////////////////////////////////////////////////////////////////////////
static
CRef<CDBServerOption>
make_server(const CTempString& specification, double& preference)
{
    CTempString server;
    // string host;
    Uint4 host = 0;
    Uint2 port = 0;
    string::size_type pos = 0;

    pos = specification.find_first_of("@(", pos);
    if (pos != string::npos) {
        server = specification.substr(0, pos);

        if (specification[pos] == '@') {
            // string::size_type old_pos = pos + 1;
            pos = specification.find_first_of(":(", pos + 1);
            if (pos != string::npos) {
                // string host_str = specification.substr(old_pos, pos - old_pos);
                // Ignore host in order to avoid dependebcy on libconnect.
                // SOCK_StringToHostPort(specification.c_str() + old_pos, &host, &port);
                if (specification[pos] == ':') {
                    port = NStr::StringToUInt(specification.substr(pos + 1),
                                              NStr::fAllowLeadingSpaces |
                                              NStr::fAllowTrailingSymbols |
                                              NStr::fConvErr_NoThrow);
                    pos = specification.find("(", pos + 1);
                    if (pos != string::npos) {
                        // preference = NStr::StringToDouble(
                        preference = NStr::StringToUInt(
                            specification.substr(pos + 1),
                            NStr::fAllowLeadingSpaces |
                            NStr::fAllowTrailingSymbols |
                            NStr::fConvErr_NoThrow);
                    }
                } else {
                    // preference = NStr::StringToDouble(
                    preference = NStr::StringToUInt(
                        specification.substr(pos + 1),
                        NStr::fAllowLeadingSpaces |
                        NStr::fAllowTrailingSymbols |
                        NStr::fConvErr_NoThrow);
                }
            } else {
                // host = specification.substr(old_pos);
                // Ignore host in order to avoid dependebcy on libconnect.
                // SOCK_StringToHostPort(specification.c_str() + old_pos, &host, &port);
            }
        } else {
            // preference = NStr::StringToDouble(
            preference = NStr::StringToUInt(
                specification.substr(pos + 1),
                NStr::fAllowLeadingSpaces |
                NStr::fAllowTrailingSymbols |
                NStr::fConvErr_NoThrow);
        }
    } else {
        server = specification;
    }

    if (server.empty() && host == 0) {
        DATABASE_DRIVER_ERROR("Either server name or host name expected.",
                              110100 );
    }

    return CRef<CDBServerOption>
        (new CDBServerOption(server, host, port, preference));
}



//////////////////////////////////////////////////////////////////////////////
CDBDefaultServiceMapper::CDBDefaultServiceMapper(void)
{
}

CDBDefaultServiceMapper::~CDBDefaultServiceMapper(void)
{
}

string CDBDefaultServiceMapper::GetName(void) const
{
    return CDBServiceMapperTraits<CDBDefaultServiceMapper>::GetName();
}

void
CDBDefaultServiceMapper::Configure(const IRegistry*)
{
    // Do nothing.
}

TSvrRef
CDBDefaultServiceMapper::GetServer(const string& service)
{
    if (m_ExcludeMap.find(service) != m_ExcludeMap.end()) {
        return TSvrRef();
    }

    return TSvrRef(new CDBServer(service));
}

void
CDBDefaultServiceMapper::SetPreference(const string&,
                                       const TSvrRef&,
                                       double)
{
    // Do nothing.
}


//////////////////////////////////////////////////////////////////////////////
CDBServiceMapperCoR::CDBServiceMapperCoR(void)
{
}

CDBServiceMapperCoR::~CDBServiceMapperCoR(void)
{
}

string CDBServiceMapperCoR::GetName(void) const
{
    if (Top().NotEmpty()) {
        return Top()->GetName();
    } else {
        return CDBServiceMapperTraits<CDBServiceMapperCoR>::GetName();
    }
}

void
CDBServiceMapperCoR::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    ConfigureFromRegistry(registry);
}

TSvrRef
CDBServiceMapperCoR::GetServer(const string& service)
{
    CFastMutexGuard mg(m_Mtx);

    TSvrRef server;
    TDelegates::reverse_iterator dg_it = m_Delegates.rbegin();
    TDelegates::reverse_iterator dg_end = m_Delegates.rend();

    for (; server.Empty() && dg_it != dg_end; ++dg_it) {
        server = (*dg_it)->GetServer(service);
    }

    return server;
}

void
CDBServiceMapperCoR::Exclude(const string&  service,
                             const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx);

    NON_CONST_ITERATE(TDelegates, dg_it, m_Delegates) {
        (*dg_it)->Exclude(service, server);
    }
}

void
CDBServiceMapperCoR::CleanExcluded(const string&  service)
{
    CFastMutexGuard mg(m_Mtx);

    NON_CONST_ITERATE(TDelegates, dg_it, m_Delegates) {
        (*dg_it)->CleanExcluded(service);
    }
}

bool CDBServiceMapperCoR::HasExclusions(const string& service) const
{
    ITERATE (TDelegates, dg_it, m_Delegates) {
        if ((*dg_it)->HasExclusions(service)) {
            return true;
        }
    }
    return false;
}

void
CDBServiceMapperCoR::SetPreference(const string&  service,
                                   const TSvrRef& preferred_server,
                                   double preference)
{
    CFastMutexGuard mg(m_Mtx);

    NON_CONST_ITERATE(TDelegates, dg_it, m_Delegates) {
        (*dg_it)->SetPreference(service, preferred_server, preference);
    }
}

void
CDBServiceMapperCoR::GetServersList(const string& service, list<string>* serv_list) const
{
    CFastMutexGuard mg(m_Mtx);

    TDelegates::const_reverse_iterator dg_it = m_Delegates.rbegin();
    TDelegates::const_reverse_iterator dg_end = m_Delegates.rend();
    for (; serv_list->empty() && dg_it != dg_end; ++dg_it) {
        (*dg_it)->GetServersList(service, serv_list);
    }
}

void
CDBServiceMapperCoR::GetServerOptions(const string& service, TOptions* options)
{
    CFastMutexGuard mg(m_Mtx);

    TDelegates::reverse_iterator dg_it = m_Delegates.rbegin();
    TDelegates::reverse_iterator dg_end = m_Delegates.rend();
    for (; options->empty() && dg_it != dg_end; ++dg_it) {
        (*dg_it)->GetServerOptions(service, options);
    }
}


bool
CDBServiceMapperCoR::RecordServer(I_ConnectionExtra& extra) const
{
    CFastMutexGuard mg(m_Mtx);
    REVERSE_ITERATE (TDelegates, dg_it, m_Delegates) {
        if ((*dg_it)->RecordServer(extra)) {
            return true;
        }
    }
    return false;
}


void
CDBServiceMapperCoR::ConfigureFromRegistry(const IRegistry* registry)
{
    NON_CONST_ITERATE (TDelegates, dg_it, m_Delegates) {
        (*dg_it)->Configure(registry);
    }
}

void
CDBServiceMapperCoR::Push(const CRef<IDBServiceMapper>& mapper)
{
    if (mapper.NotNull()) {
        CFastMutexGuard mg(m_Mtx);

        m_Delegates.push_back(mapper);
    }
}

void
CDBServiceMapperCoR::Pop(void)
{
    CFastMutexGuard mg(m_Mtx);

    m_Delegates.pop_back();
}

CRef<IDBServiceMapper>
CDBServiceMapperCoR::Top(void) const
{
    CFastMutexGuard mg(m_Mtx);

    return m_Delegates.back();
}

bool
CDBServiceMapperCoR::Empty(void) const
{
    CFastMutexGuard mg(m_Mtx);

    return m_Delegates.empty();
}

//////////////////////////////////////////////////////////////////////////////
CDBUDRandomMapper::CDBUDRandomMapper(const IRegistry* registry)
{
    random_device rdev;
    m_RandomEngine.seed(rdev());
    ConfigureFromRegistry(registry);
}

CDBUDRandomMapper::~CDBUDRandomMapper(void)
{
}

string CDBUDRandomMapper::GetName(void) const
{
    return CDBServiceMapperTraits<CDBUDRandomMapper>::GetName();
}

void
CDBUDRandomMapper::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    ConfigureFromRegistry(registry);
}

void
CDBUDRandomMapper::ConfigureFromRegistry(const IRegistry* registry)
{
    const string section_name
        (CDBServiceMapperTraits<CDBUDRandomMapper>::GetName());
    list<string> entries;

    // Get current registry ...
    if (!registry && CNcbiApplication::Instance()) {
        registry = &CNcbiApplication::Instance()->GetConfig();
    }

    if (registry) {
        // Erase previous data ...
        m_LBNameMap.clear();
        m_ServerMap.clear();
        m_FavoritesMap.clear();
        m_PreferenceMap.clear();

        registry->EnumerateEntries(section_name, &entries);
        ITERATE(list<string>, cit, entries) {
            vector<string> server_name;
            string service_name = *cit;

            NStr::Split(registry->GetString(section_name,
                                            service_name,
                                            service_name),
                        " ,;",
                        server_name,
                        NStr::fSplit_MergeDelimiters);

            // Replace with new data ...
            if (!server_name.empty()) {
                TOptions& server_list = m_ServerMap[service_name];

                ITERATE(vector<string>, sn_it, server_name) {
                    double tmp_preference = 0;

                    // Parse server preferences.
                    auto cur_server = make_server(*sn_it, tmp_preference);
                    if (tmp_preference < 0) {
                        cur_server->m_Ranking = 0;
                    } else if (tmp_preference >= 100) {
                        cur_server->m_Ranking = 100;
                        m_FavoritesMap[service_name].push_back(cur_server);
                    }

                    server_list.push_back(cur_server);

                }
                x_RecalculatePreferences(service_name);
            }
        }
    }
}

TSvrRef
CDBUDRandomMapper::GetServer(const string& service)
{
    CFastMutexGuard mg(m_Mtx);

    if (m_LBNameMap.find(service) != m_LBNameMap.end() &&
        m_LBNameMap[service] == false) {
        // We've tried this service already. It is not served by load
        // balancer. There is no reason to try it again.
        return TSvrRef();
    }

    const auto& it = m_PreferenceMap.find(service);
    if (it != m_PreferenceMap.end()) {
        m_LBNameMap[service] = true;
        return it->second.servers[(*it->second.distribution)(m_RandomEngine)];
    }

    m_LBNameMap[service] = false;
    return TSvrRef();
}

void
CDBUDRandomMapper::Add(const string&    service,
                       const TSvrRef&   server,
                       double           preference)
{
    _ASSERT(false);

    if (service.empty() || server.Empty()) {
        return;
    }

    TOptions& server_list = m_ServerMap[service];
    CRef<CDBServerOption> option
        (new CDBServerOption(server->GetName(), server->GetHost(),
                             server->GetPort(), preference));

    if (preference < 0) {
        option->m_Ranking = 0;
    } else if (preference >= 100) {
        option->m_Ranking = 100;
        m_FavoritesMap[service].push_back(option);
    }

    server_list.push_back(option);

    x_RecalculatePreferences(service);
}

void
CDBUDRandomMapper::Exclude(const string& service, const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx);
    auto svc = m_ServerMap.find(service);
    if (svc != m_ServerMap.end()) {
        for (auto svr : svc->second) {
            if (svr == server  ||  *svr == *server) {
                svr->m_State |= CDBServerOption::fState_Excluded;
            }
        }
        x_RecalculatePreferences(service);
    }
}

void
CDBUDRandomMapper::CleanExcluded(const string& service)
{
    CFastMutexGuard mg(m_Mtx);
    auto svc = m_ServerMap.find(service);
    if (svc != m_ServerMap.end()) {
        for (auto svr : svc->second) {
            svr->m_State &= ~CDBServerOption::fState_Excluded;
        }
        x_RecalculatePreferences(service);
    }
}

bool
CDBUDRandomMapper::HasExclusions(const string& service) const
{
    CFastMutexGuard mg(m_Mtx);
    auto svc   = m_ServerMap.find(service);
    auto prefs = m_PreferenceMap.find(service);
    if (svc == m_ServerMap.end()) {
        return false;
    } else if (prefs == m_PreferenceMap.end()) {
        return true;
    } else if (svc->second.size() == prefs->second.servers.size()) {
        return false;
    } else if (prefs->second.servers.size() != 1) {
        return true;
    } else {
        return prefs->second.servers[0]->GetRanking() < 100.0;
    }
}

void
CDBUDRandomMapper::SetPreference(const string&  service,
                                 const TSvrRef& preferred_server,
                                 double         preference)
{
    CFastMutexGuard mg(m_Mtx);

    auto svc = m_ServerMap.find(service);
    if (svc != m_ServerMap.end()) {
        for (auto svr : svc->second) {
            if (svr == preferred_server  ||  *svr == *preferred_server) {
                double old_preference = svr->GetRanking();
                svr->m_Ranking = max(0.0, min(100.0, preference));
                if (preference >= 100.0  &&  old_preference < 100.0) {
                    m_FavoritesMap[service].push_back(svr);
                } else if (preference < 100.0  &&  old_preference >= 100.0) {
                    auto& favorites = m_FavoritesMap[service];
                    favorites.erase(find(favorites.begin(), favorites.end(),
                                         svr));
                    if (favorites.empty()) {
                        m_FavoritesMap.erase(service);
                    }
                }
            }
        }
        x_RecalculatePreferences(service);
    }
}

void
CDBUDRandomMapper::GetServerOptions(const string& service, TOptions* options)
{
    CFastMutexGuard mg(m_Mtx);
    auto it = m_ServerMap.find(service);
    if (it == m_ServerMap.end()) {
        options->clear();
    } else {
        *options = it->second;
    }
}

IDBServiceMapper*
CDBUDRandomMapper::Factory(const IRegistry* registry)
{
    return new CDBUDRandomMapper(registry);
}

void
CDBUDRandomMapper::x_RecalculatePreferences(const string& service)
{
    SPreferences& service_preferences = m_PreferenceMap[service];
    service_preferences.servers.clear();
    {{
        TServiceMap::const_iterator favs = m_FavoritesMap.find(service);
        if (favs != m_FavoritesMap.end()) {
            REVERSE_ITERATE(TOptions, it, favs->second) {
                if ( !(*it)->IsExcluded() ) {
                    service_preferences.servers.push_back(*it);
                    service_preferences.distribution.reset
                        (new discrete_distribution<>({100.0}));
                    return;
                }
            }
        }
    }}
    vector<double> weights;
    bool all_zero = true;
    for (const auto & it : m_ServerMap[service]) {
        if ( !it->IsExcluded() ) {
            double weight = it->GetRanking();
            _ASSERT(weight < 100.0);
            if (weight > 0.0) {
                all_zero = false;
            }
        }
    }
    if (all_zero) {
        for (auto & w : weights) {
            w = 1.0;
        }
    }
    service_preferences.distribution.reset
        (new discrete_distribution<>(weights.begin(), weights.end()));
}


//////////////////////////////////////////////////////////////////////////////
CDBUDPriorityMapper::CDBUDPriorityMapper(const IRegistry* registry)
{
    ConfigureFromRegistry(registry);
}

CDBUDPriorityMapper::~CDBUDPriorityMapper(void)
{
}

string CDBUDPriorityMapper::GetName(void) const
{
    return CDBServiceMapperTraits<CDBUDPriorityMapper>::GetName();
}

void
CDBUDPriorityMapper::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    ConfigureFromRegistry(registry);
}

void
CDBUDPriorityMapper::ConfigureFromRegistry(const IRegistry* registry)
{
    const string section_name
        (CDBServiceMapperTraits<CDBUDPriorityMapper>::GetName());
    list<string> entries;

    // Get current registry ...
    if (!registry && CNcbiApplication::Instance()) {
        registry = &CNcbiApplication::Instance()->GetConfig();
    }

    if (registry) {
        // Erase previous data ...
        m_ServerMap.clear();
        m_ServiceUsageMap.clear();

        registry->EnumerateEntries(section_name, &entries);

        ITERATE(list<string>, cit, entries) {
            vector<string> server_name;
            string service_name = *cit;

            NStr::Split(registry->GetString(section_name,
                                            service_name,
                                            service_name),
                        " ,;",
                        server_name,
                        NStr::fSplit_MergeDelimiters);

            // Replace with new data ...
            if (!server_name.empty()) {
//                 TSvrMap& server_list = m_ServerMap[service_name];
//                 TServerUsageMap& usage_map = m_ServiceUsageMap[service_name];

                ITERATE(vector<string>, sn_it, server_name) {
                    double tmp_preference = 0;

                    // Parse server preferences.
                    TSvrRef cur_server = make_server(*sn_it, tmp_preference);

                    // Replaced with Add()
//                     if (tmp_preference < 0) {
//                         tmp_preference = 0;
//                     } else if (tmp_preference > 100) {
//                         tmp_preference = 100;
//                     }
//
//                     server_list.insert(
//                         TSvrMap::value_type(cur_server, tmp_preference));
//                     usage_map.insert(TServerUsageMap::value_type(
//                         100 - tmp_preference,
//                         cur_server));

                    Add(service_name, cur_server, tmp_preference);
                }
            }
        }
    }
}

TSvrRef
CDBUDPriorityMapper::GetServer(const string& service)
{
    CFastMutexGuard mg(m_Mtx);

    if (m_LBNameMap.find(service) != m_LBNameMap.end() &&
        m_LBNameMap[service] == false) {
        // We've tried this service already. It is not served by load
        // balancer. There is no reason to try it again.
        return TSvrRef();
    }

    TServerUsageMap& usage_map = m_ServiceUsageMap[service];
    TSvrMap& server_map = m_ServerMap[service];

    if (!server_map.empty() && !usage_map.empty()) {
        TServerUsageMap::iterator su_it = usage_map.begin();
        double new_preference = su_it->first;
        TSvrRef cur_server = su_it->second;

        // Recalculate preferences ...
        TSvrMap::const_iterator pr_it = server_map.find(cur_server);

        if (pr_it != server_map.end()) {
            new_preference +=  100 - pr_it->second;
        } else {
            new_preference +=  100;
        }

        // Reset usage map ...
        usage_map.erase(su_it);
        usage_map.insert(TServerUsageMap::value_type(new_preference,
                                                     cur_server));

        m_LBNameMap[service] = true;
        return cur_server;
    }

    m_LBNameMap[service] = false;
    return TSvrRef();
}

void
CDBUDPriorityMapper::Exclude(const string& service,
                             const TSvrRef& server)
{
    IDBServiceMapper::Exclude(service, server);

    CFastMutexGuard mg(m_Mtx);

    TServerUsageMap& usage_map = m_ServiceUsageMap[service];

    // Remove elements ...
    for (TServerUsageMap::iterator it = usage_map.begin();
         it != usage_map.end();) {

        if (it->second == server) {
            usage_map.erase(it++);
        }
        else {
            ++it;
        }
    }
}

void
CDBUDPriorityMapper::CleanExcluded(const string& service)
{
    IDBServiceMapper::CleanExcluded(service);

    CFastMutexGuard mg(m_Mtx);
    m_ServiceUsageMap[service] = m_OrigServiceUsageMap[service];
}

void
CDBUDPriorityMapper::SetPreference(const string&  service,
                                   const TSvrRef& preferred_server,
                                   double         preference)
{
    CFastMutexGuard mg(m_Mtx);

    TSvrMap& server_map = m_ServerMap[service];
    TSvrMap::iterator pr_it = server_map.find(preferred_server);

    if (preference < 0) {
        preference = 0;
    } else if (preference > 100) {
        preference = 100;
    }

    if (pr_it != server_map.end()) {
        pr_it->second = preference;
    }
}


void
CDBUDPriorityMapper::Add(const string&    service,
                         const TSvrRef&   server,
                         double           preference)
{
    TSvrMap& server_list = m_ServerMap[service];
    TServerUsageMap& usage_map = m_ServiceUsageMap[service];
    TServerUsageMap& usage_map2 = m_OrigServiceUsageMap[service];

    if (preference < 0) {
        preference = 0;
    } else if (preference > 100) {
        preference = 100;
    }

    server_list.insert(
        TSvrMap::value_type(server, preference)
        );
    TServerUsageMap::value_type usage(100 - preference, server);
    usage_map.insert(usage);
    usage_map2.insert(usage);
}


IDBServiceMapper*
CDBUDPriorityMapper::Factory(const IRegistry* registry)
{
    return new CDBUDPriorityMapper(registry);
}


//////////////////////////////////////////////////////////////////////////////
CDBUniversalMapper::CDBUniversalMapper(const IRegistry* registry,
                                       const TMapperConf& ext_mapper)
{
    if (!ext_mapper.first.empty() && ext_mapper.second != NULL) {
        m_ExtMapperConf = ext_mapper;
    }

    this->ConfigureFromRegistry(registry);
    CDBServiceMapperCoR::ConfigureFromRegistry(registry);
}

CDBUniversalMapper::~CDBUniversalMapper(void)
{
}

string CDBUniversalMapper::GetName(void) const
{
    if (Top().NotEmpty()) {
        return Top()->GetName();
    } else {
        return CDBServiceMapperTraits<CDBUniversalMapper>::GetName();
    }
}

void
CDBUniversalMapper::Configure(const IRegistry* registry)
{
    CFastMutexGuard mg(m_Mtx);

    this->ConfigureFromRegistry(registry);
    CDBServiceMapperCoR::ConfigureFromRegistry(registry);
}

void
CDBUniversalMapper::ConfigureFromRegistry(const IRegistry* registry)
{
    vector<string> service_name;
    const string section_name
        (CDBServiceMapperTraits<CDBUniversalMapper>::GetName());
    const string def_mapper_name =
        (m_ExtMapperConf.second ? m_ExtMapperConf.first :
         CDBServiceMapperTraits<CDBUDRandomMapper>::GetName());

    // Get current registry ...
    if (!registry && CNcbiApplication::Instance()) {
        registry = &CNcbiApplication::Instance()->GetConfig();
    }

    if (registry) {

        NStr::Split(registry->GetString
                    (section_name, "MAPPERS",
                        def_mapper_name),
                    " ,;",
                    service_name,
                    NStr::fSplit_MergeDelimiters);

    } else {
        service_name.push_back(def_mapper_name);
    }

    ITERATE(vector<string>, it, service_name) {
        IDBServiceMapper* mapper = NULL;
        string mapper_name = *it;

        if (NStr::CompareNocase
            (mapper_name,
            CDBServiceMapperTraits<CDBDefaultServiceMapper>::GetName()) ==
            0) {
            mapper = new CDBDefaultServiceMapper();
        } else if (NStr::CompareNocase
                (mapper_name,
                    CDBServiceMapperTraits<CDBUDRandomMapper>::GetName())
                == 0) {
            mapper = new CDBUDRandomMapper(registry);
        } else if (NStr::CompareNocase
                (mapper_name,
                    CDBServiceMapperTraits<CDBUDPriorityMapper>::GetName())
                == 0) {
            mapper = new CDBUDPriorityMapper(registry);
        } else if (m_ExtMapperConf.second && NStr::CompareNocase
            (mapper_name, m_ExtMapperConf.first) == 0) {
            mapper = (*m_ExtMapperConf.second)(registry);
        }

        Push(CRef<IDBServiceMapper>(mapper));
    }
}


//////////////////////////////////////////////////////////////////////////////
string
CDBServiceMapperTraits<CDBDefaultServiceMapper>::GetName(void)
{
    return "DEFAULT_NAME_MAPPER";
}

string
CDBServiceMapperTraits<CDBServiceMapperCoR>::GetName(void)
{
    return "COR_NAME_MAPPER";
}

string
CDBServiceMapperTraits<CDBUDRandomMapper>::GetName(void)
{
    return "USER_DEFINED_RANDOM_DBNAME_MAPPER";
}

string
CDBServiceMapperTraits<CDBUDPriorityMapper>::GetName(void)
{
    return "USER_DEFINED_PRIORITY_DBNAME_MAPPER";
}

string
CDBServiceMapperTraits<CDBUniversalMapper>::GetName(void)
{
    return "UNIVERSAL_NAME_MAPPER";
}

END_NCBI_SCOPE

