#ifndef __CT_BITSET_CXX17_HPP_INCLUDED__
#define __CT_BITSET_CXX17_HPP_INCLUDED__

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
 *      partially backported std::bitset using c++17 constexpr
 *
 *
 */

#include <cstdint>
#include <cstddef>

namespace compile_time_bits
{
    // this helper packs set of bits into an array usefull for initialisation of bitset
    // using C++17

    template<class _Ty, size_t _Size, class u_type>
    struct bitset_traits
    {
        using array_t = std::array<_Ty, _Size>;

        static constexpr size_t width = 8 * sizeof(_Ty);
        static constexpr size_t max_width = 8 * sizeof(_Ty) * _Size;

        static constexpr bool IsYes(char c) { return c == '1'; }
        static constexpr bool IsYes(bool c) { return c; }

        template<typename _Other>
        static constexpr void ct_set(array_t& arr, _Other _v)
        { // compile time version of set, doesn't throw an exception
            size_t _Pos = static_cast<size_t>(_v);
            if (_Pos < max_width) {
                // _Pos off end
                auto& val = arr[_Pos / width];
                _Ty mask = _Ty(1) << (_Pos % width);
                val |= mask;
            }
        }

        template<typename _Container>
        static constexpr array_t set_bits(const _Container& args)
        {
            array_t arr{};
            for (auto value: args) {
                ct_set(arr, value);
            }
            return arr;
        }
        template<typename _Iterator>
        static constexpr array_t set_bits(_Iterator begin, _Iterator end)
        {
            array_t arr{};
            for (auto it = begin; it != end; ++it) {
                if (IsYes(*it)) {
                    auto offset = std::distance(begin, it);
                    ct_set(arr, offset);
                }
            }
            return arr;
        }

        template<size_t N>
        static constexpr array_t from_string(const std::array<char, N>& in)
        {
            return set_bits(in.begin(), in.end());
        }

        template<size_t N>
        static constexpr array_t from_bool(const std::array<bool, N>& in)
        {
            return set_bits(in.begin(), in.end());
        }

        template<typename T>
        static constexpr array_t from_range(T from, T to)
        {
            array_t arr{};

            auto _from = to_real_underlying(from);
            auto _to = to_real_underlying(to);

            for (auto it = _from; it<= _to; ++it)
            {
                ct_set(arr, it);
            }
            return arr;
        }

    };

    // C++20 has constexpr std::popcount
    // See Hamming Weight: https://en.wikipedia.org/wiki/Hamming_weight
    constexpr int popcount64c(uint64_t x)
    {
        //This uses fewer arithmetic operations than any other known
        //implementation on machines with fast multiplication.
        //This algorithm uses 12 arithmetic operations, one of which is a multiply.

        //types and constants used in the functions below
        //uint64_t is an unsigned 64-bit integer variable type (defined in C99 version of C language)
        constexpr uint64_t m1  = 0x5555555555555555; //binary: 0101...
        constexpr uint64_t m2  = 0x3333333333333333; //binary: 00110011..
        constexpr uint64_t m4  = 0x0f0f0f0f0f0f0f0f; //binary:  4 zeros,  4 ones ...
        //constexpr uint64_t m8  = 0x00ff00ff00ff00ff; //binary:  8 zeros,  8 ones ...
        //constexpr uint64_t m16 = 0x0000ffff0000ffff; //binary: 16 zeros, 16 ones ...
        //constexpr uint64_t m32 = 0x00000000ffffffff; //binary: 32 zeros, 32 ones
        constexpr uint64_t h01 = 0x0101010101010101; //the sum of 256 to the power of 0,1,2,3...

        x -= (x >> 1) & m1;             //put count of each 2 bits into those 2 bits
        x = (x & m2) + ((x >> 2) & m2); //put count of each 4 bits into those 4 bits
        x = (x + (x >> 4)) & m4;        //put count of each 8 bits into those 8 bits
        return int((x * h01) >> 56);  //returns left 8 bits of x + (x<<8) + (x<<16) + (x<<24) + ...
    }

    constexpr int popcount64d(uint64_t x)
    {
        int retval = 0;
        while (x) {
            x &= x - 1;
            ++retval;
        }
        return retval;
    }

    constexpr size_t find_first_bit(uint64_t x)
    {   // returns 64 for all zeros 'x'
        //if (current == 0) return 0;

        // for any number in the form   bXXXX100000
        // only_low_bits will look like b0000111111
        // so, counting those one's would give a product
        auto only_low_bits = x ^ (x - 1);
        auto count = popcount64c(only_low_bits);
        return count;
    }

    // compile time initialized bitset with iterators
    template<size_t _MaxBits, class T>
    class const_bitset
    {
    public:
        using u_type = typename real_underlying_type<T>::type;

        using _Ty = uint64_t;

        static constexpr size_t _Bits = _MaxBits;
        static constexpr size_t _Bitsperword = 8 * sizeof(_Ty);
        static constexpr size_t _Words = (_Bits + _Bitsperword - 1) / _Bitsperword;
        using _Array_t = std::array<_Ty, _Words>;
        using traits = bitset_traits<_Ty, _Words, T>;

        constexpr const_bitset() = default;

        template<size_t, typename>
        friend class const_bitset;

