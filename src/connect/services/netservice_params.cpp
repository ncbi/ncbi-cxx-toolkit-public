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

#include "netservice_params.hpp"

BEGIN_NCBI_SCOPE


NCBI_PARAM_DEF(bool, netservice_api, use_linger2, false);
NCBI_PARAM_DEF(unsigned int, netservice_api, connection_max_retries, 4);
NCBI_PARAM_DEF(string, netservice_api, retry_delay,
    NCBI_AS_STRING(RETRY_DELAY_DEFAULT));
NCBI_PARAM_DEF(string, netservice_api, communication_timeout,
    NCBI_AS_STRING(COMMUNICATION_TIMEOUT_DEFAULT));
NCBI_PARAM_DEF(int, netservice_api, max_find_lbname_retries, 3);
NCBI_PARAM_DEF(string, netcache_api, fallback_server, "");
NCBI_PARAM_DEF(int, netservice_api, max_connection_pool_size, 0); // unlimited
NCBI_PARAM_DEF(bool, netservice_api, connection_data_logging, false);
NCBI_PARAM_DEF(unsigned, server, max_wait_for_servers, 24 * 60 * 60);
NCBI_PARAM_DEF(bool, server, stop_on_job_errors, true);
NCBI_PARAM_DEF(bool, server, allow_implicit_job_return, false);


static bool s_DefaultCommTimeout_Initialized = false;
static STimeout s_DefaultCommTimeout;

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

STimeout s_GetDefaultCommTimeout()
{
    if (s_DefaultCommTimeout_Initialized)
        return s_DefaultCommTimeout;
    NcbiMsToTimeout(&s_DefaultCommTimeout,
        s_SecondsToMilliseconds(TServConn_CommTimeout::GetDefault(),
            SECONDS_DOUBLE_TO_MS_UL(COMMUNICATION_TIMEOUT_DEFAULT)));
    s_DefaultCommTimeout_Initialized = true;
    return s_DefaultCommTimeout;
}

unsigned long s_GetRetryDelay()
{
    static unsigned long retry_delay;
    static bool retry_delay_is_set = false;

    if (!retry_delay_is_set) {
        retry_delay =
            s_SecondsToMilliseconds(TServConn_RetryDelay::GetDefault(),
                SECONDS_DOUBLE_TO_MS_UL(RETRY_DELAY_DEFAULT));

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
    _ASSERT(config);
}

void CConfigRegistry::Reset(CConfig* config, EOwnership ownership)
{
    _ASSERT(config);
    m_Config = s_MakeShared(config, ownership);
}

bool CConfigRegistry::x_Empty(TFlags flags) const
{
    NCBI_ALWAYS_TROUBLE("Not implemented");
    return false; // Not reached
}

const string& CConfigRegistry::x_Get(const string& section, const string& name, TFlags) const
{
    return m_Config->GetString(section, name, CConfig::eErr_NoThrow);
}

bool CConfigRegistry::x_HasEntry(const string& section, const string& name, TFlags flags) const
{
    try {
        m_Config->GetString(section, name, CConfig::eErr_Throw);
    }
    catch (CConfigException& ex) {
        if (ex.GetErrCode() == CConfigException::eParameterMissing) return false;
        throw;
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

CSynRegistryImpl::CSynRegistryImpl(IRegistry* registry, EOwnership ownership) :
    m_Registry(s_MakeShared(registry, ownership))
{
}

void CSynRegistryImpl::Reset(IRegistry* registry, EOwnership ownership)
{
    m_Registry = s_MakeShared(registry, ownership);
}

template <typename TType, class TFunc>
TType s_Iterate(initializer_list<string> list, TType default_value, TFunc func)
{
    _ASSERT(list.size());

    const TType empty_value{};

    for (const auto& element : list) {
        auto value = func(element, empty_value);

        if (value != empty_value) return value;
    }

    return default_value;
}

template <typename TType>
TType CSynRegistryImpl::GetImpl(const string& section, initializer_list<string> names, TType default_value)
{
    auto f = [&](const string& name, TType value)
    {
        return GetValue(section, name.c_str(), value);
    };

    return s_Iterate(names, default_value, f);
}

template <typename TType>
TType CSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, TType default_value)
{
    auto f = [&](const string& section, TType value)
    {
        return GetValue(section, name, value);
    };

    return s_Iterate(sections, default_value, f);
}

template <typename TType>
TType CSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, TType default_value)
{
    auto outer_f = [&](const string& section, TType value)
    {
        auto inner_f = [&](const string& name, TType inner_value)
        {
            // XXX:
            // GCC-4.9.3 has a bug (62272).
            // So, to avoid internal compiler error, 'this' has to be used.
            return this->GetValue(section, name.c_str(), inner_value);
        };

        return s_Iterate(names, value, inner_f);
    };

    return s_Iterate(sections, default_value, outer_f);
}

bool ISynRegistry::Has(const string& section, initializer_list<string> names)
{
    auto f = [&](const string& name, bool)
    {
        return Has(section, name.c_str());
    };

    return s_Iterate(names, false, f);
}

bool ISynRegistry::Has(initializer_list<string> sections, const char* name)
{
    auto f = [&](const string& section, bool)
    {
        return Has(section, name);
    };

    return s_Iterate(sections, false, f);
}

bool ISynRegistry::Has(initializer_list<string> sections, initializer_list<string> names)
{
    auto outer_f = [&](const string& section, bool)
    {
        auto inner_f = [&](const string& name, bool)
        {
            // XXX:
            // GCC-4.9.3 has a bug (62272).
            // So, to avoid internal compiler error, 'this' has to be used
            // (it was not failing in this particular case, added 'this' just in case).
            return this->Has(section, name.c_str());
        };

        return s_Iterate(names, false, inner_f);
    };

    return s_Iterate(sections, false, outer_f);
}

template string CSynRegistryImpl::GetImpl(const string& section, initializer_list<string> names, string default_value);
template bool   CSynRegistryImpl::GetImpl(const string& section, initializer_list<string> names, bool default_value);
template int    CSynRegistryImpl::GetImpl(const string& section, initializer_list<string> names, int default_value);
template double CSynRegistryImpl::GetImpl(const string& section, initializer_list<string> names, double default_value);
template string CSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, string default_value);
template bool   CSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, bool default_value);
template int    CSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, int default_value);
template double CSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, double default_value);
template string CSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, string default_value);
template bool   CSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, bool default_value);
template int    CSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, int default_value);
template double CSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, double default_value);

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

    TValuePtr Get(const string& section, const char* name);
    TValuePtr Get(const string& section, initializer_list<string> names);
    TValuePtr Get(initializer_list<string> sections, const char* name);
    TValuePtr Get(initializer_list<string> sections, initializer_list<string> names);

