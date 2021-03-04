#ifndef __COMPILE_TIME_BITS_HPP_INCLUDED__
#define __COMPILE_TIME_BITS_HPP_INCLUDED__

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
 *  various compile time structures and functions
 *
 *
 */

#include <cstddef>
#include <utility>
#include <cstdint>

#include <corelib/tempstr.hpp>
#include <corelib/ncbistr.hpp>
#include "ct_crc32.hpp"

#ifdef NCBI_HAVE_CXX17
#   include "ct_string_cxx17.hpp"
#else
#   include "ct_string_cxx14.hpp"
#endif

#include "ctsort_cxx14.hpp"

namespace compile_time_bits
{    
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

    template <class T, std::size_t N, std::size_t... I>
    constexpr const_array<std::remove_cv_t<T>, N>
        to_array_impl(T(&a)[N], std::index_sequence<I...>)
        {
            return { {a[I]...} };
        }

    template <class T, std::size_t N>
    constexpr auto to_array(T(&&a)[N])
    {
        return to_array_impl(a, std::make_index_sequence<N>{});
    }
    template <class T, std::size_t N>
    constexpr auto to_array(T(&a)[N])
    {
        return to_array_impl(a, std::make_index_sequence<N>{});
    }

    template<class T>
    struct const_array<T, 0>
    {
        using const_iterator = const T*;
        using value_type = T;

        static constexpr size_t m_size = 0;

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

    template<class... _Types>
    class const_tuple;

    template<>
    class const_tuple<>
    {	// empty tuple
    public:
    };

    template<class _This,
        class... _Rest>
        class const_tuple<_This, _Rest...>
        : private const_tuple<_Rest...>
    {	// recursive tuple definition
    public:
        typedef _This _This_type;
        typedef const_tuple<_Rest...> _Mybase;
        _This   _Myfirst;	// the stored element


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

    template<ncbi::NStr::ECase case_sensitive, class _Hash = ct::SaltedCRC32<case_sensitive>>
    class CHashString
    {
    public:
        using hash_func = _Hash;
        using hash_type = typename _Hash::type;
        using sv = ct_string<case_sensitive>;

        constexpr CHashString() noexcept = default;

        template<size_t N>
        constexpr CHashString(const char(&s)[N]) noexcept
            : m_view{s, N-1},  m_hash{hash_func::ct(s)}
        {}

        CHashString(const sv& s) noexcept
            : m_view{s}, m_hash(hash_func::rt(s.data(), s.size()))
        {}

        constexpr operator hash_type() const noexcept
        {
            return m_hash;
        }
        constexpr operator const sv&() const noexcept
        {
            return m_view;
        }
        constexpr hash_type get_hash() const noexcept
        {
            return m_hash;
        }
        constexpr const sv& get_view() const noexcept
        {
            return m_view;
        }

    private:
        sv        m_view;
        hash_type m_hash{ 0 };
    };

    template<ncbi::NStr::ECase _cs, typename _hash>
    constexpr bool operator<(const CHashString<_cs, _hash>& l, const CHashString<_cs, _hash>& r) noexcept
    {
        return l.get_hash() < r.get_hash();
    }
    template<ncbi::NStr::ECase _cs, typename _hash>
    constexpr bool operator!=(const CHashString<_cs, _hash>& l, const CHashString<_cs, _hash>& r) noexcept
    {
        return l.get_hash() != r.get_hash();
    }
    template<ncbi::NStr::ECase _cs, typename _hash>
    constexpr bool operator==(const CHashString<_cs, _hash>& l, const CHashString<_cs, _hash>& r) noexcept
    {
        return l.get_hash() == r.get_hash();
    }

}

namespace ct
{
    using namespace compile_time_bits;
}

#ifndef __cpp_lib_hardware_interference_size
namespace std
{// these are backported implementations of C++17 methods
    constexpr size_t hardware_destructive_interference_size = 64;
    constexpr size_t hardware_constructive_interference_size = 64;
}
#endif

namespace std
{// these are backported implementations of C++17 methods
    template<size_t i, class T, size_t N>
    constexpr const T& get(const ct::const_array<T, N>& in) noexcept
    {
        return in[i];
    }
    template<class T, size_t N>
    constexpr size_t size(const ct::const_array<T, N>& /*in*/) noexcept
    {
        return N;
    }
    template<class T, size_t N>
    constexpr auto begin(const ct::const_array<T, N>& in) noexcept
        -> typename ct::const_array<T, N>::const_iterator
    {
        return in.begin();
    }
    template<class T, size_t N>
    constexpr auto end(const ct::const_array<T, N>& in) noexcept
        -> typename ct::const_array<T, N>::const_iterator
    {
        return in.end();
    }

