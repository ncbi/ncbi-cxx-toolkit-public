#ifndef __CT_STD_HPP_INCLUDED__
#define __CT_STD_HPP_INCLUDED__

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
 *  various class backported classes and method from C++17
 *  cstd::const_array - constexpr equivalent of std::array
 *  most of contents of this file should go when C++14 and C++17 language
 *  be allowed in toolkit sources
 *
 */

namespace cstd
{

    template<class T, size_t N>
    struct const_array
    {
        using value_type = T;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = const value_type&;
        using const_reference = const value_type&;
        using pointer = const value_type*;
        using const_pointer = const value_type*;
        using cont_t = value_type[N];
        using const_iterator = const value_type*;
        using iterator = const value_type*;

        static constexpr size_t m_size = N;

        constexpr size_t size() const noexcept
        {
            return N;
        }
        constexpr size_t capacity() const noexcept
        {
            return N;
        }

        constexpr const value_type& operator[](size_t _pos) const noexcept
        {
            return m_data[_pos];
        }
        constexpr const_iterator begin() const noexcept
        {
            return m_data;
        }
        constexpr const_iterator end() const noexcept
        {
            return m_data + N;
        }
        constexpr const value_type* data() const noexcept
        {
            return m_data;
        }
        value_type* data()
        {
            return m_data;
        }

        cont_t m_data;
    };

    template<class T>
    struct const_array<T, 0>
    {
        using const_iterator = const T*;
        using value_type = T;

        static constexpr size_t m_size = 0;

        constexpr size_t size() const noexcept
        {
            return 0;
        }
        constexpr size_t capacity() const noexcept
        {
            return 0;
        }

        constexpr const_iterator begin() const noexcept
        {
            return nullptr;
        }
        constexpr const_iterator end() const noexcept
        {
            return nullptr;
        }
        const value_type* data() const noexcept
        {
            return nullptr;
        }
    };

    template<class FirstType, class SecondType>
    struct pair
    {
        typedef FirstType first_type;
        typedef SecondType second_type;

        first_type first;
        second_type second;

        // GCC doesn't forward correctly arguments types
        // when aggregated initialised

        constexpr pair() = default;

        template<typename T1, typename T2>
        constexpr pair(T1&& v1, T2&& v2)
            : first(std::forward<T1>(v1)), second(std::forward<T2>(v2))
        {}

        constexpr bool operator <(const pair& o) const
        {
            return (first < o.first) ||
                (first == o.first && second < o.second);
        }
    };

    struct string_view
    {
        constexpr string_view() = default;

        template<size_t N>
        constexpr string_view(const char(&s)[N]) noexcept
            : m_len{ N - 1 }, m_data{ s }
        {}

        constexpr string_view(const char* s, size_t len) noexcept
            : m_len{ len }, m_data{ s }
        {}

        string_view(const ncbi::CTempString& view)
            :m_len{ view.size() }, m_data{ view.data() }
        {}

        constexpr const char* data() const noexcept
        {
            return m_data;
        }
        constexpr size_t size() const noexcept
        {
            return m_len;
        }
        operator ncbi::CTempStringEx() const noexcept
        {
            return ncbi::CTempStringEx(m_data, m_len, ncbi::CTempStringEx::eHasZeroAtEnd);
        }
        operator ncbi::CTempString() const noexcept
        {
            return ncbi::CTempString(m_data, m_len);
        }
        template<class C, class T, class A>
        operator std::basic_string<C, T, A>() const
        {
            return std::string(m_data, m_len);
        }
        constexpr char operator[](size_t pos) const
        {
            return m_data[pos];
        }
        size_t m_len{ 0 };
        const char* m_data{ nullptr };
    };
};

namespace std
{// these are backported implementations of C++17 methods
    template<size_t i, class T, size_t N>
    constexpr const T& get(const cstd::const_array<T, N>& in) noexcept
    {
        return in[i];
    };
    template<class T, size_t N>
    constexpr size_t size(const cstd::const_array<T, N>& in) noexcept
    {
        return N;
    }
    template<class T, size_t N>
    constexpr auto begin(const cstd::const_array<T, N>& in) noexcept -> typename cstd::const_array<T, N>::const_iterator
    {
        return in.begin();
    }
    template<class T, size_t N>
    constexpr auto end(const cstd::const_array<T, N>& in) noexcept -> typename cstd::const_array<T, N>::const_iterator
    {
        return in.end();
    }
};

namespace const_array_bits
{
    template<size_t _cap, class T>
    struct AssembleArray
    {
        using array_t = cstd::const_array<T, _cap>;

        template<size_t i, typename _S>
        struct concat_t
        {
            using next_t = concat_t<i - 1, _S>;

            template<typename...TRest>
            constexpr array_t operator() (const _S* s, size_t N, TRest...rest) const noexcept
            {
                return next_t() (s, N, i <= N ? T(s[i - 1]) : T{}, rest...);
            }
        };

        template<typename _S>
        struct concat_t<0, _S>
        {
            template<typename...TRest>
            constexpr array_t operator() (const _S* s, size_t N, TRest...rest) const noexcept
            {
                return { { rest... } };
            }
        };

        template<class T2>
        constexpr array_t operator() (const T2* s, size_t realN) const
        {
            return concat_t<_cap, T2>()(s, realN);
        }
        template<class T2, size_t N>
        constexpr array_t operator() (const cstd::const_array<T2, N>& s) const
        {
            return concat_t<_cap, T2>()(s.data(), s.size());
        }
    };
};

namespace cstd
{
    template<typename T, typename...TArgs>
    constexpr const_array < T, 1 + sizeof...(TArgs)> make_array(T&& first, TArgs&&...rest)
    {
        return { { std::forward<T>(first), std::forward<TArgs>(rest)...} };
    }

    template<typename T, size_t N>
    constexpr auto make_array(const T(&input)[N]) -> const_array<T, N>
    {
        return const_array_bits::AssembleArray<N, T>{}(input, N);
    }
};


#endif

