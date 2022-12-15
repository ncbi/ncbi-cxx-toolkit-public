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
    template<size_t _Bits, class T>
    class const_bitset
    {
    public:
        using u_type = typename real_underlying_type<T>::type;

        using _Ty = uint64_t;

        static constexpr size_t _Bitsperword = 8 * sizeof(_Ty);
        static constexpr size_t _Words = (_Bits + _Bitsperword - 1) / _Bitsperword;
        using _Array_t = const_array<_Ty, _Words>;
        using traits = bitset_traits<_Ty, _Words, int>;

        constexpr const_bitset() = default;

        constexpr const_bitset(std::initializer_list<T> _init)
            :const_bitset(traits::set_bits(_init))
        {}

        constexpr const_bitset(T _init)
            :const_bitset(traits::set_bits({_init}))
        {}

        template<size_t N>
        explicit constexpr const_bitset(char const (&_init)[N])
            :const_bitset(make_array(_init))
        {}

        template<size_t N>
        explicit constexpr const_bitset(const const_array<char, N> &_init)
            :const_bitset(traits::set_bits(_init))
        {}

        struct range: std::pair<T, T>
        {
            using std::pair<T, T>::pair;
        };

        explicit constexpr const_bitset(const range& _range)
            :const_bitset(traits::set_range(_range.first, _range.second))
        {}

        static constexpr const_bitset set_range(T _from, T _to)
        {
            return const_bitset(range{ _from, _to });
        }

        static constexpr size_t capacity() { return _Bits; }
        constexpr size_t size() const
        { // See Hamming Weight: https://en.wikipedia.org/wiki/Hamming_weight
            size_t retval{0};
            for (auto value: _Array)
            {
                while (value) {
                    value &= value - 1;
                    ++retval;
                }
            }
            return retval;
        }
        constexpr bool empty() const
        {
            for (auto value: _Array)
            {
                if (value != 0)
                    return false;
            }
            return true;
        }

        constexpr bool test(T _Pos) const
        {
            return _Subscript(static_cast<size_t>(_Pos));
        }

        const_bitset& operator +=(T value)
        {
            set(value);
            return *this;
        }

        const_bitset& operator -=(T value)
        {
            reset(value);
            return *this;
        }

        const_bitset& operator +=(const const_bitset& _Other)
        {
            for (size_t i=0; i<_Array.size(); i++)
                _Array[i] |= _Other._Array[i];
            return *this;
        }

        const_bitset& operator -=(const const_bitset& _Other)
        {
            for (size_t i=0; i<_Array.size(); i++)
                _Array[i] &= (~_Other._Array[i]);
            return *this;
        }

        bool reset(T _v)
        {
            size_t _Pos = static_cast<size_t>(_v);
            if (_Bits <= _Pos)
                _Xran();    // _Pos off end
            auto& val = _Array[_Pos / _Bitsperword];
            _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
            bool previous = (val & mask) != 0;
            if (previous) {
                val ^= mask;
            }
            return previous;
        }
        bool set(T _v)
        {
            size_t _Pos = static_cast<size_t>(_v);
            if (_Bits <= _Pos)
                _Xran();    // _Pos off end
            auto& val = _Array[_Pos / _Bitsperword];
            _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
            bool previous = (val & mask) != 0;
            if (!previous) {
                val |= mask;
            }
            return previous;
        }

        constexpr bool ct_set(T _v)
        { // compile time version of set, doesn't throw an exception
            size_t _Pos = static_cast<size_t>(_v);
            if (_Bits <= _Pos)
                return false;    // _Pos off end
            auto& val = _Array[_Pos / _Bitsperword];
            _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
            bool previous = (val & mask) != 0;
            if (!previous) {
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

            constexpr const_iterator() = default;
            friend class const_bitset;

            constexpr bool operator==(const const_iterator& o) const
            {
                return m_bitset == o.m_bitset && m_index == o.m_index;
            }
            constexpr bool operator!=(const const_iterator& o) const
            {
                return m_bitset != o.m_bitset || m_index != o.m_index;
            }
            constexpr const_iterator& operator++()
            {
                while (m_index < m_bitset->capacity())
                {
                    ++m_index;
                    if (m_index == m_bitset->capacity() || m_bitset->x_test(m_index))
                        break;
                }
                return *this;
            }
            constexpr const_iterator operator++(int)
            {
                const_iterator _this(*this);
                operator++();
                return _this;
            }
            constexpr T operator*() const
            {
                return static_cast<T>(m_index);
            }
            constexpr T operator->() const
            {
                return static_cast<T>(m_index);
            }

        private:
            constexpr const_iterator(const const_bitset* _this, size_t index) : m_index{ index }, m_bitset{ _this }
            {
                while (m_index < m_bitset->capacity() && !m_bitset->x_test(m_index))
                {
                    ++m_index;
                }
            }
            size_t m_index{0};
            const const_bitset* m_bitset{nullptr};
        };

        using iterator = const_iterator;

        constexpr const_iterator begin() const  { return const_iterator(this, 0); }
        constexpr const_iterator end() const    { return const_iterator(this, capacity()); }
        constexpr const_iterator cbegin() const { return const_iterator(this, 0); }
        constexpr const_iterator cend() const   { return const_iterator(this, capacity()); }

        template<class _Ty, class _Alloc>
        operator std::vector<_Ty, _Alloc>() const
        {
            std::vector<_Ty, _Alloc> vec;
            vec.reserve(size());
            vec.assign(begin(), end());
            return vec;
        }

    private:
        explicit constexpr const_bitset(const _Array_t& args) : _Array(args) {}

        _Array_t _Array{};    // the set of bits

        constexpr bool x_test(size_t _Pos) const
        {
            return _Subscript(_Pos);
        }

        constexpr bool _Subscript(size_t _Pos) const
        {    // subscript nonmutable sequence
            return ((_Array[_Pos / _Bitsperword]
                & ((_Ty)1 << _Pos % _Bitsperword)) != 0);
        }
        [[noreturn]] void _Xran() const
        {
            throw std::out_of_range("invalid const_bitset<_Bits, T> position");
        }

    };

    template<typename _Key, typename _Ty, typename _Traits, typename _Multi=tag_DuplicatesNo, typename _Backend = void>
    class const_map_impl: public const_set_map_base<_Traits, _Backend, _Multi>
    { // ordered or unordered compile time map
    public:
        using _MyBase = const_set_map_base<_Traits, _Backend, _Multi>;
        using _MyType = const_map_impl<_Key, _Ty, _Traits, _Multi, _Backend>;

        using init_type       = typename _Traits::init_type;
        using intermediate    = typename _MyBase::intermediate;

        using value_type      = typename _MyBase::value_type;
        using key_type        = typename value_type::first_type;
        using mapped_type     = typename value_type::second_type;

        using _MyBase::_MyBase;

        const mapped_type& at(intermediate _key) const
        {
            auto it = _MyBase::find(_key);
            if (it == _MyBase::end())
                throw std::out_of_range("invalid const_map<K, T> key");

            return it->second;
        }

        const mapped_type& operator[](intermediate _key) const
        {
            return at(_key);
        }

        template<size_t N, typename _Enabled = std::enable_if<!_MyBase::is_presorted, init_type>>
        static constexpr auto construct(typename _Enabled::type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N, typename _Enabled = std::enable_if<!_MyBase::is_presorted, init_type>>
        static constexpr auto construct(const const_array<typename _Enabled::type, N>& init)
        {
            auto backend=_MyBase::make_backend(init);
            return const_map_impl<_Key, _Ty, _Traits, _Multi, decltype(backend)>{backend};
        }
        template<typename _Enabled = std::enable_if<_MyBase::is_presorted, _MyType>>
        static constexpr typename _Enabled::type construct(const const_vector<init_type>& init)
        {
            auto backend=_MyBase::presorted(init);
            return const_map_impl<_Key, _Ty, _Traits, _Multi, decltype(backend)>{backend};
        }
    protected:
    };

    template<typename _Key, typename _Ty>
    using const_map = const_map_impl<_Key, _Ty, straight_map_traits<_Key, _Ty>, tag_DuplicatesNo, void>;

    template<typename _Key, typename _Ty>
    using const_multi_map = const_map_impl<_Key, _Ty, straight_map_traits<_Key, _Ty>, tag_DuplicatesYes, void>;

    template<typename _Key, typename _Ty>
    using const_map_presorted = const_map_impl<_Key, _Ty,
        straight_map_traits<_Key, _Ty>,
        tag_DuplicatesNo,
        presorted_backend<const_vector<typename straight_map_traits<_Key, _Ty>::value_type>>>;

    template<typename _Ty, typename _Backend=void>
    class const_set: public const_set_map_base<simple_sort_traits<_Ty>, _Backend>
    {
    public:
        using _MyBase = const_set_map_base<simple_sort_traits<_Ty>, _Backend>;
        using _MyType = const_set<_Ty, _Backend>;

        using init_type      = typename _MyBase::init_type;

        using value_type      = typename _MyBase::value_type;
        using key_type        = value_type;

        using _MyBase::_MyBase;

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N>
        static constexpr auto construct(const const_array<init_type, N>& init)
        {
            auto backend=_MyBase::make_backend(init);
            return const_set<_Ty, decltype(backend)>{backend};
        }
    };

    template<typename T1, typename T2>
    struct const_map_twoway
    {
        using type1 = DeduceType<T1>;
        using type2 = DeduceType<T2>;

        using straight_traits = straight_map_traits<type1, type2>;
        using reverse_traits  = reverse_map_traits<type1, type2>;
        using init_type = typename straight_traits::init_type;

        using map_type1 = const_map_impl<T1, T2, straight_traits, tag_DuplicatesNo>;
        using map_type2 = const_map_impl<T2, T1, reverse_traits,  tag_DuplicatesNo>;

        template<size_t N>
        static constexpr auto construct(init_type const (&init)[N])
        {
            return construct(make_array(init));
        }
        template<size_t N>
        static constexpr auto construct(const const_array<init_type, N>& init)
        {
            return std::make_pair(
                map_type1::construct(init),
                map_type2::construct(init)
            );
        }
    };

    template<typename _Ty>
    using const_unordered_set = const_set<DeduceHashedType<_Ty>>;

    template<typename _Key, typename _Ty>
    using const_unordered_map = const_map<DeduceHashedType<_Key>, _Ty>;

    template<typename T1, typename T2>
    using const_unordered_map_twoway = const_map_twoway<DeduceHashedType<T1>, DeduceHashedType<T2>>;

    template<typename _Init, typename _Elem=array_elem_t<_Init>>
    constexpr auto sort(_Init&& init)
    {
        using sorter=TInsertSorter<simple_sort_traits<_Elem>, tag_DuplicatesYes>;
        return std::get<1>(sorter::sort(tag_SortByValues{}, std::forward<_Init>(init)));
    }

}

#ifdef const_array
#   undef const_array
#endif
#ifdef const_tuple
#   undef const_tuple
#endif

// user can define some specific debug instructions
#ifndef DEBUG_MAKE_CONST_MAP
#   define DEBUG_MAKE_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_TWOWAY_CONST_MAP
#   define DEBUG_MAKE_TWOWAY_CONST_MAP(name)
#endif
#ifndef DEBUG_MAKE_CONST_SET
#   define DEBUG_MAKE_CONST_SET(name)
#endif

// Some compilers (Clang < 3.9, GCC-7) still cannot deduce template parameter N for aggregate initialiazed arrays
// so we have to use two step initialization. This doesn't impact neither of compile time, run time or memory footprint
//

#define MAKE_CONST_MAP(name, type1, type2, ...)                                                                             \
    static constexpr auto name = ct::const_map<type1, type2>::construct(__VA_ARGS__);                                       \
    DEBUG_MAKE_CONST_MAP(name)

#define MAKE_TWOWAY_CONST_MAP(name, type1, type2, ...)                                                                      \
    static constexpr auto name = ct::const_map_twoway<type1, type2>::construct(__VA_ARGS__);                                \
    DEBUG_MAKE_TWOWAY_CONST_MAP(name)

#define MAKE_CONST_SET(name, type, ...)                                                                                     \
    static constexpr ct::const_set<type>::init_type name ## _init [] = __VA_ARGS__;                                         \
    static constexpr auto name = ct::const_set<type>::construct(name ## _init);                                             \
    DEBUG_MAKE_CONST_SET(name)

#define MAKE_CONST_SET1(name, type, ...)                                                                                    \
    static constexpr auto name = ct::const_set<type>::construct(__VA_ARGS__);                                               \
    DEBUG_MAKE_CONST_SET(name)

#ifdef CT_USE_STD_MAP
#    define MAKE_CONST_MAP_COMPAT(name, type1, type2, ...) static const std::map<type1, type2> name = __VA_ARGS__;
#else
#    define MAKE_CONST_MAP_COMPAT MAKE_CONST_MAP
#endif


#endif