    //template<class>
    // false value attached to a dependent name (for static_assert)
    //constexpr bool always_false = false;

    template<size_t _Index>
    class tuple_element<_Index, ct::const_tuple<>>
    {	// enforce bounds checking
        //static_assert(always_false<integral_constant<size_t, _Index>>,
        //    "tuple index out of bounds");
    };

    template<class _This, class... _Rest>
    class tuple_element<0, ct::const_tuple<_This, _Rest...>>
    {	// select first element
    public:
        using type = _This;
        using _Ttype = ct::const_tuple<_This, _Rest...>;
    };

    template<size_t _Index, class _This, class... _Rest>
    class tuple_element<_Index, ct::const_tuple<_This, _Rest...>>
        : public tuple_element<_Index - 1, ct::const_tuple<_Rest...>>
    {	// recursive tuple_element definition
    };

    template<size_t _Index,
        class... _Types>
        constexpr const typename tuple_element<_Index, ct::const_tuple<_Types...>>::type&
        get(const ct::const_tuple<_Types...>& _Tuple) noexcept
    {	// get const reference to _Index element of tuple
        typedef typename tuple_element<_Index, ct::const_tuple<_Types...>>::_Ttype _Ttype;
        return (((const _Ttype&)_Tuple)._Myfirst);
    }

    template<class T, size_t N>
    class tuple_size<ct::const_array<T, N>>:
        public integral_constant<size_t, N>
    { };

    template<class _Traits, ncbi::NStr::ECase cs> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const compile_time_bits::CHashString<cs>& v)
    {
        return operator<<(_Ostr, v.get_view());
    }
}

namespace compile_time_bits
{
    template<typename _Value>
    struct simple_sort_traits
    {
        using value_type = typename _Value::value_type;
        using hash_type  = typename _Value::hash_type;
        using init_type  = typename _Value::init_type;

        struct Pred
        {
            template<typename _Input>
            constexpr bool operator()(const _Input& input, size_t l, size_t r)
            {
                return input[l] < input[r];
            }
        };
        template<typename _Input>
        static constexpr value_type construct(const _Input& input, size_t I)
        {
            return input[I];
        }
        static constexpr hash_type get_hash(const init_type& v)
        {
            return v;
        }
    };

    template<typename _T1, typename _T2>
    struct straight_sort_traits
    {
        using init_type  = std::pair<typename _T1::init_type, typename _T2::init_type>;
        using value_type = std::pair<typename _T1::value_type, typename _T2::value_type>;
        using hash_type = typename _T1::hash_type;

        struct Pred
        {
            template<typename _Input>
            constexpr bool operator()(const _Input& input, size_t l, size_t r)
            {
                return input[l].first < input[r].first;
            }
        };
        static constexpr hash_type get_hash(const init_type& v)
        {
            return static_cast<hash_type>(v.first);
        }
        template<typename _Input>
        static constexpr value_type construct(const _Input& input, size_t I)
        {
            return value_type{ input[I].first, input[I].second };
        }
    };

    template<typename _T1, typename _T2>
    struct flipped_sort_traits
    {
        using init_type = std::pair<typename _T1::init_type, typename _T2::init_type>;
        using value_type = std::pair<typename _T2::value_type, typename _T1::value_type>;
        using hash_type = typename _T2::hash_type;

        struct Pred
        {
            template<typename _Input>
            constexpr bool operator()(const _Input& input, size_t l, size_t r)
            {
                return input[l].second < input[r].second;
            }
        };
        static constexpr hash_type get_hash(const init_type& v)
        {
            return v.second;
        }
        template<typename _Input>
        static constexpr value_type construct(const _Input& input, size_t I)
        {
            return value_type{ input[I].second, input[I].first };
        }
    };

    // array that is aligned within CPU cache lines
    // its base address and size are both aligned to fit cache lines
    template<typename _HashType, size_t N>
    class aligned_index
    {
    public:
        static constexpr size_t alignment = std::hardware_constructive_interference_size;

        static constexpr size_t get_aligned_size(size_t align) noexcept
        {
            size_t requested_memory = sizeof(const_array<_HashType, N>);
            size_t aligned_blocks = (requested_memory + align - 1) / align;
            size_t aligned_size = (aligned_blocks * align) / sizeof(_HashType);
            return aligned_size;
        }
        static const size_t aligned_size = get_aligned_size(alignment);
        static const size_t size = N;

        // uncomment this alignment statement to observe test_compile_time unit test failure
        alignas(alignment)
        const_array<_HashType, aligned_size> m_array{};
    };

