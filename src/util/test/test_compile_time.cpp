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
 * Authors:  Sergiy Gotvyanskyy
 *
 * File Description:
 *   Test for ct::const_unordered_map
 *            ct::const_unordered_set
 *
 */

#include <ncbi_pch.hpp>

#include <util/compile_time.hpp>
//#include <util/cpubound.hpp>
//#include <util/impl/getmember.h>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "compile time const_map Unit Test");
}

#if 0
BOOST_AUTO_TEST_CASE(TestConstCharset1)
{
    constexpr
        ct::const_translate_table<int>::init_pair_t init_charset[] = {
            {500, {'A', 'B', 'C'}},
            {700, {'x', 'y', 'z'}},
            {400, {"OPRS"}}
    };

    constexpr
        ct::const_translate_table<int> test_xlate(100, init_charset);

    BOOST_CHECK(test_xlate['A'] == 500);
    BOOST_CHECK(test_xlate['B'] == 500);
    BOOST_CHECK(test_xlate['C'] == 500);
    BOOST_CHECK(test_xlate['D'] == 100);
    BOOST_CHECK(test_xlate['E'] == 100);
    BOOST_CHECK(test_xlate[0] == 100);
    BOOST_CHECK(test_xlate['X'] == 100);
    BOOST_CHECK(test_xlate['x'] == 700);
    BOOST_CHECK(test_xlate['P'] == 400);
}

using xlat_call = std::string(*)(const char*);

static std::string call1(const char* s)
{
    return s;
}
static std::string call2(const char* s)
{
    return std::string(s) + ":" + s;
}

BOOST_AUTO_TEST_CASE(TestConstCharset2)
{
    //xlate_call c1 = [](const char*s)->std::string { return s; };
    //xlate_call c2 = [](const char*s)->std::string { return std::string(s) + ":" + s; };

    constexpr ct::const_translate_table<xlat_call>::init_pair_t init_charset[] = {
            {call1, {'A', 'B', 'c'} },
            {call2, {'x', 'y', 'z'} }
    };
    constexpr ct::const_translate_table<xlat_call> test_xlat(nullptr, init_charset);

    BOOST_CHECK(test_xlat['P'] == nullptr);
    BOOST_CHECK(test_xlat['A']("alpha") == "alpha");
    BOOST_CHECK(test_xlat['x']("beta") == "beta:beta");
}
#endif

MAKE_TWOWAY_CONST_MAP(test_two_way1, ncbi::NStr::eNocase, const char*, const char*,
    {
        {"SO:0000001", "region"},
        {"SO:0000002", "sequece_secondary_structure"},
        {"SO:0000005", "satellite_DNA"},
        {"SO:0000013", "scRNA"},
        {"SO:0000035", "riboswitch"},
        {"SO:0000036", "matrix_attachment_site"},
        {"SO:0000037", "locus_control_region"},
        {"SO:0000104", "polypeptide"},
        {"SO:0000110", "sequence_feature"},
        {"SO:0000139", "ribosome_entry_site"}
    });


MAKE_TWOWAY_CONST_MAP(test_two_way2, ncbi::NStr::eNocase, const char*, int,
    {
        {"SO:0000001", 1},
        {"SO:0000002", 2},
        {"SO:0000005", 5},
        {"SO:0000013", 13},
        {"SO:0000035", 35}
    });

//test_two_way2_init

using bitset = ct::const_bitset<64 * 3, int>;
using bitset_pair = ct::const_pair<int, bitset>;

