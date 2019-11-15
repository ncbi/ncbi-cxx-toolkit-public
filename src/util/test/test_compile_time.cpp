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
#include <array>
#include <corelib/test_boost.hpp>

#include <common/test_assert.h>  /* This header must go last */

#if 0
template<typename _Row, class _Hash, size_t N_key, size_t N_value>
struct const_index_traits
{
    using key_type = typename std::tuple_element<N_key, _Row>::type;
    using value_type = typename std::tuple_element<N_value, _Row>::type;

    using hash_type = typename _Hash::hash_type;
    using hash_value_pair = const_pair<hash_type, value_type>;

    template<size_t N, typename _T>
    struct alignment
    {
        static constexpr size_t mem_align = std::hardware_constructive_interference_size;

        static_assert(N > 0, "cannot align zero size");
        static constexpr size_t min_size = sizeof(_T[N]);
        static constexpr size_t aligned_size = ((min_size + mem_align - 1) / mem_align) * mem_align;

        static constexpr size_t align = mem_align;

        static constexpr size_t padded_size = aligned_size / sizeof(_T);

        static_assert(sizeof(_T[padded_size]) % align == 0, "alignment calculated failed");
    };

    template<size_t N>
    struct combined_container_type
    {
        using align = alignment<N, hash_type>;
        using index_type = const_array<hash_type, align::padded_size> alignas(align::align);
        using pair_type = const_pair<index_type, const_array<value_type, N>>;
    };

    template<size_t index>
    struct select_two_columns
    {
        using type = hash_value_pair;
        template<typename _T>
        constexpr type operator()(const _T& rows) const noexcept
        {
            return {
                     std::get<N_key>(rows[index]),
                     std::get<N_value>(rows[index])
            };
        }
    };

    template<size_t N>
    static constexpr const_array<hash_value_pair, N>
        slice_pair(const _Row(&input)[N]) noexcept
    {
        return Reorder(compile_time_bits::forward_sequence<N, const_array<hash_value_pair, N>, select_two_columns>{}(input));
    }

    template<size_t _index, typename _Input_type, typename return_type>
    struct slice_array_column
    {
        using input_type = typename std::remove_reference<_Input_type>::type;
        using col_type = typename std::tuple_element<_index, typename input_type::value_type>::type;

        static constexpr size_t max_size = std::tuple_size<input_type>::value;
        static constexpr size_t size = std::tuple_size<return_type>::value;

        template<size_t i>
        struct get_tuple_member
        {
            constexpr auto operator()(const input_type& rows) const noexcept
                -> col_type
            {
                return std::get<_index>(std::get < i<max_size ? i : max_size - 1>(rows));
            }
        };

        constexpr auto operator()(const input_type& input) const noexcept
            -> return_type
        {
            using sequence_type = compile_time_bits::forward_sequence<size, return_type, get_tuple_member>;
            return sequence_type{}(input);
        }
    };

    template<typename _T, size_t N>
    static constexpr auto CombineIndexPair(const const_array<_T, N>& input) noexcept
        -> typename combined_container_type<N>::pair_type
    {
        using return_type = combined_container_type<N>;
        return {
            slice_array_column<0, decltype(input), typename return_type::pair_type::first_type  >{}(input),
            slice_array_column<1, decltype(input), typename return_type::pair_type::second_type >{}(input)
        };
    }
};

template<typename _Ty>
class const_vector
{
public:
    using value_type = _Ty;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = const value_type&;
    using const_reference = const value_type&;
    using pointer = const value_type*;
    using const_pointer = const value_type*;
    using const_iterator = const value_type*;
    using iterator = const value_type*;

    constexpr const_vector() noexcept = default;
    constexpr const_vector(const_pointer* _data, size_t _size) :
        m_data{ _data }, m_size{ _size }
    {
    }
    template<size_t N>
    constexpr const_vector(const const_array<_Ty, N>& _ar) :
        m_data{ _ar.data() }, m_size{ _ar.size() }
    {
    }
    template<size_t N>
    constexpr const_vector(const _Ty(&_ar)[N]) :
        m_data{ _ar }, m_size{ N }
    {
    }
    constexpr size_t size() const noexcept
    {
        return m_size;
    }
    constexpr size_t capacity() const noexcept
    {
        return m_size;
    }

    constexpr const value_type& operator[](size_t _pos) const noexcept
    {
        return m_data[_pos];
    }
    constexpr const_iterator begin() const noexcept
    {
        return m_data;
    }
    constexpr const_iterator end() const noexcept
    {
        return m_data + size();
    }
    constexpr const value_type* data() const noexcept
    {
        return m_data;
    }
    value_type* data()
    {
        return m_data;
    }

protected:
    size_t m_size = 0;
    const_pointer m_data = nullptr;
};

