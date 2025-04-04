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
#include <corelib/impl/std_backport.hpp>


// forward declarations to avoid unnecessary includes
namespace ncbi
{
    class CTempString;
    class CTempStringEx;
}

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
        using value_type = std::tuple<typename DeduceType<_Types>::value_type...>;
        using init_type  = std::tuple<typename DeduceType<_Types>::init_type...>;
        using hash_type  = void;
    };

    // there is nothing to deduce if type has init_type and value_type members
    template<class _Self>
    struct DeduceType<_Self,
        std::enable_if_t<
            std::is_copy_constructible<typename _Self::init_type>::value &&
            std::is_copy_constructible<typename _Self::value_type>::value,
            _Self>> : _Self {};


    template<typename _Ty, typename = _Ty>
    struct DeduceHashedType: DeduceType<_Ty> {};

    template<size_t, typename...>
    struct DeduceIndexSizeImpl;

    template<size_t N>
    struct DeduceIndexSizeImpl<N>
    {
        using type = void;
    };

    template<size_t N, typename _First, typename...TArgs>
    struct DeduceIndexSizeImpl<N, _First, TArgs...>
    {
        static constexpr auto max_value = std::numeric_limits<_First>::max();

        using type = std::conditional_t<  (N<max_value), _First,
            typename DeduceIndexSizeImpl<N, TArgs...>::type>;
    };

    template<size_t size>
    struct DeduceIndexSize
    {
        using type = typename DeduceIndexSizeImpl<size, uint8_t, uint16_t, uint32_t, uint64_t>::type;

        static_assert(!std::is_void_v<type>, "Failed to determine data type");

    };



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

        constexpr simple_backend(const std::pair<_SizeType, _ArrayType>& init)
            : m_array { std::get<1>(init) },
              m_realsize { std::get<0>(init) }
        {}

        constexpr auto  realsize()   const noexcept { return m_realsize; }
        constexpr auto& get_values() const noexcept { return m_array; }
    private:
        _ArrayType  m_array;
        std::size_t m_realsize{0};
    };

    template<typename _SizeType, typename _ArrayType, typename _IndexType>
    class simple_backend<std::tuple<_SizeType, _ArrayType, _IndexType>>
    {
    public:
        using can_use_fast_index = std::true_type;
        using value_type = typename _ArrayType::value_type;
        using index_type = typename _IndexType::value_type;
        using tuple_type = std::tuple<_SizeType, _ArrayType, _IndexType>;

        //static constexpr auto capacity = array_size<_ArrayType>::value;

        constexpr simple_backend() = default;

        constexpr simple_backend(const tuple_type& init)
            : m_index { std::get<2>(init) },
              m_array { std::get<1>(init) },
              m_realsize { std::get<0>(init) }
        {}

        constexpr auto  realsize()   const noexcept { return m_realsize; }
        constexpr auto& get_values() const noexcept { return m_array; }
        constexpr auto& get_index()  const noexcept { return m_index; }
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

        constexpr presorted_backend() = default;
        constexpr presorted_backend(const _ArrayType& _Other)
            : m_vec{_Other}
        {}

        constexpr auto  realsize()   const noexcept { return m_vec.size(); }
        constexpr auto& get_values() const noexcept { return m_vec; }

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
        using can_use_fast_index = std::integral_constant<bool, !std::is_same_v<index_type, value_type>>;

        template<typename...TArgs>
        constexpr portable_backend(const simple_backend<std::tuple<TArgs...>>& _Other)
            : m_values { _Other.get_values().data(), _Other.realsize() },
              m_index  { _Other.get_index().data(),  _Other.realsize() }
        {}
        template<typename...TArgs>
        constexpr portable_backend(const simple_backend<std::pair<TArgs...>>& _Other)
            : m_values { _Other.get_values().data(), _Other.realsize() },
              m_index  { _Other.get_values().data(), _Other.realsize() }
        {}
        template<typename...TArgs>
        constexpr portable_backend(const presorted_backend<TArgs...>& _Other)
            : m_values { _Other.get_values() },
              m_index  { _Other.get_values() }
        {}

        constexpr auto  realsize()   const noexcept { return m_values.size(); }
        constexpr auto& get_values() const noexcept { return m_values; }
        constexpr auto& get_index()  const noexcept { return m_index; }

        constexpr auto begin() const { return m_values.begin(); }
        constexpr auto end()   const { return m_values.end(); }
        constexpr auto size()  const { return m_values.size(); }

    private:
        const_vector<value_type> m_values;
        const_vector<index_type> m_index;
    };

    template<typename _Backend, typename = void>
    struct can_use_fast_index: std::false_type {};

    template<typename _Backend>
    struct can_use_fast_index<_Backend, std::enable_if_t<_Backend::can_use_fast_index::value>>: std::true_type {};

    template<typename _IndexTraits, typename _Iterator>
    class binary_search_traits
    {
    public:
        using const_iterator = _Iterator;
        using iterator = const_iterator;

        //static_assert(std::input_iterator<const_iterator>);
        //static_assert(std::forward_iterator<const_iterator>);
        //static_assert(std::bidirectional_iterator<const_iterator>);
        //static_assert(std::random_access_iterator<const_iterator>);
        #ifdef __cpp_concepts
        static_assert(std::contiguous_iterator<const_iterator>);

        //static_assert(std::input_or_output_iterator<const_iterator>);
        static_assert(std::indirectly_readable<const_iterator>);
        #endif

        using key_compare  = typename _IndexTraits::hashed_key_type::value_compare;

        struct value_compare
        {
            constexpr bool operator()( const typename _IndexTraits::value_type& l, const typename _IndexTraits::value_type& r ) const
            {
                return key_compare{}(_IndexTraits::get_key(l), _IndexTraits::get_key(r));
            }
        };

        using intermediate = typename _IndexTraits::key_type;

        template<typename _BE, bool _CanIndex = can_use_fast_index<_BE>::value>
        static const_iterator lower_bound(const _BE& be, intermediate _key)
        {
            init_key_type key{_key};
            SearchKey hash{key};
            if constexpr (_CanIndex) {
                auto _begin = be.get_index().begin();
                auto it = std::lower_bound(_begin, _begin + be.size(), hash, hash_compare_op{});
                auto offset = std::distance(_begin, it);
                return be.begin() + offset;
            } else {
                return std::lower_bound(be.begin(), be.end(), hash, hash_compare_op{});
            }
        }

        template<typename _BE, bool _CanIndex = can_use_fast_index<_BE>::value>
        static const_iterator upper_bound(const _BE& be, intermediate _key)
        {
            init_key_type key{_key};
            SearchKey hash{key};
            if constexpr (_CanIndex) {
                auto _begin = be.get_index().begin();
                auto it = std::upper_bound(_begin, _begin + be.size(), hash, hash_compare_op{});
                auto offset = std::distance(_begin, it);
                return be.begin() + offset;
            } else {
                return std::upper_bound(be.begin(), be.end(), hash, hash_compare_op{});
            }
        }

        template<typename _BE, bool _CanIndex = can_use_fast_index<_BE>::value>
        static std::pair<const_iterator, const_iterator>
            equal_range(const _BE& be, intermediate _key)
        {
            init_key_type key{_key};
            SearchKey hash{key};
            if constexpr (_CanIndex) {
                auto _begin = be.get_index().begin();
                auto _range = std::equal_range(_begin, _begin + be.size(), hash, hash_compare_op{});
                return std::make_pair(
                    be.begin() + std::distance(_begin, _range.first),
                    be.begin() + std::distance(_begin, _range.second));
            } else {
                return std::equal_range(be.begin(), be.end(), hash, hash_compare_op{});
            }
        }

        template<typename _BE, bool _CanIndex = can_use_fast_index<_BE>::value>
        static const_iterator find(const _BE& be, intermediate _key)
        {
            init_key_type key{_key};
            SearchKey hash{key};
            if constexpr (_CanIndex) {
                auto _begin = be.get_index().begin();
                auto it = std::lower_bound(_begin, _begin + be.size(), hash, hash_compare_op{});
                auto dist = std::distance(_begin, it);
                auto ret_it = be.begin() + dist;

                if (( dist >= std::ptrdiff_t(be.size()))
                        || key_compare{}(_key, _IndexTraits::get_key(*ret_it))) {
                    ret_it = be.end();
                }
                return ret_it;
            } else {
                auto it = std::lower_bound(be.begin(), be.end(), hash, hash_compare_op{});
                if (it == be.end() || key_compare{}(_key, _IndexTraits::get_key(*it))) {
                    return be.end();
                } else {
                        return it;
                }
            }
        }


    private:
        using init_key_type = typename _IndexTraits::init_key_type;

        struct SearchKey
        {
            typename _IndexTraits::hash_type hash;
            typename _IndexTraits::hashed_key_type::hash_compare m_hash_compare = {};
        };

        struct hash_compare_op
        {
            template<typename _TL>
            bool operator()(const _TL & l, const SearchKey& r ) const
            {
                return r.m_hash_compare(_IndexTraits::get_key(l), r.hash);
            }
            template<typename _TR>
            bool operator()(const SearchKey& l, const _TR & r ) const
            {
                return l.m_hash_compare(l.hash, _IndexTraits::get_key(r));
            }
        };
    };
    template<typename _Traits, typename _Duplicates, typename _Backend>
    struct const_set_map_backend
    {

        using sorter_type  = TInsertSorter<_Traits, _Duplicates>;
        using is_presorted = std::is_same<_Backend, presorted_backend<const_vector<typename _Traits::value_type>>>;

        using index_type   = std::conditional_t<
                                    sorter_type::can_index && !is_presorted::value,
                                    typename _Traits::hash_type,
                                    typename _Traits::value_type>;

        using portable = portable_backend<index_type, typename _Traits::value_type>;
        using backend_type = std::conditional_t<std::is_void<_Backend>::value,
                                    portable,
                                    _Backend>;

        template<typename _ArrayType>
        static constexpr auto construct(const _ArrayType& init)
        {
            auto proxy = sorter_type::sort(init);
            return simple_backend<decltype(proxy)>(proxy);
        }

        constexpr
            std::conditional_t<std::is_void_v<_Backend>, const backend_type&, portable>
                get_portable() const
        {
            return m_backend;
        }

        constexpr const_set_map_backend() =  default;
        constexpr const_set_map_backend(const backend_type& _o) : m_backend{_o} {}

        template<typename _Other,
            typename = std::enable_if_t<
                std::is_constructible_v<backend_type, typename const_set_map_backend<_Traits, _Duplicates, _Other>::backend_type >>>
        constexpr const_set_map_backend(const const_set_map_backend<_Traits, _Duplicates, _Other>& _other)
            : m_backend {_other.m_backend}
        {
        }

        backend_type m_backend;
    };

    struct const_set_map_base_base
    {
    };

    template<
        template<typename...> typename _RealType,
        typename _Traits, typename _Duplicates = tag_DuplicatesNo, typename _Backend = void>
    class const_set_map_base: public const_set_map_base_base
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

        friend class const_set_map_base<_RealType, _Traits, _Duplicates, void>;

        template<typename _Other>
        using recast_backend = _RealType<_Traits, _Duplicates, _Other>;

        using init_type    = typename _Traits::init_type;
        using backend_type = const_set_map_backend<_Traits, _Duplicates, _Backend>;
        using search_core  = binary_search_traits<_Traits, const_iterator>;
        using intermediate = typename search_core::intermediate;

        // standard methods

        constexpr size_type      size()     const noexcept { return m_backend.m_backend.realsize(); }
        constexpr size_type      max_size() const noexcept { return size(); }
        constexpr size_type      capacity() const noexcept { return m_backend.m_backend.capacity(); }
        constexpr bool           empty()    const noexcept { return size() == 0; }
        constexpr const_iterator begin()    const noexcept { return m_backend.m_backend.get_values().data(); }
        constexpr const_iterator cbegin()   const noexcept { return begin(); }
        constexpr const_iterator end()      const noexcept { return begin() + size(); }
        constexpr const_iterator cend()     const noexcept { return end(); }
        constexpr const_reverse_iterator rbegin()  const noexcept { return const_reverse_iterator{ end() }; }
        constexpr const_reverse_iterator rcbegin() const noexcept { return rbegin(); }
        constexpr const_reverse_iterator rend()    const noexcept { return const_reverse_iterator{ begin() }; }
        constexpr const_reverse_iterator rcend()   const noexcept { return rend(); }

        using key_compare   = typename search_core::key_compare;
        using value_compare = typename search_core::value_compare;

        const_iterator lower_bound(intermediate key) const
        {
            return search_core::lower_bound(m_backend.get_portable(), key);
        }
        const_iterator upper_bound(intermediate key) const
        {
            return search_core::upper_bound(m_backend.get_portable(), key);
        }
        std::pair<const_iterator, const_iterator>
            equal_range(intermediate key) const
        {
            return search_core::equal_range(m_backend.get_portable(), key);
        }
        const_iterator find(intermediate key) const
        {
            return search_core::find(m_backend.get_portable(), key);
        }

        constexpr const_set_map_base() = default;

        constexpr const_set_map_base(const typename backend_type::backend_type& backend)
            : m_backend{ backend }
        {
        }

        template<
            typename _Container,
            typename = std::enable_if_t<
                std::is_void_v<_Backend> &&
                std::is_constructible_v<backend_type, typename _Container::backend_type> &&
                std::is_base_of<const_set_map_base_base, _Container>::value
            >>
        constexpr const_set_map_base(const _Container& _other)
            : m_backend{ _other.m_backend.m_backend }
        {
        }

        // non-standard methods

        size_t get_alignment() const
        { // debug method for unit tests
            return std::uintptr_t(m_backend.get_index()) % std::hardware_constructive_interference_size;
        }

        constexpr auto& get_backend() const noexcept { return m_backend; }

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return from_array(std::to_array(init));
        }

        template<size_t N>
        static constexpr auto construct(const std::array<init_type, N>& init)
        {
            return from_array(init);
        }

        static constexpr auto presorted(const const_vector<init_type>& init)
        {
            presorted_backend be{init};
            return recast_backend<decltype(be)>{be};
        }

    protected:
        template<size_t N>
        static constexpr auto from_array(const std::array<init_type, N>& init)
        {
            auto be = backend_type::construct(init);
            return recast_backend<decltype(be)>{be};
        }

    private:
        backend_type m_backend;
    };

}

