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

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, CONNECTION_MAX_RETRIES);
NCBI_PARAM_DEF(double, netservice_api, retry_delay, RETRY_DELAY_DEFAULT);
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, "");
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited
NCBI_PARAM_DEF(bool, netservice_api, connection_data_logging, false);
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

void CSynRegistryImpl::Add(const IRegistry& registry)
{
    // Always add a registry as new top priority
    m_Registry.Add(registry, ++m_Priority);
}

IRegistry& CSynRegistryImpl::GetIRegistry()
{
    return m_Registry;
}

template <typename TType>
TType CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, TType default_value)
{
    _ASSERT(m_Priority);
    _ASSERT(sections.size());
    _ASSERT(names.size());

    for (const auto& section : sections) {
        for (const auto& name : names) {
            if (!HasImpl(section, name)) continue;

            try {
                return m_Registry.GetValue(section, name, default_value, IRegistry::eThrow);
            }
            catch (CStringException& ex) {
                LOG_POST(Warning << ex.what());
            }
            catch (CRegistryException& ex) {
                LOG_POST(Warning << ex.what());
            }
        }
    }

    return default_value;
}

bool ISynRegistry::Has(const SRegSynonyms& sections, SRegSynonyms names)
{
    _ASSERT(sections.size());
    _ASSERT(names.size());

    for (const auto& section : sections) {
        for (const auto& name : names) {
             if (HasImpl(section, name)) return true;
        }
    }

    return false;
}

template string CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, string default_value);
template bool   CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, bool default_value);
template int    CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, int default_value);
template double CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, double default_value);

class CCachedSynRegistryImpl::CCache
{
public:
    struct SValue
    {
        struct SValueHolder
        {
            template <typename TType>
            operator TType() const
            {
                _ASSERT(typeid(TType).hash_code() == m_TypeHash);
                return *static_cast<TType*>(m_Value.get());
            }

            template <typename TType>
            SValueHolder& operator=(TType value)
            {
                _ASSERT(m_TypeHash = typeid(TType).hash_code());
                m_Value = make_shared<TType>(value);
                return *this;
            }

        private:
            size_t m_TypeHash = 0;
            shared_ptr<void> m_Value;
        };

        enum { New, Default, Read } type = New;
        SValueHolder value;
    };

    using TValuePtr = shared_ptr<SValue>;
    using TValues = unordered_map<string, unordered_map<string, TValuePtr>>;

    bool Has(const SRegSynonyms& sections, SRegSynonyms names);
    TValuePtr Get(const SRegSynonyms& sections, SRegSynonyms names);

private:
    TValuePtr Find(const SRegSynonyms& sections, SRegSynonyms names);

    TValues m_Values;
};

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Find(const SRegSynonyms& sections, SRegSynonyms names)
{
    _ASSERT(sections.size());
    _ASSERT(names.size());

    auto section = sections.front();
    auto name = names.front();
    auto& section_values = m_Values[section];
    auto found = section_values.find(name);

    return found != section_values.end() ? found->second : nullptr;
}

bool CCachedSynRegistryImpl::CCache::Has(const SRegSynonyms& sections, SRegSynonyms names)
{
    auto found = Find(sections, names);
    return found && found->type == SValue::Read;
}

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Get(const SRegSynonyms& sections, SRegSynonyms names)
{
    if (auto found = Find(sections, names)) return found;

    TValuePtr value(new SValue);

    for (auto& s : sections) {
        for (auto& n : names) {
            auto& sv = m_Values[s];

            // If failed, corresponding parameter has already been read with a different set of synonyms
            _VERIFY(sv.insert(make_pair(n, value)).second);
        }
    }

    return value;
}

CCachedSynRegistryImpl::CCachedSynRegistryImpl(ISynRegistry::TPtr registry) :
    m_Registry(registry),
    m_Cache(new CCache)
{
}

CCachedSynRegistryImpl::~CCachedSynRegistryImpl()
{
}

void CCachedSynRegistryImpl::Add(const IRegistry& registry)
{
    _ASSERT(m_Registry);

    m_Registry->Add(registry);
}