template<typename _KeyType, typename _ValueType, typename _Hash>
class const_index
{
public:
    using hash_type = typename _Hash::hash_type;
    using intermediate = typename _Hash::intermediate;
    //using key_type = _KeyType;
    using mapped_type = _ValueType;

    using mapped_values_type = const_vector<mapped_type>;

    // alias to decide whether _Key can be constructed from _Arg
    constexpr const_index() noexcept = default;
    template<typename _Container>
    constexpr const_index(const _Container& data) noexcept
        : m_hashes{ data.first },
        m_values{ data.second }
    {
    }

    constexpr const mapped_values_type& values() const noexcept
    {
        return m_values;
    }

    template<typename _K, typename _Arg>
    using if_available = typename std::enable_if<
        std::is_constructible<intermediate, _K>::value, _Arg>::type;

    template<typename _Ret, typename _Arg>
    using if_returnable = typename std::enable_if<
        std::is_assignable<_Ret, _Arg>::value, _Arg>::type;

    template<typename K>
    if_available<K, const mapped_type&>
        at(K&& _key) const
    {
        intermediate temp = std::forward<K>(_key);
        typename _Hash::type key(temp);
        auto it = std::lower_bound(m_hashes.begin(), m_hashes.end(), std::move(key));
        if (it == m_hashes.end())
            throw std::out_of_range("invalid const_index<K, T> key");

        return m_values[std::distance(m_hashes.begin(), it)];
    }

    template<typename K>
    if_available<K, const mapped_type&>
        operator[](K&& _key) const
    {
        return at(std::forward<K>(_key));
    }

    template<typename _Ret, typename _KeyTy>
    if_available<_KeyTy, bool>
        find_any(_Ret& o_value, _KeyTy&& _key) const
        //if_returnable<_Ret, _Ret>& o_value, _KeyTy&& _key) const
    {
        intermediate temp = std::forward<_KeyTy>(_key);
        typename _Hash::type key(temp);
        auto it = std::lower_bound(m_hashes.begin(), m_hashes.end(), std::move(key));
        if (it == m_hashes.end())
            return false;
        else
            o_value = m_values[std::distance(m_hashes.begin(), it)];

        return true;
    }

protected:
    const_vector<hash_type>   m_hashes = {  };
    mapped_values_type m_values = {  };
};

template<ncbi::NStr::ECase case_sensitive, typename..._Types>
struct const_table
{
    using row_type = const_tuple<typename DeduceHashedType<case_sensitive, NeedHash::yes, _Types>::type...>;

    template<size_t N_key, size_t N_value>
    struct MakeIndex
    {
        using hash_type = DeduceHashedType<case_sensitive, NeedHash::yes, typename std::tuple_element<N_key, row_type>::type>;
        using traits = const_index_traits<row_type, hash_type, N_key, N_value>;
        using key_type = typename traits::key_type;
        using value_type = typename traits::value_type;

        using index_type = const_index<key_type, value_type, hash_type>;

        template<size_t N>
        constexpr auto operator()(const row_type(&input)[N]) const noexcept
            -> decltype(traits::CombineIndexPair(traits::slice_pair(input)))
        {
            return traits::CombineIndexPair(traits::slice_pair(input));
        }
    };
};
#endif

namespace compile_time_bits
{
    template<class _Array,
        class _FwdIt,
        class _Ty,
        class _Pr>
        //[[nodiscard]] 
        constexpr _FwdIt const_lower_bound(const _Array& ar, _FwdIt _First, const _FwdIt _Last, const _Ty& _Val, _Pr _Pred)
    {	// find first element not before _Val, using _Pred
        //_Adl_verify_range(_First, _Last);
        auto _UFirst = _First; // _Get_unwrapped(_First);
        auto _Count = _Last - _UFirst; // std::distance(_UFirst, _Last); // _Get_unwrapped(_Last));

        while (0 < _Count)
        {	// divide and conquer, find half that contains answer
            const auto _Count2 = _Count >> 1; // TRANSITION, VSO#433486
            const auto _UMid = _UFirst + _Count2; // std::next(_UFirst, _Count2);
            if (_Pred(ar[_UMid], _Val))
            {	// try top half
                _UFirst = (_UMid + 1); // _Next_iter(_UMid);
                _Count -= _Count2 + 1;
            }
            else
            {
                _Count = _Count2;
            }
        }

        _First = _UFirst;
        return (_First);
    }

    template<typename _Array, typename _Iterator>
    constexpr void move_down(_Array& ar, _Iterator head, _Iterator tail, _Iterator current)
    {
        auto saved = ar[current];
        while (head != tail)
        {
            auto prev = tail--;
            ar[prev] = ar[tail];
        }
        ar[head] = saved;
    }

