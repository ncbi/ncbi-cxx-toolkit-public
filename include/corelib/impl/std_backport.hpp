#ifndef CORELIB___STDBACKPORT__HPP
#define CORELIB___STDBACKPORT__HPP

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
 *  various C++ backported templates
 *
 *
 */

#include <type_traits>
#include <array>

namespace std
{ // backported C++20,23 defs

#ifndef __cpp_lib_remove_cvref
    template< class T >
    struct remove_cvref {
        typedef std::remove_cv_t<std::remove_reference_t<T>> type;
    };
    template< class T >
    using remove_cvref_t = typename remove_cvref<T>::type;
#endif

// backported C++23
#ifndef __cpp_lib_to_underlying
    template< class Enum >
    constexpr std::underlying_type_t<Enum> to_underlying( Enum e ) noexcept
    {
        return static_cast<std::underlying_type_t<Enum>>(e);
    }
#endif

}

#ifndef __cpp_lib_hardware_interference_size
namespace std
{// these are backported implementations of C++17 methods
    constexpr size_t hardware_destructive_interference_size = 64;
    constexpr size_t hardware_constructive_interference_size = 64;
}
#endif

namespace compile_time_bits
{
    template <typename T>
    struct array_size {};

    template <typename T>
    struct array_size<T&>: array_size<std::remove_cv_t<T>> {};

    template <typename T, std::size_t N>
    struct array_size<std::array<T, N>>: std::integral_constant<size_t, N> {};

    template <typename T, std::size_t N>
    struct array_size<T[N]>: std::integral_constant<size_t, N> {};

    template<typename T>
    constexpr size_t get_array_size(T&&) noexcept
    {
        return array_size<T>::value;
    }

    template<typename T>
    struct array_elem{};

    template<typename T>
    struct array_elem<T&>: array_elem<std::remove_cv_t<T>> {};

    template<typename T>
    struct array_elem<T&&>: array_elem<std::remove_cv_t<T>> {};

    template<typename T, size_t N>
    struct array_elem<T[N]>
    {
        using type = T;
    };
    template<typename T, size_t N>
    struct array_elem<std::array<T,N>>
    {
        using type = T;
    };
    template<typename T>
    using array_elem_t = typename array_elem<T>::type;

    template <typename T,
              size_t N = array_size<T>::value,
              typename _Elem = array_elem_t<T>,
              std::size_t... I>
    constexpr auto to_array_impl(T&& a, std::index_sequence<I...>)
        -> std::array<_Elem, N>
    {
        return {a[I]...};
    }

    #if 0
    template <typename T, size_t N = array_size<T>::value>
    constexpr auto make_array(T&& a)
    {
        return to_array_impl(std::forward<T>(a), std::make_index_sequence<N>{});
    }
    template <typename T, size_t N>
    constexpr auto make_array(T const (&a)[N])
    {
        return to_array_impl(a, std::make_index_sequence<N>{});
    }
    #endif

    template <
        typename...TArgs,
        size_t N=sizeof...(TArgs),
        typename _Tuple=typename std::enable_if<(N>1),
             std::tuple<TArgs...>>::type,
        typename T = std::tuple_element_t<0, _Tuple>
        >
    constexpr auto make_array(TArgs&&...args)
    {
        T _array[] = { std::forward<TArgs>(args)... };
        return to_array_impl(_array, std::make_index_sequence<N>{});
    }

}

// Backporting C++ P0325R4 which is enabled in C++20
#ifndef __cpp_lib_to_array
namespace std
{
    template <typename T, size_t N = compile_time_bits::array_size<T>::value>
    constexpr auto to_array(T&& a)
    {
        return compile_time_bits::to_array_impl(std::forward<T>(a), std::make_index_sequence<N>{});
    }
    template <typename T, size_t N>
    constexpr auto to_array(T const (&a)[N])
    {
        return compile_time_bits::to_array_impl(a, std::make_index_sequence<N>{});
    }
}
#endif


#endif

