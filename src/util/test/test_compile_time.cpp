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

#define DEBUG_MAKE_CONST_MAP(name)                                                    \
    BOOST_CHECK(name.get_alignment() == 0);

#define DEBUG_MAKE_TWOWAY_CONST_MAP(name)                                                   \
    BOOST_CHECK(name.first.get_alignment() == 0 && name.second.get_alignment()==0);

 //#define DEBUG_MAKE_CONST_SET(name)

#include <util/compile_time.hpp>
#include <array>

#include <corelib/test_boost.hpp>

//#include <cassert>


//static_assert(name ## _proxy.in_order(), "ct::const_unordered_map " #name "is not in order");

#include <common/test_assert.h>  /* This header must go last */

#define SO_MAP_DATA {\
        { "SO:0000001", "region"},\
        { "SO:0000002", "sequece_secondary_structure"},\
        { "SO:0000005", "satellite_DNA"},\
        { "SO:0000013", "scRNA"},\
        { "SO:0000035", "riboswitch"},\
        { "SO:0000036", "matrix_attachment_site"},\
        { "SO:0000037", "locus_control_region"},\
        { "SO:0000104", "polypeptide"},\
        { "SO:0000110", "sequence_feature"},\
        { "SO:0000139", "ribosome_entry_site"},\
        { "SO:0000140", "attenuator"},\
        { "SO:0000141", "terminator"},\
        { "SO:0000147", "exon"},\
        { "SO:0000165", "enhancer"},\
        { "SO:0000167", "promoter"},\
        { "SO:0000172", "CAAT_signal"},\
        { "SO:0000173", "GC_rich_promoter_region"},\
        { "SO:0000174", "TATA_box"},\
        { "SO:0000175", "minus_10_signal"},\
        { "SO:0000176", "minus_35_signal"},\
        { "SO:0000178", "operon"},\
        { "SO:0000185", "primary_transcript"},\
        { "SO:0000188", "intron"},\
        { "SO:0000204", "five_prime_UTR"},\
        { "SO:0000205", "three_prime_UTR"},\
        { "SO:0000234", "mRNA"},\
        { "SO:0000252", "rRNA"},\
        { "SO:0000253", "tRNA"},\
        { "SO:0000274", "snRNA"},\
        { "SO:0000275", "snoRNA"},\
        { "SO:0000276", "miRNA"},\
        { "SO:0000286", "long_terminal_repeat"},\
        { "SO:0000289", "microsatellite"},\
        { "SO:0000294", "inverted_repeat"},\
        { "SO:0000296", "origin_of_replication"},\
        { "SO:0000297", "D_loop"},\
        { "SO:0000298", "recombination_feature"},\
        { "SO:0000305", "modified_DNA_base"},\
        { "SO:0000313", "stem_loop"},\
        { "SO:0000314", "direct_repeat"},\
        { "SO:0000315", "TSS"},\
        { "SO:0000316", "CDS"},\
        { "SO:0000330", "conserved_region"},\
        { "SO:0000331", "STS"},\
        { "SO:0000336", "pseudogene"},\
        { "SO:0000374", "ribozyme"},\
        { "SO:0000380", "hammerhead_ribozyme"},\
        { "SO:0000385", "RNase_MRP_RNA"},\
        { "SO:0000386", "RNase_P_RNA"},\
        { "SO:0000404", "vault_RNA"},\
        { "SO:0000405", "Y_RNA"},\
        { "SO:0000409", "binding_site"},\
        { "SO:0000410", "protein_binding_site"},\
        { "SO:0000413", "sequence_difference"},\
        { "SO:0000418", "signal_peptide"},\
        { "SO:0000419", "mature_protein_region"},\
        { "SO:0000433", "non_LTR_retrotransposon_polymeric_tract"},\
        { "SO:0000454", "rasiRNA"},\
        { "SO:0000458", "D_gene_segment"},\
        { "SO:0000466", "V_gene_segment"},\
        { "SO:0000470", "J_gene_segment"},\
        { "SO:0000478", "C_gene_segment"},\
        { "SO:0000507", "pseudogenic_exon"},\
        { "SO:0000516", "pseudogenic_transcript"},\
        { "SO:0000551", "polyA_signal_sequence"},\
        { "SO:0000553", "polyA_site"},\
        { "SO:0000577", "centromere"},\
        { "SO:0000584", "tmRNA"},\
        { "SO:0000588", "autocatalytically_spliced_intron"},\
        { "SO:0000590", "SRP_RNA"},\
        { "SO:0000602", "guide_RNA"},\
        { "SO:0000624", "telomere"},\
        { "SO:0000625", "silencer"},\
        { "SO:0000627", "insulator"},\
        { "SO:0000644", "antisense_RNA"},\
        { "SO:0000646", "siRNA"},\
        { "SO:0000655", "ncRNA"},\
        { "SO:0000657", "repeat_region"},\
        { "SO:0000658", "dispersed_repeat"},\
        { "SO:0000673", "transcript"},\
        { "SO:0000685", "DNAsel_hypersensitive_site"},\
        { "SO:0000704", "gene"},\
        { "SO:0000705", "tandem_repeat"},\
        { "SO:0000714", "nucleotide_motif"},\
        { "SO:0000723", "iDNA"},\
        { "SO:0000724", "oriT"},\
        { "SO:0000725", "transit_peptide"},\
        { "SO:0000730", "gap"},\
        { "SO:0000777", "pseudogenic_rRNA"},\
        { "SO:0000778", "pseudogenic_tRNA"},\
        { "SO:0001021", "chromosome_preakpoint"},\
        { "SO:0001035", "piRNA"},\
        { "SO:0001037", "mobile_genetic_element"},\
        { "SO:0001055", "transcriptional_cis_regulatory_region"},\
        { "SO:0001059", "sequence_alteration"},\
        { "SO:0001062", "propeptide"},\
        { "SO:0001086", "sequence_uncertainty"},\
        { "SO:0001087", "cross_link"},\
        { "SO:0001088", "disulfide_bond"},\
        { "SO:0001268", "recoding_stimulatory_region"},\
        { "SO:0001411", "biological_region"},\
        { "SO:0001484", "X_element_combinatorical_repeat"},\
        { "SO:0001485", "Y_prime_element"},\
        { "SO:0001496", "telomeric_repeat"},\
        { "SO:0001649", "nested_repeat"},\
        { "SO:0001682", "replication_regulatory_region"},\
        { "SO:0001720", "epigenetically_modified_region"},\
        { "SO:0001797", "centromeric_repeat"},\
        { "SO:0001833", "V_region"},\
        { "SO:0001835", "N_region"},\
        { "SO:0001836", "S_region"},\
        { "SO:0001877", "lnc_RNA"},\
        { "SO:0001917", "CAGE_cluster"},\
        { "SO:0002020", "boundary_element"},\
        { "SO:0002072", "sequence_comparison"},\
        { "SO:0002087", "pseudogenic_CDS"},\
        { "SO:0002094", "non_allelic_homologous_recombination_region"},\
        { "SO:0002154", "mitotic_recombination_region"},\
        { "SO:0002155", "meiotic_recombination_region"},\
        { "SO:0002190", "enhancer_blocking_element"},\
        { "SO:0002191", "imprinting_control_region"},\
        { "SO:0002205", "response_element"},\
        { "SO:0005836", "regulatory_region"},\
        { "SO:0005850", "primary_binding_site"},\
        { "SO:0000000", ""}\
}


