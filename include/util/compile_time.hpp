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

#ifdef NCBI_HAVE_CXX17
#   include "impl/ct_bitset_cxx17.hpp"
#else
#   include "impl/ct_bitset_cxx14.hpp"
#endif



namespace ct
{
    using namespace compile_time_bits;

    template<typename _Key, typename _Ty, typename _Traits, typename _Multi=tag_DuplicatesNo, typename _Backend = void>
    class const_map_impl: public const_set_map_base<_Traits, _Backend, _Multi>
    { // ordered or unordered compile time map
    public:
        using _MyBase = const_set_map_base<_Traits, _Backend, _Multi>;
        using _MyType = const_map_impl<_Key, _Ty, _Traits, _Multi, _Backend>;

        using init_type       = typename _Traits::init_type;
        using intermediate    = typename _MyBase::intermediate;

        using value_type      = typename _MyBase::value_type;
        using key_type        = typename value_type::first_type;
        using mapped_type     = typename value_type::second_type;

        using _MyBase::_MyBase;

        const mapped_type& at(intermediate _key) const
        {
            auto it = _MyBase::find(_key);
            if (it == _MyBase::end())
                throw std::out_of_range("invalid const_map<K, T> key");

            return it->second;
        }

        const mapped_type& operator[](intermediate _key) const
        {
            return at(_key);
        }

        template<size_t N, typename _Enabled = std::enable_if<!_MyBase::is_presorted, init_type>>
        static constexpr auto construct(typename _Enabled::type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N, typename _Enabled = std::enable_if<!_MyBase::is_presorted, init_type>>
        static constexpr auto construct(const const_array<typename _Enabled::type, N>& init)
        {
            auto backend=_MyBase::make_backend(init);
            return const_map_impl<_Key, _Ty, _Traits, _Multi, decltype(backend)>{backend};
        }
        template<typename _Enabled = std::enable_if<_MyBase::is_presorted, _MyType>>
        static constexpr typename _Enabled::type construct(const const_vector<init_type>& init)
        {
            auto backend=_MyBase::presorted(init);
            return const_map_impl<_Key, _Ty, _Traits, _Multi, decltype(backend)>{backend};
        }
    protected:
    };

    template<typename _Key, typename _Ty>
    using const_map = const_map_impl<_Key, _Ty, straight_map_traits<_Key, _Ty>, tag_DuplicatesNo, void>;

    template<typename _Key, typename _Ty>
    using const_multi_map = const_map_impl<_Key, _Ty, straight_map_traits<_Key, _Ty>, tag_DuplicatesYes, void>;

    template<typename _Key, typename _Ty>
    using const_map_presorted = const_map_impl<_Key, _Ty,
        straight_map_traits<_Key, _Ty>,
        tag_DuplicatesNo,
        presorted_backend<const_vector<typename straight_map_traits<_Key, _Ty>::value_type>>>;

    template<typename _Ty, typename _Backend=void>
    class const_set: public const_set_map_base<simple_sort_traits<_Ty>, _Backend>
    {
    public:
        using _MyBase = const_set_map_base<simple_sort_traits<_Ty>, _Backend>;
        using _MyType = const_set<_Ty, _Backend>;

        using init_type      = typename _MyBase::init_type;

        using value_type      = typename _MyBase::value_type;
        using key_type        = value_type;

        using _MyBase::_MyBase;

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N>
        static constexpr auto construct(const const_array<init_type, N>& init)
        {
            auto backend=_MyBase::make_backend(init);
            return const_set<_Ty, decltype(backend)>{backend};
        }
    };

    template<typename T1, typename T2>
    struct const_map_twoway
    {
        using type1 = DeduceType<T1>;
        using type2 = DeduceType<T2>;

        using straight_traits = straight_map_traits<type1, type2>;
        using reverse_traits  = reverse_map_traits<type1, type2>;
        using init_type = typename straight_traits::init_type;

        using map_type1 = const_map_impl<T1, T2, straight_traits, tag_DuplicatesNo>;
        using map_type2 = const_map_impl<T2, T1, reverse_traits,  tag_DuplicatesNo>;

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N>
        static constexpr auto construct(const const_array<init_type, N>& init)
        {
            return std::make_pair(
                map_type1::construct(init),
                map_type2::construct(init)
            );
        }
    };

    template<typename _Ty>
    using const_unordered_set = const_set<DeduceHashedType<_Ty>>;

    template<typename _Key, typename _Ty>
    using const_unordered_map = const_map<DeduceHashedType<_Key>, _Ty>;

    template<typename T1, typename T2>
    using const_unordered_map_twoway = const_map_twoway<DeduceHashedType<T1>, DeduceHashedType<T2>>;

    template<typename _Init, typename _Elem=array_elem_t<_Init>>
    constexpr auto sort(_Init&& init)
    {
        using sorter=TInsertSorter<simple_sort_traits<_Elem>, tag_DuplicatesYes>;
        return std::get<1>(sorter::sort(tag_SortByValues{}, std::forward<_Init>(init)));
    }

}

#ifdef const_array
#   undef const_array
#endif
#ifdef const_tuple
#   undef const_tuple
#endif

// user can define some specific debug instructions
#ifndef DEBUG_MAKE_CONST_MAP
#   define DEBUG_MAKE_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_TWOWAY_CONST_MAP
#   define DEBUG_MAKE_TWOWAY_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_CONST_SET
#   define DEBUG_MAKE_CONST_SET(name)
#endif

// Some compilers (Clang < 3.9, GCC-7) still cannot deduce template parameter N for aggregate initialiazed arrays
// so we have to use two step initialization. This doesn't impact neither of compile time, run time or memory footprint
//

#define MAKE_CONST_MAP(name, type1, type2, ...)                                                                             \
    static constexpr auto name = ct::const_map<type1, type2>::construct(__VA_ARGS__);                                       \
    DEBUG_MAKE_CONST_MAP(name)

#define MAKE_TWOWAY_CONST_MAP(name, type1, type2, ...)                                                                      \
    static constexpr auto name = ct::const_map_twoway<type1, type2>::construct(__VA_ARGS__);                                \
    DEBUG_MAKE_TWOWAY_CONST_MAP(name)

#define MAKE_CONST_SET(name, type, ...)                                                                                     \
    static constexpr ct::const_set<type>::init_type name ## _init [] = __VA_ARGS__;                                         \
    static constexpr auto name = ct::const_set<type>::construct(name ## _init);                                             \
    DEBUG_MAKE_CONST_SET(name)

#define MAKE_CONST_SET1(name, type, ...)                                                                                    \
    static constexpr auto name = ct::const_set<type>::construct(__VA_ARGS__);                                               \
    DEBUG_MAKE_CONST_SET(name)

#ifdef CT_USE_STD_MAP
#    define MAKE_CONST_MAP_COMPAT(name, type1, type2, ...) static const std::map<type1, type2> name = __VA_ARGS__;
#else
#    define MAKE_CONST_MAP_COMPAT MAKE_CONST_MAP
#endif

/// Macro to calculate a length of a static string at compile time.
/// Especially usefull for char* strings.
/// @note
///   std::size() can be used with char[], but not with char* strings.
///   This macro can be used with both.
/// @example
///   static const char* str = "some string";
///   static const char  str[] = "some string";
///   static const size_t len = CT_CONST_STR_LEN(str);
///
#define CT_CONST_STR_LEN(s) std::string_view(s).size()


#endif