        constexpr const_bitset(std::initializer_list<T> _init)
            :const_bitset(traits::set_bits(_init))
        {}

        template<typename...TArgs>
        constexpr const_bitset(T _first, TArgs&&... _extra)
            :const_bitset{{_first, _extra...}}
        {}

        template<size_t N>
        constexpr const_bitset(const std::array<T, N>& _init)
            :const_bitset(traits::set_bits(_init))
        {}

        template<size_t N>
        static constexpr auto from_string(char const (&_init)[N])
        {
            return const_bitset(traits::from_string(std::to_array( _init)));
        }

        template<size_t N>
        constexpr const_bitset(const std::array<bool, N> &_init)
            :const_bitset(traits::from_bool(_init))
        {}

        static constexpr const_bitset set_range(T _from, T _to)
        {
            return const_bitset(traits::from_range( _from, _to ));
        }

        static constexpr size_t capacity() { return _Bits; }
        constexpr size_t size() const
        {
            size_t retval{0};
            for (auto value: _Array)
            {
                retval += popcount64c(value);
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

        constexpr const_bitset& operator +=(const const_bitset& _Other)
        {
            for (size_t i=0; i<_Array.size(); i++)
                _Array[i] |= _Other._Array[i];
            return *this;
        }

        constexpr const_bitset& operator -=(const const_bitset& _Other)
        {
            for (size_t i=0; i<_Array.size(); i++)
                _Array[i] &= (~_Other._Array[i]);
            return *this;
        }

        template<size_t _MaxBits2>
        constexpr
        auto operator+ (const const_bitset<_MaxBits2, T>& _Other) const
        {
            constexpr size_t new_N = std::max(_MaxBits, _MaxBits2);
            using new_type = const_bitset<new_N, T>;

            typename new_type::_Array_t arr{};
            for (size_t i=0; i<arr.size(); i++) {
                auto l = i < _Array.size() ? _Array[i] : 0;
                auto r = i < _Other._Array.size() ? _Other._Array[i] : 0;
                arr[i] = l | r;
            }
            return new_type{arr};
        }

        template<size_t _MaxBits2>
        constexpr
        auto operator- (const const_bitset<_MaxBits2, T>& _Other) const
        {
            constexpr size_t new_N = std::max(_MaxBits, _MaxBits2);
            using new_type = const_bitset<new_N, T>;

            typename new_type::_Array_t arr{};
            for (size_t i=0; i<arr.size(); i++) {
                auto l = i < _Array.size() ? _Array[i] : 0;
                auto r = i < _Other._Array.size() ? _Other._Array[i] : 0;
                arr[i] = l & (~ r);
            }
            return new_type{arr};
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

        constexpr bool ct_test(T _Pos) const
        {
            return _Subscript(static_cast<size_t>(_Pos));
        }

        class const_iterator
        {
        public:
            using difference_type = std::ptrdiff_t;
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
                if (m_index < _Bits) {
                    x_find_next_bit();
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
            constexpr void x_find_next_bit()
            {
                // See Hamming Weight: https://en.wikipedia.org/wiki/Hamming_weight

                _Ty current = m_current;

                if (current & _Ty(1)) {
                    m_current = current >> 1;
                    m_index++;
                    return;
                }

                size_t index = m_index;
                if constexpr (_Words > 1) {
                    if (current == 0) {
                        size_t offset = (index / _Bitsperword) + 1;
                        while (offset<_Words && (current = m_bitset->_Array[offset]) == 0)
                            ++offset;

                        index = offset*_Bitsperword - 1;
                    }
                }
                if (current == 0) {
                    m_index = _Bits;
                    return;
                }

                auto delta = find_first_bit(current);
                m_index = index + delta;
                // shift overflow is not well supported by all CPUs
                current = delta == _Bitsperword ? 0 : current >> delta;
                m_current = current;
            }

            // begin operator
            constexpr const_iterator(const const_bitset* _this, std::true_type) : m_bitset{ _this }
            {
                _Ty current = _this->_Array[0];
                m_current = current >> 1;

                if (current & _Ty(1)) {
                } else {
                    x_find_next_bit();
                }
            }

            // end operator
            constexpr const_iterator(const const_bitset* _this, std::false_type) : m_index{ _Bits }, m_bitset{ _this }
            {}

            size_t m_index{0};
            const const_bitset* m_bitset{nullptr};
            _Ty m_current{0};
        };

        #ifdef __cpp_concepts
        static_assert(std::input_iterator<const_iterator>);
        #endif

        using iterator = const_iterator;

        constexpr const_iterator cbegin() const noexcept { return const_iterator(this, std::true_type{}); }
        constexpr const_iterator cend()   const noexcept { return const_iterator(this, std::false_type{}); }
        constexpr const_iterator begin()  const noexcept { return cbegin(); }
        constexpr const_iterator end()    const noexcept { return cend(); }

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
            return _Pos<_Bits ? _Subscript(_Pos) : false;
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

} // namespace compile_time_bits

#if __cpp_inline_variables >= 201606L

namespace ct
{

template<auto arg0, auto...args>
constexpr auto inline_bitset = compile_time_bits::const_bitset<
        static_cast<size_t>(std::max({arg0, args...})) + 1,
        std::decay_t<decltype(arg0)>>
    {arg0, args...};

};


#endif


#endif
