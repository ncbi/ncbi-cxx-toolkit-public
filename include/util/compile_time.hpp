#ifndef __COMPILE_TIME_HPP_INCLUDED__
#define __COMPILE_TIME_HPP_INCLUDED__

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
 *  const_map   -- constexpr equivalent of std::map
 *
 *
 */

#include "impl/compile_time_bits.hpp"

namespace ct
{
    using namespace compile_time_bits;

    // partially backported std::bitset until it's constexpr version becomes available
    template<size_t _Bits, class _T>
    class const_bitset
    {
    public:
        using _Ty = uint64_t;
        using T = _T;
        static constexpr size_t _Bitsperword = 8 * sizeof(_Ty);
        static constexpr size_t _Words = (_Bits + _Bitsperword - 1) / _Bitsperword;
        using _Array_t = const_array<_Ty, _Words>;

        constexpr const_bitset() = default;

        template<typename...TArgs>
        constexpr const_bitset(_T first, TArgs...args)
            : m_size(1+sizeof...(args)), 
              _Array(bitset_traits<
                  _Words, 
                  typename enforce_same<unsigned, _T, TArgs...>::type,
                  _Ty,
                  1+sizeof...(args)>::set_bits(first, args...))
        {}

        template<size_t N>
        constexpr const_bitset(const char(&s)[N])
            :m_size(N-1),
            _Array(bitset_traits<_Words, unsigned, _Ty, N-1>::set_bits(s))
        {}
        static constexpr const_bitset set_range(T _from, T _to)
        {//this uses private constructor
            return const_bitset(bitset_traits<_Words, _T, _Ty>::set_range(_from, _to), _to - _from + 1);
        }

        constexpr size_t size() const
        {
            return m_size;
        }
        static constexpr size_t capacity()
        {
            return _Bits;
        }
        constexpr bool empty() const
        {
            return m_size == 0;
        }
        constexpr bool test(size_t _Pos) const
        {
            //if (_Bits <= _Pos)
            //    _Xran();    // _Pos off end
            return _Subscript(_Pos);
        }

        class const_iterator
        {
        public:
            const_iterator() = default;
            const_iterator(const const_bitset* _this, size_t index) : m_index{ index }, m_bitset{ _this }
            {
                while (m_index < m_bitset->capacity() && !m_bitset->test(m_index))
                {
                    ++m_index;
                }
            }
            bool operator==(const const_iterator& o) const
            {
                return m_bitset == o.m_bitset && m_index == o.m_index;
            }
            bool operator!=(const const_iterator& o) const
            {
                return m_bitset != o.m_bitset || m_index != o.m_index;
            }
            const_iterator& operator++()
            {
                while (m_index < m_bitset->capacity())
                {
                    ++m_index;
                    if (m_bitset->test(m_index))
                        break;
                }
                return *this;
            }
            const_iterator operator++(int)
            {
                const_iterator _this(*this);
                operator++();
                return _this;
            }
            T operator*() const
            {
                return static_cast<T>(m_index);
            }
            T operator->() const
            {
                return static_cast<T>(m_index);
            }

        private:
            size_t m_index;
            const const_bitset* m_bitset;
        };

        const_iterator begin() const
        {
            return const_iterator(this, 0);
        }

        const_iterator end() const
        {
            return const_iterator(this, capacity());
        }

    private:
        explicit constexpr const_bitset(const _Array_t& args, size_t _size) : m_size(_size), _Array(args)
        {
        }

        size_t m_size{ 0 };
        _Array_t _Array{};    // the set of bits

        constexpr bool _Subscript(size_t _Pos) const
        {    // subscript nonmutable sequence
            return ((_Array[_Pos / _Bitsperword]
                & ((_Ty)1 << _Pos % _Bitsperword)) != 0);
        }
    };


    template<typename _T>
    class const_xlate_table
    {
    public:
        using charset = const_bitset<256, char>;
        using traits = xlate_traits<charset, _T>;
        using init_pair_t = typename traits::init_pair_t;
        using array_t = typename traits::array_t;

        //constexpr const_xlate_table() = default;

        template<size_t N>
        constexpr const_xlate_table(_T _default, const init_pair_t(&args)[N])
            : m_data(traits{}(_default, args))
        {
        }
        _T operator[](char _v) const
        {
            return m_data[_v];
        }
    private:
        array_t m_data;
    };

    template<size_t N, typename _Key, typename _Value>
    class const_map
    {
    public:
        static_assert(N > 0, "empty const_map not supported");
        using key_type = _Key;
        using mapped_type = _Value;
        using value_type = const_pair<_Key, _Value>;
        using size_type = size_t;
        using array_t = const_array<value_type, N>;

        using const_iterator = typename array_t::const_iterator;