    template<bool unique = true, typename _array, typename _iterator, typename _pred>
    //[[nodiscard]]
    constexpr auto insert_sort(_array& ar, const _iterator begin, const _iterator end, _pred pred)
    {
        auto size = end - begin; // std::distance(begin, end);
        if (size < 2)
            return size;

        // current is the first element of the unsorted part of the array
        auto current = begin;
        // the last inserted element into sorted part of the array
        auto last = current;
        ++current;

        while (current != end)
        {
            if (pred(ar[last], ar[current]))
            {   // optimization for presorted arrays, don't move anything if things are 
                // already in order or identical
                // the two adjacent elements not in order reinsert the current
                ++last;
                if (last != current)
                    ar[last] = ar[current];
            }
            else {
                auto fit = const_lower_bound(ar, begin, last, ar[current], pred);
                // no need to check the fit result
                // since it's already known the current is less then last
                bool move{};
                if (unique)
                    move = pred(ar[current], ar[fit]);
                else
                    move = true;
                if (move)
                {	// *fit is greater or equal to *current
                    // identical elements are rejected
                    ++last;
                    move_down(ar, fit, last, current);
                }
            }
            ++current;
        }
        return 1 + (last - begin); // std::distance(begin, last);
    }
    class CompareLess
    {
    public:
        template<typename T1, typename T2>
        constexpr bool operator()(const ct::const_pair<T1, T2>& l, const ct::const_pair<T1, T2>& r) const
        {
            return l.first < r.first;
        }
        template<typename T>
        constexpr bool operator()(const T& l, const T& r) const
        {
            return l < r;
        }
    };

    template<typename _array, typename _pred = CompareLess>
    //[[nodiscard]]
    constexpr auto insert_sort(const _array& ar, _pred pred = _pred()) -> typename std::remove_cv<_array>::type
    {
        using non_const = typename std::remove_cv<_array>::type;
        non_const copied = ar;
        insert_sort<true, non_const, size_t,_pred>(copied, 0, copied.size(), pred);
        return copied;
    };

};

//#undef MAKE_CONST_MAP
//#undef MAKE_TWOWAY_CONST_MAP

#define MAKE_CONST_MAP_NEW(name, case_sensitive, type1, type2, ...)                                                              \
    using name ## _deduce_type = typename ct::MakeConstMap<case_sensitive, ct::TwoWayMap::no, type1, type2>;                 \
    static constexpr size_t name ## _init_size = name ## _deduce_type::get_size(__VA_ARGS__);                                \
    using name ## _map_type = typename name ## _deduce_type::DeduceType<name ## _init_size>::type;                           \
    static constexpr name ## _map_type name (                                                                                \
        compile_time_bits::insert_sort<typename name ## _map_type :: array_t>({__VA_ARGS__}));  

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
    for (auto& rec : test_two_way1)
        std::cout << rec.first.m_hash << ":" << rec.first << ":" << rec.second << std::endl;

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
#if 0
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
#endif
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


BOOST_AUTO_TEST_CASE(TestCompileTimeSort)
{
    static constexpr ct::const_array<int, 5> pre_sorted = { 1, 2, 3, 4, 5 };
    static constexpr ct::const_array<int, 5> ts1 = { 5, 4, 1, 3, 2 };
    constexpr auto sorted = compile_time_bits::insert_sort(ts1);
    for (auto& rec : sorted)
    {
        std::cout << rec << std::endl;
    }
    BOOST_CHECK(0 != memcmp(sorted.data(), ts1.data(), sizeof(sorted)));
    BOOST_CHECK(0 == memcmp(sorted.data(), pre_sorted.data(), sizeof(sorted)));

    static constexpr ct::const_array<ct::const_pair<int, int>, 5> ts2 = { {
        {5, 50},
        {1, 10},
        {3, 30},
        {4, 40},
        {2, 20}
    } };
    constexpr auto ts2_sorted = compile_time_bits::insert_sort(ts2);
    for (auto& rec : ts2_sorted)
    {
        std::cout << rec.first << std::endl;
    }

#if 0
    static constexpr ct::const_array<ct::const_pair<ct::CHashString<ncbi::NStr::eCase>, int>, 5> ts3 = { {
        {"a5", 50},
        {"a1", 10},
        {"a3", 30},
        {"a4", 40},
        {"a2", 20}
    } };
    constexpr auto ts3_sorted = compile_time_bits::insert_sort(ts3);
    for (auto& rec : ts3_sorted)
    {
        std::cout << rec.first << std::endl;
    }

    MAKE_CONST_MAP_NEW(ts4, ncbi::NStr::eCase, const char*, int,
      { { "a5", 50 },
        { "a1", 10 },
        { "a3", 30 },
        { "a4", 40 },
        { "a2", 20 }
        });
 
    for (auto& rec : ts4)
        std::cout << rec.first << ":" << rec.second << std::endl;
#endif
};