private:
    TValues m_Values;
};

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Get(const string& section, const char* name)
{
    auto& section_values = m_Values[section];
    auto found = section_values.find(name);

    if (found != section_values.end()) return section_values[name];

    TValuePtr value(new SValue);
    section_values.insert(make_pair(name, value));
    return value;
}

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Get(const string& section, initializer_list<string> names)
{
    _ASSERT(names.size());

    auto name = *names.begin();
    auto& section_values = m_Values[section];
    auto found = section_values.find(name);

    if (found != section_values.end()) return section_values[name];

    TValuePtr value(new SValue);

    for (auto& n : names) {
        auto result = section_values.insert(make_pair(n, value));

        // If failed, corresponding parameter has already been read with a different set of synonyms
        _ASSERT(result.second);
    }

    return value;
}

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Get(initializer_list<string> sections, const char* name)
{
    _ASSERT(sections.size());

    auto section = *sections.begin();
    auto& section_values = m_Values[section];
    auto found = section_values.find(name);

    if (found != section_values.end()) return section_values[name];

    TValuePtr value(new SValue);

    for (auto& s : sections) {
        auto& sv = m_Values[s];
        auto result = sv.insert(make_pair(name, value));

        // If failed, corresponding parameter has already been read with a different set of synonyms
        _ASSERT(result.second);
    }

    return value;
}

CCachedSynRegistryImpl::CCache::TValuePtr CCachedSynRegistryImpl::CCache::Get(initializer_list<string> sections, initializer_list<string> names)
{
    _ASSERT(sections.size());
    _ASSERT(names.size());

    auto section = *sections.begin();
    auto name = *names.begin();
    auto& section_values = m_Values[section];
    auto found = section_values.find(name);

    if (found != section_values.end()) return section_values[name];

    TValuePtr value(new SValue);

    for (auto& s : sections) {
        for (auto& n : names) {
            auto& sv = m_Values[s];
            auto result = sv.insert(make_pair(n, value));

            // If failed, corresponding parameter has already been read with a different set of synonyms
            _ASSERT(result.second);
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

void CCachedSynRegistryImpl::Reset(IRegistry* registry, EOwnership ownership)
{
    m_Registry->Reset(registry, ownership);
}

template <typename TSections, typename TNames, typename TType>
TType CCachedSynRegistryImpl::GetImpl(TSections sections, TNames names, TType default_value)
{
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

bool CCachedSynRegistryImpl::HasImpl(const string& section, const char* name)
{
    auto cached = m_Cache->Get(section, name);

    return (cached->type == CCache::SValue::Read) || m_Registry->Has(section, name);
}

template string CCachedSynRegistryImpl::GetImpl(string section, const char* name, string default_value);
template bool   CCachedSynRegistryImpl::GetImpl(string section, const char* name, bool default_value);
template int    CCachedSynRegistryImpl::GetImpl(string section, const char* name, int default_value);
template double CCachedSynRegistryImpl::GetImpl(string section, const char* name, double default_value);
template string CCachedSynRegistryImpl::GetImpl(string section, initializer_list<string> names, string default_value);
template bool   CCachedSynRegistryImpl::GetImpl(string section, initializer_list<string> names, bool default_value);
template int    CCachedSynRegistryImpl::GetImpl(string section, initializer_list<string> names, int default_value);
template double CCachedSynRegistryImpl::GetImpl(string section, initializer_list<string> names, double default_value);
template string CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, string default_value);
template bool   CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, bool default_value);
template int    CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, int default_value);
template double CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, const char* name, double default_value);
template string CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, string default_value);
template bool   CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, bool default_value);
template int    CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, int default_value);
template double CCachedSynRegistryImpl::GetImpl(initializer_list<string> sections, initializer_list<string> names, double default_value);

END_NCBI_SCOPE
