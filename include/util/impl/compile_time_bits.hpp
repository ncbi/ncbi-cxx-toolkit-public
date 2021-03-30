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
#include <array>
#include <tuple>
#include <type_traits>

#include <corelib/tempstr.hpp>
#include <corelib/ncbistr.hpp>
#include "ct_crc32.hpp"

namespace std
{ // backported C++20 defs
    template< class T >
    struct remove_cvref {
        typedef std::remove_cv_t<std::remove_reference_t<T>> type;
    };
    template< class T >
    using remove_cvref_t = typename remove_cvref<T>::type;
}

// on some compilers tuple are undeveloped for constexpr
#if defined(NCBI_COMPILER_ICC) || (defined(NCBI_COMPILER_MSVC) && NCBI_COMPILER_VERSION<=1916)
#   define ct_const_tuple compile_time_bits::const_tuple
#   include "const_tuple.hpp"
#else
#   define ct_const_tuple std::tuple
#   define const_tuple std::tuple
#endif

#ifdef NCBI_HAVE_CXX17
#   define ct_const_array std::array
#   define const_array std::array
#   include "ct_string_cxx17.hpp"
#else
#   define ct_const_array compile_time_bits::const_array
#   include "ct_string_cxx14.hpp"
#endif

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
    struct array_size<const_array<T, N>>: std::integral_constant<size_t, N> {};

    template <typename T, std::size_t N>
    struct array_size<T[N]>: std::integral_constant<size_t, N> {};

    template<typename T>
    constexpr size_t get_array_size(T&&)
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
    struct array_elem<const_array<T,N>>
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
        -> const_array<_Elem, N>
    {
        return {a[I]...};
    }

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
        return make_array(_array);
    }

    template<ncbi::NStr::ECase case_sensitive, class _Hash = ct::SaltedCRC32<case_sensitive>>
    class CHashString
    {
    public:
        using hash_func = _Hash;
        using hash_type = typename _Hash::type;
        using sv = ct_string;

        constexpr CHashString() noexcept = default;

        template<size_t N>
        constexpr CHashString(char const (&s)[N]) noexcept
            : m_view{s, N-1},  m_hash{hash_func::ct(s)}
        {}

        CHashString(const sv& s) noexcept
            : m_view{s}, m_hash(hash_func::rt(s.data(), s.size()))
        {}

        constexpr operator hash_type() const noexcept { return m_hash; }
        constexpr operator const sv&() const noexcept { return m_view; }
        constexpr hash_type get_hash() const noexcept { return m_hash; }
        constexpr const sv& get_view() const noexcept { return m_view; }

    private:
        sv        m_view;
        hash_type m_hash{ 0 };
    };
}

namespace std
{
    template<class _Traits, ncbi::NStr::ECase cs> inline
        basic_ostream<char, _Traits>& operator<<(basic_ostream<char, _Traits>& _Ostr, const compile_time_bits::CHashString<cs>& v)
    {
        return operator<<(_Ostr, v.get_view());
    }
}

namespace compile_time_bits
{
    template<typename T, typename _B=T>
    struct real_underlying_type
    {
        using type = T;
    };

    template<typename T>
    struct real_underlying_type<T, std::enable_if_t<std::is_enum<T>::value, T>>
    {
        using type = std::underlying_type_t<T>;
    };
    template<typename T>
    using real_underlying_type_t = typename real_underlying_type<T>::type;

    // write your own DeduceType specialization if required
    template<typename _BaseType, typename _BackType=_BaseType>
    struct DeduceType
    {
        using value_type = _BaseType;
        using init_type  = _BackType;
        using hash_type  = void;  //this is basic type, it doesn't have any comparison rules
    };

    template<typename _BaseType>
    struct DeduceType<_BaseType, std::enable_if_t<
            std::is_enum<_BaseType>::value || 
            std::numeric_limits<_BaseType>::is_integer, 
            _BaseType>>
    {
        using value_type = _BaseType;
        using init_type  = _BaseType;
        using hash_type  = _BaseType;
        //using hash_type  = real_underlying_type_t<_BaseType>;
        using hash_compare   = std::less<hash_type>;
        using value_compare  = std::less<value_type>;
    };

    template<class _CharType, typename tag=tagStrCase>
    struct StringType
    {
        using view = ct_basic_string<_CharType>;

        using value_type = view;
        using init_type  = view;
        using hash_type  = view;

        using hash_compare  = std::less<tag>;
        using value_compare = std::less<tag>;
    };

    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceType<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
        : StringType<char, std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
    {};

    template<class _CharType>
    struct DeduceType<
        _CharType*,
        std::enable_if_t<
            std::is_same<_CharType, char>::value ||
            std::is_same<_CharType, wchar_t>::value ||
            std::is_same<_CharType, char16_t>::value ||
            std::is_same<_CharType, char32_t>::value,
            _CharType*>
        > : StringType<_CharType> {};

