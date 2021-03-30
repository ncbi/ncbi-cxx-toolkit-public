#ifndef __CT_CONST_TUPLE_HPP_INCLUDED__
#define __CT_CONST_TUPLE_HPP_INCLUDED__

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
 */

namespace compile_time_bits
{
template<class... _Types>
    class const_tuple;

    template<>
    class const_tuple<>
    { // empty tuple
    public:
    };

    template<class _This,
        class... _Rest>
        class const_tuple<_This, _Rest...>
        : private const_tuple<_Rest...>
    { // recursive tuple definition
    public:
        typedef _This _This_type;
        typedef const_tuple<_Rest...> _Mybase;
        _This   _Myfirst {}; // the stored element


        constexpr const_tuple() noexcept = default;
        constexpr const_tuple(const _This& _f, const _Rest&..._rest) noexcept
            : _Mybase(_rest...), _Myfirst( _f )
        {
        }
        template<typename _T0, typename..._Other>
        constexpr const_tuple(_T0&& _f0, _Other&&...other) noexcept
            : _Mybase(std::forward<_Other>(other)...), _Myfirst( std::forward<_T0>(_f0) )
        {
        }
    };
} // namespace compile_time_bits

namespace std
{// these are backported implementations of C++17 methods

    template<size_t _Index>
    class tuple_element<_Index, ct_const_tuple<>>
    { // enforce bounds checking
        //static_assert(always_false<integral_constant<size_t, _Index>>,
        //    "tuple index out of bounds");
    };

    template<class _This, class... _Rest>
    class tuple_element<0, ct_const_tuple<_This, _Rest...>>
    { // select first element
    public:
        using type = _This;
        using _Ttype = ct_const_tuple<_This, _Rest...>;
    };

    template<size_t _Index, class _This, class... _Rest>
    class tuple_element<_Index, ct_const_tuple<_This, _Rest...>>
        : public tuple_element<_Index - 1, ct_const_tuple<_Rest...>>
    { // recursive tuple_element definition
    };

    template<size_t _Index,
        class... _Types>
        constexpr const typename tuple_element<_Index, ct_const_tuple<_Types...>>::type&
        get(const ct_const_tuple<_Types...>& _Tuple) noexcept
    { // get const reference to _Index element of tuple
        typedef typename tuple_element<_Index, ct_const_tuple<_Types...>>::_Ttype _Ttype;
        return (((const _Ttype&)_Tuple)._Myfirst);
    }
}

#endif 
