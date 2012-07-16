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
TSvrRef
make_server(const string& specification, double& preference)
{
    vector<string> server_attr;
    string server;
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
                    port = NStr::StringToUInt(specification.c_str() + pos + 1,
                                              NStr::fAllowLeadingSpaces |
                                              NStr::fAllowTrailingSymbols |
                                              NStr::fConvErr_NoThrow);
                    pos = specification.find("(", pos + 1);
                    if (pos != string::npos) {
                        // preference = NStr::StringToDouble(
                        preference = NStr::StringToUInt(
                            specification.c_str() + pos + 1,
                            NStr::fAllowLeadingSpaces |
                            NStr::fAllowTrailingSymbols |
                            NStr::fConvErr_NoThrow);
                    }
                } else {
                    // preference = NStr::StringToDouble(
                    preference = NStr::StringToUInt(
                        specification.c_str() + pos + 1,
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
                specification.c_str() + pos + 1,
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

    return TSvrRef(new CDBServer(server, host, port));
}



//////////////////////////////////////////////////////////////////////////////
CDBDefaultServiceMapper::CDBDefaultServiceMapper(void)
{
}

CDBDefaultServiceMapper::~CDBDefaultServiceMapper(void)
{
}

void
CDBDefaultServiceMapper::Configure(const IRegistry*)
{
    // Do nothing.
}

TSvrRef
CDBDefaultServiceMapper::GetServer(const string& service)
{
    if (m_SrvSet.find(service) != m_SrvSet.end()) {
        return TSvrRef();
    }

    return TSvrRef(new CDBServer(service));
}

void
CDBDefaultServiceMapper::Exclude(const string&  service,
                                 const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx);

    m_SrvSet.insert(service);
}

void
CDBDefaultServiceMapper::CleanExcluded(const string& service)
{
    CFastMutexGuard mg(m_Mtx);

    m_SrvSet.erase(service);
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
    ConfigureFromRegistry(registry);
}

