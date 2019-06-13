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

#include "ct_std.hpp"
#include "ct_crc32.hpp"
#include <corelib/tempstr.hpp>

namespace ct_sorting_bits
{
    // Welcome to The Nightmare on Template Recursion Street

    template<typename _Input, typename _Pair, size_t N>
    struct traits_general
    {
        static constexpr size_t size = N;

        using inpair_t = _Pair;
        using sorted_t = cstd::const_array<_Pair, N>;
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
    struct traits_flipped
    {
        static constexpr size_t size = N;
        using inpair_t = _Pair;
        using pair_t = typename cstd::pair<typename _Pair::second_type, typename _Pair::first_type>;
        using sorted_t = cstd::const_array<pair_t, N>;
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
}

namespace ct
{
    using namespace ct_sorting_bits;
    template<typename T, size_t N>
    constexpr auto Reorder(const cstd::const_array<T, N>& input)
        -> typename traits_general<decltype(input), T, N>::sorted_t
    {
        return sorter<traits_general<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto Reorder(const T(&input)[N])
        -> typename traits_general<decltype(input), T, N>::sorted_t
    {
        return sorter<traits_general<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const cstd::const_array<T, N>& input)
        -> typename traits_flipped<decltype(input), T, N>::sorted_t
    {
        return sorter<traits_flipped<decltype(input), T, N>>::order_array(input);
    }

    template<typename T, size_t N>
    constexpr auto FlipReorder(const T(&input)[N])
        -> typename traits_flipped<decltype(input), T, N>::sorted_t
    {
        return sorter<traits_flipped<decltype(input), T, N>>::order_array(input);
    }

};

namespace ct
{
    template<bool case_sensitive = true, class _Hash = SaltedCRC32<case_sensitive>>
    class CHashString : public cstd::string_view
    {
    public:
        using hash_func = _Hash;
        using hash_t = typename _Hash::type;
        using sv = ncbi::CTempString;

        constexpr CHashString() = default;
        template<size_t N>
        constexpr CHashString(const char(&s)[N])
            : cstd::string_view(s, N - 1), m_hash(hash_func::ct(s))
        {}

        explicit CHashString(const sv& s)
            : cstd::string_view(s), m_hash(hash_func::general(s.data(), s.size()))
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
};

namespace ct_const_map_bits
{
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
    constexpr bool CheckOrder(const cstd::const_array<T, N>& tup)
    {
        return check_order_t<cstd::const_array<T, N>, N - 1>{}(tup);
    }

    template<typename T>
    struct recast
    {
        using type = T;
        using intermediate = T;
    };

    template<bool _C, class _H>
    struct recast<ct::CHashString<_C, _H>>
    {
        using type = ct::CHashString<_C, _H>;
        using intermediate = typename type::sv;
    };
};

#endif

