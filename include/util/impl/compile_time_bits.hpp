#ifndef __COMPILE_TIME_BITS_HPP_INCLUDED__
#define __COMPILE_TIME_BITS_HPP_INCLUDED__

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
 *
 *  various compile time structures and functions
 *
 *
 */

#include <cstddef>
#include <utility>
#include <cstdint>
#include <corelib/tempstr.hpp>

#include "ct_crc32.hpp"
#include <corelib/ncbistr.hpp>

namespace compile_time_bits
{
    template<class T, size_t N>
    struct const_array
    {
        using _MyT = const_array<T, N>;
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = value_type&;
        using const_reference = const value_type&;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using container_type = value_type[N];
        using const_iterator = const value_type*;
        using iterator = value_type*;

        static constexpr size_t m_size = N;

        static constexpr size_t size() noexcept { return N; }
        static constexpr size_t capacity() noexcept { return N; }
        constexpr const value_type& operator[](size_t _pos) const noexcept { return m_data[_pos]; }
        constexpr value_type& operator[](size_t _pos) noexcept { return m_data[_pos]; }
        constexpr const_iterator begin() const noexcept { return m_data; }
        constexpr const_iterator end() const noexcept { return m_data + size(); }
        constexpr const_iterator cbegin() const noexcept { return m_data; }
        constexpr const_iterator cend() const noexcept { return m_data + size(); }
        constexpr iterator begin() noexcept { return m_data; }
        constexpr iterator end() noexcept { return m_data + size(); }
        constexpr const value_type* data() const noexcept { return m_data; }


        container_type m_data;

        template<typename _Alloc>
        operator std::vector<value_type, _Alloc>() const
        {
            std::vector<value_type, _Alloc> vec;
            vec.reserve(size());
            vec.assign(begin(), end());
            return vec;
        }
    };

    template <class T, std::size_t N, std::size_t... I>
    constexpr const_array<std::remove_cv_t<T>, N>
        to_array_impl(T(&a)[N], std::index_sequence<I...>)
        {
            return { {a[I]...} };
        }

    template <class T, std::size_t N>
    constexpr const_array<std::remove_cv_t<T>, N> to_array(T(&&a)[N])
    {
        return to_array_impl(a, std::make_index_sequence<N>{});
    }
    template <class T, std::size_t N>
    constexpr const_array<std::remove_cv_t<T>, N> to_array(T(&a)[N])
    {
        return to_array_impl(a, std::make_index_sequence<N>{});
    }

    template<class T>
    struct const_array<T, 0>
    {
        using const_iterator = const T*;
        using value_type = T;

        static constexpr size_t m_size = 0;

        constexpr size_t size() const noexcept { return 0; }
        constexpr size_t capacity() const noexcept { return 0; }
        constexpr const_iterator begin() const noexcept { return nullptr; }
        constexpr const_iterator end() const noexcept { return nullptr; }
        const value_type* data() const noexcept { return nullptr; }

        template<typename _Alloc>
        operator std::vector<value_type, _Alloc>() const
        {
            return std::vector<value_type, _Alloc>();
        }
    };

    template<class FirstType, class SecondType>
    struct const_pair
    {
        typedef FirstType first_type;
        typedef SecondType second_type;

        first_type first {};
        second_type second {};

        // GCC 4.9.3 and 5.3.0 doesn't forward correctly arguments types
        // when aggregated initialised

#if defined(__GNUC__) && (__GNUC__ < 6) && !defined(__clang__)
        constexpr const_pair() = default;

        template<typename T1, typename T2>
        constexpr const_pair(T1&& v1, T2&& v2)
            : first(std::forward<T1>(v1)), second(std::forward<T2>(v2))
        {}
        constexpr const_pair(const first_type& f, const second_type& s)
            : first(f), second(s)
        {
        }
#endif

        constexpr bool operator <(const const_pair& o) const
        {
            return (first < o.first) ||
                (first == o.first && second < o.second);
        }
        constexpr const first_type& get(std::integral_constant<size_t, 0>) const noexcept
        {	// get reference to element 0 in pair _Pr
            return first;
        }
        constexpr const second_type& get(std::integral_constant<size_t, 1>) const noexcept
        {	// get reference to element 0 in pair _Pr
            return second;
        }
    };

    template<class... _Types>
    class const_tuple;

    template<>
    class const_tuple<>
    {	// empty tuple
    public:
    };

