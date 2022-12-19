#ifndef __CT_BITSET_CXX14_HPP_INCLUDED__
#define __CT_BITSET_CXX14_HPP_INCLUDED__

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
 *      partially backported std::bitset using c++14 constexpr
 *
 *
 */

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
        template <size_t I, typename _Other>
        static constexpr _Ty assemble_mask(const std::initializer_list<_Other>& _init)
        {
            _Ty ret = 0;
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width - 1;
            for (_Other _rec : _init)
            {
                size_t rec = static_cast<size_t>(static_cast<u_type>(_rec));
                if (_min <= rec && rec <= _max)
                {
                    ret |= _Ty(1) << (rec % width);
                }
            }
            return ret;
        }

        static constexpr bool IsYes(char c) { return c == '1'; }
        static constexpr bool IsYes(bool c) { return c; }

        template <size_t I, size_t N, typename _Base>
        static constexpr _Ty assemble_mask(const const_array<_Base, N>& _init)
        {
            _Ty ret = 0;
            _Ty mask = 1;
            constexpr auto _min = I * width;
            constexpr auto _max = I * width + width;
            for (size_t pos = _min; pos < _max && pos < N; ++pos)
            {
                if (IsYes(_init[pos])) ret |= mask;
                mask = mask << 1;
            }
            return ret;
        }
        template <typename _Input, std::size_t... I>
        static constexpr array_t assemble_bitset(const _Input& _init, std::index_sequence<I...>)
        {
            return {assemble_mask<I>(_init)...};
        }

        template<typename _Other>
        static constexpr array_t set_bits(std::initializer_list<_Other> args)
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
        template<size_t N>
        static constexpr array_t set_bits(const const_array<bool, N>& in)
        {
            return assemble_bitset(in, std::make_index_sequence<_Size>{});
        }
    };

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
}


#endif
