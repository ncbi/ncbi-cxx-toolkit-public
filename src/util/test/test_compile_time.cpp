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
 *            ct::const_set
 *            ct::const_map
 *
 */

#include <ncbi_pch.hpp>

#if 0
#define DEBUG_MAKE_CONST_MAP(name)                                                          \
    BOOST_CHECK(name.get_alignment() == 0);

#define DEBUG_MAKE_TWOWAY_CONST_MAP(name)                                                   \
    BOOST_CHECK(name.first.get_alignment() == 0 && name.second.get_alignment()==0);

#define DEBUG_MAKE_CONST_SET(name)                                                          \
    BOOST_CHECK(name.get_alignment() == 0);

#endif

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

// local file printout functions
namespace std
{
    template<typename _Traits, size_t _Bits, class _T>
    basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const ct::const_bitset<_Bits, _T>& v)
    {
        using enum_type = typename ct::const_bitset<_Bits, _T>::u_type;
        for (auto rec: v)
           _Ostr << static_cast<enum_type>(rec) << " ";

        return _Ostr;
    }

    template<typename _Traits, typename _F, typename _S> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const pair<_F, _S>& v)
    {
        return _Ostr << v.first;
    }
}


enum class ENumbers
{
    one, two, three, many, more, much_more,
};

using bitset = ct::const_bitset<64 * 3, int>;
using bitset_pair = std::pair<int, bitset>;
using ebitset = ct::const_bitset<64, ENumbers>;

BOOST_AUTO_TEST_CASE(TestConstBitset)
{
    // this test ensures various approaches of instantiating const bitsets
    constexpr bitset t1{ 1, 10, 20, 30 };

    constexpr bitset t2{ 1, 10, 20, 30, 30 };

    constexpr bitset t3[] = { { 5,10}, {15, 10} };
    constexpr bitset_pair t4{ 4, t1 };
    constexpr bitset_pair t5(t4);
    constexpr bitset_pair t6[] = { {4, t1}, {5, t2} };
    constexpr bitset_pair t7[] = { {4, {1, 2}}, {5, t3[0]} };

    constexpr bitset t8 { bitset::set_range(5, 12) };

    // array of pairs, all via aggregate initialization
    constexpr
        bitset_pair t9[] = {
            {4,                //first
                {5, 6, 7, 8}}, //second
            {5,                //first
                {4, 5}}        //second
    };

    constexpr bitset t10{ "010101" };

    bitset t11 ( bitset::range{1, 5} );
    constexpr bitset t12 { bitset::range{2, 20} };
    constexpr bitset t13 = 5;

    BOOST_CHECK(t1.test(10));
    BOOST_CHECK(t2.test(20));
    BOOST_CHECK(t1.size() == 4);
    BOOST_CHECK(t2.size() == 4);
    BOOST_CHECK(t8.size() == 8);
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
    BOOST_CHECK(!t11.test(9));
    BOOST_CHECK(t12.test(20));
    BOOST_CHECK(t13.test(5));

    t11 += 9;
    BOOST_CHECK(t11.test(9));

    t11 += {10, 12};
    BOOST_CHECK(t11.test(10));
    std::cout << "t11: " << t11 << std::endl;
    t11 -= {1, 3};
    BOOST_CHECK(t11.test(2));
    BOOST_CHECK(!t11.test(1));
    BOOST_CHECK(!t11.test(3));
    std::cout << "t11: " << t11 << std::endl;
}