    template<typename _HashType>
    class const_index
    {
    public:
        constexpr const_index() = default;

        template<size_t N>
        constexpr const_index(const aligned_index<_HashType, N>& _init)
            : m_values{ _init.m_array.data() }, 
              m_realsize{ N },
              m_aligned_size{ _init.m_array.size() }
        {
        }
        size_t lower_bound(_HashType value) const noexcept
        {
            auto it = std::lower_bound(m_values, m_values + m_realsize, value);
            return std::distance(m_values, it);
        }
        size_t upper_bound(_HashType value) const noexcept
        {
            auto it = std::upper_bound(m_values, m_values + m_realsize, value);
            return std::distance(m_values, it);
        }
        std::pair<size_t,size_t> equal_range(_HashType value) const noexcept
        {
            auto range = std::equal_range(m_values, m_values + m_realsize, value);
            return std::make_pair(
                std::distance(m_values, range.first),
                std::distance(m_values, range.second));
        }
        const _HashType* m_values = nullptr;
        const size_t     m_realsize = 0;
        const size_t     m_aligned_size = 0;
    };

    using tagStrCase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eCase>;
    using tagStrNocase = std::integral_constant<ncbi::NStr::ECase, ncbi::NStr::eNocase>;

    // we only support strings, integral types and enums
    // write your own DeduceHashedType specialization if required
    template<typename _BaseType, typename _InitType = _BaseType, typename _HashType = void>
    struct DeduceHashedType
    {
        using value_type = _BaseType;
        using init_type = _InitType;
        using hash_type = _HashType;
        static constexpr bool can_index = std::numeric_limits<hash_type>::is_integer;
    };

    template<typename _BaseType>
    struct DeduceHashedType<_BaseType,
        typename std::enable_if<std::is_enum<_BaseType>::value, _BaseType>::type>
        : DeduceHashedType<_BaseType, _BaseType, typename std::underlying_type<_BaseType>::type>
    {
    };

    template<typename _BaseType>
    struct DeduceHashedType<_BaseType,
        typename std::enable_if<std::numeric_limits<_BaseType>::is_integer, _BaseType>::type>
        : DeduceHashedType<_BaseType, _BaseType, _BaseType>
    {
    };

    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceHashedType<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
        : DeduceHashedType<typename CHashString<case_sensitive>::sv, CHashString<case_sensitive>, typename CHashString<case_sensitive>::hash_type>
    {
    };

    template<>
    struct DeduceHashedType<const char*>
        : DeduceHashedType<tagStrCase>
    {
    };

    template<>
    struct DeduceHashedType<char*>
        : DeduceHashedType<tagStrCase>
    {
    };

    template<class Traits, class Allocator>
    struct DeduceHashedType<std::basic_string<char, Traits, Allocator>>
        : DeduceHashedType<tagStrCase>
    {
    };

    template<typename _First, typename _Second>
    struct DeduceHashedType<std::pair<_First, _Second>>
    {
        using first_type  = DeduceHashedType<_First>;
        using second_type = DeduceHashedType<_Second>;
        using value_type  = std::pair<typename first_type::value_type, typename second_type::value_type>;
        using init_type   = value_type;
    };
    template<typename..._Types>
    struct DeduceHashedType<std::tuple<_Types...>>
    {
        using hashed_type = ct::const_tuple<DeduceHashedType<_Types>...>;
        using value_type  = ct::const_tuple<typename DeduceHashedType<_Types>::value_type...>;
        using init_type   = value_type;
    };

    template<typename _Key, typename _Value>
    class const_set_map_base
    {
    public:
        using hashed_key      = DeduceHashedType<_Key>;
        using hashed_value    = DeduceHashedType<_Value>;
        using key_value_type  = typename hashed_key::value_type;
        using init_key_type   = typename hashed_key::init_type;

        using value_type      = typename hashed_value::value_type;        
        using size_type       = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference       = const value_type&;
        using const_reference = const value_type&;
        using pointer         = const value_type*;
        using const_pointer   = const value_type*;
        using iterator        = const value_type*;
        using const_iterator  = const value_type*;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        using intermediate = typename std::conditional<
            sizeof(key_value_type) <= sizeof(uintptr_t)*2,
            const key_value_type,
            const key_value_type& >::type;

        constexpr const_set_map_base() = default;

        template<typename _InitProxy>
        constexpr const_set_map_base(const _InitProxy& _proxy)
            : m_values{ _proxy.second.data() },
              m_index{ _proxy.first },
              m_realsize{ _proxy.second.size() }
        {}

