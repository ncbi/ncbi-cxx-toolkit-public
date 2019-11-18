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
                  typename enforce_all_constructable<unsigned, _T, TArgs...>::type,
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

        constexpr size_t size() const { return m_size; }
        static constexpr size_t capacity() { return _Bits; }
        constexpr bool empty() const { return m_size == 0; }

        constexpr bool test(size_t _Pos) const
        {
            //if (_Bits <= _Pos)
            //    _Xran();    // _Pos off end
            return _Subscript(_Pos);
        }

        class const_iterator
        {
        public:
            using difference_type = size_t;
            using value_type = T;
            using pointer = const T*;
            using reference = const T&;
            using iterator_category = std::forward_iterator_tag;

            const_iterator() = default;
            friend class const_bitset;

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
            const_iterator(const const_bitset* _this, size_t index) : m_index{ index }, m_bitset{ _this }
            {
                while (m_index < m_bitset->capacity() && !m_bitset->test(m_index))
                {
                    ++m_index;
                }
            }
            size_t m_index;
            const const_bitset* m_bitset;
        };

        const_iterator begin() const {  return const_iterator(this, 0);  }
        const_iterator end() const {  return const_iterator(this, capacity()); }
        const_iterator cbegin() const { return const_iterator(this, 0); }
        const_iterator cend() const { return const_iterator(this, capacity()); }

        template<class _Ty, class _Alloc>
        operator std::vector<_Ty, _Alloc>() const
        {
            std::vector<_Ty, _Alloc> vec;
            vec.reserve(size());
            vec.assign(begin(), end());
            return vec;
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


    template<size_t N, typename _HashedKey, typename _Value>
    class const_unordered_map
    {
    public:
        static_assert(N > 0, "empty const_map not supported");
        using hash_type    = typename _HashedKey::hash_type;
        using intermediate = typename _HashedKey::intermediate;

        using key_type     = typename _HashedKey::type;
        using mapped_type = _Value;
        using value_type = const_pair<typename _HashedKey::type, typename _Value::type>;
        using size_type = size_t;
        using array_t = const_array<value_type, N>;

        using const_iterator = typename array_t::const_iterator;

        constexpr const_unordered_map(const array_t& init)
            : m_array(init)
        {}

        constexpr bool in_order() const
        {
            return CheckOrder(m_array);
        }

        constexpr const_iterator begin() const noexcept { return m_array.begin(); }
        constexpr const_iterator cbegin() const noexcept { return m_array.begin(); }
        constexpr size_t capacity() const noexcept { return N; }
        constexpr size_t size() const noexcept { return N; }
        constexpr size_t max_size() const noexcept { return N; }
        constexpr const_iterator end() const noexcept { return m_array.end(); }
        constexpr const_iterator cend() const noexcept { return m_array.end(); }
        constexpr bool empty() const noexcept { return N == 0; }

        // alias to decide whether _Key can be constructed from _Arg
        template<typename _K, typename _Arg>
        using if_available = typename std::enable_if<
            std::is_constructible<intermediate, _K>::value, _Arg>::type;

        template<typename K>
        if_available<K, const_iterator>
            find(K&& _key) const
        {
            intermediate temp = std::forward<K>(_key);
            key_type key(temp);
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
            intermediate temp = std::forward<K>(_key);
            key_type key(std::move(temp));
            return std::lower_bound(begin(), end(), std::move(key), Pred());
        }

        struct Pred
        {
            bool operator()(const value_type& l, const hash_type& r) const
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

    template<typename T1, typename T2, ncbi::NStr::ECase case_sensitive = ncbi::NStr::eCase, TwoWayMap two_way = TwoWayMap::no>
    struct MakeConstMap
    {
        using first_type = DeduceHashedType<case_sensitive, NeedHash::yes, T1>;
        using second_type = DeduceHashedType<case_sensitive, two_way==TwoWayMap::yes ? NeedHash::yes : NeedHash::no, T2>;
        using init_pair_t = const_pair<typename first_type::type, typename second_type::type>;

        template<size_t N>
        struct DeduceType
        {
            static constexpr size_t size = N;
            static_assert(size > 0, "empty const_map not supported");

            using type = const_unordered_map<size, first_type, second_type>;
        };

        template<size_t N>
        constexpr auto operator()(const const_array<init_pair_t, N> &input) const ->
            typename DeduceType<N>::type
        {
            return typename DeduceType<N>::type(input);
        }
    };

    template<size_t N, typename _HashedKey, typename _Value>
    class const_unordered_set
    {
    public:
        static_assert(N > 0, "empty const_set not supported");
        using hash_type = typename _HashedKey::hash_type;
        using intermediate = typename _HashedKey::intermediate;

        using key_type = typename _HashedKey::type;
        using value_type = typename _Value::type;
        using size_type = size_t;

        using array_t = const_array<value_type, N>;
        using const_iterator = typename array_t::const_iterator;

        constexpr const_unordered_set(const array_t& init)
            : m_array(init)
        {}

        constexpr bool in_order() const
        {
            return CheckOrder(m_array);
        }

        constexpr const_iterator begin() const noexcept { return m_array.begin(); }
        constexpr const_iterator cbegin() const noexcept { return m_array.begin(); }
        constexpr size_t capacity() const noexcept { return N; }
        constexpr size_t size() const noexcept { return N; }
        constexpr size_t max_size() const noexcept { return N; }
        constexpr const_iterator end() const noexcept { return m_array.end(); }
        constexpr const_iterator cend() const noexcept { return m_array.end(); }
        constexpr bool empty() const noexcept { return N == 0; }

        // alias to decide whether _Key can be constructed from _Arg
        template<typename _K, typename _Arg>
        using if_available = typename std::enable_if<
            std::is_constructible<intermediate, _K>::value, _Arg>::type;

        template<typename K>
        if_available<K, const_iterator>
            find(K&& _key) const
        {
            intermediate temp = std::forward<K>(_key);
            key_type key(temp);
            auto it = std::lower_bound(begin(), end(), std::move(key), Pred());
            if (it != end())
            {
                if (*it != temp)
                    it = end();
            }
            return it;
        }

    protected:
        struct Pred
        {
            bool operator() (const key_type& l, const key_type& r) const
            {
                return l < r;
            }
        };

        template<typename K>
        if_available<K, const_iterator>
            lower_bound(K&& _key) const
        {
            intermediate temp = std::forward<K>(_key);
            key_type key(std::move(temp));
            return std::lower_bound(begin(), end(), std::move(key), Pred());
        }

        array_t m_array = {};
    };
    template<typename _T, ncbi::NStr::ECase case_sensitive = ncbi::NStr::eCase>
    struct MakeConstSet
    {
        using value_type = DeduceHashedType<case_sensitive, NeedHash::yes, _T>;
        using init_t = typename value_type::type;

        template<size_t N>
        struct DeduceType
        {
            static constexpr size_t size = N;
            static_assert(size > 0, "empty const_set not supported");

            using type = const_unordered_set<size, value_type, value_type>;
        };

        template<size_t N>
        constexpr auto operator()(const init_t (&input)[N]) const -> typename DeduceType<N>::type
        {
            return SimpleReorder(input);
        }
    };
};

#define MAKE_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                              \
    static constexpr ct::MakeConstMap<type1, type2, case_sensitive, ct::TwoWayMap::no>::init_pair_t                          \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<type1, type2, case_sensitive, ct::TwoWayMap::no>{}                         \
        (ct::Reorder(name ## _init));                                                                                        \
    static_assert(name.in_order(), "const map " #name "is not in order");


#define MAKE_TWOWAY_CONST_MAP(name, case_sensitive, type1, type2, ...)                                                       \
    static constexpr ct::MakeConstMap<type1, type2, case_sensitive, ct::TwoWayMap::yes>::init_pair_t                         \
        name ## _init[]  __VA_ARGS__;                                                                                        \
    static constexpr auto name = ct::MakeConstMap<type1, type2, case_sensitive, ct::TwoWayMap::yes>{}                        \
        (ct::Reorder(name ## _init));                                                                                        \
    static constexpr auto name ## _flipped = ct::MakeConstMap<type2, type1, case_sensitive, ct::TwoWayMap::yes>{}            \
        (ct::FlipReorder(name ## _init));                                                                                    \
    static_assert(name.in_order(), "const map " #name "is not in order");                                                    \
    static_assert(name ## _flipped.in_order(), "const map " #name "_flipped is not in order");

#define MAKE_CONST_SET(name, case_sensitive, type, ...)                                                                      \
    static constexpr ct::MakeConstSet<type, case_sensitive>::init_t name ## _init[]= __VA_ARGS__;                            \
    static constexpr auto name = ct::MakeConstSet<type, case_sensitive>{}                                                    \
        (name ## _init);                                                                                                     \
    static_assert(name.in_order(), "const set " #name "is not in order");

#endif