// bits for compile time table with multiple indices for searching
namespace compile_time_bits
{

    template<typename _ValueType, typename _IndexType>
    class indexed_iterator
    {
    public:
        // standard mandadory definitions
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::decay_t<_ValueType>;
        using pointer           = const value_type*;
        using reference         = const value_type&;
        using const_pointer     = const value_type*;
        using const_reference   = const value_type&;

    #ifdef __cpp_concepts
        using iterator_category = std::contiguous_iterator_tag;
    #else
        using iterator_category = std::random_access_iterator_tag;
    #endif

        constexpr indexed_iterator() = default;
        constexpr explicit indexed_iterator(const _ValueType* be, const _IndexType* index)
            : m_pos { index },
              m_values { be }
        {}

        const_reference operator*()  const noexcept { return x_get_row();     }
        const_pointer   operator->() const noexcept { return & (x_get_row()); }

        constexpr bool operator==(const indexed_iterator& other) const
        {
            return (m_pos == other.m_pos) && (m_values == other.m_values);
        }
        constexpr bool operator!=(const indexed_iterator& other) const
        {
            return !((m_pos == other.m_pos) && (m_values == other.m_values));
        }
        indexed_iterator& operator++()
        {
            ++m_pos;
            return *this;
        }
        indexed_iterator operator++(int)
        {
            indexed_iterator _this(*this);
            operator++();
            return _this;
        }
        indexed_iterator& operator--()
        {
            --m_pos;
            return *this;
        }
        indexed_iterator operator--(int)
        {
            indexed_iterator _this(*this);
            operator--();
            return _this;
        }
        indexed_iterator& operator-=(difference_type off)
        {
            m_pos -= off;
            return *this;
        }
        indexed_iterator& operator+=(difference_type off)
        {
            m_pos += off;
            return *this;
        }
        indexed_iterator operator+(difference_type off) const {
            indexed_iterator _this{*this};
            _this += off;
            return _this;
        }
        indexed_iterator operator-(difference_type off) const {
            indexed_iterator _this{*this};
            _this -= off;
            return _this;
        }
        difference_type operator-(indexed_iterator other) const
        {
            return m_pos - other.m_pos;
        }

