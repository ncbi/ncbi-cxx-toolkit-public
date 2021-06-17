#ifndef __CT_STRING_CXX14_HPP_INCLUDED__
#define __CT_STRING_CXX14_HPP_INCLUDED__

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
 *  1. compile time string that can be compared at compile time with or without case sensivity
 *     it backports some std::string_view implementations from C++17
 *  2. backported std::array and all of its methods are constexp
 *     requires C++14
 *
 *
 */

namespace compile_time_bits
{
    using tagStrCase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eCase>;
    using tagStrNocase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eNocase>;

    template<class _Char>
    struct ct_basic_string
    {
        using char_type = _Char;
        using sv = ncbi::CTempString;

        template<typename T, typename _Ty=std::remove_cvref_t<T>, typename _Arg=_Ty>
        using if_available_to = std::enable_if_t<std::is_constructible<const _Ty&, sv>::value, _Arg>;

        template<typename T, typename _Ty=std::remove_cvref_t<T>, typename _Arg=_Ty>
        using if_available_from = std::enable_if_t<std::is_constructible<sv, const _Ty& >::value, _Arg>;

        constexpr ct_basic_string() = default;

        constexpr ct_basic_string(const char_type* s, size_t len) noexcept
            : m_len{ len }, m_data{ s }
        {}

        template<size_t N>
        constexpr ct_basic_string(char_type const (&s)[N]) noexcept
            : ct_basic_string { s, N-1 }
        {}

        explicit ct_basic_string(const sv& o)
            : ct_basic_string { o.data(), o.size() }
        {}

        template<class T, class _Ty=if_available_from<T>>
        ct_basic_string(const T& o)
            : ct_basic_string{ sv{o} }
        {}

        constexpr const char_type* c_str() const noexcept { return m_data; }
        constexpr const char_type* data() const noexcept { return m_data; }
        constexpr size_t size() const noexcept { return m_len; }

        template<class _Ty, class _Ty1=if_available_to<_Ty>>
        constexpr operator _Ty() const noexcept { return _Ty{sv{data(), size()}}; }

        constexpr char_type operator[](size_t pos) const
        {
            return m_data[pos];
        }

    private:
        size_t m_len{ 0 };
        const char_type* m_data{ nullptr };
    };

    using ct_string = ct_basic_string<char>;

