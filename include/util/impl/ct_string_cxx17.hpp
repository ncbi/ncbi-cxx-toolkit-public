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

namespace compile_time_bits
{
    using tagStrCase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eCase>;
    using tagStrNocase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eNocase>;

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
  
#if 0
    template<class _Char>
    using ct_basic_string = std::basic_string_view<_Char>;
#else    
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

        template<typename _T, typename _NonRef=typename std::remove_cv<typename std::remove_reference<_T>::type>::type>
        using if_available_from = typename std::enable_if<
            std::is_constructible<sv, const _NonRef&>::value, _NonRef>::type;

        template<class _T, class _NonRef=if_available_from<_T>>
        constexpr ct_basic_string(_T&& o)
            : sv{ std::forward<_T>(o) }
        {}

        template<class _Traits, class _Alloc>
        operator std::basic_string<_Char, _Traits, _Alloc>() const 
        { 
            const sv& _this = *this;
            return std::basic_string<_Char, _Traits, _Alloc>{_this};
        }

    };
#endif    
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