    template<class _This,
        class... _Rest>
        class const_tuple<_This, _Rest...>
        : private const_tuple<_Rest...>
    {	// recursive tuple definition
    public:
        typedef _This _This_type;
        typedef const_tuple<_Rest...> _Mybase;
        _This   _Myfirst;	// the stored element


        constexpr const_tuple() noexcept = default;
        constexpr const_tuple(const _This& _f, const _Rest&..._rest) noexcept
            : _Mybase(_rest...), _Myfirst(_f)
        {
        }
        template<typename _T0, typename..._Other>
        constexpr const_tuple(_T0&& _f0, _Other&&...other) noexcept
            : _Mybase(std::forward<_Other>(other)...), _Myfirst(std::forward<_T0>(_f0))
        {
        }
    };

    struct string_view
    {
        constexpr string_view() = default;

        template<size_t N>
        constexpr string_view(const char(&s)[N]) noexcept
            : m_len{ N - 1 }, m_data{ s }
        {}

        constexpr string_view(const char* s, size_t len) noexcept
            : m_len{ len }, m_data{ s }
        {}

        string_view(const ncbi::CTempString& view)
            :m_len{ view.size() }, m_data{ view.data() }
        {}

        constexpr const char* c_str() const noexcept { return m_data; }
        constexpr const char* data() const noexcept { return m_data; }
        constexpr size_t size() const noexcept { return m_len; }

        operator ncbi::CTempStringEx() const noexcept
        {
            return ncbi::CTempStringEx(m_data, m_len, ncbi::CTempStringEx::eNoZeroAtEnd);
        }
        operator ncbi::CTempString() const noexcept
        {
            return ncbi::CTempString(m_data, m_len);
        }
        template<class C, class T, class A>
        operator std::basic_string<C, T, A>() const
        {
            return std::string(m_data, m_len);
        }
        constexpr char operator[](size_t pos) const
        {
            return m_data[pos];
        }
        size_t m_len{ 0 };
        const char* m_data{ nullptr };
    };

    template<class...> struct conjunction : std::true_type { };
    template<class B1> struct conjunction<B1> : B1 { };
    template<class B1, class... Bn>
    struct conjunction<B1, Bn...>
        : std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

    template<ncbi::NStr::ECase case_sensitive, class _Hash = ct::SaltedCRC32<case_sensitive>>
    class CHashString : public string_view
    {
    public:
        using hash_func = _Hash;
        using hash_type = typename _Hash::type;
        using sv = ncbi::CTempString;

        constexpr CHashString() noexcept = default;

        template<size_t N>
        constexpr CHashString(const char(&s)[N]) noexcept
            : string_view(s, N - 1), m_hash(hash_func::ct(s))
        {}

        CHashString(const sv& s) noexcept
            : string_view(s), m_hash(hash_func::rt(s.data(), s.size()))
        {}

        constexpr bool operator<(const CHashString& o) const noexcept
        {
            return m_hash < o.m_hash;
        }
        constexpr bool operator!=(const CHashString& o) const noexcept
        {
            return m_hash != o.m_hash;
        }
        constexpr bool operator==(const CHashString& o) const noexcept
        {
            return m_hash == o.m_hash;
        }
        bool operator!=(const sv& o) const noexcept
        {
            return (case_sensitive == ncbi::NStr::eCase ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) != 0;
        }
        bool operator==(const sv& o) const noexcept
        {
            return (case_sensitive == ncbi::NStr::eCase ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) == 0;
        }
        constexpr operator hash_type() const noexcept
        {
            return m_hash;
        }

        hash_type m_hash{ 0 };
    };
};

namespace ct
{
    using namespace compile_time_bits;
};

namespace std
{// these are backported implementations of C++17 methods
    constexpr size_t hardware_destructive_interference_size = 64;
    constexpr size_t hardware_constructive_interference_size = 64;

    template<size_t i, class T, size_t N>
    constexpr const T& get(const ct::const_array<T, N>& in) noexcept
    {
        return in[i];
    };
    template<class T, size_t N>
    constexpr size_t size(const ct::const_array<T, N>& in) noexcept
    {
        return N;
    }
    template<class T, size_t N>
    constexpr auto begin(const ct::const_array<T, N>& in) noexcept
        -> typename ct::const_array<T, N>::const_iterator
    {
        return in.begin();
    }
    template<class T, size_t N>
    constexpr auto end(const ct::const_array<T, N>& in) noexcept
        -> typename ct::const_array<T, N>::const_iterator
    {
        return in.end();
    }

    template<std::size_t I, typename T1, typename T2>
    class tuple_element<I, ct::const_pair<T1, T2> >
    {
        static_assert(I < 2, "compile_time_bits::const_pair has only 2 elements!");
    };

    template<typename T1, typename T2>
    class tuple_element<0, ct::const_pair<T1, T2> >
    {
    public:
        using type = T1;
    };
    template<typename T1, typename T2>
    class tuple_element<1, ct::const_pair<T1, T2> >
    {
    public:
        using type = T2;
    };

    //template<class>
    // false value attached to a dependent name (for static_assert)
    //constexpr bool always_false = false;