BOOST_AUTO_TEST_CASE(TestConstBitsetEnumClass)
{
    ebitset b1 { ENumbers::one, ENumbers::much_more };
    BOOST_CHECK(b1.test(ENumbers::one));
    BOOST_CHECK(!b1.test(ENumbers::two));
    std::cout << "b1: " << b1 << std::endl;
    constexpr ebitset b2 { ebitset::range{ENumbers::one, ENumbers::many} };
    std::cout << "b2: " << b2 << std::endl;

    b1 += ENumbers::two;
    BOOST_CHECK(b1.test(ENumbers::two));
    b1 -= {ENumbers::two, ENumbers::much_more};
    BOOST_CHECK(!b1.test(ENumbers::two));
    BOOST_CHECK(!b1.test(ENumbers::much_more));
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

    //assert(test_two_way1.first.size() == 10);
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

    MAKE_CONST_SET(ts3, ct::tagStrCase,
      { "a2", "a1", "a3" } );

    MAKE_CONST_SET(ts4, ENumbers, { ENumbers::one, ENumbers::much_more } );

    for (auto r : ts1)
    {
        std::cout << r << std::endl;
    }
    for (auto r : ts2)
    {
        std::cout << r << std::endl;
    }
    for (auto r : ts4)
    {
        std::cout << static_cast<int>(r) << std::endl;
    }

    BOOST_CHECK(ts3.size() == 3);
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
    BOOST_CHECK((*b1_it) != std::string("B1"));

    BOOST_CHECK(ts4.find( ENumbers::one) != ts4.end());
    BOOST_CHECK(ts4.find( ENumbers::much_more) != ts4.end());
    BOOST_CHECK(ts4.find( ENumbers::many) == ts4.end());
    
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

template<typename _Left, typename _Right>
static void CompareArray(const _Left& l, const _Right& r)
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
static void CompareArrayFlipped(const _Left& l, const _Right& r)
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
    MAKE_CONST_MAP(cm3, ENumbers, ct::tagStrNocase, 
    {
        { ENumbers::one, "One" },
        { ENumbers::much_more, "Much More" },
    });


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

    BOOST_CHECK(cm3.find(ENumbers::two) == cm3.end());
    auto cm3_it = cm3.find(ENumbers::one);
    BOOST_CHECK(cm3_it != cm3.end());
    if (cm3_it != cm3.end())
    {
        //BOOST_CHECK(cm3_it->second == "one"); //case insensitive
        BOOST_CHECK(cm3_it->second == "One"); //case sensitive
        BOOST_CHECK(cm3_it->second != "two"); //case sensitive
    }
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
void ReportSortedList(const char* prefix, const _T& input)
{
    std::cout << prefix << ": " << std::get<0>(input) << ": ";
    for (auto rec: std::get<1>(input))
    {
        std::cout << rec << " ";
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(TestConstSorter)
{
    //constexpr int data[] = { 100, 90, 50, 90, 1 };
    constexpr ct_const_array<int, 5> data{ { 100, 90, 50, 90, 1 } };

    using deduce1 = ct::DeduceType<int>;
    using traits = ct::simple_sort_traits<deduce1>;
    using sorter_unique = ct::TInsertSorter<traits, true>;
    using sorter_non_unique = ct::TInsertSorter<traits, false>;

    constexpr auto sorted_u = sorter_unique::sort(data);
    constexpr auto sorted_nu = sorter_non_unique::sort(data);

    ReportSortedList("Unique values", sorted_u);
    ReportSortedList("Non-unique values", sorted_nu);
}

BOOST_AUTO_TEST_CASE(TestSorting)
{
    constexpr int init1[] = { 3, 5, 1 };
    constexpr auto res1 = ct::sort(init1);
    
    constexpr auto init2 = ct::make_array({ 1, 2, 3});
    constexpr auto res2 = ct::sort(init2);

    static constexpr int init3[] = { 1, 2, 3};
    constexpr auto res3 = ct::sort(init3);
    
    static constexpr auto init4 = ct::make_array({ 1, 2, 3});
    constexpr auto res4 = ct::sort(init4);

    constexpr auto init5 = ct::make_array(1, 2, 3);
    constexpr auto res5 = ct::sort(init5);

    constexpr auto res6 = ct::sort("A1B2C3D4"); // it can sort this too, because char[] is array

    // little help to deduce array element type
    constexpr auto init7 = ct::make_array(ct::ct_string{"1"}, "2", "3");
    constexpr auto res7 = ct::sort(init7);

    BOOST_CHECK(res1.size() == 3);
    BOOST_CHECK(res2.size() == 3);
    BOOST_CHECK(res3.size() == 3);
    BOOST_CHECK(res4.size() == 3);
    BOOST_CHECK(res5.size() == 3);
    BOOST_CHECK(res6.size() == 9);
    const char* expected = "\0001234ABCD";
    BOOST_CHECK(memcmp(res6.data(), expected, res5.size())==0);
    BOOST_CHECK(res7.size() == 3);
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

#ifndef NCBI_COMPILER_ICC
// constexpr tuples are not working in ICC
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

    BOOST_CHECK(m2.size() == 2);
    BOOST_CHECK(m3.size() == 2);
#endif
    BOOST_CHECK(m1.size() == 3);
}

BOOST_AUTO_TEST_CASE(TestOrderedSet)
{
    MAKE_CONST_SET(c_set1, ct::tagStrNocase, {"a", "ab", "aA"});
    //MAKE_CONST_SET(c_set2, ct::tagStrNocase, {"AA", "C", "aA", "b", "a", "B"});

    //ReportSortedList("TestOrderedSet Proxy1", proxy1);
    //ReportSortedList("TestOrderedSet Proxy2", proxy2);

    //static constexpr maker::type c_set1 = proxy1;
    //constexpr maker::type c_set2 = proxy2;

    BOOST_CHECK(c_set1.find("a") != c_set1.end());
    BOOST_CHECK(c_set1.find("d") == c_set1.end());
    BOOST_CHECK(c_set1.find("ac") == c_set1.end());
}

BOOST_AUTO_TEST_CASE(TestOrderedMap)
{
    MAKE_CONST_MAP(c_map, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"b",  "b1" },
        {"ac", "ac1"},
        {"A",  "a1" },
    });

    BOOST_CHECK( c_map.find("a") != c_map.end());
    BOOST_CHECK( c_map.find("d") == c_map.end());
    BOOST_CHECK( c_map.find("ab") == c_map.end());
    BOOST_CHECK( c_map.find("aC") != c_map.end());

    BOOST_CHECK( c_map.find("a")->second != "b1");
    BOOST_CHECK( c_map.find("a")->second == "a1");
    BOOST_CHECK( c_map.find("a")->second != "A1");

    // this is a demonstration of temporal object instantiation
    // that may not work with too strong std::string_view to std::string implicit conversion rules
    const std::string& temp1 = c_map["a"];
    //auto temp1 = c_map["a"];
    BOOST_CHECK( temp1 == "a1");
    const ncbi::CTempString& temp2 = c_map["a"];
    BOOST_CHECK( temp2 == "a1");
    const ncbi::CTempStringEx& temp3 = c_map["a"];
    BOOST_CHECK( temp3 == "a1");
#ifdef __cpp_lib_string_view    
    const std::string_view& temp4 = c_map["a"];
    BOOST_CHECK( temp4 == "a1");   
#endif    

    std::string temp5;
    temp5 = c_map["a"];
}

BOOST_AUTO_TEST_CASE(TestOrderedIntMap)
{
    MAKE_CONST_MAP(c_map, int, ct::tagStrNocase,
    {
        {20,  "b1"},
        {1,   "ac1"},
        {100, "a1"},
    });

#if 0
    std::cout << "sorted: " << std::get<1>(proxy).m_array.size() << std::endl;
    for (auto rec: std::get<1>(proxy).m_array)
    {
        std::cout << "  " << rec.first << ":" << rec.second << std::endl;
    }
    std::cout << std::endl;
#endif

    BOOST_CHECK( c_map.find(1)  != c_map.end());
    BOOST_CHECK( c_map.find(10) == c_map.end());
    BOOST_CHECK( c_map.find(20) != c_map.end());
    BOOST_CHECK( c_map.find(20)->second == "b1");
    BOOST_CHECK( c_map.find(20)->second != "B1");

}

template<typename _Map>
static void ReportMapKeys(const _Map& m)
{
    std::cout << "size: " << std::size(m) << std::endl;
    for (auto it: m)
    {
        std::cout << it.first << " ";
    }
    std::cout << std::endl;
}

BOOST_AUTO_TEST_CASE(TestNonUniqueTwoWay)
{
    // similar to std::set and std::map, const_set/map uses the last element of duplicate items
    // in this example, record "20:d1" must override "20:b1" because the latter appeared earlier
    // similarly, record "x1:201" must override "x1:200" for the same reason

    MAKE_TWOWAY_CONST_MAP(c_map, int, ct::tagStrNocase,
    {
        {20,  "b1"},
        {1,   "ac1"},
        {20,  "d1"},
        {100, "a1"},
        {200, "x1"},
        {201, "x1"},
    });

    ReportMapKeys(c_map.first);
    ReportMapKeys(c_map.second);

    BOOST_CHECK( c_map.first.find(1)->second == "ac1");
    BOOST_CHECK( c_map.first.find(20)->second == "d1");
    BOOST_CHECK( c_map.second.find("b1")->second == 20);
    BOOST_CHECK( c_map.second.find("d1")->second == 20);

    BOOST_CHECK( c_map.first.find(200)->second == "x1");
    BOOST_CHECK( c_map.first.find(201)->second == "x1");

    BOOST_CHECK( c_map.second.find("x1")->second == 201);
}

#if 0
BOOST_AUTO_TEST_CASE(TestArrayCasting)
{
    //ct::const_array
    using type1 = ct::const_set<int>;
    using type2 = ct::const_set<ct::tagStrNocase>;
    using type3 = ct::const_map<ct::tagStrNocase, ct::tagStrNocase>;

    auto a1 = type1::construct({1, 2, 3});
    auto a2 = type2::construct({{"1"}, {"2"}, {"3"}});
    //auto a2_bis = type2::construct({"1", "2", "3"});
    auto a3 = type3::construct({
        {"a", "a1"},
        {"b", "b1"},
        {"c", "c1"},
    });

}
#endif

