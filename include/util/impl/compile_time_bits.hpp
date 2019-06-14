
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
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = const value_type&;
        using const_reference = const value_type&;
        using pointer = const value_type*;
        using const_pointer = const value_type*;
        using cont_t = value_type[N];
        using const_iterator = const value_type*;
        using iterator = const value_type*;

        static constexpr size_t m_size = N;

        constexpr size_t size() const noexcept
        {
            return N;
        }
        constexpr size_t capacity() const noexcept
        {
            return N;
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
            return m_data + N;
        }
        constexpr const value_type* data() const noexcept
        {
            return m_data;
        }
        value_type* data()
        {
            return m_data;
        }

        cont_t m_data;
    };

    template<class T>
    struct const_array<T, 0>
    {
        using const_iterator = const T*;
        using value_type = T;

        static constexpr size_t m_size = 0;

        constexpr size_t size() const noexcept
        {
            return 0;
        }
        constexpr size_t capacity() const noexcept
        {
            return 0;
        }

        constexpr const_iterator begin() const noexcept
        {
            return nullptr;
        }
        constexpr const_iterator end() const noexcept
        {
            return nullptr;
        }
        const value_type* data() const noexcept
        {
            return nullptr;
        }
    };

    template<class FirstType, class SecondType>
    struct const_pair
    {
        typedef FirstType first_type;
        typedef SecondType second_type;

        first_type first;
        second_type second;

        // GCC 4.9.3 doesn't forward correctly arguments types
        // when aggregated initialised

#ifdef __GNUC__
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

        constexpr const char* data() const noexcept
        {
            return m_data;
        }
        constexpr size_t size() const noexcept
        {
            return m_len;
        }
        operator ncbi::CTempStringEx() const noexcept
        {
            return ncbi::CTempStringEx(m_data, m_len, ncbi::CTempStringEx::eHasZeroAtEnd);
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

    template<size_t _cap, class T>
    struct AssembleArray
    {
        using array_t = const_array<T, _cap>;

        template<size_t i, typename _S>
        struct concat_t
        {
            using next_t = concat_t<i - 1, _S>;

            template<typename...TRest>
            constexpr array_t operator() (const _S* s, size_t N, TRest...rest) const noexcept
            {
                return next_t() (s, N, i <= N ? T(s[i - 1]) : T{}, rest...);
            }
        };

        template<typename _S>
        struct concat_t<0, _S>
        {
            template<typename...TRest>
            constexpr array_t operator() (const _S* s, size_t N, TRest...rest) const noexcept
            {
                return { { rest... } };
            }
        };

        template<class T2>
        constexpr array_t operator() (const T2* s, size_t realN) const
        {
            return concat_t<_cap, T2>()(s, realN);
        }
        template<class T2, size_t N>
        constexpr array_t operator() (const const_array<T2, N>& s) const
        {
            return concat_t<_cap, T2>()(s.data(), s.size());
        }
    };

    template<typename T, typename...TArgs>
    constexpr const_array < T, 1 + sizeof...(TArgs)> make_array(T&& first, TArgs&&...rest)
    {
        return { { std::forward<T>(first), std::forward<TArgs>(rest)...} };
    }

    template<typename T, size_t N>
    constexpr auto make_array(const T(&input)[N]) -> const_array<T, N>
    {
        return AssembleArray<N, T>{}(input, N);
    }

    template<class...> struct conjunction : std::true_type { };
    template<class B1> struct conjunction<B1> : B1 { };
    template<class B1, class... Bn>
    struct conjunction<B1, Bn...>
        : std::conditional<bool(B1::value), conjunction<Bn...>, B1>::type {};

    template<typename _Input, typename _Pair, size_t N>
    struct straight_sort_traits
    {
        static constexpr size_t size = N;

        using inpair_t = _Pair;
        using sorted_t = const_array<_Pair, N>;
        using unsorted_t = _Input;

        static constexpr bool compare_less(const _Input& input, size_t l, size_t r)
        {
            return input[l].first < input[r].first;
        }
        template<class...TArgs>
        static constexpr sorted_t construct(const _Input& input, TArgs...ordered)
        {
            return { { input[ordered]...} };
        }
    };

    template<typename _Input, typename _Pair, size_t N>
    struct flipped_sort_traits
    {
        static constexpr size_t size = N;
        using inpair_t = _Pair;
        using pair_t = const_pair<typename _Pair::second_type, typename _Pair::first_type>;
        using sorted_t = const_array<pair_t, N>;
        using unsorted_t = _Input;

        static constexpr pair_t make_pair(const _Input& input, size_t i)
        {
            return { input[i].second, input[i].first };
        }
        static constexpr bool compare_less(const _Input& input, size_t l, size_t r)
        {
            return input[l].second < input[r].second;
        }
        template<class...TArgs>
        static constexpr sorted_t construct(const _Input& input, TArgs...ordered)
        {
            return { { make_pair(input, ordered)...} };
        }
    };

    template<typename _Traits>
    struct sorter
    {
        static constexpr size_t max_size = std::numeric_limits<size_t>::max();

        using outarray_t = typename _Traits::sorted_t;
        using inarray_t  = typename _Traits::unsorted_t;

        struct select_min
        {
            constexpr size_t operator() (const inarray_t& input, size_t i, size_t _min, size_t _thresold) const
            {
                return (
                    (i != _min) &&
                    ((_thresold == max_size) || (_Traits::compare_less(input, _thresold, i))) &&
                    ((_min == max_size) || (_Traits::compare_less(input, i, _min))))
                    ? i : _min;
            }
        };

        template<size_t i, typename _Pred>
        struct find_minmax_t
        {
            using next_t = find_minmax_t<i - 1, _Pred>;
            constexpr size_t operator()(const inarray_t& input, size_t _min, size_t _thresold) const
            {
                return
                    next_t{}(input, _Pred{ }(input, i - 1, _min, _thresold), _thresold);
            }
        };

        template<typename _Pred>
        struct find_minmax_t<0, _Pred>
        {
            constexpr size_t operator()(const inarray_t& input, size_t _min, size_t _thresold) const
            {
                return _min;
            }
        };

        template<size_t i, typename _Pred>
        struct order_array_t
        {
            using next_t = order_array_t<i - 1, _Pred>;

            template<class...TArgs>
            constexpr outarray_t operator()(size_t _min, const inarray_t& input, TArgs...ordered) const
            {
                return next_t{}(
                    _Pred{}(input, max_size, _min),
                    input,
                    ordered...,
                    _min);
            }
        };

        template<typename _Pred>
        struct order_array_t<0, _Pred>
        {
            template<class...TArgs>
            constexpr outarray_t operator()(size_t _min, const inarray_t& input, TArgs...ordered) const
            {
                return _Traits::construct(input, ordered..., _min);
            }
        };

        using min_t = find_minmax_t<_Traits::size, select_min>;

        static constexpr outarray_t order_array(const inarray_t& input)
        {
            return order_array_t<_Traits::size-1, min_t>{}(
                min_t{}(input, max_size, max_size),
                input);
        }

    };

    template<typename T, size_t N>
    constexpr auto Reorder(const const_array<T, N>& input)
        -> typename straight_sort_traits<decltype(input), T, N>::sorted_t
    {
        return sorter<straight_sort_traits<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto Reorder(const T(&input)[N])
        -> typename straight_sort_traits<decltype(input), T, N>::sorted_t
    {
        return sorter<straight_sort_traits<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const const_array<T, N>& input)
        -> typename flipped_sort_traits<decltype(input), T, N>::sorted_t
    {
        return sorter<flipped_sort_traits<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const T(&input)[N])
        -> typename flipped_sort_traits<decltype(input), T, N>::sorted_t
    {
        return sorter<flipped_sort_traits<decltype(input), T, N>>::order_array(input);
    }

    template<bool case_sensitive = true, class _Hash = ct::SaltedCRC32<case_sensitive>>
    class CHashString : public string_view
    {
    public:
        using hash_func = _Hash;
        using hash_t = typename _Hash::type;
        using sv = ncbi::CTempString;

        constexpr CHashString() = default;
        template<size_t N>
        constexpr CHashString(const char(&s)[N])
            : string_view(s, N - 1), m_hash(hash_func::ct(s))
        {}

        explicit CHashString(const sv& s)
            : string_view(s), m_hash(hash_func::general(s.data(), s.size()))
        {}

        constexpr bool operator<(const CHashString& o) const
        {
            return m_hash < o.m_hash;
        }
        constexpr bool operator!=(const CHashString& o) const
        {
            return m_hash != o.m_hash;
        }
        constexpr bool operator==(const CHashString& o) const
        {
            return m_hash == o.m_hash;
        }
        bool operator!=(const sv& o) const
        {
            return (case_sensitive ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) != 0;
        }
        bool operator==(const sv& o) const
        {
            return (case_sensitive ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) == 0;
        }

        hash_t m_hash{ 0 };
    };

    template<typename _TTuple, size_t i>
    struct check_order_t
    {
        using next_t = check_order_t<_TTuple, i - 1>;
        constexpr bool operator() (const _TTuple& tup) const
        {
            return (std::get<i - 1>(tup).first < std::get<i>(tup).first) &&
                next_t {}(tup);
        }
    };

    template<typename _TTuple>
    struct check_order_t<_TTuple, 0>
    {
        constexpr bool operator()(const _TTuple& tup) const
        {
            return true;
        };
    };

    template<typename T, size_t N>
    constexpr bool CheckOrder(const const_array<T, N>& tup)
    {
        return check_order_t<const_array<T, N>, N - 1>{}(tup);
    }

    template<typename T>
    struct recast
    {
        using type = T;
        using intermediate = T;
    };

    template<bool _C, class _H>
    struct recast<CHashString<_C, _H>>
    {
        using type = CHashString<_C, _H>;
        using intermediate = typename type::sv;
    };

    // this helper packs set of bits into an array usefull for initialisation of bitset
    // ugly C++11 templates until C++14 or C++17 advanced constexpr becomes available

    template<size_t array_size, class TIndex, class _Ty, size_t bit_count = array_size * sizeof(_Ty) * 8>
    struct bitset_traits
    {
        using const_input_array = const_array<TIndex, bit_count>;
        using array_t = const_array<_Ty, array_size>;

        static constexpr int width = 8 * sizeof(_Ty);

        template<size_t index>
        static constexpr _Ty set_bit(size_t high, unsigned char low)
        { // set only bit if high size is equal to index
            return (index == high) ? (_Ty(1) << (low & (width - 1))) : 0;
        }

        template<size_t index>
        static constexpr _Ty set_bit(TIndex v)
        {
            return set_bit<index>(v / width, v % width);
        }

        struct range_t
        {
            size_t m_from;
            size_t m_to;
        };

        template<size_t index, size_t i, typename...TArgs>
        static constexpr _Ty getbits(const std::tuple<TArgs...>& tup)
        {
            return set_bit<index>(std::get<i>(tup));
        }

        template<size_t index, size_t i>
        static constexpr _Ty getbits(const const_input_array& tup)
        {
            return set_bit<index>(tup.m_data[i]);
        }

        template<size_t index, size_t i>
        static constexpr _Ty getbits(const range_t& tup)
        {
            return (tup.m_from <= i && i <= tup.m_to) ? set_bit<index>(static_cast<TIndex>(i)) : 0;
        }
        template<size_t index, size_t i, typename _O>
        static constexpr _Ty getbits(const std::initializer_list<_O>& tup)
        {
            return (i < tup.size()) ? set_bit<index>(static_cast<TIndex>(*(tup.begin() + i))) : 0;
        }
        template<size_t index, size_t i>
        static constexpr _Ty getbits(const char (&tup)[bit_count+1])
        {
            return set_bit<index>(static_cast<TIndex>(tup[i]));
        }
        template<size_t index, size_t i>
        struct assemble_mask
        {
            using next_t = assemble_mask<index, i - 1>;

            template<class _TTuple>
            constexpr _Ty operator()(const _TTuple& tup) const
            {
                return getbits<index, i - 1>(tup) | next_t()(tup);
            }
        };

        template<size_t index>
        struct assemble_mask<index, 0>
        {
            template<class _TTuple>
            constexpr _Ty operator()(const _TTuple& tup) const
            {
                return 0;
            }
        };

        template<size_t i, typename...Unused>
        struct assemble_bitset
        {
            using next_t = assemble_bitset<i - 1, Unused...>;

            template<class _TTuple, typename...TArgs>
            constexpr array_t operator()(const _TTuple& tup, TArgs...args) const
            {
                return next_t()(
                    tup,
                    assemble_mask<i - 1, bit_count>()(tup),
                    args...);
            }
        };

        template<typename...Unused>
        struct assemble_bitset<0, Unused...>
        {
            template<class _TTuple, typename...TArgs>
            constexpr array_t operator()(const _TTuple& tup, TArgs...args) const
            {
                return array_t{ { args... } };
            }
        };

        template<typename...TArgs>
        static constexpr array_t set_bits(TArgs...args)
        {
            return array_t(assemble_bitset<array_size>{}(const_input_array{ {static_cast<TIndex>(args)...} }));
        }
        static constexpr array_t set_bits(const char (&s)[bit_count+1])
        {
            return array_t(assemble_bitset<array_size>{}(s));
        }
        template<typename _O>
        static constexpr array_t set_bits(const std::initializer_list<_O>& args)
        {
            return array_t(assemble_bitset<array_size>{}(args));
        }

        template<typename _T>
        static constexpr array_t set_range(_T from, _T to)
        {
            return assemble_bitset<array_size>{} (range_t{ static_cast<size_t>(from), static_cast<size_t>(to) });
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

    template<typename _Charset, typename _T>
    struct xlate_traits
    {
        using value_type = _T;
        using init_pair_t = const_pair<_T, _Charset>;
        using array_t = const_array<_T, _Charset::capacity()>;

        template<unsigned i, unsigned _v>
        struct find_value
        {
            using next_t = find_value<i - 1, _v>;
            template<size_t N>
            constexpr _T operator()(_T _default, const init_pair_t(&init)[N]) const
            {
                return
                    init[i - 1].second.test(_v) ? init[i - 1].first : next_t{}(_default, init);
            }
        };
        template<unsigned _v>
        struct find_value<0, _v>
        {
            template<size_t N>
            constexpr _T operator()(_T _default, const init_pair_t(&init)[N]) const
            {
                return _default;
            }
        };

        template<unsigned i, typename...Unused>
        struct fill_array
        {
            using next_t = fill_array<i - 1, Unused...>;

            template<size_t N, typename...TArgs>
            constexpr array_t operator()(_T _default, const init_pair_t(&init)[N], TArgs...args) const
            {
                return next_t{}
                    (_default, init,
                        find_value<N, i - 1>{}(_default, init),
                        args...
                        );

            }
        };

        template<typename...Unused>
        struct fill_array<0, Unused...>
        {
            template<size_t N, typename...TArgs>
            constexpr array_t operator()(_T _default, const init_pair_t(&init)[N], TArgs...args) const
            {
                return { {args...} };
            }
        };


        template<size_t N>
        constexpr array_t operator()(_T _default, const init_pair_t(&init)[N]) const
        {
            return fill_array<256>{}(_default, init);
        }
    }; 
};

namespace std
{// these are backported implementations of C++17 methods
    template<size_t i, class T, size_t N>
    constexpr const T& get(const compile_time_bits::const_array<T, N>& in) noexcept
    {
        return in[i];
    };
    template<class T, size_t N>
    constexpr size_t size(const compile_time_bits::const_array<T, N>& in) noexcept
    {
        return N;
    }
    template<class T, size_t N>
    constexpr auto begin(const compile_time_bits::const_array<T, N>& in) noexcept 
        -> typename compile_time_bits::const_array<T, N>::const_iterator
    {
        return in.begin();
    }
    template<class T, size_t N>
    constexpr auto end(const compile_time_bits::const_array<T, N>& in) noexcept 
        -> typename compile_time_bits::const_array<T, N>::const_iterator
    {
        return in.end();
    }
};


#endif


