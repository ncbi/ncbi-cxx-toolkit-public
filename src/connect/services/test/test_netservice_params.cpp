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
 * Authors: Rafael Sadyrov
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/test_boost.hpp>
#include <corelib/request_ctx.hpp>

#include <connect/ncbi_socket.hpp>

#include "../netservice_params.hpp"

#include <random>
#include <array>

#include <common/test_assert.h>  /* This header must go last */

USING_NCBI_SCOPE;

// Priorities: not set, bottom, middle, top
const size_t kPrioritiesSize = 4;

const char kAllowedChars[] = "0123456789_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

class TTypes
{
    using TTuple = tuple<string, bool, int, double>;

public:
    static size_t Size() { return tuple_size<TTuple>::value; }
    template <size_t i> using GetType = typename tuple_element<i, TTuple>::type;
    template <typename T> static size_t GetIndex();
};

template <> size_t TTypes::GetIndex<typename tuple_element<0, TTypes::TTuple>::type>() { return 0; }
template <> size_t TTypes::GetIndex<typename tuple_element<1, TTypes::TTuple>::type>() { return 1; }
template <> size_t TTypes::GetIndex<typename tuple_element<2, TTypes::TTuple>::type>() { return 2; }
template <> size_t TTypes::GetIndex<typename tuple_element<3, TTypes::TTuple>::type>() { return 3; }

// Is used to generate random strings
struct SStringGenerator
{
protected:
    SStringGenerator();

    string GetString(size_t length);

    mt19937 m_Generator;

private:
    uniform_int_distribution<size_t> m_AllowedCharsRange;
};

SStringGenerator::SStringGenerator() :
    m_Generator(random_device()()),
    m_AllowedCharsRange(0, sizeof(kAllowedChars) - 2)
{
}

string SStringGenerator::GetString(size_t length)
{
    auto get_char = [&]() { return kAllowedChars[m_AllowedCharsRange(m_Generator)]; };

    string result;
    generate_n(back_inserter(result), length, get_char);
    return result;
}

// Is used to generate random values
struct SValueGenerator : protected SStringGenerator
{
    SValueGenerator();

    template <typename TType>
    TType GetValue();

private:
    uniform_int_distribution<size_t> m_String;
    bernoulli_distribution m_Bool;
    uniform_int_distribution<int> m_Int;
    uniform_real_distribution<double> m_Double;
};

SValueGenerator::SValueGenerator() :
    m_String(10, 100), // Range for length of a string value
    m_Bool(0.5),
    m_Int(numeric_limits<int>::min()),
    m_Double(numeric_limits<float>::min(), numeric_limits<float>::max()) // Cannot use whole double range, comparison would fail
{
}

template <>
string SValueGenerator::GetValue()
{
    return GetString(m_String(m_Generator));
}

template <>
bool SValueGenerator::GetValue()
{
    return m_Bool(m_Generator);
}

template <>
int SValueGenerator::GetValue()
{
    return m_Int(m_Generator);
}

template <>
double SValueGenerator::GetValue()
{
    return m_Double(m_Generator);
}

// Is used to randomly generate everything else remaining
struct SRandomGenerator : SValueGenerator
{
    SRandomGenerator();

    size_t GetParamsNumber()   { return m_ParamsRange(  m_Generator); }
    size_t GetSectionsNumber() { return m_SectionsRange(m_Generator); }
    size_t GetNamesNumber()    { return m_NamesRange(   m_Generator); }
    size_t GetRandom()         { return m_RandomRange(  m_Generator); }

    string GetSection() { return GetString(m_SectionLengthRange(m_Generator)); }
    string GetName()    { return GetString(m_NameLengthRange(   m_Generator)); }

private:
    uniform_int_distribution<size_t> m_ParamsRange;
    uniform_int_distribution<size_t> m_SectionsRange;
    uniform_int_distribution<size_t> m_NamesRange;
    uniform_int_distribution<size_t> m_RandomRange;
    uniform_int_distribution<size_t> m_SectionLengthRange;
    uniform_int_distribution<size_t> m_NameLengthRange;
};

SRandomGenerator::SRandomGenerator() :
    m_ParamsRange(16, 64),       // Range for number of parameters
    m_SectionsRange(1, 4),       // Range for number of sections for a parameter
    m_NamesRange(1, 2),          // Range for number of names for a parameter
    m_SectionLengthRange(5, 15), // Range for length of a section name
    m_NameLengthRange(5, 30)     // Range for length of a param name
{
}