    template<size_t _Index>
    class tuple_element<_Index, ct::const_tuple<>>
    {	// enforce bounds checking
        //static_assert(always_false<integral_constant<size_t, _Index>>,
        //    "tuple index out of bounds");
    };

    template<class _This, class... _Rest>
    class tuple_element<0, ct::const_tuple<_This, _Rest...>>
    {	// select first element
    public:
        using type = _This;
        using _Ttype = ct::const_tuple<_This, _Rest...>;
    };

    template<size_t _Index, class _This, class... _Rest>
    class tuple_element<_Index, ct::const_tuple<_This, _Rest...>>
        : public tuple_element<_Index - 1, ct::const_tuple<_Rest...>>
    {	// recursive tuple_element definition
    };

    template<std::size_t I, typename T1, typename T2>
    constexpr auto get(const ct::const_pair<T1, T2>& v) noexcept
        -> const typename tuple_element<I, ct::const_pair<T1, T2>>::type&
    {
        return v.get(integral_constant<size_t, I>());
    }

    template<size_t _Index,
        class... _Types>
        constexpr const typename tuple_element<_Index, ct::const_tuple<_Types...>>::type&
        get(const ct::const_tuple<_Types...>& _Tuple) noexcept
    {	// get const reference to _Index element of tuple
        typedef typename tuple_element<_Index, ct::const_tuple<_Types...>>::_Ttype _Ttype;
        return (((const _Ttype&)_Tuple)._Myfirst);
    };

    template<class _Traits> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const ct::string_view& v)
    {// Padding is not implemented yet
        _Ostr.write(v.data(), v.size());
        return _Ostr;
    }

    template<class T, size_t N>
    class tuple_size<ct::const_array<T, N>>:
        public integral_constant<size_t, N>
    { };
};

namespace compile_time_bits
{
    template<typename _Value>
    struct simple_sort_traits
    {
        using value_type = _Value;

        template<typename _Input>
        static constexpr bool compare_less(const _Input& input, size_t l, size_t r)
        {
            return input[l] < input[r];
        }
        template<typename _Input>
        static constexpr auto construct(const _Input& input)
        {
            return input;
        }
    };

    template<typename _Pair>
    struct straight_sort_traits
    {
        using value_type = _Pair;

        template<typename _Input>
        static constexpr bool compare_less(const _Input& input, size_t l, size_t r)
        {
            return input[l].first < input[r].first;
        }
        template<typename _Input>
        static constexpr auto construct(const _Input& input)
        {
            return input;
        }
    };

    template<typename _Pair>
    struct flipped_sort_traits
    {
        using value_type = const_pair<typename _Pair::second_type, typename _Pair::first_type>;

        template<typename _Input>
        static constexpr bool compare_less(const _Input& input, size_t l, size_t r)
        {
            return input[l].second < input[r].second;
        }
        template<typename _Input>
        static constexpr value_type construct(const _Input& input)
        {
            return value_type{ input.second, input.first };
        }
    };

    template<typename _Traits, typename _Input, size_t N>
    struct sorter
    {
        static constexpr size_t size = N;
        static constexpr size_t max_size = std::numeric_limits<size_t>::max();

        using TIndices   = typename ct::const_array<size_t, size>;

        constexpr size_t select_min(size_t i, size_t _min, size_t _thresold) const
        {
            return (
                (i != _min) &&
                ((_thresold == max_size) || (_Traits::compare_less(m_input, _thresold, i))) &&
                ((_min == max_size) || (_Traits::compare_less(m_input, i, _min))))
                ? i : _min;
        }

        constexpr size_t find_min(size_t _min, size_t _thresold) const
        {
            for (size_t i=0; i<size; ++i)
            {
                _min = select_min(i, _min, _thresold);
            }
            return _min;
        }

        template <std::size_t... I>
        constexpr auto unroll(const TIndices& indices, std::index_sequence<I...>) const
        {
            return const_array<typename _Traits::value_type, size>
                { {_Traits::construct(m_input[indices[I]])...} };
        }

        constexpr auto operator()() const
        {
            TIndices indices{};
            size_t _min = max_size;
            for (size_t i=0; i<size; ++i)
            {
                _min = find_min(max_size, _min);
                indices[i] = _min;
            }
            return unroll(indices, std::make_index_sequence<size>{});
        }
        const _Input& m_input;
    };

