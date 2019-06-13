#ifndef __COMPILE_TIME_HPP_INCLUDED__
#define __COMPILE_TIME_HPP_INCLUDED__

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
 *  const_map   -- constexpr equivalent of std::map
 *
 *
 */

#include "impl/compile_time_bits.hpp"

namespace ct
{
    template<size_t N, typename _Key, typename _Value>
    class const_map
    {
    public:
        static_assert(N > 0, "empty const_map not supported");
        using key_type = _Key;
        using mapped_type = _Value;
        using value_type = cstd::pair<_Key, _Value>;
        using size_type = size_t;
        using array_t = cstd::const_array<value_type, N>;

        using const_iterator = typename array_t::const_iterator;

        using TRecastKey = ct_const_map_bits::recast<key_type>;

        constexpr const_map(const array_t& init)
            : m_array(init)
        {}

        constexpr bool in_order() const
        {
            return ct_const_map_bits::CheckOrder(m_array);
        }
        constexpr const_iterator begin() const noexcept
        {
            return m_array.begin();
        }
        constexpr const_iterator cbegin() const noexcept
        {
            return m_array.begin();
        }
        constexpr size_t capacity() const noexcept
        {
            return N;
        }
        constexpr size_t size() const noexcept
        {
            return N;
        }
        constexpr size_t max_size() const noexcept
        {
            return N;
        }
        constexpr const_iterator end() const noexcept
        {
            return m_array.end();
        }
        constexpr const_iterator cend() const noexcept
        {
            return m_array.end();
        }
        constexpr bool empty() const noexcept
        {
            return N == 0;
        }

        // alias to decide whether _Key can be constructed from _Arg
        template<typename _K, typename _Arg>
        using if_available = typename std::enable_if<
            std::is_constructible<typename TRecastKey::intermediate, _K>::value, _Arg>::type;

        template<typename K>
        if_available<K, const_iterator>
            find(K&& _key) const
        {
            typename TRecastKey::intermediate temp = std::forward<K>(_key);
            typename TRecastKey::type key(temp);
            auto it = std::lower_bound(begin(), end(), std::move(key), Pred());
            if (it != end())
            {
                if (it->first != temp)
                    it = end();
            }
            return it;
        }

        template<typename K>
        if_available<K, const mapped_type&>
            at(K&& _key) const
        {
            auto it = find(std::forward<K>(_key));
            if (it == end())
                throw std::out_of_range("invalid const_map<K, T> key");

            return it->second;
        }
        template<typename K>
        if_available<K, const mapped_type&>
            operator[](K&& _key) const
        {
            return at(std::forward<K>(_key));
        }

    protected:
        template<typename K>
        if_available<K, const_iterator>
            lower_bound(K&& _key) const
        {
            typename TRecastKey::intermediate temp = std::forward<K>(_key);
            typename TRecastKey::type key(std::move(temp));
            return std::lower_bound(begin(), end(), std::move(key), Pred());
        }

        struct Pred
        {
            bool operator()(const value_type& l, const typename TRecastKey::type& r) const
            {
                return l.first < r;
            }
            bool operator() (const key_type& l, const key_type& r) const
            {
                return l < r;
            }
        };

        array_t m_array = {};
    };


    template<bool case_sensitive, bool two_way, typename T1, typename T2>
    struct MakeConstMap
    {
        template<bool need_hash, typename T, typename...Unused>
        struct DeduceType
        {
            using type = T;
        };

        template<typename...Unused>
        struct DeduceType<true, const char*, Unused...>
        {
            using type = ct::CHashString<case_sensitive>;
        };

        template<typename...Unused>
        struct DeduceType<false, const char*, Unused...>
        {
            using type = cstd::string_view;
        };

        template<bool need_hash, class C, class T, class A>
        struct DeduceType<need_hash, std::basic_string<C, T, A>>
        {
            using type = typename DeduceType<need_hash, const char*>::type;
        };

        using first_type = typename DeduceType<true, T1>::type;
        using second_type = typename DeduceType<two_way, T2>::type;
        using init_pair_t = typename cstd::pair<first_type, second_type>;

        template<size_t N>
        struct DeduceMapType
        {
            static constexpr size_t size = N;
            static_assert(size > 0, "empty const_map not supported");

            using map_type = ct::const_map<size, first_type, second_type>;
        };

        template<size_t N>
        constexpr auto operator()(const init_pair_t(&input)[N]) const ->
            typename DeduceMapType<N>::map_type
        {
            return typename DeduceMapType<N>::map_type(input);
        }
        template<size_t N>
        constexpr auto operator()(const cstd::const_array<init_pair_t, N> &input) const ->
            typename DeduceMapType<N>::map_type
        {
            return typename DeduceMapType<N>::map_type(input);
        }
    };

};

#define MAKE_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                              \
    static constexpr ct::MakeConstMap<case_sensitive, false, type1, type2>::init_pair_t                                      \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<case_sensitive, false, type1, type2>{}                                     \
        (ct::Reorder(name ## _init));


#define MAKE_TWOWAY_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                       \
    static constexpr ct::MakeConstMap<case_sensitive, true, type1, type2>::init_pair_t                                       \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<case_sensitive, true, type1, type2>{}                                      \
        (ct::Reorder(name ## _init));                                                                                        \
    static constexpr auto name ## _flipped = ct::MakeConstMap<case_sensitive, true, type2, type1>{}                          \
        (ct::FlipReorder(name ## _init));

/*static_assert(name.in_order(), "const map " #name "is not in order");*/


#endif