// Is used to hold type erasured values
struct SValueHolder
{
    template <size_t i>
    typename TTypes::GetType<i> Get() const
    {
        _ASSERT(m_Index == i);
        return *static_cast<typename TTypes::GetType<i>*>(m_Value.get());
    }

    template <typename TType>
    void Set(TType value)
    {
        const size_t index = TTypes::GetIndex<TType>();
        _ASSERT(!m_Index || m_Index == index);
        m_Index = index;
        m_Value = make_shared<TType>(value);
    }

    size_t GetIndex() const { return m_Index; }

private:
    size_t m_Index = 0;
    shared_ptr<void> m_Value;
};

using TMemoryRegistries = array<CMemoryRegistry, kPrioritiesSize - 1>;

struct SParamIndices
{
    SParamIndices(size_t r) : m_Random(r) {}

    size_t GetPriority() const          { return   m_Random % kPrioritiesSize; }
    size_t GetType()const               { return  (m_Random / kPrioritiesSize) % TTypes::Size(); }
    size_t GetSection(size_t max) const { return ((m_Random / kPrioritiesSize) / TTypes::Size()) % max; }
    size_t GetName(size_t max) const    { return ((m_Random / kPrioritiesSize) / TTypes::Size()) % max; }

private:
    const size_t m_Random;
};

struct SRandomParam
{
private:
    vector<string> sections_values;
    vector<string> names_values;

public:
    SRegSynonyms sections;
    SRegSynonyms names;
    SValueHolder value;
    string included;
    SParamIndices indices;

    SRandomParam(SRandomGenerator& generator, vector<string> sv, vector<string> nv) :
        sections_values(move(sv)),
        names_values(move(nv)),
        sections(sections_values.front()),
        names(names_values.front()),
        included(generator.GetSection()),
        indices(generator.GetRandom())
    {
        Init(generator);
    }

    SRandomParam(const SRandomParam& src) :
        sections_values(src.sections_values),
        names_values(src.names_values),
        sections(sections_values.front()),
        names(names_values.front()),
        value(src.value),
        included(src.included),
        indices(src.indices)
    {
        Init();
    }

    set<pair<string, string>> GetCartesianProduct() const;
    void SetValue(IRWRegistry& registry) const;
    void CheckValue(IRegistry& registry) const;

    void SetValue(TMemoryRegistries& registries) const;
    void CheckValue(CSynRegistry& registry) const;

    void SetIncluded(TMemoryRegistries& registries) const;

private:
    template <typename TType>
    void CompareValue(IRegistry& registry, TType expected_value) const
    {
        const auto& section = sections.front();
        const auto& name = names.front();

        BOOST_CHECK(registry.GetValue(section, name, TType{}) == expected_value);
    }

    template <typename TType>
    void CompareValue(CSynRegistry& registry, TType expected_value) const
    {
        BOOST_CHECK(registry.Get(sections, names, TType{}) == expected_value);
    }


    // Doubles cannot be compared with operator==, so these overloads added

    void CompareValue(IRegistry& registry, double expected_value) const
    {
        const auto& section = sections.front();
        const auto& name = names.front();

        const auto read_value = registry.GetValue(section, name, 0.0);
        const auto epsilon = numeric_limits<double>::epsilon();

        BOOST_CHECK(abs(read_value - expected_value) <= epsilon * (abs(read_value) < abs(expected_value) ? abs(read_value) : abs(expected_value)));
    }

    void CompareValue(CSynRegistry& registry, double expected_value) const
    {
        const auto read_value = registry.Get(sections, names, 0.0);
        const auto epsilon = numeric_limits<double>::epsilon();

        BOOST_CHECK(abs(read_value - expected_value) <= epsilon * (abs(read_value) < abs(expected_value) ? abs(read_value) : abs(expected_value)));
    }

    void Init();
    void Init(SRandomGenerator& generator);
};

set<pair<string, string>> SRandomParam::GetCartesianProduct() const
{
    set<pair<string, string>> result;

    for (const auto& section : sections) {
        for (const auto& name : names) {
            result.emplace(section, name);
        }
    }

    return result;
}