    template<typename T, size_t N>
    constexpr auto SimpleReorder(const T(&input)[N])
    {
        return sorter<simple_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr auto SimpleReorder(const const_array<T, N>& input)
    {
        return sorter<simple_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr auto Reorder(const const_array<T, N>& input)
    {
        return sorter<straight_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr auto Reorder(const T(&input)[N])
    {
        return sorter<straight_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const const_array<T, N>& input)
    {
        return sorter<flipped_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const T(&input)[N])
    {
        return sorter<flipped_sort_traits<T>, decltype(input), N>{input}();
    }

    template<typename T, size_t N>
    constexpr bool CheckOrder(const const_array<T, N>& tup)
    {
        for (size_t i=1; i<N; ++i)
            if (!(tup[i-1]<tup[i]))
                return false;
        return true;
    }

    // this helper packs set of bits into an array usefull for initialisation of bitset
    // using C++14

    template<class _Ty, size_t array_size>
    struct bitset_traits
    {
        using array_t = const_array<_Ty, array_size>;

        static constexpr int width = 8 * sizeof(_Ty);

        struct range_t
        {
            size_t m_from;
            size_t m_to;
        };

        static constexpr bool check_range(const range_t& range, size_t i)
        {
            return (range.m_from <= i && i <= range.m_to);
        }

        template <size_t I>
        static constexpr _Ty assemble_mask(const range_t& _init)
        {
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width - 1;
            if (check_range(_init, _min) && check_range(_init, _max))
                return std::numeric_limits<_Ty>::max();

            _Ty ret = 0;
            for (size_t j = 0; j < width; ++j)
            {
                bool is_set = check_range(_init, j + _min);
                if (is_set)
                    ret |= (_Ty(1) << j);
            }
            return ret;
        }
        template <size_t I, typename _O>
        static constexpr _Ty assemble_mask(const std::initializer_list<_O>& _init)
        {
            _Ty ret = 0;
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width - 1;
            for (unsigned rec : _init)
            {
                if (_min <= rec && rec <= _max)
                {
                    ret |= _Ty(1) << (rec % width);
                }
            }
            return ret;
        }
        template <typename _Input, std::size_t... I>
        static constexpr array_t assemble_bitset(const _Input& _init, std::index_sequence<I...>)
        {
            return { {assemble_mask<I>(_init)...} };
        }
        template<typename _O>
        static constexpr array_t set_bits(std::initializer_list<_O> args)
        {
            return assemble_bitset(args, std::make_index_sequence<array_size>{});
        }
        template<typename _T>
        static constexpr array_t set_range(_T from, _T to)
        {
            return assemble_bitset(range_t{ static_cast<size_t>(from), static_cast<size_t>(to) }, std::make_index_sequence<array_size>{});
        }
    };

    template<class _T, class _First,
        class... _Rest>
        struct enforce_same
    {
        static_assert(conjunction<std::is_same<_First, _Rest>...>::value, "variadic arguments must be of the same type");
        //    "N4687 26.3.7.2 [array.cons]/2: "
        //    "Requires: (is_same_v<T, U> && ...) is true. Otherwise the program is ill-formed.");
        using type = _T;
    };

    template<class _T, class _First,
        class... _Rest>
        struct enforce_all_constructable
    {
        static_assert(conjunction<std::is_constructible<_First, _Rest>...>::value, "variadic arguments must be of the same type");
        //    "N4687 26.3.7.2 [array.cons]/2: "
        //    "Requires: (is_same_v<T, U> && ...) is true. Otherwise the program is ill-formed.");
        using type = _T;
    };

    enum class TwoWayMap
    {
        no,
        yes
    };

    enum class NeedHash
    {
        no,
        yes
    };

    template<ncbi::NStr::ECase case_sensitive, NeedHash need_hash, typename _BaseType>
    struct DeduceHashedType
    {
        using type = _BaseType;
        using intermediate = _BaseType;
        using hash_type = _BaseType;
        static constexpr const hash_type& get_hash(const type& v) noexcept
        {
            return v;
        }
    };

    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceHashedType<case_sensitive, NeedHash::yes, const char*>
    {
        using type = CHashString<case_sensitive>;
        using intermediate = typename type::sv;
        using hash_type = typename type::hash_type;
        static constexpr hash_type get_hash(const type& v) noexcept
        {
            return v;
        }
    };

    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceHashedType<case_sensitive, NeedHash::no, const char*>
    {  // this doesn't have any hashes
        using type = string_view;
    };

    template<ncbi::NStr::ECase case_sensitive, NeedHash need_hash, class CharT, class Traits, class Allocator>
    struct DeduceHashedType<case_sensitive, need_hash, std::basic_string<CharT, Traits, Allocator>>
         : DeduceHashedType<case_sensitive, need_hash, const char*>
    {
    };   

    template<ncbi::NStr::ECase case_sensitive, NeedHash need_hash, typename _Hash>
    struct DeduceHashedType<case_sensitive, need_hash, CHashString<case_sensitive, _Hash>>
         : DeduceHashedType<case_sensitive, need_hash, const char*>
    {
    };

};

#endif