        bool operator<(indexed_iterator other) const
        {
            return (m_values == other.m_values) && (m_pos < other.m_pos);
        }
        bool operator>(indexed_iterator other) const
        {
            return (m_values == other.m_values) && (m_pos > other.m_pos);
        }
        bool operator<=(indexed_iterator other) const
        {
            return (m_values == other.m_values) && (m_pos <= other.m_pos);
        }
        bool operator>=(indexed_iterator other) const
        {
            return (m_values == other.m_values) && (m_pos >= other.m_pos);
        }
        const_reference operator[](difference_type off) const {
            auto it = indexed_iterator(m_pos + off, m_values);
            return *it;
        }

    private:
        auto& x_get_row() const {
            auto pos = *m_pos;
            return m_values[pos];
        }
        const std::decay_t<_IndexType>* m_pos = nullptr;
        const value_type* m_values = nullptr;
    };

    template<typename...TArgs>
    auto operator+(std::ptrdiff_t off, const indexed_iterator<TArgs...>& other)
    {
        return other + off;
    }

}

namespace std
{
    template<typename _Container, typename = std::enable_if_t<
        std::is_base_of<compile_time_bits::const_set_map_base_base, std::decay_t<_Container>>::value>>
    constexpr size_t size(_Container&& _container)
    {
        return _container.size();
    }
}

#include "ctsort_cxx14.hpp"

#include "ct_crc32.hpp"



#endif