void SRandomParam::SetValue(IRWRegistry& registry) const
{
    const size_t priority = indices.GetPriority();

    if (!priority) return;

    const auto& section = sections.front();
    const auto& name = names.front();

    switch (value.GetIndex()) {
        case 0:  registry.SetValue(section, name, value.Get<0>()); break;
        case 1:  registry.SetValue(section, name, value.Get<1>()); break;
        case 2:  registry.SetValue(section, name, value.Get<2>()); break;
        case 3:  registry.SetValue(section, name, value.Get<3>()); break;
        default: _TROUBLE;
    }
}

void SRandomParam::CheckValue(IRegistry& registry) const
{
    const size_t priority = indices.GetPriority();
    const auto& section = sections.front();
    const auto& name = names.front();

    if (priority) {
        BOOST_CHECK(registry.HasEntry(section, name));

        switch (value.GetIndex()) {
            case 0:  CompareValue(registry, value.Get<0>()); break;
            case 1:  CompareValue(registry, value.Get<1>()); break;
            case 2:  CompareValue(registry, value.Get<2>()); break;
            case 3:  CompareValue(registry, value.Get<3>()); break;
            default: _TROUBLE;
        }
    } else {
        BOOST_CHECK(!registry.HasEntry(section, name));
    }
}

void SRandomParam::SetValue(TMemoryRegistries& registries) const
{
    const size_t priority = indices.GetPriority();

    if (!priority) return;

    const auto& section = sections[indices.GetSection(sections.size())];
    const auto& name = names[indices.GetName(names.size())];

    switch (value.GetIndex()) {
        case 0:  registries[priority - 1].SetValue(section, name, value.Get<0>()); break;
        case 1:  registries[priority - 1].SetValue(section, name, value.Get<1>()); break;
        case 2:  registries[priority - 1].SetValue(section, name, value.Get<2>()); break;
        case 3:  registries[priority - 1].SetValue(section, name, value.Get<3>()); break;
        default: _TROUBLE;
    }
}

void SRandomParam::CheckValue(CSynRegistry& registry) const
{
    const size_t priority = indices.GetPriority();

    if (priority) {
        BOOST_CHECK(registry.Has(sections, names));

        switch (value.GetIndex()) {
            case 0:  CompareValue(registry, value.Get<0>()); break;
            case 1:  CompareValue(registry, value.Get<1>()); break;
            case 2:  CompareValue(registry, value.Get<2>()); break;
            case 3:  CompareValue(registry, value.Get<3>()); break;
            default: _TROUBLE;
        }
    } else {
        BOOST_CHECK(!registry.Has(sections, names));
    }
}

void SRandomParam::SetIncluded(TMemoryRegistries& registries) const
{
    const size_t priority = indices.GetPriority();

    if (!priority) return;

    const auto& section = sections[indices.GetSection(sections.size())];
    const auto& name = names[indices.GetName(names.size())];

    switch (value.GetIndex()) {
        case 0:  registries[priority - 1].SetValue(included, name, value.Get<0>()); break;
        case 1:  registries[priority - 1].SetValue(included, name, value.Get<1>()); break;
        case 2:  registries[priority - 1].SetValue(included, name, value.Get<2>()); break;
        case 3:  registries[priority - 1].SetValue(included, name, value.Get<3>()); break;
        default: _TROUBLE;
    }

    auto already_included = registries[0].GetValue(section, ".Include", kEmptyStr);
    if (!already_included.empty()) already_included += ", ";
    registries[0].SetValue(section, ".Include", already_included + included);
}

void SRandomParam::Init()
{
    // This strange initialization of sections/names is added to avoid
    // introducing special SRegSynonyms ctors for testing only
    if (sections_values.size() > 1) {
        copy(++sections_values.begin(), sections_values.end(), back_inserter(sections));
    }

    if (names_values.size() > 1) {
        copy(++names_values.begin(), names_values.end(), back_inserter(names));
    }
}

void SRandomParam::Init(SRandomGenerator& generator)
{
    Init();

    const size_t type = indices.GetType();
    switch (type) {
        case 0:  value.Set(generator.GetValue<TTypes::GetType<0>>()); break;
        case 1:  value.Set(generator.GetValue<TTypes::GetType<1>>()); break;
        case 2:  value.Set(generator.GetValue<TTypes::GetType<2>>()); break;
        case 3:  value.Set(generator.GetValue<TTypes::GetType<3>>()); break;
        default: _TROUBLE;
    }
}

