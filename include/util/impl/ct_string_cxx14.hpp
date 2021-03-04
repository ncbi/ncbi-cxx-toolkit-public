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
 *  compile time string that can calculate it's hash at compile time
 *
 *
 */

namespace compile_time_bits
{

    template<ncbi::NStr::ECase case_sensitive>
    struct ct_string
    {
        using sv = ncbi::CTempString;

        template<typename _T, typename _Ty=typename std::remove_reference<_T>::type, typename _Arg=_Ty>
        using if_available_to = typename std::enable_if<
            std::is_constructible<_Ty, sv>::value, _Arg>::type;

        template<typename _T, typename _Ty=typename std::remove_reference<_T>::type, typename _Arg=_Ty>
        using if_available_from = typename std::enable_if<
            std::is_constructible<sv, const _Ty& >::value, _Arg>::type;

        constexpr ct_string() noexcept = default;

        template<size_t N>
        constexpr ct_string(const char(&s)[N]) noexcept
            : m_len{ N - 1 }, m_data{ s } 
        {}

        constexpr ct_string(const char* s, size_t len) noexcept
            : m_len{ len }, m_data{ s }
        {}

        template<class _T, class _Ty=if_available_from<_T>>
        ct_string(const _T& o)
        {
            ncbi::CTempString temp{o};
            m_len = temp.size();
            m_data = temp.data();
        }

        constexpr const char* c_str() const noexcept { return m_data; }
        constexpr const char* data() const noexcept { return m_data; }
        constexpr size_t size() const noexcept { return m_len; }

        template<class _T, class _Ty=if_available_to<_T>>
        constexpr operator _T() const noexcept { return _T{sv{data(), size()}}; }
        
        constexpr char operator[](size_t pos) const
        {
            return m_data[pos];
        }
        bool operator!=(const sv& o) const noexcept
        {
            return (case_sensitive == ncbi::NStr::eCase ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) != 0;
        }
        bool operator==(const sv& o) const noexcept
        {
            return (case_sensitive == ncbi::NStr::eCase ? ncbi::NStr::CompareCase(*this, o) : ncbi::NStr::CompareNocase(*this, o)) == 0;
        }
        size_t m_len{ 0 };
        const char* m_data{ nullptr };
    };
}

namespace std
{
    template<class _Traits, ncbi::NStr::ECase cs> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const compile_time_bits::ct_string<cs>& v)
    {// Padding is not implemented yet
        _Ostr.write(v.data(), v.size());
        return _Ostr;
    }
};

#endif