BOOST_AUTO_TEST_CASE(TestConstBitset)
{
    // this test ensures various approaches of instantiating const bitsets
    constexpr bitset t1(1, 10, 20, 30);
    constexpr bitset t2{ 1, 10, 20, 30 };

    constexpr bitset t3[] = { { 5,10}, {15, 10} };
    constexpr bitset_pair t4{ 4, t1 };
    constexpr bitset_pair t5(t4);
    constexpr bitset_pair t6[] = { {4, t1}, {5, t2} };
    constexpr bitset_pair t7[] = { {4, bitset(1, 2)}, {5, t3[0]} };

    constexpr bitset t8 = bitset::set_range(5, 12);

    BOOST_CHECK(t1.test(10));
    BOOST_CHECK(t2.test(20));
    BOOST_CHECK(!t3[1].test(11));
    BOOST_CHECK(!t4.second.test(11));
    BOOST_CHECK(t5.second.test(10));
    BOOST_CHECK(t6[1].second.test(1));
    BOOST_CHECK(t7[0].second.test(2));
    BOOST_CHECK(t8.test(5));
    BOOST_CHECK(t8.test(12));
    BOOST_CHECK(!t8.test(4));
    BOOST_CHECK(!t8.test(13));

    // array of pairs, all via aggregate initialization
    constexpr 
    bitset_pair t9[] = {
        {4,                //first
            {5, 6, 7, 8}}, //second
        {5,                //first
            {4, 5}}        //second
    };

    BOOST_CHECK(t9[1].second.test(5));
}

BOOST_AUTO_TEST_CASE(TestConstMap)
{
#if 1
    auto t1 = test_two_way1.find("SO:0000001");
    BOOST_CHECK((t1 != test_two_way1.end()) && (ncbi::NStr::CompareCase(t1->second, "region") == 0));

    auto t2 = test_two_way1.find("SO:0000003");
    BOOST_CHECK(t2 == test_two_way1.end());

    auto t3 = test_two_way1_flipped.find("RegioN");
    BOOST_CHECK((t3 != test_two_way1_flipped.end()) && (ncbi::NStr::CompareNocase(t3->second, "so:0000001") == 0));

    assert(test_two_way1.size() == 10);
    BOOST_CHECK(test_two_way1_flipped.size() == 10);

    auto t4 = test_two_way2.find("SO:0000002");
    BOOST_CHECK((t4 != test_two_way2.end()) && (t4->second == 2));

    auto t5 = test_two_way2.find("SO:0000003");
    BOOST_CHECK((t5 == test_two_way2.end()));

    auto t6 = test_two_way2_flipped.find(5);
    BOOST_CHECK((t6 != test_two_way2_flipped.end()) && (ncbi::NStr::CompareNocase(t6->second, "so:0000005") == 0));
#endif
};

BOOST_AUTO_TEST_CASE(TestConstSet)
{
    MAKE_CONST_SET(ts1, ncbi::NStr::eCase, const char*,
        { "a2", "a1", "a3" });

    MAKE_CONST_SET(ts2, ncbi::NStr::eNocase, const char*,
        { "b1", "b3", "B2"});

    for (auto r : ts1)
    {
        std::cout << r << std::endl;
    }
    for (auto r : ts2)
    {
        std::cout << r << std::endl;
    }

    BOOST_CHECK(ts1.find("A1") == ts1.end());
    BOOST_CHECK(ts1.find(std::string("A1")) == ts1.end());
    BOOST_CHECK(ts1.find("a1") != ts1.end());
    BOOST_CHECK(ts1.find(std::string("a1")) != ts1.end());
    BOOST_CHECK(ts1.find("a2") != ts1.end());
    BOOST_CHECK(ts1.find("a3") != ts1.end());
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a1"), "a1") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a2"), "a2") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a3"), "a3") == 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a1"), "b1") != 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a2"), "b2") != 0);
    BOOST_CHECK(ncbi::NStr::Compare(*ts1.find("a3"), "b3") != 0);

    BOOST_CHECK(ts2.find("A1") == ts2.end());
    BOOST_CHECK(ts2.find("a1") == ts2.end());
    BOOST_CHECK(ts2.find("b1") != ts2.end());
    BOOST_CHECK(ts2.find("B1") != ts2.end());
    BOOST_CHECK(ts2.find("b2") != ts2.end());
    BOOST_CHECK(ts2.find("B2") != ts2.end());
    BOOST_CHECK(ts2.find("b3") != ts2.end());
    BOOST_CHECK(ts2.find("B3") != ts2.end());

    auto b1_it = ts2.find("B1");
    BOOST_CHECK((*b1_it) == std::string("b1"));
    BOOST_CHECK((*b1_it) == std::string("B1"));
};