        using TRecastKey = recast<key_type>;

        constexpr const_map(const array_t& init)
            : m_array(init)
        {}

        constexpr bool in_order() const
        {
            return CheckOrder(m_array);
        }
        constexpr const_iterator begin() const noexcept
        {
            return m_array.begin();
        }
        constexpr const_iterator cbegin() const noexcept
        {
            return m_array.begin();
        }
        constexpr size_t capacity() const noexcept
        {
            return N;
        }
        constexpr size_t size() const noexcept
        {
            return N;
        }
        constexpr size_t max_size() const noexcept
        {
            return N;
        }
        constexpr const_iterator end() const noexcept
        {
            return m_array.end();
        }
        constexpr const_iterator cend() const noexcept
        {
            return m_array.end();
        }
        constexpr bool empty() const noexcept
        {
            return N == 0;
        }

        // alias to decide whether _Key can be constructed from _Arg
        template<typename _K, typename _Arg>
        using if_available = typename std::enable_if<
            std::is_constructible<typename TRecastKey::intermediate, _K>::value, _Arg>::type;

        template<typename K>
        if_available<K, const_iterator>
            find(K&& _key) const
        {
            typename TRecastKey::intermediate temp = std::forward<K>(_key);
            typename TRecastKey::type key(temp);
            auto it = std::lower_bound(begin(), end(), std::move(key), Pred());
            if (it != end())
            {
                if (it->first != temp)
                    it = end();
            }
            return it;
        }

        template<typename K>
        if_available<K, const mapped_type&>
            at(K&& _key) const
        {
            auto it = find(std::forward<K>(_key));
            if (it == end())
                throw std::out_of_range("invalid const_map<K, T> key");

            return it->second;
        }
        template<typename K>
        if_available<K, const mapped_type&>
            operator[](K&& _key) const
        {
            return at(std::forward<K>(_key));
        }

    protected:
        template<typename K>
        if_available<K, const_iterator>
            lower_bound(K&& _key) const
        {
            typename TRecastKey::intermediate temp = std::forward<K>(_key);
            typename TRecastKey::type key(std::move(temp));
            return std::lower_bound(begin(), end(), std::move(key), Pred());
        }

        struct Pred
        {
            bool operator()(const value_type& l, const typename TRecastKey::type& r) const
            {
                return l.first < r;
            }
            bool operator() (const key_type& l, const key_type& r) const
            {
                return l < r;
            }
        };

        array_t m_array = {};
    };


    template<bool case_sensitive, bool two_way, typename T1, typename T2>
    struct MakeConstMap
    {
        template<bool need_hash, typename T, typename...Unused>
        struct DeduceType
        {
            using type = T;
        };

        template<typename...Unused>
        struct DeduceType<true, const char*, Unused...>
        {
            using type = CHashString<case_sensitive>;
        };

        template<typename...Unused>
        struct DeduceType<false, const char*, Unused...>
        {
            using type = string_view;
        };

        template<bool need_hash, class C, class T, class A>
        struct DeduceType<need_hash, std::basic_string<C, T, A>>
        {
            using type = typename DeduceType<need_hash, const char*>::type;
        };

        using first_type = typename DeduceType<true, T1>::type;
        using second_type = typename DeduceType<two_way, T2>::type;
        using init_pair_t = const_pair<first_type, second_type>;

        template<size_t N>
        struct DeduceMapType
        {
            static constexpr size_t size = N;
            static_assert(size > 0, "empty const_map not supported");

            using map_type = ct::const_map<size, first_type, second_type>;
        };

        template<size_t N>
        constexpr auto operator()(const init_pair_t(&input)[N]) const ->
            typename DeduceMapType<N>::map_type
        {
            return typename DeduceMapType<N>::map_type(input);
        }
        template<size_t N>
        constexpr auto operator()(const const_array<init_pair_t, N> &input) const ->
            typename DeduceMapType<N>::map_type
        {
            return typename DeduceMapType<N>::map_type(input);
        }
    };

};

#define MAKE_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                              \
    static constexpr ct::MakeConstMap<case_sensitive, false, type1, type2>::init_pair_t                                      \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<case_sensitive, false, type1, type2>{}                                     \
        (ct::Reorder(name ## _init));


#define MAKE_TWOWAY_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                       \
    static constexpr ct::MakeConstMap<case_sensitive, true, type1, type2>::init_pair_t                                       \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<case_sensitive, true, type1, type2>{}                                      \
        (ct::Reorder(name ## _init));                                                                                        \
    static constexpr auto name ## _flipped = ct::MakeConstMap<case_sensitive, true, type2, type1>{}                          \
        (ct::FlipReorder(name ## _init));

/*static_assert(name.in_order(), "const map " #name "is not in order");*/


#endif

