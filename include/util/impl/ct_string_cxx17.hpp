#ifndef __CT_STRING_CXX17_HPP_INCLUDED__
#define __CT_STRING_CXX17_HPP_INCLUDED__

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
 *  compile time string that can be compared at compile time with or without case sensivity
 *  uses std::string_view as backend
 *  requires C++17
 *
 */

#include <string_view>
#include <functional>


namespace compile_time_bits
{

    using tagStrCase = std::true_type;
    using tagStrNocase = std::false_type;

    /*
        See: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/n4640.pdf
        Standard made implicit conversion of std::basic_string_view into std::string too strong,
        it prevents implicit conversion and temporal object instantiation in some cases.
        For example some of these operations don't work:

        std::string_view view_a = "aaa";
        const std::string_view& view_ref_a = view_a;
        std::string str_a {view_a};
        // std::string str_b = view_a; // this doens't work
        // const std::string& str_ref_a = view_a; // this doens't work, cannot instantiate temporal object
        const std::string& str_ref_b = "aaa";  // this works by instantiating temporal std::string object
        const std::string_view& view_ref_b = "aaa";  // this works by instantiating temporal std::string_view object
        std::string ReturnString()
        {
            static constexpr std::string_view str {"a"};
            //return str;  // this fails
            return std::string{str}; // this works
        }
    */

    template<class _Char=char>
    class ct_basic_string: public std::basic_string_view<_Char>
    {
    public:
        using char_type = _Char;
        using sv = std::basic_string_view<char_type>;

        constexpr ct_basic_string() noexcept = default;

        template<size_t N>
        constexpr ct_basic_string(const char_type(&s)[N]) noexcept
            : sv { s, N-1 }
        {}

        template<typename T, typename _Ty=std::remove_cvref_t<T>, typename _Arg=_Ty>
        using if_available_to = std::enable_if_t<std::is_constructible<const _Ty&, sv>::value, _Arg>;

        template<typename T, typename _Ty=std::remove_cvref_t<T>, typename _Arg=_Ty>
        using if_available_from = std::enable_if_t<std::is_constructible<sv, const _Ty& >::value, _Arg>;


        template<class T, class _NonRef=if_available_from<T>>
        constexpr ct_basic_string(T&& o)
            : sv{ std::forward< T>(o) }
        {}

        template<class _Traits, class _Alloc>
        operator std::basic_string<_Char, _Traits, _Alloc>() const
        {
            const sv& _this = *this;
            return std::basic_string<_Char, _Traits, _Alloc>{_this};
        }

    };

    using ct_string = ct_basic_string<char>;

    constexpr int CompareNocase(const std::string_view& l, const std::string_view& r)
    {
        size_t _min = std::min(l.size(), r.size());
        int result = 0;
        size_t i=0;
        while (i<_min && result==0)
        {
            int lc = l[i];
            int rc = r[i];
            lc = ('A' <= lc && lc <= 'Z') ? lc + 'a' - 'A' : lc;
            rc = ('A' <= rc && rc <= 'Z') ? rc + 'a' - 'A' : rc;
            result = (lc-rc);
            i++;
        }
        if (result == 0)
        {
            if (l.size()<r.size())
                result = -1;
            else
            if (l.size()>r.size())
                result = +1;
        }
        return result;
    }

    // fixed_string is a compile time class that can be used as cNTTP: class non-type template parameter
    // instantiation as template parameters requires C++20 or N3599 for C++17
    // all members must be public and constexpr capable
    // example of use looks like this:
    //
    // declaration, notice no template parameters are used
    // template<fixed_string arg> struct nnn {}; //
    //
    // instantiation
    // using instance_nnn = nnn<"Hello World">;
    // using instance_vvv = nnn<"Goor Morning">;

    template<typename _CharType, size_t N>
    struct fixed_string
    {   // N is a size of string which is stored as _CharType[N+1] with zero-ending
        // it allows to be used in runtime as a plain C-type string
        using char_type = _CharType;
        using array_type = char_type[N+1];

        constexpr size_t size() const {return N;};
        constexpr fixed_string() = default;
        constexpr fixed_string(const _CharType(&s)[N+1])
        {
            for (size_t i=0; i<=N; ++i)
               m_str[i] = s[i];
        }
        explicit constexpr fixed_string(const std::array<_CharType, N>&s)
        {
            for (size_t i=0; i<N; ++i)
               m_str[i] = s[i];
            m_str[N] = 0;
        }
        template<typename _CharTraits>
        constexpr operator std::basic_string_view<_CharType, _CharTraits>() const
        {
            return {m_str, N};
        }
        constexpr const array_type& get_array() const { return m_str; }
        constexpr auto to_view() const -> std::basic_string_view<_CharType>
        {
            return {m_str, N};
        }
        constexpr _CharType operator[](size_t i) const { return m_str[i];}