BOOST_AUTO_TEST_CASE(TestCRC32)
{
    constexpr auto hash_good_cs = ct::SaltedCRC32<ncbi::NStr::eCase>::ct("Good");
    constexpr auto hash_good_ncs = ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good");
    static_assert(hash_good_cs != hash_good_ncs, "not good");

    static_assert(948072359 == hash_good_cs, "not good");
    static_assert(
        ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good") == ct::SaltedCRC32<ncbi::NStr::eCase>::ct("good"),
        "not good");

    BOOST_CHECK(hash_good_cs  == ct::SaltedCRC32<ncbi::NStr::eCase>::general("Good", 4));
    BOOST_CHECK(hash_good_ncs == ct::SaltedCRC32<ncbi::NStr::eNocase>::general("Good", 4));
}

struct Implementation_via_type
{
    struct member
    {
        static const char* call()
        {
            return "member call";
        };
        const char* nscall() const
        {
            return "ns member call";
        };
    };
    struct general
    {
        static constexpr int value = 111;
        static const char* call()
        {
            return "general call";
        };
        const char* nscall() const
        {
            return "ns general call";
        };
    };
    struct sse41
    {
        static constexpr int value = 222;
        static const char* call()
        {
            return "sse41 call";
        };
        const char* nscall() const
        {
            return "ns sse41 call";
        };
    };
    struct avx
    {
        static constexpr int value = 333;
        static const char* call()
        {
            return "avx call";
        };
        const char* nscall() const
        {
            return "ns avx call";
        };
    };
};

struct Implementation_via_method
{
    static const char* member()
    {
        return "general method";
    };
    static const char* general()
    {
        return "general method";
    };
    static const char* sse41()
    {
        return "sse41 method";
    };
    static const char* avx()
    {
        return "avx";
    };
};

template<class _Impl>
struct RebindClass
{
    static std::string rebind(int a)
    {
        auto s1 = _Impl{}.nscall(); // non static access to _Impl class
        auto s2 = _Impl::call();
        return std::string("rebind class + ") + s1 + " + " + s2;
    };
};

template<class _Impl>
struct RebindDirect
{
    static std::string rebind(int a)
    {
        auto s = _Impl{}(); //call via operator()
        return std::string("rebind direct + ") + s;
    };
};

struct NothingImplemented
{
};

template<typename I>
struct empty_rebind
{
    using type = I;
};

#ifdef __CPUBOUND_HPP_INCLUDED__
BOOST_AUTO_TEST_CASE(TestCPUCaps)
{
    using Type0 = cpu_bound::TCPUSpecific<NothingImplemented, const char*>;
    using Type1 = cpu_bound::TCPUSpecific<Implementation_via_method, const char*>;
    using Type2 = cpu_bound::TCPUSpecific<Implementation_via_type, const char*>;
    using Type3 = cpu_bound::TCPUSpecificEx<RebindDirect, Implementation_via_method, std::string, int>;
    using Type4 = cpu_bound::TCPUSpecificEx<RebindClass,  Implementation_via_type, std::string, int>;

    Type0 inst0;
    Type1 inst1;
    Type2 inst2;
    Type3 inst3;
    Type4 inst4;

    //auto r = inst0();  it should assert

    std::string res1 = inst1();
    BOOST_CHECK(res1 == "sse41 method");
    std::string res2 = inst2();
    BOOST_CHECK(res2 == "sse41 call");
    std::string res3 = inst3(3);
    BOOST_CHECK(res3 == "rebind direct + sse41 method");
    std::string res4 = inst4(3);
    BOOST_CHECK(res4 == "rebind class + ns sse41 call + sse41 call");
}
#else
#if 0
BOOST_AUTO_TEST_CASE(TestCPUCaps)
{
    using T0 = GetMember<empty_rebind, NothingImplemented>;
    using T1 = GetMember<empty_rebind, Implementation_via_type>;
    using T2 = GetMember<RebindClass, Implementation_via_type>;
    using T3 = GetMember<empty_rebind, Implementation_via_method>;
    using T4 = GetMember<RebindDirect, Implementation_via_method>;

    //size_t n0 = sizeof(decltype(T0::Derived::member));
    //size_t n1 = sizeof(decltype(T1::Derived::member));
    std::cout << (T0::f == nullptr) << std::endl;
    std::cout << (T1::f == nullptr) << std::endl;
    //std::cout << (T2::f == nullptr) << std::endl;
    //std::cout << (T3::f == nullptr) << std::endl;
    //std::cout << (T4::f == nullptr) << std::endl;
}
#endif