    template<typename T>
    struct DeduceType<const T*>: DeduceType<T*> {};

    template<class T>
    struct DeduceType<
        T,
        std::enable_if_t<
            std::is_same<T, ncbi::CTempString>::value ||
            std::is_same<T, ncbi::CTempStringEx>::value,
            T>
        > : StringType<char>
    {};

    template<class _CharType>
    struct DeduceType<ct_basic_string<_CharType>> : StringType<_CharType> {};

    template<class _CharType, class Traits, class Allocator>
    struct DeduceType<std::basic_string<_CharType, Traits, Allocator>>
        : StringType<_CharType>
    {};

#ifdef __cpp_lib_string_view
    template<class _CharType, class Traits>
    struct DeduceType<std::basic_string_view<_CharType, Traits>>
        : StringType<_CharType>
    {};
#endif

    template<typename..._Types>
    struct DeduceType<std::pair<_Types...>>
    {
        using value_type = std::pair<typename DeduceType<_Types>::value_type...>;
        using init_type  = std::pair<typename DeduceType<_Types>::init_type...>;
        using hash_type  = void;
    };

    template<typename..._Types>
    struct DeduceType<std::tuple<_Types...>>
    {
        using value_type = const_tuple<typename DeduceType<_Types>::value_type...>;
        using init_type  = const_tuple<typename DeduceType<_Types>::init_type...>;
        using hash_type  = void;
    };

    // there is nothing to deduce if type has init_type and value_type members
    template<class _Self>
    struct DeduceType<_Self,
        std::enable_if_t<
            std::is_copy_constructible<typename _Self::init_type>::value &&
            std::is_copy_constructible<typename _Self::value_type>::value,
            _Self>> : _Self {};


    template<typename T>
    struct DeduceHashedType: DeduceType<T> {};

    // this crc32 hashed string, probably nobody will use it
    template<ncbi::NStr::ECase case_sensitive>
    struct DeduceHashedType<std::integral_constant<ncbi::NStr::ECase, case_sensitive>>
    {
        using init_type  = CHashString<case_sensitive>;
        using value_type = typename init_type::sv;
        using hash_type  = typename init_type::hash_type;
        using tag        = std::integral_constant<ncbi::NStr::ECase, case_sensitive>;
        using hash_compare   = std::less<hash_type>;
        using value_compare  = std::less<tag>;
    };
}

namespace compile_time_bits
{
    template<typename _Traits, bool remove_duplicates>
    class TInsertSorter;

    template<typename _Ty>
    struct simple_sort_traits
    {
        using hashed_key_type = DeduceType<_Ty>;

        using value_type = typename hashed_key_type::value_type;
        using hash_type  = typename hashed_key_type::hash_type;
        using init_type  = typename hashed_key_type::init_type;
        using init_key_type = typename hashed_key_type::init_type;
        using key_type = typename hashed_key_type::value_type;

        static constexpr hash_type get_init_hash(const init_type& v)
        {
            return v;
        }
        static constexpr const key_type& get_key(const value_type& v)
        {
            return v;
        }
        static constexpr value_type construct(const init_type& v)
        {
            return v;
        }
    };

    template<typename _T1, typename _T2>
    struct straight_map_traits
    {
        using HT1 = DeduceType<_T1>;
        using HT2 = DeduceType<_T2>;
        using pair_type = DeduceType<std::pair<HT1, HT2>>;
        using hashed_key_type = HT1;

        using value_type = typename pair_type::value_type;
        using hash_type  = typename HT1::hash_type;
        using init_type  = typename pair_type::init_type;
        using init_key_type = typename HT1::init_type;
        using key_type = typename HT1::value_type;

        static constexpr hash_type get_init_hash(const init_type& v)
        {
            return v.first;
        }
        static constexpr const key_type& get_key(const value_type& v)
        {
            return v.first;
        }
        static constexpr const key_type& get_key(const key_type& v)
        {
            return v;
        }
        static constexpr value_type construct(const init_type& v)
        {
            return value_type{ v.first, v.second };
        }
    };

    template<typename _T1, typename _T2>
    struct reverse_map_traits
    {
        using init_type  = typename straight_map_traits<_T1, _T2>::init_type;
        using HT1 = DeduceType<_T2>;
        using HT2 = DeduceType<_T1>;
        using pair_type = DeduceType<std::pair<HT1, HT2>>;
        using hashed_key_type = HT1;

        using value_type = typename pair_type::value_type;
        using hash_type  = typename HT1::hash_type;
        using init_key_type = typename HT1::init_type;
        using key_type = typename HT1::value_type;