CDBUDRandomMapper::~CDBUDRandomMapper(void)
{
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
        m_ServerMap.clear();
        m_PreferenceMap.clear();

        registry->EnumerateEntries(section_name, &entries);
        ITERATE(list<string>, cit, entries) {
            vector<string> server_name;
            string service_name = *cit;

            NStr::Tokenize(registry->GetString(section_name,
                                               service_name,
                                               service_name),
                           " ,;",
                           server_name);

            // Replace with new data ...
            if (!server_name.empty()) {
                TSvrMap& server_list = m_ServerMap[service_name];
                TSvrMap& service_preferences = m_PreferenceMap[service_name];

                // Set equal preferences for all servers.
                double curr_preference =
                    static_cast<double>(100 / server_name.size());
                bool non_default_preferences = false;

                ITERATE(vector<string>, sn_it, server_name) {
                    double tmp_preference = 0;

                    // Parse server preferences.
                    TSvrRef cur_server = make_server(*sn_it, tmp_preference);

                    // Should be raplaced with Add() one day ...
                    {
                        if (tmp_preference > 0.01) {
                            non_default_preferences = true;
                        }

                        server_list.insert(
                            TSvrMap::value_type(cur_server, tmp_preference));
                        service_preferences.insert(
                            TSvrMap::value_type(cur_server, curr_preference));
                    }

//                     Add(service_name, cur_server, tmp_preference);
                }

                // Should become a part of Add() ...
                if (non_default_preferences) {
                    ITERATE(TSvrMap, sl_it, server_list) {
                        if (sl_it->second > 0) {
                            SetServerPreference(service_name,
                                                sl_it->second,
                                                sl_it->first);
                        }
                    }
                }
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

    const TSvrMap& svr_map = m_PreferenceMap[service];
    if (!svr_map.empty()) {
        srand((unsigned int)time(NULL));
        double cur_pref = rand() / (RAND_MAX / 100);
        double pref = 0;

        ITERATE(TSvrMap, sr_it, svr_map) {
            pref += sr_it->second;
            if (pref >= cur_pref) {
                m_LBNameMap[service] = true;
                return sr_it->first;
            }
        }
    }

    m_LBNameMap[service] = false;
    return TSvrRef();
}

void
CDBUDRandomMapper::ScalePreference(const string& service, double coeff)
{
    TSvrMap& svr_map = m_PreferenceMap[service];

    NON_CONST_ITERATE(TSvrMap, sr_it, svr_map) {
        sr_it->second *= coeff;
    }
}

void
CDBUDRandomMapper::SetPreference(const string& service, double pref)
{
    TSvrMap& svr_map = m_PreferenceMap[service];

    NON_CONST_ITERATE(TSvrMap, sr_it, svr_map) {
        sr_it->second = pref;
    }
}

void
CDBUDRandomMapper::SetServerPreference(const string& service,
                                       double preference,
                                       const TSvrRef& server)
{
    TSvrMap& svr_map = m_PreferenceMap[service];
    TSvrMap::iterator sr_it = svr_map.find(server);

    if (sr_it != svr_map.end()) {
        // Scale ...
        if (preference >= 100) {
            // Set the rest of service preferences to 0 ...
            SetPreference(service, 0);
        } else if (preference <= 0) {
            // Means *no preferences*
            SetServerPreference(service,
                                static_cast<double>(100 /
                                                    m_PreferenceMap.size()),
                                server);
        } else {
            // (100 - new) / (100 - old)
            ScalePreference(service,
                            (100 - preference) / (100 - sr_it->second));
        }

        // Set the server preference finally ...
        sr_it->second = preference;
    }
}

// Implementation below doesn't work correctly at the moment ...
void
CDBUDRandomMapper::Add(const string&    service,
                       const TSvrRef&   server,
                       double           preference)
{
    _ASSERT(false);

    if (service.empty() || server.Empty()) {
        return;
    }

    TSvrMap& server_list = m_ServerMap[service];
    TSvrMap& service_preferences = m_PreferenceMap[service];
    bool non_default_preferences = false;
    double curr_preference =
        static_cast<double>(100 / m_ServerMap.size() + 1);

    if (preference < 0) {
        preference = 0;
    } else if (preference > 100) {
        preference = 100;
    }

    if (preference > 0.01) {
        non_default_preferences = true;
    }

    server_list.insert(
        TSvrMap::value_type(server, preference));
    service_preferences.insert(
        TSvrMap::value_type(server, curr_preference));

    // Recalculate preferences ...
    if (non_default_preferences) {
        ITERATE(TSvrMap, sl_it, server_list) {
            if (sl_it->second > 0) {
                SetServerPreference(service,
                                    sl_it->second,
                                    sl_it->first);
            }
        }
    }
}

void
CDBUDRandomMapper::Exclude(const string& service, const TSvrRef& server)
{
    CFastMutexGuard mg(m_Mtx);

    TSvrMap& svr_map = m_PreferenceMap[service];
    TSvrMap::iterator sr_it = svr_map.find(server);
    if (sr_it != svr_map.end()) {
        // Recalculate preferences ...
        if (svr_map.size() > 1) {
            if (sr_it->second >= 100) {
                // Divide preferences equally.
                SetPreference(service,
                    static_cast<double>(100 / (m_PreferenceMap.size() - 1)));
            } else {
                // Rescale preferences.
                ScalePreference(service, 100 / (100 - sr_it->second));
            }
        }

        svr_map.erase(sr_it);
    }
}

void
CDBUDRandomMapper::CleanExcluded(const string& service)
{
    CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO, "Not implemented");
}

void
CDBUDRandomMapper::SetPreference(const string&  service,
                                 const TSvrRef& preferred_server,
                                 double         preference)
{
    CFastMutexGuard mg(m_Mtx);

    // Set absolute value.
    m_ServerMap[service][preferred_server] = preference;
    // Set relative value;
    SetServerPreference(service, preference, preferred_server);
}


IDBServiceMapper*
CDBUDRandomMapper::Factory(const IRegistry* registry)
{
    return new CDBUDRandomMapper(registry);
}


//////////////////////////////////////////////////////////////////////////////
CDBUDPriorityMapper::CDBUDPriorityMapper(const IRegistry* registry)
{
    ConfigureFromRegistry(registry);
}

CDBUDPriorityMapper::~CDBUDPriorityMapper(void)
{
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

            NStr::Tokenize(registry->GetString(section_name,
                                               service_name,
                                               service_name),
                           " ,;",
                           server_name);

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
    CNcbiDiag::DiagTrouble(DIAG_COMPILE_INFO, "Not implemented");
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

    if (preference < 0) {
        preference = 0;
    } else if (preference > 100) {
        preference = 100;
    }

    server_list.insert(
        TSvrMap::value_type(server, preference)
        );
    usage_map.insert(TServerUsageMap::value_type(
        100 - preference,
        server)
        );
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

        NStr::Tokenize(registry->GetString
                    (section_name, "MAPPERS",
                        def_mapper_name),
                    " ,;",
                    service_name);

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