NCBITEST_AUTO_INIT()
{
    boost::unit_test::framework::master_test_suite().p_name->assign(
        "compile time const_map Unit Test");
}

//test_two_way2_init

using bitset = ct::const_bitset<64 * 3, int>;
using bitset_pair = std::pair<int, bitset>;

BOOST_AUTO_TEST_CASE(TestConstBitset)
{
    // this test ensures various approaches of instantiating const bitsets
    constexpr bitset t1{ 1, 10, 20, 30 };

    constexpr bitset t2{ 1, 10, 20, 30 };

    constexpr bitset t3[] = { { 5,10}, {15, 10} };
    constexpr bitset_pair t4{ 4, t1 };
    constexpr bitset_pair t5(t4);
    constexpr bitset_pair t6[] = { {4, t1}, {5, t2} };
    constexpr bitset_pair t7[] = { {4, {1, 2}}, {5, t3[0]} };

    constexpr bitset t8 = bitset::set_range(5, 12);

    // array of pairs, all via aggregate initialization
    constexpr
        bitset_pair t9[] = {
            {4,                //first
                {5, 6, 7, 8}}, //second
            {5,                //first
                {4, 5}}        //second
    };

    constexpr bitset t10{ "010101" };

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

    BOOST_CHECK(t9[1].second.test(5));

    BOOST_CHECK(t10.size() == 3);
    BOOST_CHECK(!t10.test(0));
    BOOST_CHECK(t10.test(1));
    BOOST_CHECK(!t10.test(2));
    BOOST_CHECK(t10.test(3));
    BOOST_CHECK(!t10.test(4));
    BOOST_CHECK(t10.test(5));
    BOOST_CHECK(!t10.test(6));
    BOOST_CHECK(!t10.test(7));
    BOOST_CHECK(!t10.test(8));
    BOOST_CHECK(!t10.test(9));
}

