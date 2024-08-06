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

#include <type_traits>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <array>
#include <algorithm>
#include <corelib/ncbistd.hpp>


// forward declarations to avoid unnecessary includes
namespace ncbi
{
    class CTempString;
    class CTempStringEx;
}

// on some compilers tuple are undeveloped for constexpr
#if defined(NCBI_COMPILER_ICC) || (defined(NCBI_COMPILER_MSVC) && NCBI_COMPILER_VERSION<=1916)
#   define ct_const_tuple compile_time_bits::const_tuple
#   include "const_tuple.hpp"
#else
#   define ct_const_tuple std::tuple
#   define const_tuple std::tuple
#endif

#define ct_const_array std::array
#define const_array std::array

#include "ct_string_cxx17.hpp"


namespace compile_time_bits
{
    template<typename T, typename _Unused=T>
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

    template< class Enum>
    constexpr real_underlying_type_t<Enum> to_real_underlying( Enum e ) noexcept
    {
        return static_cast<real_underlying_type_t<Enum>>(e);
    }


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
        using case_tag = tag;

        using value_type = view;
        using init_type  = view;
        using hash_type  = view;

        using hash_compare  = std::less<tag>;
        using value_compare = std::less<tag>;
    };

    template<typename, typename = void>
    struct is_string_type : std::false_type {};

    template<typename _T>
    struct is_string_type<_T, std::void_t<typename _T::case_tag>> : std::true_type {};


    template<>
    struct DeduceType<tagStrCase>: StringType<char, tagStrCase> {};

    template<>
    struct DeduceType<tagStrNocase>: StringType<char, tagStrNocase> {};


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

    template<class _CharType, class Traits>
    struct DeduceType<std::basic_string_view<_CharType, Traits>>
        : StringType<_CharType>
    {};

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

}

namespace compile_time_bits
{
    // non-containing constexpr vector
    template<typename _Type>
    class const_vector
    {
    public:
        using value_type       = _Type;
        using size_type	       = std::size_t;
        using difference_type  = std::ptrdiff_t;
        using reference        = const value_type&;
        using const_reference  = const value_type&;
        using pointer          = const value_type*;
        using const_pointer    = const value_type*;
        using iterator         = pointer;
        using const_iterator   = const_pointer;
        using reverse_iterator = std::reverse_iterator<iterator>;
        using const_reverse_iterator = std::reverse_iterator<const_iterator>;

        constexpr const_vector() = default;
        constexpr const_vector(const_pointer data, size_t size )
            : m_size{size}, m_data{data} {}

        constexpr const_vector(std::nullptr_t) {}

        template<size_t N>
        constexpr const_vector(const std::array<value_type, N>& data )
            : m_size{data.size()}, m_data{data.data()} {}

        template<size_t N>
        constexpr const_vector(value_type const (&init)[N]) : m_size{N}, m_data{init} {}

        constexpr const_reference operator[](size_t index) const { return m_data[index]; }
        constexpr const_reference at(size_t index) const         { return m_data[index]; }
        constexpr const_pointer   data() const noexcept          { return m_data; }

        constexpr const_reference front() const                  { return m_data[0]; }
        constexpr const_reference back() const                   { return m_data[m_size-1]; }

        constexpr size_type      size()      const noexcept { return m_size; }
        constexpr size_type      max_size()  const noexcept { return size(); }
        constexpr size_type      capacity()  const noexcept { return size(); }
        [[nodiscard]] constexpr bool empty() const noexcept { return size() == 0; }

        constexpr const_iterator begin()     const noexcept { return m_data; }
        constexpr const_iterator cbegin()    const noexcept { return begin(); }
        constexpr const_iterator end()       const noexcept { return begin() + size(); }
        constexpr const_iterator cend()      const noexcept { return end(); }
        constexpr const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator{ end() }; }
        constexpr const_reverse_iterator rcbegin() const noexcept { return rbegin(); }
        constexpr const_reverse_iterator rend()    const noexcept { return const_reverse_iterator{ begin() }; }
        constexpr const_reverse_iterator rcend()   const noexcept { return rend(); }

    protected:
        size_t m_size = 0;
        const_pointer m_data = nullptr;
    };
}