        array_type m_str {};

    };

    template<typename _CharType, size_t N>
    fixed_string(const _CharType (&)[N]) -> fixed_string<_CharType, N-1>;

    template<typename _CharType, size_t N, size_t M>
    constexpr auto
    operator+ (const fixed_string<_CharType, N>& l, const fixed_string<_CharType, M>& r) -> fixed_string<_CharType, N+M>
    {
        std::array<_CharType, M+N> res {};
        size_t o_pos = 0;
        for(auto c: l.to_view())
           res[o_pos++] = c;

        for(auto c: r.to_view())
           res[o_pos++] = c;

        return fixed_string(res);
    }

    template<size_t, typename...>
    struct DeduceBaseSizeImpl;

    template<size_t N>
    struct DeduceBaseSizeImpl<N>
    {
        using type = void;
    };

    template<size_t N, typename _First, typename...TArgs>
    struct DeduceBaseSizeImpl<N, _First, TArgs...>
    {
        static constexpr size_t max_size = sizeof(_First);

        using type = std::conditional_t<  (N<=max_size), _First,
            typename DeduceBaseSizeImpl<N, TArgs...>::type>;
    };

    template<size_t size>
    struct DeduceBaseSize
    {
        using type = typename DeduceBaseSizeImpl<size, uint8_t, uint16_t, uint32_t, uint64_t>::type;

        static_assert(!std::is_void_v<type>, "Failed to determine data type");

    };


    // Packed fixed string is used to represent string shorted than 8 bytes
    // as generic numeric types which are faster for comparison operations
    template<size_t N>
    struct packed_fixed_string
    {
        using _MyType = packed_fixed_string<N>;

        using value_type = _MyType;
        using init_type  = _MyType;
        using hash_type  = _MyType;
        using packed_type  = DeduceBaseSize<N>::type;

        using hash_compare  = std::less<_MyType>;
        using value_compare = std::less<_MyType>;

        using char_type = char;
        using sv = std::basic_string_view<char_type>;

        constexpr size_t size() const {return N;};

        constexpr packed_fixed_string() = default;
        constexpr packed_fixed_string(const char_type(&s)[N+1])
            : m_packed{pack_string(s)}
        {}

        packed_fixed_string(const sv& s) noexcept
            : m_packed{pack_string(s)}
        {}
        packed_fixed_string(const ncbi::CTempString& s) noexcept
            : packed_fixed_string{sv(s)}
        {}

        constexpr auto operator<=>(const _MyType& r) const noexcept
        {
            return m_packed <=> r.m_packed;
        }

        constexpr std::basic_string<char_type> to_view() const
        {
            return *this;
        }

        template<typename _CharTraits, typename _Allocator>
        constexpr operator std::basic_string<char_type, _CharTraits, _Allocator>() const
        {
            if (m_packed) {
                packed_type h = m_packed;
                uint8_t v;
                std::basic_string<char_type, _CharTraits, _Allocator> res;
                res.resize(N);
                for (size_t i=0; i<N; ++i) {
                    v = h & 0xFF;
                    h = h >> 8;
                    res[N-1-i] = v;
                }
                return res;
            }
            else
                return {};
        }

        constexpr static packed_type pack_string(const sv& s) noexcept
        {
            packed_type res = 0;
            if (s.length() == N)
                for (size_t i = 0; i<N; ++i)
                    res = (res << 8) | s[i];

            return res;
        }
        constexpr static packed_type pack_string(const char_type(&s)[N+1]) noexcept
        {
            packed_type res = 0;
            for (size_t i = 0; i<N; ++i)
                res = (res << 8) | s[i];

            return res;
        }

        packed_type m_packed {};
    };

    template<size_t N>
    packed_fixed_string(const char (&)[N]) -> packed_fixed_string<N-1>;

    template<size_t N>
    constexpr bool operator==(const packed_fixed_string<N>& l, std::string_view r) noexcept
    {
        return l.m_packed == packed_fixed_string<N>::pack_string(r);
    }

}

namespace std
{
    template<>
    struct less<compile_time_bits::tagStrCase>: less<void> {};
    template<>
    struct equal_to<compile_time_bits::tagStrCase>: equal_to<void> {};

    template<>
    struct less<compile_time_bits::tagStrNocase>
    {
        constexpr bool operator()(const compile_time_bits::ct_string& l, const compile_time_bits::ct_string& r) const
        {
            return compile_time_bits::CompareNocase(l, r) < 0;
        }
    };
    template<>
    struct equal_to<compile_time_bits::tagStrNocase>
    {
        constexpr bool operator()(const compile_time_bits::ct_string& l, const compile_time_bits::ct_string& r) const
        {
            return (l.size() == r.size())? compile_time_bits::CompareNocase(l, r)==0 : false;
        }
    };
}


#endif