        constexpr const_iterator begin()    const noexcept { return m_values; }
        constexpr const_iterator cbegin()   const noexcept { return m_values; }
        constexpr const_iterator end()      const noexcept { return m_values + m_realsize; }
        constexpr const_iterator cend()     const noexcept { return m_values + m_realsize; }
        constexpr size_type      capacity() const noexcept { return m_realsize; }
        constexpr size_type      size()     const noexcept { return m_realsize; }
        constexpr size_type      max_size() const noexcept { return m_realsize; }
        constexpr bool           empty()    const noexcept { return m_realsize == 0; }

        // alias to decide whether _Key can be constructed from _Arg
        template<typename _K, typename _Arg>
        using if_available = typename std::enable_if<
            std::is_constructible<intermediate, _K>::value, _Arg>::type;

        const_iterator lower_bound(intermediate _key) const
        {
            init_key_type key(_key);
            hash_type hash(std::move(key));
            auto offset = m_index.lower_bound(hash);
            return begin() + offset;
        }
        const_iterator upped_bound(intermediate _key) const
        {
            init_key_type key(_key);
            hash_type hash(std::move(key));
            auto offset = m_index.upped_bound(hash);
            return begin() + offset;
        }

        std::pair<const_iterator, const_iterator>
            equal_range(intermediate _key) const
        {
            init_key_type key(_key);
            hash_type hash(std::move(key));
            auto _range = m_index.equal_range(hash);
            return std::make_pair(
                begin() + _range.first,
                begin() + _range.second);
        }
        size_t get_alignment() const
        {
            return std::uintptr_t(m_index.m_values) % std::hardware_constructive_interference_size;
        }
    protected:

        using hash_type = typename hashed_key::hash_type;
        const value_type* m_values = nullptr;
        const_index<hash_type> m_index{};
        size_type         m_realsize = 0;
    };

    // this helper packs set of bits into an array usefull for initialisation of bitset
    // using C++14

    template<class _Ty, size_t array_size>
    struct bitset_traits
    {
        using array_t = const_array<_Ty, array_size>;

        static constexpr int width = 8 * sizeof(_Ty);

        struct range_t
        {
            size_t m_from;
            size_t m_to;
        };

        static constexpr bool check_range(const range_t& range, size_t i)
        {
            return (range.m_from <= i && i <= range.m_to);
        }

        template <size_t I>
        static constexpr _Ty assemble_mask(const range_t& _init)
        {
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width - 1;
            if (check_range(_init, _min) && check_range(_init, _max))
                return std::numeric_limits<_Ty>::max();

            _Ty ret = 0;
            for (size_t j = 0; j < width; ++j)
            {
                bool is_set = check_range(_init, j + _min);
                if (is_set)
                    ret |= (_Ty(1) << j);
            }
            return ret;
        }
        template <size_t I, typename _O>
        static constexpr _Ty assemble_mask(const std::initializer_list<_O>& _init)
        {
            _Ty ret = 0;
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width - 1;
            for (unsigned rec : _init)
            {
                if (_min <= rec && rec <= _max)
                {
                    ret |= _Ty(1) << (rec % width);
                }
            }
            return ret;
        }
        template <size_t I, size_t N>
        static constexpr _Ty assemble_mask(const char(&_init)[N])
        {
            _Ty ret = 0;
            _Ty mask = 1;
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width;
            for (size_t pos = _min; pos < _max && pos < N; ++pos)
            {
                if (_init[pos] == '1') ret |= mask;
                mask = mask << 1;
            }
            return ret;
        }
        template <typename _Input, std::size_t... I>
        static constexpr array_t assemble_bitset(const _Input& _init, std::index_sequence<I...>)
        {
            return { {assemble_mask<I>(_init)...} };
        }

        template<typename _O>
        static constexpr array_t set_bits(std::initializer_list<_O> args)
        {
            return assemble_bitset(args, std::make_index_sequence<array_size>{});
        }
        template<typename _T>
        static constexpr array_t set_range(_T from, _T to)
        {
            return assemble_bitset(range_t{ static_cast<size_t>(from), static_cast<size_t>(to) }, std::make_index_sequence<array_size>{});
        }
        template<size_t N>
        static constexpr size_t count_bits(const char(&in)[N])
        {
            size_t ret{ 0 };
            for (size_t i = 0; i < N; ++i)
            {
                if (in[i]=='1') ++ret;
            }
            return ret;
        }
        template<size_t N>
        static constexpr array_t set_bits(const char(&in)[N])
        {
            return assemble_bitset(in, std::make_index_sequence<array_size>{});
        }
    };
}

#endif

