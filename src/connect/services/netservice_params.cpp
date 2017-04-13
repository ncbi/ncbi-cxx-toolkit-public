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

#include "netservice_params.hpp"

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, CONNECTION_MAX_RETRIES);
NCBI_PARAM_DEF(string, netservice_api, retry_delay,
    NCBI_AS_STRING(RETRY_DELAY_DEFAULT));
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, "");
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited
NCBI_PARAM_DEF(bool, netservice_api, connection_data_logging, false);
NCBI_PARAM_DEF(unsigned, server, max_wait_for_servers, 24 * 60 * 60);
NCBI_PARAM_DEF(bool, server, stop_on_job_errors, true);
NCBI_PARAM_DEF(bool, server, allow_implicit_job_return, false);


unsigned long s_SecondsToMilliseconds(
    const string& seconds, unsigned long default_value)
{
    const signed char* ch = (const signed char*) seconds.c_str();

    unsigned long result = 0;
    register int digit = *ch - '0';

    if (digit >= 0 && digit <= 9) {
        do
            result = result * 10 + digit;
        while ((digit = *++ch - '0') >= 0 && digit <= 9);
        if (*ch == '\0')
            return result * 1000;
    }

    if (*ch != '.')
        return default_value;

    static unsigned const powers_of_ten[4] = {1, 10, 100, 1000};
    int exponent = 3;

    while ((digit = *++ch - '0') >= 0 && digit <= 9) {
        if (--exponent < 0)
            return default_value;
        result = result * 10 + digit;
    }

    return *ch == '\0' ? result * powers_of_ten[exponent] : default_value;
}

unsigned long s_GetRetryDelay()
{
    static unsigned long retry_delay;
    static bool retry_delay_is_set = false;

    if (!retry_delay_is_set) {
        retry_delay =
            s_SecondsToMilliseconds(TServConn_RetryDelay::GetDefault(),
                RETRY_DELAY_DEFAULT * kMilliSecondsPerSecond);

        retry_delay_is_set = true;
    }

    return retry_delay;
}

template <class T>
shared_ptr<T> s_MakeShared(T* ptr, EOwnership ownership)
{
    if (ownership == eTakeOwnership) return shared_ptr<T>(ptr);
    return { shared_ptr<T>(), ptr };
}

CConfigRegistry::CConfigRegistry(CConfig* config, EOwnership ownership) :
    m_Config(s_MakeShared(config, ownership))
{
}

void CConfigRegistry::Reset(CConfig* config, EOwnership ownership)
{
    m_Config = s_MakeShared(config, ownership);
    m_SubConfigs.clear();
}

bool CConfigRegistry::x_Empty(TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return false; // Not reached
}

const unique_ptr<CConfig>& CConfigRegistry::GetSubConfig(const string& section) const
{
    auto it = m_SubConfigs.find(section);

    if (it != m_SubConfigs.end()) return it->second;

    unique_ptr<CConfig> sub_config;

    if (const CConfig::TParamTree* tree = m_Config->GetTree()) {
        if (const CConfig::TParamTree* sub_tree = tree->FindSubNode(section)) {
            sub_config.reset(new CConfig(sub_tree));
        }
    }

    auto result = m_SubConfigs.emplace(section, move(sub_config));
    return result.first->second;
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

bool CConfigRegistry::x_HasEntry(const string& section, const string& name, TFlags flags) const
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

const string& CConfigRegistry::x_GetComment(const string& section, const string& name, TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return kEmptyStr; // Not reached
}

void CConfigRegistry::x_Enumerate(const string& section, list<string>& entries, TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
}

CSynRegistryImpl::CSynRegistryImpl(IRegistry* registry, EOwnership ownership)
{
    if (registry) Add(registry, ownership);
}

void CSynRegistryImpl::Add(IRegistry* registry, EOwnership ownership)
{
    _ASSERT(registry);

    m_SubRegistries.push_back(s_MakeShared(registry, ownership));

    // Always add a registry as new top priority
    const auto priority = static_cast<int>(m_SubRegistries.size());
    m_Registry.Add(*m_SubRegistries.back(), priority);
}

template <typename TType>
TType CSynRegistryImpl::TGet(const SRegSynonyms& sections, SRegSynonyms names, TType default_value)
{
    _ASSERT(m_SubRegistries.size());
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
    using TValues = map<string, map<string, TValuePtr>>;

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

CCachedSynRegistryImpl::CCachedSynRegistryImpl(ISynRegistry* registry, EOwnership ownership) :
    m_Registry(s_MakeShared(registry, ownership)),
    m_Cache(new CCache)
{
}

CCachedSynRegistryImpl::~CCachedSynRegistryImpl()
{
}

void CCachedSynRegistryImpl::Add(IRegistry* registry, EOwnership ownership)
{
    m_Registry->Add(registry, ownership);
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

END_NCBI_SCOPE
