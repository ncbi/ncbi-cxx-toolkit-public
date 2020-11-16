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
 * File Description:
 *   Definitions of the configuration parameters for connect/services.
 *
 * Authors:
 *   Dmitry Kazimirov
 *
 */

#include <ncbi_pch.hpp>
#include <corelib/ncbireg.hpp>
#include <corelib/env_reg.hpp>

#include "netservice_params.hpp"

#include <mutex>
#include <unordered_map>

BEGIN_NCBI_SCOPE


// All these parameters must be added to CSynRegistry::CReport for conf reporting
NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, CONNECTION_MAX_RETRIES);
NCBI_PARAM_DEF(double, netservice_api, retry_delay, RETRY_DELAY_DEFAULT);
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, "");
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited
NCBI_PARAM_DEF(bool, netservice_api, connection_data_logging, false);
NCBI_PARAM_DEF(bool, netservice_api, warn_on_unexpected_reply, false);
NCBI_PARAM_DEF(unsigned, server, max_wait_for_servers, 24 * 60 * 60);
NCBI_PARAM_DEF(bool, server, stop_on_job_errors, true);
NCBI_PARAM_DEF(bool, server, allow_implicit_job_return, false);


CConfigRegistry::CConfigRegistry(CConfig* config) :
    m_Config(config)
{
}

void CConfigRegistry::Reset(CConfig* config)
{
    m_Config = config;
    m_SubConfigs.clear();
}

bool CConfigRegistry::x_Empty(TFlags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return false; // Not reached
}

CConfig* CConfigRegistry::GetSubConfig(const string& section) const
{
    auto it = m_SubConfigs.find(section);

    if (it != m_SubConfigs.end()) return it->second.get();

    if (const CConfig::TParamTree* tree = m_Config->GetTree()) {
        if (const CConfig::TParamTree* sub_tree = tree->FindSubNode(section)) {
            unique_ptr<CConfig> sub_config(new CConfig(sub_tree));
            auto result = m_SubConfigs.emplace(section, move(sub_config));
            return result.first->second.get();
        }
    }

    return m_Config;
}

const string& CConfigRegistry::x_Get(const string& section, const string& name, TFlags) const
{
    _ASSERT(m_Config);

    try {
        const auto& found = GetSubConfig(section);
        return found ? found->GetString(section, name, CConfig::eErr_Throw) : kEmptyStr;
    }
    catch (CConfigException& ex) {
        if (ex.GetErrCode() == CConfigException::eParameterMissing) return kEmptyStr;

        NCBI_RETHROW2(ex, CRegistryException, eErr, ex.GetMsg(), 0);
    }
}

bool CConfigRegistry::x_HasEntry(const string& section, const string& name, TFlags) const
{
    _ASSERT(m_Config);

    try {
        const auto& found = GetSubConfig(section);
        if (!found) return false;
        found->GetString(section, name, CConfig::eErr_Throw);
    }
    catch (CConfigException& ex) {
        if (ex.GetErrCode() == CConfigException::eParameterMissing) return false;

        NCBI_RETHROW2(ex, CRegistryException, eErr, ex.GetMsg(), 0);
    }

    return true;
}

const string& CConfigRegistry::x_GetComment(const string&, const string&, TFlags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return kEmptyStr; // Not reached
}

void CConfigRegistry::x_Enumerate(const string&, list<string>&, TFlags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
}

template <typename TType>
static string s_ToString(TType value)
{
    return to_string(value);
}

string s_ToString(string value)
{
    return '"' + value + '"';
}

using TValues = map<string, map<string, string>>;

template <class TParam>
struct SParamReporter
{
};

template <class TDescription>
TValues& operator<<(TValues& vs, SParamReporter<CParam<TDescription>>)
{
    const auto& section = TDescription::sm_ParamDescription.section;
    const auto& name = TDescription::sm_ParamDescription.name;
    const auto value = CParam<TDescription>::GetDefault();

    vs[section].emplace(name, s_ToString(value));
    return vs;
}

template <class... TParams>
struct SListReporter
{
};

// Clang needs TSecond (sees ambiguity between these two below otherwise)
template <class TFirst, class TSecond, class... TOther>
TValues& operator<<(TValues& vs, SListReporter<TFirst, TSecond, TOther...>)
{
    return vs << SParamReporter<TFirst>() << SListReporter<TSecond, TOther...>();
}

template <class TParam>
TValues& operator<<(TValues& vs, SListReporter<TParam>)
{
    return vs << SParamReporter<TParam>();
}

class CSynRegistry::CReport
{
public:
    CReport();

    template <typename TType>
    void Add(const string& section, const string& name, TType value);

    void Report(ostream& os) const;

private:
    TValues m_Values;
    mutable mutex m_Mutex;
};