        static constexpr hash_type get_init_hash(const init_type& v)
        {
            return v.second;
        }
        static constexpr const key_type& get_key(const value_type& v)
        {
            return v.first;
        }
        static constexpr const key_type& get_key(const key_type& v)
        {
            return v;
        }
        static constexpr value_type construct(const init_type& v)
        {
            return value_type{ v.second, v.first };
        }
    };

    // This is 'contained' backend for binary search containers, it can be beared with it's owning class
    template<typename _ProxyType>
    class simple_backend
    {};

    template<typename _SizeType, typename _ArrayType>
    class simple_backend<std::pair<_SizeType, _ArrayType>>
    {
    public:
        constexpr simple_backend() = default;

        constexpr simple_backend(const std::pair<_SizeType, _ArrayType>& init)
            : m_array { std::get<1>(init) },
              m_realsize { std::get<0>(init) }
        {}

        constexpr auto realsize()   const noexcept { return m_realsize; }
        constexpr auto indexsize()  const noexcept { return m_realsize; }
        constexpr auto get_values() const noexcept { return m_array.data(); }
        constexpr auto get_index()  const noexcept { return m_array.data(); }
    private:
        _ArrayType  m_array;
        std::size_t m_realsize{0};
    };

    template<typename _SizeType, typename _ArrayType, typename _IndexType>
    class simple_backend<std::tuple<_SizeType, _ArrayType, _IndexType>>
    {
    public:
        using value_type = typename _ArrayType::value_type;
        using hash_type = typename _IndexType::value_type;

        static constexpr size_t m_size = array_size<_IndexType>::value;

        constexpr simple_backend() = default;

        constexpr simple_backend(const std::tuple<_SizeType, _ArrayType, _IndexType>& init)
            : m_index { std::get<2>(init) },
              m_array { std::get<1>(init) },
              m_realsize { std::get<0>(init) }
        {}

        constexpr auto realsize()   const noexcept { return m_realsize; }
        constexpr auto indexsize()  const noexcept { return m_realsize; }
        constexpr auto get_values() const noexcept { return m_array.data(); }
        constexpr auto get_index()  const noexcept { return m_index.data(); }
    private:
        _IndexType m_index{}; // index is first because it can be aligned
        _ArrayType m_array{};
        _SizeType  m_realsize{0};
    };

    // This is 'referring' backend of fixed size, it points to a external data
    template<typename index_type, typename value_type>
    class portable_backend
    {
    public:
        constexpr portable_backend() = default;

        template<typename _ArrayType>
        constexpr portable_backend(const simple_backend<_ArrayType>& _Other)
            : m_realsize   { _Other.realsize() },
              m_values     { _Other.get_values() },
              m_index_size { _Other.realsize() },
              m_index      { _Other.get_index() }
        {}

        constexpr const value_type* get_values() const noexcept { return m_values; }
        constexpr const index_type* get_index() const noexcept { return m_index; }
        constexpr size_t realsize() const noexcept { return m_realsize; }
    private:
        size_t m_realsize {0};
        const value_type* m_values {nullptr};
        size_t m_index_size {0};
        const index_type* m_index {nullptr};
    };

    template<typename _Traits, typename _Backend>
    class const_set_map_base
    {
    public:
        // standard definitions
        using value_type      = typename _Traits::value_type;
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

        // non-standard definitions
        friend class const_set_map_base<_Traits, void>;

        using hash_type       = typename _Traits::hash_type;
        using intermediate    = typename _Traits::key_type;
        using init_key_type   = typename _Traits::init_key_type;
        using init_type       = typename _Traits::init_type;

        static constexpr bool can_index = std::numeric_limits<hash_type>::is_integer;
        using index_type      = std::conditional_t<
                                    can_index,
                                    hash_type,
                                    typename _Traits::value_type>;

        using backend_type    = std::conditional_t<std::is_void<_Backend>::value,
                                    portable_backend<index_type, value_type>,
                                    _Backend>;

        // standard methods

        constexpr size_type      size()     const noexcept { return m_backend.realsize(); }
        constexpr size_type      max_size() const noexcept { return size(); }
        constexpr size_type      capacity() const noexcept { return size(); }
        constexpr bool           empty()    const noexcept { return size() == 0; }
        constexpr const_iterator begin()    const noexcept { return m_backend.get_values(); }
        constexpr const_iterator cbegin()   const noexcept { return begin(); }
        constexpr const_iterator end()      const noexcept { return begin() + m_backend.realsize(); }
        constexpr const_iterator cend()     const noexcept { return end(); }
        constexpr const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator{ end() }; }
        constexpr const_reverse_iterator rcbegin() const noexcept { return rbegin(); }
        constexpr const_reverse_iterator rend()    const noexcept { return const_reverse_iterator{ begin() }; }
        constexpr const_reverse_iterator rcend()   const noexcept { return rend(); }