#endif

#if 0
#include <unordered_set>

#ifdef NCBI_OS_MSWIN
#include <intrin.h>
#endif

class CTimeLapsed
{
private:
    uint64_t started = __rdtsc();
public:
    uint64_t lapsed() const
    {
        return __rdtsc() - started;
    };
};

class CTestPerformance
{
public:
    using string_set = std::set<std::string>;
    using unordered_set = std::unordered_set<std::string>;
    using hash_vector = std::vector<uint32_t>;
    using hash_t = cpu_bound::TCPUSpecific<ct::SaltedCRC32<ncbi::NStr::eCase>, uint32_t, const char*, size_t>;
    static constexpr size_t size = 16384;
    //static constexpr size_t search_size = size / 16;

    string_set   m_set;
    unordered_set m_hash_set;
    hash_vector  m_vector;
    std::vector<std::string> m_pool;
    hash_t m_hash;

    void FillData()
    {
        char buf[32];
        m_pool.reserve(size);
        m_vector.reserve(size);
        for (size_t i = 0; i < size; ++i)
        {
            size_t len = 5 + rand() % 10;
            for (size_t l = 0; l < len; ++l)
                buf[l] = '@' + rand() % 64;
            buf[len] = 0;
            m_pool.push_back(buf);
            m_set.emplace(buf);
            m_hash_set.emplace(buf);
            auto h = m_hash(buf, len);
            m_vector.push_back(h);
        }
        std::sort(m_vector.begin(), m_vector.end());
    }

    std::pair<uint64_t, uint64_t> TestMap()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto found = m_set.find(it);
            sum += found->size();
        }

        return { sum, lapsed.lapsed() };
    }

    std::pair<uint64_t, uint64_t> TestUnordered()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto found = m_hash_set.find(it);
            sum += found->size();
        }

        return { sum, lapsed.lapsed() };
    }

    std::pair<uint64_t, uint64_t> TestHashes()
    {
        CTimeLapsed lapsed;
        uint64_t sum = 0;
        for (auto it : m_pool)
        {
            auto h = m_hash(it.data(), it.size());
            auto found = std::lower_bound(m_vector.begin(), m_vector.end(), h);
            sum += *found;
        }

        return { sum, lapsed.lapsed() };
    }

    void RunTest()
    {
        FillData();
        auto res1 = TestMap();
        auto res2 = TestHashes();
        auto res3 = TestUnordered();

        std::cout << res1.second/1000 
            << " vs " << res2.second/1000 
            << " vs " << res3.second/1000
            << std::endl;
    }
};

BOOST_AUTO_TEST_CASE(TestPerformance)
{
    CTestPerformance test;
    test.RunTest();
}
#endif

#if 0

using table_type = ct::const_table<ncbi::NStr::ECase::eCase, const char*, int, int>;
using D01_t = table_type::MakeIndex<0, 1>;
using D10_t = table_type::MakeIndex<1, 0>;
using index01_t = D01_t::index_type;
using index10_t = D10_t::index_type;


constexpr 
table_type::row_type init_values[] = {
{ "Test1", 80, 200},
{ "Test2", 70, 300},
{ "Test3", 60, 300},
{ "Test4", 50, 300},
{ "Test5", 40, 300},
{ "Test6", 30, 300},
{ "Test7", 20, 300},
{ "Test8", 10, 300}
};

constexpr auto container01 = D01_t{}(init_values);
constexpr index01_t index01 = container01;

//alignas(std::hardware_constructive_interference_size)
constexpr auto container10 = D10_t{}(init_values);
constexpr index10_t index10 = container10;

BOOST_AUTO_TEST_CASE(TestPerformance)
{
    //std::cout << container01.first.size() << std::endl;
    //std::cout << container01.second.size() << std::endl;

    std::cout << std::hex << &container01.first << std::endl;
    std::cout << std::hex << &container10.first << std::endl;
    std::cout << container01.first[0] << std::endl;

    auto test1 = index01.at("Test3");
    auto test2 = index01.at("Test7");
    std::cout << test1 << ":" << test2 << std::endl;
    auto test3 = index10.at(60);
    std::cout << test3 << std::endl;
}

#endif