IRegistry& CCachedSynRegistryImpl::GetIRegistry()
{
    _ASSERT(m_Registry);

    return m_Registry->GetIRegistry();
}

template <typename TType>
TType CCachedSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, TType default_value)
{
    _ASSERT(m_Registry);

    auto cached = m_Cache->Get(sections, names);
    auto& cached_type = cached->type;
    auto& cached_value = cached->value;

    // Has a non-default cached value
    if (cached_type == CCache::SValue::Read) return cached_value;

    // Has a non-default value
    if (m_Registry->Has(sections, names)) {
        cached_type = CCache::SValue::Read;
        cached_value = m_Registry->Get(sections, names, default_value);

    // Has no (default) value cached
    } else if (cached_type == CCache::SValue::New) {
        cached_type = CCache::SValue::Default;
        cached_value = default_value;
    }

    return cached_value;
}

bool CCachedSynRegistryImpl::HasImpl(const string& section, const string& name)
{
    _ASSERT(m_Registry);

    return (m_Cache->Has(section, name)) || m_Registry->Has(section, name);
}

template string CCachedSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, string default_value);
template bool   CCachedSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, bool default_value);
template int    CCachedSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, int default_value);
template double CCachedSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, double default_value);

CIncludeSynRegistryImpl::CIncludeSynRegistryImpl(ISynRegistry::TPtr registry) :
    m_Registry(registry)
{
}

void CIncludeSynRegistryImpl::Add(const IRegistry& registry)
{
    _ASSERT(m_Registry);

    m_Registry->Add(registry);
}

IRegistry& CIncludeSynRegistryImpl::GetIRegistry()
{
    _ASSERT(m_Registry);

    return m_Registry->GetIRegistry();
}

template <typename TType>
TType CIncludeSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, TType default_value)
{
    _ASSERT(m_Registry);

    return m_Registry->Get(GetSections(sections), names, default_value);
}

bool CIncludeSynRegistryImpl::HasImpl(const string& section, const string& name)
{
    _ASSERT(m_Registry);

    return m_Registry->Has(GetSections(section), name);
}

SRegSynonyms CIncludeSynRegistryImpl::GetSections(const SRegSynonyms& sections)
{
    _ASSERT(m_Registry);

    SRegSynonyms rv{};

    for (const auto& section : sections) {
        auto result = m_Includes.insert({section, {}});
        auto& included = result.first->second;

        // Have not checked for '.include' yet
        if (result.second) {
            auto included_value = m_Registry->Get(string(section), ".include", kEmptyStr);
            auto flags = NStr::fSplit_MergeDelimiters | NStr::fSplit_Truncate;
            NStr::Split(included_value, ",; \t\n\r", included, flags);
        }

        rv.push_back(section);
        rv.insert(rv.end(), included.begin(), included.end());
    }

    return rv;
}

template string CIncludeSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, string default_value);
template bool   CIncludeSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, bool default_value);
template int    CIncludeSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, int default_value);
template double CIncludeSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, double default_value);

ISynRegistry::TPtr s_CreateISynRegistry(const CNcbiApplication* app)
{
    auto syn_registry = new CSynRegistry;

    // TODO: Fix caching and enable
    // auto cached_registry = new CCachedSynRegistry(syn_registry->MakePtr());

    ISynRegistry::TPtr registry(new CIncludeSynRegistry(syn_registry->MakePtr()));

    if (app) {
        registry->Add(app->GetConfig());
    } else {
        // This code is safe, as the sub-registry ends up in CCompoundRegistry which uses CRef, too
        CRef<IRegistry> env_registry(new CEnvironmentRegistry);
        registry->Add(*env_registry);
    }

    return registry;
}

ISynRegistry::TPtr s_CreateISynRegistry()
{
    CMutexGuard guard(CNcbiApplication::GetInstanceMutex());
    return s_CreateISynRegistry(CNcbiApplication::Instance());
}

