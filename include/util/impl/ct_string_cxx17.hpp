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
 *  compile time string that can calculate it's hash at compile time
 *  uses std::string_view as backend
 *  requires C++17
 *
 */

namespace compile_time_bits
{

    template<ncbi::NStr::ECase case_sensitive>
    struct StrCompare
    {        
    };

    template<>
    struct StrCompare<ncbi::NStr::ECase::eCase>
    {        
        constexpr bool Equal(const std::string_view& l, const std::string_view& r) const
        {
            return l == r;
        }
        constexpr bool NotEqual(const std::string_view& l, const std::string_view& r) const
        {
            return l != r;
        }
        constexpr bool Less(const std::string_view& l, const std::string_view& r) const
        {
            return l < r;
        }

    };
    template<>
    struct StrCompare<ncbi::NStr::ECase::eNocase>
    {        
        constexpr int Compare(const std::string_view& l, const std::string_view& r) const
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
        constexpr bool Equal(const std::string_view& l, const std::string_view& r) const
        {
            return (l.size() == r.size())? Compare(l, r)==0 : false;
        }
        constexpr bool NotEqual(const std::string_view& l, const std::string_view& r) const
        {
            return (l.size() == r.size())? Compare(l, r)!=0 : true;
        }
        constexpr bool Less(const std::string_view& l, const std::string_view& r) const
        {
            return Compare(l, r) < 0;
        }
    };

    template<ncbi::NStr::ECase case_sensitive>
    class ct_string
    {
    public:
        template<typename _T, typename _Ty=typename std::remove_reference<_T>::type, typename _Arg=_Ty>
        using if_available_to = typename std::enable_if<
            std::is_constructible<_Ty, std::string_view>::value, _Arg>::type;

        template<typename _T, typename _Ty=typename std::remove_reference<_T>::type, typename _Arg=_Ty>
        using if_available_from = typename std::enable_if<
            std::is_constructible<std::string_view, const _Ty& >::value, _Arg>::type;

        constexpr ct_string() noexcept = default;

        template<size_t N>
        constexpr ct_string(const char(&s)[N]) noexcept
            : m_view{ s, N-1 } 
        {}

        constexpr ct_string(const char* s, size_t len) noexcept
            : m_view{ s, len } 
        {}

        template<class _T, class _Ty=if_available_from<_T>>
        ct_string(const _T& o)
            : m_view{ o }
        {}

        constexpr operator const std::string_view&() const noexcept { return m_view; }

        template<class _Ty, class _Ty1=if_available_to<_Ty>>
        constexpr operator _Ty() const noexcept { return _Ty{m_view}; }

        constexpr const char* c_str() const noexcept { return m_view.data(); }
        constexpr const char* data() const noexcept { return m_view.data(); }
        constexpr size_t size() const noexcept { return m_view.size(); }
        constexpr char operator[](size_t pos) const { return m_view.operator[](pos); }

    private:
        std::string_view m_view;
    };

    template<ncbi::NStr::ECase case_sensitive>
    constexpr bool operator<(const ct_string<case_sensitive>& l, const std::string_view& r) noexcept
    {
        return StrCompare<case_sensitive>{}.Less(l, r);
    }
    template<ncbi::NStr::ECase case_sensitive>
    constexpr bool operator!=(const ct_string<case_sensitive>& l, const std::string_view& r) noexcept
    {
        return StrCompare<case_sensitive>{}.NotEqual(l, r);
    }
    template<ncbi::NStr::ECase case_sensitive>
    constexpr bool operator==(const ct_string<case_sensitive>& l, const std::string_view& r) noexcept
    {
        return StrCompare<case_sensitive>{}.Equal(l, r);
    }

}
namespace std
{
    template<class _Traits, ncbi::NStr::ECase cs> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const compile_time_bits::ct_string<cs>& v)
    {
        const std::string_view& sv = v;
        return operator<<(_Ostr, sv);
    }

};

#endif