BOOST_AUTO_TEST_CASE(TestConstMap)
{
    MAKE_TWOWAY_CONST_MAP(test_two_way1, ct::tagStrNocase, ct::tagStrNocase,
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


    MAKE_TWOWAY_CONST_MAP(test_two_way2, ct::tagStrNocase, int,
        {
            {"SO:0000001", 1},
            {"SO:0000002", 2},
            {"SO:0000005", 5},
            {"SO:0000013", 13},
            {"SO:0000035", 35}
        });

    test_two_way1.first.find("SO:0000001");
    test_two_way1.first.find(ncbi::CTempString("SO:0000001"));
    test_two_way1.first.find(std::string("SO:0000001"));
#ifdef __cpp_lib_string_view
    test_two_way1.first.find(std::string_view("SO:0000001"));
#endif    

    auto t1 = test_two_way1.first.find("SO:0000001");
    BOOST_CHECK((t1 != test_two_way1.first.end()) && (ncbi::NStr::CompareCase(t1->second, "region") == 0));

    auto t2 = test_two_way1.first.find("SO:0000003");
    BOOST_CHECK(t2 == test_two_way1.first.end());

    auto t3 = test_two_way1.second.find("RegioN");
    BOOST_CHECK((t3 != test_two_way1.second.end()) && (ncbi::NStr::CompareNocase(t3->second, "so:0000001") == 0));

    assert(test_two_way1.first.size() == 10);
    BOOST_CHECK(test_two_way1.second.size() == 10);

    auto t4 = test_two_way2.first.find("SO:0000002");
    BOOST_CHECK((t4 != test_two_way2.first.end()) && (t4->second == 2));

    auto t5 = test_two_way2.first.find("SO:0000003");
    BOOST_CHECK((t5 == test_two_way2.first.end()));

    auto t6 = test_two_way2.second.find(5);
    BOOST_CHECK((t6 != test_two_way2.second.end()) && (ncbi::NStr::CompareNocase(t6->second, "so:0000005") == 0));
}

BOOST_AUTO_TEST_CASE(TestConstSet)
{
    MAKE_CONST_SET(ts1, ct::tagStrCase, 
      { {"a2"}, {"a1"}, {"a3"} });

    MAKE_CONST_SET(ts2, ct::tagStrNocase,
      { {"b1"}, {"b3"}, {"B2"} });

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
}

BOOST_AUTO_TEST_CASE(TestCRC32)
{
    using hash_type = ct::SaltedCRC32<ncbi::NStr::eCase>::type;

    constexpr hash_type good_cs = 0x38826fa7;
    constexpr hash_type good_ncs = 0xefa77b2c;
    constexpr auto hash_good_cs = ct::SaltedCRC32<ncbi::NStr::eCase>::ct("Good");
    constexpr auto hash_good_ncs = ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good");

    auto hash_good_cs_rt = ct::SaltedCRC32<ncbi::NStr::eCase>::rt("Good", 4);
    auto hash_good_ncs_rt = ct::SaltedCRC32<ncbi::NStr::eNocase>::rt("Good", 4);

    static_assert(hash_good_cs != hash_good_ncs, "not good");
    static_assert(good_cs == hash_good_cs, "not good");
    static_assert(good_ncs == hash_good_ncs, "not good");

    std::cout << std::hex
              << "Hash values:" << std::endl
              << hash_good_cs << std::endl
              << hash_good_ncs << std::endl
              << hash_good_cs_rt << std::endl
              << hash_good_ncs_rt << std::endl
              << good_cs << std::endl
              << good_ncs << std::endl
              << std::dec
    ;

    static_assert(
        ct::SaltedCRC32<ncbi::NStr::eNocase>::ct("Good") == ct::SaltedCRC32<ncbi::NStr::eCase>::ct("good"),
        "not good");

    BOOST_CHECK(hash_good_cs  == hash_good_cs_rt);
    BOOST_CHECK(hash_good_ncs == hash_good_ncs_rt);
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


template<typename _Left, typename _Right>
void CompareArray(const _Left& l, const _Right& r)
{
    for (const auto& rec : l)
    {
        auto it = r.find(rec.first);
        bool found = it != r.end();
        BOOST_CHECK(found);
        if (found)
        {
            BOOST_CHECK(0 == ncbi::NStr::CompareNocase(rec.first, it->first));
            BOOST_CHECK(0 == ncbi::NStr::CompareNocase(rec.second, it->second));
        }
    }
}
template<typename _Left, typename _Right>
void CompareArrayFlipped(const _Left& l, const _Right& r)
{
    for (const auto& rec : l)
    {
        auto it = r.find(rec.second);
        bool found = it != r.end();
        BOOST_CHECK(found);
        if (found)
        {
            BOOST_CHECK(0 == ncbi::NStr::CompareNocase(rec.first, it->second));
            BOOST_CHECK(0 == ncbi::NStr::CompareNocase(rec.second, it->first));
        }
    }
}

BOOST_AUTO_TEST_CASE(TestConstMap2)
{
    MAKE_CONST_MAP(cm1, ct::tagStrNocase, ct::tagStrNocase, SO_MAP_DATA);
    MAKE_TWOWAY_CONST_MAP(cm2, ct::tagStrNocase, ct::tagStrNocase, SO_MAP_DATA);

    std::cout << "Const map sizes:" << std::endl
        << "Record size:" << sizeof(decltype(cm1)::value_type) << std::endl
        << "Straight map:" << cm1.size() << ":" << sizeof(cm1) << std::endl
        << "Two way map:" << cm2.first.size() << ":" << sizeof(cm2) << std::endl;

    const std::pair<const char*, const char*> init[] = SO_MAP_DATA;

    CompareArray(init, cm1);
    CompareArray(init, cm2.first);
    CompareArrayFlipped(init, cm2.second);

    auto ret1 = cm1.equal_range("A");
    BOOST_CHECK(ret1.first == ret1.second);
    BOOST_CHECK(ret1.first != cm1.end() && ret1.first->first != "A");


    auto ret2 = cm1.equal_range("SO:0000013");
    BOOST_CHECK(ret2.first != ret2.second);
    BOOST_CHECK(ret2.first != cm1.end()
        && ret2.first->first == "SO:0000013"
        && ret2.second->first != "SO:0000013");
}

BOOST_AUTO_TEST_CASE(TestConstMap3)
{
    enum eEnum
    {
        a, b, c
    };

    enum class cEnum: char
    {
        one,
        two,
        three,
        many
    };
    static_assert(std::is_enum<cEnum>::value, "Not enum");
    static_assert(std::is_enum<eEnum>::value, "Not enum");
    static_assert(!std::numeric_limits<cEnum>::is_integer, "Why integer");
    static_assert(!std::numeric_limits<eEnum>::is_integer, "Why integer");

    MAKE_TWOWAY_CONST_MAP(cm1, cEnum, std::string,
        {
        {cEnum::one, "One"},
        {cEnum::two, "Two"},
        {cEnum::three, "Three"},
        {cEnum::many, "Many"}
        });

    for (auto rec : cm1.first)
    {
        std::cout << rec.second << std::endl;
    }

}

template<typename _T>
static
void ReportSortList(const _T& input)
{
    std::cout << input.first.size << std::endl;
    std::cout << input.second.size() << std::endl;
    std::cout << "Hashtable: ";
    for (auto rec: input.first.m_array)
    {
        std::cout << rec << " ";
    }
    std::cout << std::endl;
    std::cout << "Values: ";
    for (auto rec: input.second)
    {
        std::cout << rec << " ";
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(TestConstSorter)
{
    constexpr int data[] = { 100, 90, 50, 90, 1 };

    using deduce1 = ct::DeduceHashedType<int>;
    using traits = ct::simple_sort_traits<deduce1>;
    using sorter_unique = ct::TInsertSorter<traits, true>;
    using sorter_non_unique = ct::TInsertSorter<traits, false>;

    auto sorted_u = sorter_unique{}(data);
    auto sorted_nu = sorter_non_unique{}(data);

    ReportSortList(sorted_u);
    ReportSortList(sorted_nu);

}

BOOST_AUTO_TEST_CASE(TestMapOfPair)
{
    using bitset1 = ct::const_bitset<100, int>;
    using Pair1 = std::pair<int, int>;
    using Tuple1 = std::tuple<int, const char*, int>;
    using Tuple2 = std::tuple<int, const char*, bitset1>;

    MAKE_CONST_MAP(m1, std::string, Pair1,
        {
            { "one", {1, 1}},
            { "two", {2, 2}},
            { "many", {3, 3}},
        }
        )

    MAKE_CONST_MAP(m2, ct::tagStrNocase, Tuple1,
        {
            {"one", { 1, "word", 1}},
            {"two", { 2, "another", 2}},
        }
        )

    MAKE_CONST_MAP(m3, ct::tagStrNocase, Tuple2,
        {
            {"one", {1, "ABC", {1,3,5}}},
            {"two", {2, "abc", {2,4,6}}},
        })
}



    struct EmptyIndex
    {
    };

    template<size_t I, typename _InitTypes, typename _HashTypes>
    struct tuple_sort_traits
    {
        using hash_type = typename std::tuple_element<I, _HashTypes>::type::hash_type;
        using value_type = size_t;
        using init_type = _InitTypes;

        struct Pred
        {
            template<typename _Input>
            constexpr bool operator()(const _Input& input, size_t l, size_t r)
            {
                return std::get<I>(input[l]) < std::get<I>(input[r]);
            }
        };
        template<typename _Input>
        static constexpr auto construct(const _Input&, size_t J)
        {
            return J;
        }
        static constexpr hash_type get_hash(const init_type& v)
        {
            return std::get<I>(v);
        }
    };

    // is_selected template searches std::index_sequence for a presence of I value
    // _T must be void, or std::index_sequence<...> of any length, including empty
    // this template generates no CPU code and can be used in expressions
    template<size_t I, typename _T>
    struct is_included
    { }; // random types are not supported

    template<size_t I, size_t...Ints>
    struct is_included<I, std::index_sequence<Ints...> >
    {
        template<size_t _First, size_t..._Rest>
        struct check_selected
        {
            static constexpr bool value = (I == _First) | check_selected<_Rest...>::value;
        };
        template<size_t _First>
        struct check_selected<_First>
        {
            static constexpr bool value = (I == _First);
        };

        static constexpr bool value = check_selected<Ints...>::value;
    };
    // any selected
    template<size_t I>
    struct is_included<I, void>
    {
        static constexpr bool value = true;
    };
    // any selected
    template<size_t I>
    struct is_included<I, std::index_sequence<>>: is_included<I, void>
    { };


template<typename _HashTypes, typename _Rows, typename _Indices>
class const_indexed_table_proxy: public _Rows
{
public:
    using rows_t = _Rows;
    using row_t = typename _Rows::value_type;
    using indices_t = typename std::remove_cv<_Indices>::type;
    static constexpr size_t m_width = std::tuple_size<indices_t>::value;

    using const_iterator = typename rows_t::const_iterator;
    using iterator       = typename rows_t::iterator;

    constexpr const_indexed_table_proxy() = default;
    constexpr const_indexed_table_proxy(const _Rows& rows, const indices_t& indices)
        : _Rows(rows), m_indices{ indices }
    {}

    static constexpr size_t width() noexcept { return m_width; }

    template<size_t i, typename _K, typename _ColumnType = typename std::tuple_element<i, _HashTypes>::type>
    typename std::enable_if<
            std::is_constructible<typename _ColumnType::value_type, _K>::value &&
            !std::is_same<EmptyIndex, typename std::tuple_element<i, indices_t>::type>::value,
            const_iterator>::type
    find(_K&& _key) const
    {
        typename _ColumnType::value_type temp = std::forward<_K>(_key);
        typename _ColumnType::init_type  key(temp);
        const auto& hashes = std::get<i>(m_indices).first;
        size_t hash_it = std::distance(hashes.m_array.begin(), std::lower_bound(hashes.m_array.begin(), hashes.m_array.begin() + _Rows::size(), key));
        if (hash_it != _Rows::size())
        {
            if (hashes.m_array[hash_it] == key)
            {
                const auto& row_index = std::get<i>(m_indices).second;
                auto row_offset = row_index[hash_it];
                auto row_it = _Rows::begin() + row_offset;
                typename _ColumnType::value_type found = std::get<i>(*row_it);
                if (found == temp)
                    return row_it;
            }
        }
        return _Rows::end();
    }
    template<size_t i, size_t col, typename _K, 
        typename _ColumnType = typename std::tuple_element<i, _HashTypes>::type,
        typename _ResultType = typename std::tuple_element<col, row_t>::type>
    typename std::enable_if<
        std::is_constructible<typename _ColumnType::value_type, _K>::value &&
        !std::is_same<EmptyIndex, typename std::tuple_element<i, indices_t>::type>::value,
        const _ResultType&>::type
    get(_K&& _key) const
    {
        auto it = find<i>(std::forward<_K>(_key));
        if (it == _Rows::end())
            throw std::out_of_range("invalid const_table record");
        return std::get<col>(*it);
    }
    protected:
    indices_t m_indices{};
};

// _Selected parameter limit which columns will receive an searchable index
// _Selected parameter can be void or empty std::index_sequence<> (that means that all columns should receive index if it can)
//  or non empty std::index_sequence with enlisted number of columns in free order
template<typename _Selected, typename..._Types>
class MakeConstTable
{
public:
    using hash_types = ct::const_tuple<
        typename ct::DeduceHashedType<_Types> ...>;
    using init_types = ct::const_tuple<
        typename ct::DeduceHashedType<_Types>::init_type ...>;
    using value_types = ct::const_tuple<
        typename ct::DeduceHashedType<_Types>::value_type ...>;

    template<size_t N>
    constexpr auto operator()(const init_types(&init)[N]) const
    {
        auto indices = make_indices(init, std::make_index_sequence<sizeof...(_Types)>{});
        auto rows = fill_rows(init, std::make_index_sequence<N>{});
        return const_indexed_table_proxy<hash_types, decltype(rows), decltype(indices)>(rows, indices);
    }
private:

    // empty index for a column that cannot be indexed or not needed
    template<typename _Init, size_t _Column>
    static constexpr auto make_table_index(const _Init& init, 
         std::integral_constant<size_t, _Column>,
         std::integral_constant<bool, false> can_sort)
    {
        return EmptyIndex{};
    }
    // indices are constucted only for those columns that set can_index and
    // its number is included in _Selected parameter
    template<
        typename _Init, 
        size_t _Column,
        class _SortTraits = tuple_sort_traits<_Column, init_types, hash_types>,
        class _Sorter = ct::TInsertSorter<_SortTraits, false>>
    static constexpr auto make_table_index(const _Init& init, 
         std::integral_constant<size_t, _Column>,
         std::integral_constant<bool, true> can_sort)
    {
        return _Sorter{}(init);
    }
    template<
        typename _Init,
        size_t _Column,
        bool _CanIndex = 
            std::tuple_element<_Column, hash_types>::type::can_index &&
            is_included<_Column, _Selected>::value>
    static constexpr auto make_table_index(const _Init& init, std::integral_constant<size_t, _Column> column)
    {   
        return make_table_index(init, column, std::integral_constant<bool, _CanIndex>{});
    }
    template<typename _Init, std::size_t... _Columns>
    static constexpr auto make_indices(const _Init& init, std::index_sequence<_Columns...>)
    {
        return std::make_tuple(make_table_index(init, std::integral_constant<size_t, _Columns>{}) ...);
    }

    template<size_t...Ints>
    static constexpr value_types convert_tuple(const init_types& tup, std::index_sequence<Ints...>)
    {
        return value_types(std::get<Ints>(tup)...);
    }
    static constexpr value_types convert_tuple(const init_types& tup)
    {
        return convert_tuple(tup, std::make_index_sequence<sizeof...(_Types)>{});
    }

    // this needed to convert 'init' tuples into 'values' tuples
    template<typename _Input, size_t...Ints>
    static constexpr auto fill_rows(const _Input& input, std::index_sequence<Ints...>)
        -> ct::const_array<value_types, sizeof...(Ints)>
    {
        return { { convert_tuple(input[Ints])...} };
    }
};

typedef int (*func_ptr)(int);
static int SomeFunc(int i)
{
    return i * 2;
}

template<typename _Index>
void ReportTableIndex(const _Index& index)
{
    std::cout << sizeof(index) << std::endl;
    std::cout << "  " << index.first.size << std::endl;
    std::cout << "  " << index.first.m_array.size() << std::endl;
    std::cout << "  " << index.second.size() << std::endl;
    for (auto rec: index.second)
    {
        std::cout << "    " << rec << std::endl;
    }
}

BOOST_AUTO_TEST_CASE(TestConstTable)
{
    using selected = void;
    //using selected = std::index_sequence<1, 3>;
    //using selected = std::index_sequence<>;

    using type = MakeConstTable<selected, uint8_t, int, ct::tagStrNocase, ct::tagStrNocase, func_ptr > ;

    static constexpr type::init_types init[] = {
        {5, 500, "Five", "Five hundred", SomeFunc },
        {1, 100, "One", "Hundred", SomeFunc },
        {100, 1'000'000, "Many", "Million", SomeFunc},
    };
    constexpr auto sorted = type{}(init);

    std::cout << sizeof(sorted) << std::endl;
    auto it1 = sorted.find<1>(500);
    auto it2 = sorted.find<1>(501);
    auto it3 = sorted.find<1>(1);
    auto it4 = std::get<4>(*sorted.find<1>(1'000'000));
    auto it4_1 = sorted.get<1, 4>(1'000'000)(50);
    auto it5 = sorted.find<1>(1'000'001);
    auto it6 = sorted.find<1>(std::numeric_limits<int>::max());

    auto it7 = sorted.find<2>("five");

    //auto it8 = sorted.find<4>(&SomeFunc);
#if 0
    //ReportTableIndex(std::get<0>(sorted));
    //ReportTableIndex(std::get<1>(sorted));
    //ReportTableIndex(std::get<2>(sorted));
    //ReportTableIndex(std::get<3>(sorted));
    //ReportTableIndex(std::get<4>(sorted));
#endif

}

template<typename _Selected, size_t...Ints>
static auto make_selected(std::index_sequence<Ints...>)
->std::array<bool, sizeof...(Ints)>
{
    return { { is_included<Ints, _Selected>::value ... } };
}
BOOST_AUTO_TEST_CASE(TestIntSequence)
{
    using select = std::index_sequence<0, 5, 9>;
    //using select = void;

    auto ar = make_selected<select>(std::make_index_sequence<10>{});
    for (auto rec: ar)
    {
        std::cout << rec << " ";
    }
    std::cout << std::endl;
    BOOST_CHECK(ar[0]);
    BOOST_CHECK(ar[5]);
    BOOST_CHECK(ar[9]);
    BOOST_CHECK(!ar[1]);
    BOOST_CHECK(!ar[2]);
    BOOST_CHECK(!ar[3]);
    BOOST_CHECK(!ar[4]);
    BOOST_CHECK(!ar[6]);
    BOOST_CHECK(!ar[7]);
    BOOST_CHECK(!ar[8]);
}

#if 0
template<bool _If, typename _T1, typename _T2>
struct choose_type
{
};
template<typename _T1, typename _T2>
struct choose_type<true, _T1, _T2>
{
    using type = _T1;
};
template<typename _T1, typename _T2>
struct choose_type<false, _T1, _T2>
{
    using type = _T2;
};

template<typename _Selected, typename _Tuple, typename _Sequence>
struct build_tuple
{
};
template<typename _Selected, typename _Tuple, size_t...Ints>
struct build_tuple<_Selected, _Tuple, std::index_sequence<Ints...>>
{
    using type = _Tuple;
    //using type = std::tuple<
    //    std::tuple_element<Ints, _Tuple>::type ...>;
};
template<typename _Selected, typename..._Types>
struct selected_tuple
{
    using tuple_type = std::tuple<_Types...>;
    using type = typename build_tuple<_Selected, tuple_type, std::make_index_sequence<sizeof...(_Types)>>::type;
};

//using tuple_type = selected_tuple<select, int, int, int>::type;
#endif


#ifdef NCBI_HAVE_CXX17

using namespace std::literals;

namespace compile_time_bits
{
    template<typename _T>
    struct DeduceOrderedType: DeduceHashedType<_T>
    {                
    };

    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceOrderedType<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
            : DeduceHashedType<ct_string<case_sensitive>, ct_string<case_sensitive>, ct_string<case_sensitive>>
    {
    };
}

namespace ct
{   
    template<typename _T>
    struct MakeOrderedConstSet
    {
        using deduced = DeduceOrderedType<_T>;
        //using type = const_unordered_set<_T>;

        using sorter_t = TInsertSorter<simple_sort_traits<deduced>, true>;
        using init_type = typename sorter_t::init_type;

        template<size_t N>
        constexpr auto operator()(const init_type(&input)[N]) const 
        {
            return sorter_t{}(input);
        }

    };
}

template<typename _Array>
static void PrintSorted(const _Array& sorted)
{
    std::cout << "sorted: " << sorted.second.m_size;
    for (auto rec: sorted.second.m_data)
    {
        std::string_view v = rec;
        std::cout << " " << v;
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(TestCXX17StringCompare)
{
    using maker = ct::MakeOrderedConstSet<compile_time_bits::tagStrNocase>;

    //maker::init_type _init[] = {"AA", "C", "aA", "b", "a", "B"};
    //maker::init_type _init[] = {"a", "ab", "aA"};

    constexpr auto sorted1 = maker{}({"a", "ab", "aA"});
    constexpr auto sorted2 = maker{}({"AA", "C", "aA", "b", "a", "B"});

    PrintSorted(sorted1);
    PrintSorted(sorted2);
}

#endif