CSynRegistry::CReport::CReport()
{
    m_Values << SListReporter<
        TServConn_ConnMaxRetries,
        TServConn_RetryDelay,
        TServConn_UserLinger2,
        TServConn_MaxFineLBNameRetries,
        TCGI_NetCacheFallbackServer,
        TServConn_MaxConnPoolSize,
        TServConn_ConnDataLogging,
        TServConn_WarnOnUnexpectedReply,
        TWorkerNode_MaxWaitForServers,
        TWorkerNode_StopOnJobErrors,
        TWorkerNode_AllowImplicitJobReturn>();
}

void CSynRegistry::CReport::Report(ostream& os) const
{
    lock_guard<mutex> lock(m_Mutex);

    for (const auto& section : m_Values) {
        os << '[' << section.first << ']' << endl;;

        for (const auto& param : section.second) {
            os << param.first << '=' << param.second << endl;
        }

        os << endl;
    }
}

template <typename TType>
void CSynRegistry::CReport::Add(const string& section, const string& name, TType value)
{
    lock_guard<mutex> lock(m_Mutex);
    m_Values[section].emplace(name, s_ToString(value));
}

class CSynRegistry::CInclude
{
public:
    SRegSynonyms Get(const SRegSynonyms& sections, IRegistry& registry);

private:
    unordered_map<string, vector<string>> m_Includes;
    mutex m_Mutex;
};

SRegSynonyms CSynRegistry::CInclude::Get(const SRegSynonyms& sections, IRegistry& registry)
{
    SRegSynonyms rv{};
    lock_guard<mutex> lock(m_Mutex);

    for (const auto& section : sections) {
        auto result = m_Includes.insert({section, {}});
        auto& included = result.first->second;

        // Have not checked for '.include' yet
        if (result.second) {
            auto included_value = registry.Get(string(section), ".include");
            auto flags = NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate;
            NStr::Split(included_value, ",; \t\n\r", included, flags);
        }

        rv.push_back(section);
        rv.insert(rv.end(), included.begin(), included.end());
    }

    return rv;
}

class CSynRegistry::CAlert
{
public:
    void Set(string message);
    void Report(ostream& os) const;
    bool Ack(size_t id);

private:
    map<size_t, string> m_Alerts;
    size_t m_CurrentID = 0;
    mutable mutex m_Mutex;
};

void CSynRegistry::CAlert::Set(string message)
{
    lock_guard<mutex> lock(m_Mutex);
    m_Alerts.emplace(++m_CurrentID, message);
}

void CSynRegistry::CAlert::Report(ostream& os) const
{
    lock_guard<mutex> lock(m_Mutex);
    for (const auto& alert : m_Alerts) {
        os << "Alert_" << alert.first << ": \"" << alert.second << '"' << endl;
    }
}

bool CSynRegistry::CAlert::Ack(size_t id)
{
    lock_guard<mutex> lock(m_Mutex);
    return m_Alerts.erase(id) == 1;
}

CSynRegistry::CSynRegistry() :
    m_Report(new CReport),
    m_Include(new CInclude),
    m_Alert(new CAlert)
{
}

CSynRegistry::~CSynRegistry()
{
}

void CSynRegistry::Add(const IRegistry& registry)
{
    // Always add a registry as new top priority
    m_Registry.Add(registry, ++m_Priority);
}

IRegistry& CSynRegistry::GetIRegistry()
{
    return m_Registry;
}

void CSynRegistry::Report(ostream& os) const
{
    m_Report->Report(os);
}

void CSynRegistry::Alerts(ostream& os) const
{
    m_Alert->Report(os);
}

bool CSynRegistry::AckAlert(size_t id)
{
    return m_Alert->Ack(id);
}

template <typename TType>
TType CSynRegistry::TGet(const SRegSynonyms& sections, SRegSynonyms names, TType default_value)
{
    _ASSERT(m_Priority);
    _ASSERT(sections.size());
    _ASSERT(names.size());

    const auto sections_plus_included = m_Include->Get(sections, m_Registry);

    for (const auto& section : sections_plus_included) {
        for (const auto& name : names) {
            if (!m_Registry.HasEntry(section, name)) continue;

            try {
                auto rv = m_Registry.GetValue(section, name, default_value, IRegistry::eThrow);
                m_Report->Add(section, name, rv);
                return rv;
            }
            catch (CException& ex) {
                string msg;
                string separator;

                for (const auto* p = &ex; p; p = p->GetPredecessor()) {
                    msg += separator;
                    msg += p->GetMsg();
                    separator = ". ";
                }

                ERR_POST(Warning << msg);
                m_Alert->Set(NStr::JsonEncode(msg));
            }
        }
    }

    // Cannot use sections.front() here.
    // Otherwise, some default values would be reported twice (with different sections)
    // due to configuration loading from NetSchedule which adds a special section in front.
    m_Report->Add(sections.back(), names.front(), default_value);
    return default_value;
}

bool CSynRegistry::Has(const SRegSynonyms& sections, SRegSynonyms names)
{
    _ASSERT(sections.size());
    _ASSERT(names.size());

    const auto sections_plus_included = m_Include->Get(sections, m_Registry);

    for (const auto& section : sections_plus_included) {
        for (const auto& name : names) {
            if (m_Registry.HasEntry(section, name)) return true;
        }
    }

    return false;
}