SISynRegistryBuilder::SISynRegistryBuilder(const IRegistry& registry) :
    m_Registry(s_CreateISynRegistry())
{
    _ASSERT(m_Registry);

    m_Registry->Add(registry);
}

SISynRegistryBuilder::SISynRegistryBuilder(CConfig* config) :
    m_Registry(s_CreateISynRegistry())
{
    _ASSERT(m_Registry);

    if (config) {
        // This code is safe, as the sub-registry ends up in CCompoundRegistry which uses CRef, too
        CRef<IRegistry> config_registry(new CConfigRegistry(config));
        m_Registry->Add(*config_registry);
    }
}

SISynRegistryBuilder::SISynRegistryBuilder(const CNcbiApplication& app) :
    m_Registry(s_CreateISynRegistry(&app))
{
    _ASSERT(m_Registry);
}

ISynRegistryToIRegistry::ISynRegistryToIRegistry(ISynRegistry::TPtr registry) :
    m_Registry(registry)
{
}

bool ISynRegistryToIRegistry::x_Empty(TFlags flags) const
{
    return GetIRegistry().Empty(flags);
}

bool ISynRegistryToIRegistry::x_Modified(TFlags flags) const
{
    return GetIRegistry().Modified(flags);
}

void ISynRegistryToIRegistry::x_SetModifiedFlag(bool modified, TFlags flags)
{
    return GetIRegistry().SetModifiedFlag(modified, flags);
}

const string& ISynRegistryToIRegistry::x_Get(const string&, const string&, TFlags) const
{
    // Get* overrides must never call this method
    _TROUBLE;
    return kEmptyStr;
}

bool ISynRegistryToIRegistry::x_HasEntry(const string&, const string&, TFlags) const
{
    // Has override must never call this method
    _TROUBLE;
    return false;
}

const string& ISynRegistryToIRegistry::x_GetComment(const string&, const string&, TFlags) const
{
    // GetComment override must never call this method
    _TROUBLE;
    return kEmptyStr;
}

void ISynRegistryToIRegistry::x_Enumerate(const string&, list<string>&, TFlags) const
{
    // Enumerate* overrides must never call this method
    _TROUBLE;
}

void ISynRegistryToIRegistry::x_ChildLockAction(FLockAction action)
{
    return (GetIRegistry().*action)();
}

const string& ISynRegistryToIRegistry::Get(const string& section, const string& name, TFlags flags) const
{
    _ASSERT(m_Registry);

    // Cannot return reference to a temporary, so first call for caching and next for returning
    m_Registry->Get(section, name, kEmptyStr);
    return GetIRegistry().Get(section, name, flags);
}

bool ISynRegistryToIRegistry::HasEntry(const string& section, const string& name, TFlags) const
{
    _ASSERT(m_Registry);

    return m_Registry->Has(section, name);
}

string ISynRegistryToIRegistry::GetString(const string& section, const string& name, const string& default_value, TFlags) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

int ISynRegistryToIRegistry::GetInt(const string& section, const string& name, int default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

bool ISynRegistryToIRegistry::GetBool(const string& section, const string& name, bool default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

double ISynRegistryToIRegistry::GetDouble(const string& section, const string& name, double default_value, TFlags, EErrAction) const
{
    _ASSERT(m_Registry);

    return m_Registry->Get(section, name, default_value);
}

const string& ISynRegistryToIRegistry::GetComment(const string& section, const string& name, TFlags flags) const
{
    return GetIRegistry().GetComment(section, name, flags);
}

void ISynRegistryToIRegistry::EnumerateInSectionComments(const string& section, list<string>* comments, TFlags flags) const
{
    return GetIRegistry().EnumerateInSectionComments(section, comments, flags);
}

void ISynRegistryToIRegistry::EnumerateSections(list<string>* sections, TFlags flags) const
{
    return GetIRegistry().EnumerateSections(sections, flags);
}

void ISynRegistryToIRegistry::EnumerateEntries(const string& section, list<string>* entries, TFlags flags) const
{
    return GetIRegistry().EnumerateEntries(section, entries, flags);
}


END_NCBI_SCOPE