namespace compile_time_bits
{
    using tag_DuplicatesYes = std::true_type;
    using tag_DuplicatesNo  = std::false_type;

    using tag_SortByHashes  = std::true_type;
    using tag_SortByValues  = std::false_type;

    template<typename _Traits, typename _AllowDuplicates>
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

        using index_type = typename _ArrayType::value_type;

        constexpr simple_backend(const std::pair<_SizeType, _ArrayType>& init)
            : m_array { std::get<1>(init) },
              m_realsize { std::get<0>(init) }
        {}

        static constexpr auto capacity = array_size<_ArrayType>::value;
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
        using index_type = typename _IndexType::value_type;
        using tuple_type = std::tuple<_SizeType, _ArrayType, _IndexType>;

        static constexpr auto capacity = array_size<_ArrayType>::value;

        constexpr simple_backend() = default;

        constexpr simple_backend(const tuple_type& init)
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

    template<typename _ArrayType>
    class presorted_backend
    {
    public:
        using value_type = typename _ArrayType::value_type;
        using index_type = typename _ArrayType::value_type;

        constexpr presorted_backend() = default;
        constexpr presorted_backend(const _ArrayType& _Other)
            : m_vec{_Other}
        {}

        constexpr auto get_values() const noexcept { return m_vec.data(); }
        constexpr auto get_index()  const noexcept { return m_vec.data(); }
        constexpr size_t realsize() const noexcept { return m_vec.size(); }

    private:
        _ArrayType m_vec;
    };

    // This is 'referring' backend of fixed size, it points to a external data
    template<typename _IndexType, typename _ValueType>
    class portable_backend
    {
    public:
        constexpr portable_backend() = default;

        using index_type = _IndexType;
        using value_type = _ValueType;

        template<typename _ArrayType>
        constexpr portable_backend(const simple_backend<_ArrayType>& _Other)
            : m_values { _Other.get_values(), _Other.realsize() },
              m_index  { _Other.get_index(),  _Other.realsize() }
        {}

        constexpr auto get_values() const noexcept { return m_values.data(); }
        constexpr auto get_index()  const noexcept { return m_index.data(); }
        constexpr size_t realsize() const noexcept { return m_values.size(); }
    private:
        const_vector<value_type> m_values;
        const_vector<index_type> m_index;
    };


    template<typename _Traits, typename _Backend, typename _Duplicates = tag_DuplicatesNo>
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
        using traits_type     = _Traits;
        friend class const_set_map_base<_Traits, void, _Duplicates>;

        using hash_type       = typename _Traits::hash_type;
        using intermediate    = typename _Traits::key_type;
        using init_key_type   = typename _Traits::init_key_type;
        using init_type       = typename _Traits::init_type;
        using sorter_type     = TInsertSorter<_Traits, _Duplicates>;

        static constexpr bool can_index = sorter_type::can_index;

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
        constexpr size_type      capacity() const noexcept { return m_backend.capacity(); }
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
            constexpr bool operator()( const value_type& l, const value_type& r ) const
            {
                return key_compare{}(_Traits::get_key(l), _Traits::get_key(r));
            }
        };

        struct hash_compare
        {
            template<typename _TL, typename _TR>
            constexpr bool operator()(const _TL & l, const _TR & r ) const
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

        // debug method for unit tests
        const auto& get_backend() const noexcept { return m_backend; }

        static constexpr bool is_presorted = std::is_same_v<presorted_backend<const_vector<typename _Traits::value_type>>, backend_type>;

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

        template<typename _ArrayType>
        static constexpr auto make_backend(const _ArrayType& init)
        {
            auto proxy = sorter_type::sort(init);
            using backend_type = simple_backend<decltype(proxy)>;
            return backend_type{proxy};
        }

        template<typename _ArrayType>
        static constexpr auto presorted(const _ArrayType& init)
        {
            using backend_type = presorted_backend<_ArrayType>;
            return backend_type{init};
        }

        backend_type m_backend;
    };

}

namespace std
{
    template<typename...TArgs>
    constexpr size_t size(const compile_time_bits::const_set_map_base<TArgs...>& _container)
    {
        return _container.size();
    }
}

#include "ctsort_cxx14.hpp"

#include "ct_crc32.hpp"



#endif