template string CSynRegistry::TGet(const SRegSynonyms& sections, SRegSynonyms names, string default_value);
template bool   CSynRegistry::TGet(const SRegSynonyms& sections, SRegSynonyms names, bool default_value);
template int    CSynRegistry::TGet(const SRegSynonyms& sections, SRegSynonyms names, int default_value);
template double CSynRegistry::TGet(const SRegSynonyms& sections, SRegSynonyms names, double default_value);

CSynRegistry::TPtr s_CreateISynRegistry(const CNcbiApplication* app)
{
    CSynRegistry::TPtr registry(new CSynRegistry);


    if (app) {
        registry->Add(app->GetConfig());
    } else {
        // This code is safe, as the sub-registry ends up in CCompoundRegistry which uses CRef, too
        CRef<IRegistry> env_registry(new CEnvironmentRegistry);
        registry->Add(*env_registry);
    }

    return registry;
}

CSynRegistry::TPtr s_CreateISynRegistry()
{
    CNcbiApplicationGuard guard = CNcbiApplication::InstanceGuard();
    return s_CreateISynRegistry(guard.Get());
}

CSynRegistryBuilder::CSynRegistryBuilder(const IRegistry& registry) :
    m_Registry(s_CreateISynRegistry())
{
    _ASSERT(m_Registry);

    m_Registry->Add(registry);
}

CSynRegistryBuilder::CSynRegistryBuilder(CConfig* config) :
    m_Registry(s_CreateISynRegistry())
{
    _ASSERT(m_Registry);

    if (config) {
        // This code is safe, as the sub-registry ends up in CCompoundRegistry which uses CRef, too
        CRef<IRegistry> config_registry(new CConfigRegistry(config));
        m_Registry->Add(*config_registry);
    }
}

CSynRegistryBuilder::CSynRegistryBuilder(const CNcbiApplication& app) :
    m_Registry(s_CreateISynRegistry(&app))
{
    _ASSERT(m_Registry);
}

CSynRegistryToIRegistry::CSynRegistryToIRegistry(CSynRegistry::TPtr registry) :
    m_Registry(registry)
{
}

bool CSynRegistryToIRegistry::x_Empty(TFlags flags) const
{
    return GetIRegistry().Empty(flags);
}

bool CSynRegistryToIRegistry::x_Modified(TFlags flags) const
{
    return GetIRegistry().Modified(flags);
}

void CSynRegistryToIRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    return GetIRegistry().SetModifiedFlag(modified, flags);
}

const string& CSynRegistryToIRegistry::x_Get(const string&, const string&, TFlags) const
{
    // Get* overrides must never call this method
    _TROUBLE;
    return kEmptyStr;
}

bool CSynRegistryToIRegistry::x_HasEntry(const string&, const string&, TFlags) const
{
    // Has override must never call this method
    _TROUBLE;
    return false;
}

const string& CSynRegistryToIRegistry::x_GetComment(const string&, const string&, TFlags) const
{
    // GetComment override must never call this method
    _TROUBLE;
    return kEmptyStr;
}

void CSynRegistryToIRegistry::x_Enumerate(const string&, list<string>&, TFlags) const
{
    // Enumerate* overrides must never call this method
    _TROUBLE;
}

void CSynRegistryToIRegistry::x_ChildLockAction(FLockAction action)
{
    return (GetIRegistry().*action)();
}

const string& CSynRegistryToIRegistry::Get(const string& section, const string& name, TFlags flags) const
{
    _ASSERT(m_Registry);

    // Cannot return reference to a temporary, so first call for caching and next for returning
    m_Registry->Get(section, name, kEmptyStr);
    return GetIRegistry().Get(section, name, flags);
}

bool CSynRegistryToIRegistry::HasEntry(const string& section, const string& name, TFlags) const
{
    _ASSERT(m_Registry);

    return m_Registry->Has(section, name);
}

string CSynRegistryToIRegistry::GetString(const string& section, const string& name, const string& default_value, TFlags) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

int CSynRegistryToIRegistry::GetInt(const string& section, const string& name, int default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

bool CSynRegistryToIRegistry::GetBool(const string& section, const string& name, bool default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

double CSynRegistryToIRegistry::GetDouble(const string& section, const string& name, double default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

const string& CSynRegistryToIRegistry::GetComment(const string& section, const string& name, TFlags flags) const
{
    return GetIRegistry().GetComment(section, name, flags);
}

void CSynRegistryToIRegistry::EnumerateInSectionComments(const string& section, list<string>* comments, TFlags flags) const
{
    return GetIRegistry().EnumerateInSectionComments(section, comments, flags);
}

void CSynRegistryToIRegistry::EnumerateSections(list<string>* sections, TFlags flags) const
{
    return GetIRegistry().EnumerateSections(sections, flags);
}

void CSynRegistryToIRegistry::EnumerateEntries(const string& section, list<string>* entries, TFlags flags) const
{
    return GetIRegistry().EnumerateEntries(section, entries, flags);
}


END_NCBI_SCOPE
