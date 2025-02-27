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
 *  const_map  -- constexpr equivalent of std::map
 *  const_set  -- constexpr equivalent of std::set
 *
 */

#include "impl/compile_time_bits.hpp"

#include "impl/ct_bitset_cxx17.hpp"

namespace ct
{
    using namespace compile_time_bits;

    template<typename...TArgs>
    class const_map_impl: public const_set_map_base<const_map_impl, TArgs...>
    { // ordered or unordered compile time map
    public:
        using _MyBase = const_set_map_base<const_map_impl, TArgs...>;

        using value_type  = typename _MyBase::value_type;
        using key_type    = typename value_type::first_type;
        using mapped_type = typename value_type::second_type;

        using _MyBase::_MyBase;

        const mapped_type& at(typename _MyBase::intermediate key) const
        {
            auto it = _MyBase::find(key);
            if (it == _MyBase::end())
                throw std::out_of_range("invalid const_map<K, T> key");

            return it->second;
        }

        const mapped_type& operator[](typename _MyBase::intermediate key) const
        {
            return at(key);
        }

    };

    template<typename _Key, typename _Ty>
    using const_map = const_map_impl<straight_map_traits<_Key, _Ty>, tag_DuplicatesNo>;

    template<typename _Key, typename _Ty>
    using const_multi_map = const_map_impl<straight_map_traits<_Key, _Ty>, tag_DuplicatesYes>;

    template<typename...TArgs>
    class const_set_impl: public const_set_map_base<const_set_impl, TArgs...>
    {
    public:
        using _MyBase    = const_set_map_base<const_set_impl, TArgs...>;
        using value_type = typename _MyBase::value_type;
        using key_type   = value_type;

        using _MyBase::_MyBase;
    };

    template<typename _Ty>
    using const_set = const_set_impl<simple_sort_traits<_Ty>>;

    template<typename T1, typename T2>
    struct const_map_twoway
    {
        using type1 = DeduceType<T1>;
        using type2 = DeduceType<T2>;

        using straight_traits = straight_map_traits<type1, type2>;
        using reverse_traits  = reverse_map_traits<type1, type2>;
        using init_type       = typename straight_traits::init_type;

        using map_type1 = const_map_impl<straight_traits, tag_DuplicatesNo>;
        using map_type2 = const_map_impl<reverse_traits,  tag_DuplicatesNo>;

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return construct(std::to_array(init));
        }
        template<size_t N>
        static constexpr auto construct(const std::array<init_type, N>& init)
        {
            return std::make_pair(
                map_type1::construct(init),
                map_type2::construct(init)
            );
        }
    };



    template<typename _Init, typename _Elem=array_elem_t<_Init>>
    constexpr auto sort(_Init&& init)
    {
        using sorter=TInsertSorter<simple_sort_traits<_Elem>, tag_DuplicatesYes>;
        return std::get<1>(sorter::sort(tag_SortByValues{}, std::forward<_Init>(init)));
    }

}

// Some compilers (Clang < 3.9, GCC-7) still cannot deduce template parameter N for aggregate initialiazed arrays
// so we have to use two step initialization. This doesn't impact neither of compile time, run time or memory footprint
//

#define MAKE_CONST_MAP(name, type1, type2, ...)                                                                             \
    static constexpr auto name = ct::const_map<type1, type2>::construct(__VA_ARGS__);

#define MAKE_TWOWAY_CONST_MAP(name, type1, type2, ...)                                                                      \
    static constexpr auto name = ct::const_map_twoway<type1, type2>::construct(__VA_ARGS__);

#define MAKE_CONST_SET(name, type, ...)                                                                                     \
    static constexpr auto name = ct::const_set<type>::construct(__VA_ARGS__);

#endif
