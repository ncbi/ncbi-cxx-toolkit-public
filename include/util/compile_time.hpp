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
        using traits = bitset_traits<_Ty, _Words>;

        constexpr const_bitset() = default;

        constexpr const_bitset(std::initializer_list<T> _init)
            :m_size{_init.size()},
            _Array(traits::set_bits(_init))
        {}

        template<size_t N>
        constexpr const_bitset(const char(&init)[N])
            : m_size{ traits::count_bits(init) },
              _Array(traits::set_bits(init))
        {}

        static constexpr const_bitset set_range(T _from, T _to)
        {//this uses private constructor
            return const_bitset(traits::set_range(_from, _to), _to - _from + 1);
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

        bool reset(size_t _Pos)
        {
            if (_Bits <= _Pos)
                _Xran();    // _Pos off end
            auto& val = _Array[_Pos / _Bitsperword];
            _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
            bool previous = (val & mask) != 0;
            if (previous) {
                m_size--;
                val ^= mask;
            }
            return previous;
        }
        bool set(size_t _Pos)
        {
            if (_Bits <= _Pos)
                _Xran();    // _Pos off end
            auto& val = _Array[_Pos / _Bitsperword];
            _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
            bool previous = (val & mask) != 0;
            if (!previous) {
                m_size++;
                val |= mask;
            }
            return previous;
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
        [[noreturn]] void _Xran() const
        {
            throw std::out_of_range("invalid const_bitset<_Bits,_T> position");
        }

    };

    template<typename _Key, typename _T, typename _Pair = std::pair<_Key, _T>>
    class const_unordered_map: public const_set_map_base<_Key, _Pair>
    {
    public:
        using _MyBase = const_set_map_base<_Key, _Pair>;

        using hashed_key      = typename _MyBase::hashed_key;
        using intermediate    = typename _MyBase::intermediate;

        using value_type      = typename _MyBase::value_type;
        using key_type        = typename value_type::first_type;
        using mapped_type     = typename value_type::second_type;
        using size_type       = typename _MyBase::size_type;
        using difference_type = typename _MyBase::difference_type;
        using reference       = typename _MyBase::reference;
        using const_reference = typename _MyBase::const_reference;
        using pointer         = typename _MyBase::pointer;
        using const_pointer   = typename _MyBase::const_pointer;
        using iterator        = typename _MyBase::iterator;
        using const_iterator  = typename _MyBase::const_iterator;
        using reverse_iterator = typename _MyBase::reverse_iterator;
        using const_reverse_iterator = typename _MyBase::const_reverse_iterator;

        using _MyBase::_MyBase;

        const_iterator find(intermediate _key) const
        {
            auto it = _MyBase::lower_bound(_key);
            if (it == _MyBase::end() || (it->first != _key))
                return _MyBase::end();
            else
                return it;
        }

        const mapped_type& at(intermediate _key) const
        {
            auto it = find(_key);
            if (it == _MyBase::end())
                throw std::out_of_range("invalid const_unordered_map<K, T> key");

            return it->second;
        }

        const mapped_type& operator[](intermediate _key) const
        {
            return at(_key);
        }

    protected:
    };

    template<typename _Key, typename _T>
    struct MakeConstMap
    {
        using type = const_unordered_map<_Key, _T>;

        using key_type = typename type::hashed_key;
        using mapped_type = typename type::hashed_value::second_type;

        using sorter_t = TInsertSorter<straight_sort_traits<key_type, mapped_type>, true>;
        using init_type = typename sorter_t::init_type;

        template<size_t N>
        constexpr auto operator()(const init_type (&input)[N]) const
        {
            return sorter_t{}(input);
        }
    };

    template<typename T1, typename T2>
    struct MakeConstMapTwoWay
    {
        using type = std::pair<
            const_unordered_map<T1, T2>,
            const_unordered_map<T2, T1>>;

        using first_type  = typename type::first_type::hashed_key;
        using second_type = typename type::second_type::hashed_key;

        using straight_sorter_t = TInsertSorter<straight_sort_traits<first_type, second_type>, true>;
        using flipped_sorter_t = TInsertSorter<flipped_sort_traits<first_type, second_type>, true>;
        using init_type = typename straight_sorter_t::init_type;

        template<size_t N>
        constexpr auto operator()(const init_type(&input)[N]) const
        {
            return std::make_pair(
                straight_sorter_t{}(input),
                flipped_sorter_t{}(input));
        }
    };

    template<typename _T>
    class const_unordered_set: public const_set_map_base<_T, _T>
    {
    public:
        using _MyBase = const_set_map_base<_T, _T>;

        using hashed_key     = typename _MyBase::hashed_key;
        using value_type     = typename _MyBase::value_type;
        using const_iterator = typename _MyBase::const_iterator;
        using iterator       = typename _MyBase::iterator;
        using key_type       = value_type;

        using _MyBase::_MyBase;

        const_iterator find(const key_type& _key) const
        {
            auto it = _MyBase::lower_bound(_key);
            if (it == _MyBase::end() || (*it != _key))
                return _MyBase::end();
            else
                return it;
        }
    };

    template<typename _T>
    struct MakeConstSet
    {
        using type = const_unordered_set<_T>;

        using sorter_t = TInsertSorter<simple_sort_traits<typename type::hashed_key>, true>;
        using init_type = typename sorter_t::init_type;

        template<size_t N>
        constexpr auto operator()(const init_type(&input)[N]) const 
        {
            return sorter_t{}(input);
        }
    };
}

// user can define some specific debug instructions
#ifndef DEBUG_MAKE_CONST_MAP
    #define DEBUG_MAKE_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_TWOWAY_CONST_MAP
    #define DEBUG_MAKE_TWOWAY_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_CONST_SET
    #define DEBUG_MAKE_CONST_SET(name)
#endif

// Some compilers (Clang < 3.9) still cannot deduce template parameter N for aggregate initialiazed arrays
// so we have to use two step initialization. This doesn't impact neither of compile time, run time or memory footprint
//

#define MAKE_CONST_CONTAINER(debug, name, maker, ...) \
    static debug maker ::init_type name ## _init[] = __VA_ARGS__;                                                        \
    static debug auto name ## _proxy = maker {}(name ## _init);                                                          \
    static debug maker ::type name = name ## _proxy;

#define MAKE_CONST_MAP(name, type1, type2, ...)                                                                              \
    using name ## _maker_type = ct::MakeConstMap<type1, type2>;                                                              \
    MAKE_CONST_CONTAINER(constexpr, name, name ## _maker_type, __VA_ARGS__)                                                  \
    DEBUG_MAKE_CONST_MAP(name)

#define MAKE_TWOWAY_CONST_MAP(name, type1, type2, ...)                                                                       \
    using name ## _maker_type = ct::MakeConstMapTwoWay<type1, type2>;                                                        \
    MAKE_CONST_CONTAINER(constexpr, name, name ## _maker_type, __VA_ARGS__)                                                  \
    DEBUG_MAKE_TWOWAY_CONST_MAP(name)

#define MAKE_CONST_SET(name, type, ...)                                                                                      \
    MAKE_CONST_CONTAINER(constexpr, name, ct::MakeConstSet<type>, __VA_ARGS__)                                               \
    DEBUG_MAKE_CONST_SET(name)

#endif