struct SFixture
{
    SRandomGenerator generator;
    vector<SRandomParam> random_params;

    SFixture();

private:
    bool IsUnique(const SRandomParam& new_param);
};

SFixture::SFixture()
{
    size_t sections_number = generator.GetSectionsNumber();

    set<string> unique_sections;

    while (sections_number--) {
        unique_sections.insert(generator.GetSection());
    }

    vector<string> sections(unique_sections.begin(), unique_sections.end());

    size_t params_number = generator.GetParamsNumber();
    random_params.reserve(params_number);

    while (params_number--) {
        vector<string> sections_values;

        sections_number = sections.size();

        while (sections_number--) {
            if (generator.GetValue<bool>() || ! sections_number) {
                sections_values.push_back(sections[sections_number]);
            }
        }

        set<string> unique_names;
        size_t names_number = generator.GetNamesNumber();

        while (names_number--) {
            if (generator.GetValue<bool>() || !names_number) unique_names.insert(generator.GetName());
        }

        vector<string> names_values;

        for (auto unique_name : unique_names) {
            names_values.push_back(unique_name);
        }

        SRandomParam param(generator, move(sections_values), move(names_values));

        if (IsUnique(param)) {
            random_params.push_back(move(param));
        }
    }
}

bool SFixture::IsUnique(const SRandomParam& new_param)
{
    auto new_product = new_param.GetCartesianProduct();

    for (const auto& existing_param : random_params) {
        auto product = existing_param.GetCartesianProduct();
        decltype(new_product) intersection;

        set_intersection(product.begin(), product.end(),
                    new_product.begin(), new_product.end(),
                    inserter(intersection, intersection.end()));

        if (intersection.size()) return false;
    }

    return true;
}

BOOST_FIXTURE_TEST_SUITE(NetServiceParams, SFixture)

NCBITEST_AUTO_INIT()
{
    // Set client IP so AppLog entries could be filtered by it
    CRequestContext& req = CDiagContext::GetRequestContext();
    unsigned addr = CSocketAPI::GetLocalHostAddress();
    req.SetClientIP(CSocketAPI::ntoa(addr));
}

BOOST_AUTO_TEST_CASE(ConfigRegistry)
{
    CMemoryRegistry memory_registry;

    for (auto& param : random_params) {
        param.SetValue(memory_registry);
    }


    // Testing config containing all set sections

    CConfig config(memory_registry);
    CConfigRegistry config_registry(&config);

    for (auto& param : random_params) {
        param.CheckValue(config_registry);
    }


    // Testing sub-configs (one per set section)

    map<string, vector<SRandomParam>> params_by_sections;

    for (auto& param : random_params) {
        if (!param.indices.GetPriority()) continue;

        const auto& section = param.sections.front();
        params_by_sections[section].push_back(param);
    }

    for (auto& section_params : params_by_sections) {
        const CConfig::TParamTree* tree = config.GetTree();
        BOOST_CHECK(tree);

        const CConfig::TParamTree* sub_tree = tree->FindSubNode(section_params.first);
        BOOST_CHECK(sub_tree);

        CConfig sub_config(sub_tree);
        CConfigRegistry sub_config_registry(&config);

        for (auto& param : section_params.second) {
            param.CheckValue(sub_config_registry);
        }
    }
}

BOOST_AUTO_TEST_CASE(SynRegistry)
{
    TMemoryRegistries registries;

    for (auto& param : random_params) {
        param.SetValue(registries);
    }

    CSynRegistry syn_registry;

    for (auto& registry : registries) {
        syn_registry.Add(registry);
    }

    for (auto& param : random_params) {
        param.CheckValue(syn_registry);
    }
}

BOOST_AUTO_TEST_CASE(IncludeSynRegistry)
{
    TMemoryRegistries registries;

    for (auto& param : random_params) {
        param.SetIncluded(registries);
    }

    CSynRegistry include_registry;

    for (auto& registry : registries) {
        include_registry.Add(registry);
    }

    for (auto param : random_params) {
        param.CheckValue(include_registry);
    }
}

BOOST_AUTO_TEST_SUITE_END()