        using key_compare = typename _Traits::hashed_key_type::value_compare;

        struct value_compare
        {
            bool operator()( const value_type& l, const value_type& r ) const         
            {
                return key_compare{}(_Traits::get_key(l), _Traits::get_key(r));
            }
        };

        struct hash_compare
        {
            template<typename _TL, typename _TR>
            bool operator()(const _TL & l, const _TR & r ) const
            {
                return typename _Traits::hashed_key_type::hash_compare{}(_Traits::get_key(l), _Traits::get_key(r));
            }
        };   

        const_iterator lower_bound(intermediate _key) const
        {
            init_key_type key{_key};
            hash_type hash{key};
            auto it = std::lower_bound(m_backend.get_index(), m_backend.get_index()+m_backend.realsize(), hash, hash_compare{});
            auto offset = std::distance(m_backend.get_index(), it);
            return begin() + offset;
        }
        const_iterator upper_bound(intermediate _key) const
        {
            init_key_type key{_key};
            hash_type hash{key};
            auto it = std::upper_bound(m_backend.get_index(), m_backend.get_index()+m_backend.realsize(), hash, hash_compare{});
            auto offset = std::distance(m_backend.get_index(), it);
            return begin() + offset;
        }
        std::pair<const_iterator, const_iterator>
            equal_range(intermediate _key) const
        {
            init_key_type key{_key};
            hash_type hash{key};
            auto _range = std::equal_range(m_backend.get_index(), m_backend.get_index()+m_backend.realsize(), hash, hash_compare{});
            return std::make_pair(
                begin() + std::distance(m_backend.get_index(), _range.first),
                begin() + std::distance(m_backend.get_index(), _range.second));
        }

        const_iterator find(intermediate _key) const
        {
            auto it = lower_bound(_key);
            if (it == end() || key_compare{}(_key, _Traits::get_key(*it)))
                return end();
            else
                return it;
        }

        size_t get_alignment() const
        { // debug method for unit tests
            return std::uintptr_t(m_backend.get_index()) % std::hardware_constructive_interference_size;
        }

        constexpr const_set_map_base() = default;

        constexpr const_set_map_base(const backend_type& backend)
            : m_backend{ backend }
        {}

        template<
            typename _Other,
            typename _NotUsed = std::enable_if_t<
                std::is_void<_Backend>::value &&
                std::is_constructible<backend_type, typename _Other::backend_type
                    >::value>
            >
        constexpr const_set_map_base(const _Other& _other)
            : m_backend{ _other.m_backend }
        {}

    protected:
    
        template<size_t N>
        static constexpr auto make_backend(init_type const (&init)[N])
        {
            return make_backend(make_array(init));
        }
        template<size_t N>
        static constexpr auto make_backend(const const_array<init_type, N>& init)
        {
            auto proxy = TInsertSorter<_Traits, true>::sort(init);
            using backend_type = simple_backend<decltype(proxy)>;
            return backend_type{proxy};
        }     

        backend_type m_backend;
    };
}

namespace compile_time_bits
{
    // this helper packs set of bits into an array usefull for initialisation of bitset
    // using C++14

    template<class _Ty, size_t _Size, class u_type>
    struct bitset_traits
    {
        using array_t = const_array<_Ty, _Size>;

        static constexpr size_t width = 8 * sizeof(_Ty);

        struct range_t
        {
            size_t m_from;
            size_t m_to;
        };

        static constexpr bool check_range(const range_t& range, size_t i)
        {
            return (range.m_from <= i && i <= range.m_to);
        };

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
            for (_O _rec : _init)
            {
                size_t rec = static_cast<size_t>(static_cast<u_type>(_rec));
                if (_min <= rec && rec <= _max)
                {
                    ret |= _Ty(1) << (rec % width);
                }
            }
            return ret;
        }
        template <size_t I, size_t N>
        static constexpr _Ty assemble_mask(const const_array<char, N>& _init)
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
            return {assemble_mask<I>(_init)...};
        }

        template<typename _O>
        static constexpr array_t set_bits(std::initializer_list<_O> args)
        {
            return assemble_bitset(args, std::make_index_sequence<_Size>{});
        }
        template<typename T>
        static constexpr array_t set_range(T from, T to)
        {
            return assemble_bitset(range_t{ static_cast<size_t>(from), static_cast<size_t>(to) }, std::make_index_sequence<_Size>{});
        }
        template<size_t N>
        static constexpr array_t set_bits(const const_array<char, N>& in)
        {
            return assemble_bitset(in, std::make_index_sequence<_Size>{});
        }
    };
}

namespace std
{
    template<typename...TArgs>
    size_t size(const compile_time_bits::const_set_map_base<TArgs...>& _container)
    {
        return _container.size();
    }
}

#include "ctsort_cxx14.hpp"

#endif