    template<class _CharType, ncbi::NStr::ECase case_sensitive>
    constexpr int Compare(
        std::integral_constant<ncbi::NStr::ECase, case_sensitive> /*tag*/, 
        const ct_basic_string<_CharType>& l, 
        const ct_basic_string<_CharType>& r)
    {
        using int_type=typename std::char_traits<_CharType>::int_type;
        size_t _min = std::min(l.size(), r.size());
        int result = 0;
        size_t i=0;
        while (i<_min && result==0)
        {
            int_type lc = l[i];
            int_type rc = r[i];
            // we support case insensitivity only for char* 
            if (std::is_same<_CharType, char>::value && (case_sensitive == ncbi::NStr::ECase::eNocase))
            {
                lc = ('A' <= lc && lc <= 'Z') ? lc + 'a' - 'A' : lc;
                rc = ('A' <= rc && rc <= 'Z') ? rc + 'a' - 'A' : rc;
            }
            result = (lc-rc); // this will not work for all char32_t values, better to switch to C++17
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

    template<class _CharType, ncbi::NStr::ECase case_sensitive>
    constexpr bool Equal(std::integral_constant<ncbi::NStr::ECase, case_sensitive> tag, const ct_basic_string<_CharType>& l, const ct_basic_string<_CharType>& r)
    {
        return (l.size() == r.size())? Compare(tag, l, r)==0 : false;
    }
    template<class _CharType, ncbi::NStr::ECase case_sensitive>
    constexpr bool NotEqual(std::integral_constant<ncbi::NStr::ECase, case_sensitive> tag, const ct_basic_string<_CharType>& l, const ct_basic_string<_CharType>& r)
    {
        return (l.size() == r.size())? Compare(tag, l, r)!=0 : true;
    } 

    template<typename T>
    constexpr std::enable_if_t<std::is_constructible<ct_string, const T&>::value, bool>
        operator==(const ct_string& l, const T& r) noexcept
    {
        return Equal(tagStrCase{}, l, ct_string{r});
    }
    template<typename T>
    constexpr std::enable_if_t<std::is_constructible<ct_string, const T&>::value, bool>
        operator!=(const ct_string& l, const T& r) noexcept
    {
        return NotEqual(tagStrCase{}, l, ct_string{r});
    }
    template<typename T>
    constexpr std::enable_if_t<std::is_constructible<ct_string, const T&>::value, bool>
        operator==(const T& l, const ct_string& r) noexcept
    {
        return Equal(tagStrCase{}, ct_string{l}, r);
    }
    template<typename T>
    constexpr std::enable_if_t<std::is_constructible<ct_string, const T&>::value, bool>
        operator!=(const T& l, const ct_string& r) noexcept
    {
        return NotEqual(tagStrCase{}, ct_string{l}, r);
    }

}

namespace std
{
    template<ncbi::NStr::ECase case_sensitive>
    struct less<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
    {        
        using tag = std::integral_constant<ncbi::NStr::ECase, case_sensitive>;
        template<class _CharType>
        constexpr bool operator()(const compile_time_bits::ct_basic_string<_CharType>& l, const compile_time_bits::ct_basic_string<_CharType>& r) const
        {
            return Compare(tag{}, l, r)<0;
        }
    };   
    template<ncbi::NStr::ECase case_sensitive>
    struct equal_to<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
    {        
        using tag = std::integral_constant<ncbi::NStr::ECase, case_sensitive>;
        template<class _CharType>
        constexpr bool operator()(const compile_time_bits::ct_basic_string<_CharType>& l, const compile_time_bits::ct_basic_string<_CharType>& r) const
        {
            return Compare(tag{}, l, r)==0;
        }
    };
}

namespace std
{
    template<class _Traits> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const compile_time_bits::ct_string& v)
    {
        typename compile_time_bits::ct_string::sv temp = v;
        _Ostr << temp;
        return _Ostr;
    }
    template<class _CharType>
    std::size_t size(const compile_time_bits::ct_basic_string<_CharType>& _str)
    {
        return _str.size();
    }
}

namespace compile_time_bits
{// these are backported implementations of C++17 methods

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
        constexpr const_reference operator[](size_t _pos) const noexcept { return m_data[_pos]; }
        constexpr reference operator[](size_t _pos) noexcept { return m_data[_pos]; }
        constexpr const_iterator begin() const noexcept { return m_data; }
        constexpr const_iterator end() const noexcept { return m_data + size(); }
        constexpr const_iterator cbegin() const noexcept { return m_data; }
        constexpr const_iterator cend() const noexcept { return m_data + size(); }
        constexpr iterator begin() noexcept { return m_data; }
        constexpr iterator end() noexcept { return m_data + size(); }
        constexpr const value_type* data() const noexcept { return m_data; }

        constexpr operator const container_type&() const { return m_data; }

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

    template<class T>
    struct const_array<T, 0>
    {
        using value_type = T;
        using const_iterator = const value_type*;
        using iterator = value_type*;

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
} // namespace compile_time_bits

namespace std
{// these are backported implementations of C++17 methods
    template<size_t i, class T, size_t N>
    constexpr const T& get(const ct_const_array<T, N>& in) noexcept
    {
        return in[i];
    }
    template<class T, size_t N>
    constexpr size_t size(const ct_const_array<T, N>& /*in*/) noexcept
    {
        return N;
    }
    template<class T, size_t N>
    constexpr auto begin(const ct_const_array<T, N>& in) noexcept
    {
        return in.begin();
    }
    template<class T, size_t N>
    constexpr auto end(const ct_const_array<T, N>& in) noexcept
    {
        return in.end();
    }
} //namespace std

#endif 

