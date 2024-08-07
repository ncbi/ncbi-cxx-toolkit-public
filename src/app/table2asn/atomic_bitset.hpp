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
* Authors: Sergiy Gotvyanskyy
*
* File Description:
*   atomic bit set
*      allows setting/resetting/checking of bit set without use of mutexes
*
*/

#ifndef _TABLE2ASN_ATOMIC_BITSET_HPP_
#define _TABLE2ASN_ATOMIC_BITSET_HPP_

#include <corelib/ncbistl.hpp>
#include <vector>
#include <atomic>
#include <stdexcept>
#include <limits>



BEGIN_NCBI_SCOPE

class CAtomicBitSet
{
public:
    using _Ty = uint64_t;
    using atomic_type = std::atomic<_Ty>;
    static_assert(sizeof(_Ty) == sizeof(atomic_type));

    static constexpr size_t _Bitsperword = 8 * sizeof(_Ty);
    using container_type = std::vector<_Ty>;

    CAtomicBitSet() = default;
    CAtomicBitSet(CAtomicBitSet&&) = delete;
    CAtomicBitSet(const CAtomicBitSet&) = delete;
    CAtomicBitSet(size_t size, bool is_set = true)
    {
        Init(size, is_set);
    }
    void Init(size_t size, bool is_set = true)
    {
        m_array.resize(0);
        _Ty init_value = is_set ? std::numeric_limits<_Ty>::max() : 0;
        size_t _Words = (size + _Bitsperword - 1) / _Bitsperword;
        m_array.resize(_Words, init_value);
        if (is_set)
            m_bits = size;
        else
            m_bits = 0;
    }

    size_t capacity() const { return m_array.size() * _Bitsperword; }
    size_t size()     const { return m_bits; }
    bool empty()      const { return m_bits == 0; }

    bool test(size_t _Pos) const
    {
        return _Subscript(_Pos);
    }

    CAtomicBitSet& operator +=(size_t value)
    {
        set(value);
        return *this;
    }

    CAtomicBitSet& operator -=(size_t value)
    {
        reset(value);
        return *this;
    }

#if 0
    // underimplemented yet
    CAtomicBitSet& operator +=(const CAtomicBitSet& _Other)
    {
        if (capacity() != _Other.capacity())
            throw std::invalid_argument("CAtomicBitSet capacity not equal");

        for (size_t i=0; i<m_array.size(); i++)
            m_array[i] |= _Other.m_array[i];
        return *this;
    }

    CAtomicBitSet& operator -=(const CAtomicBitSet& _Other)
    {
        if (capacity() != _Other.capacity())
            throw std::invalid_argument("CAtomicBitSet capacity not equal");

        for (size_t i=0; i<m_array.size(); i++)
            m_array[i] &= (~_Other.m_array[i]);
        return *this;
    }
#endif

    // atomically resets bit and returns true if it was set before call
    bool reset(size_t _Pos)
    {
        auto& val = x_setnode(_Pos);
        _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
        auto prev_value = val.fetch_and(~mask);
        bool previous = (prev_value & mask) != 0;
        if (previous)
            --m_bits;
        return previous;
    }

    // atomically sets bit and and returns true if it was set before call
    bool set(size_t _Pos)
    {
        auto& val = x_setnode(_Pos);
        _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
        auto prev_value = val.fetch_or(mask);
        bool previous = (prev_value & mask) != 0;
        if (!previous)
            ++m_bits;
        return previous;
    }

    // atomically flips bit to the opposite state and and returns true if it was set before call
    bool flip(size_t _Pos)
    {
        auto& val = x_setnode(_Pos);
        _Ty mask = (_Ty)1 << _Pos % _Bitsperword;
        auto prev_value = val.fetch_xor(mask);
        bool previous = (prev_value & mask) != 0;
        if (previous)
            --m_bits;
        else
            ++m_bits;
        return previous;
    }

private:
    container_type m_array;    // the set of bits
    std::atomic<size_t> m_bits;

    atomic_type& x_setnode(size_t _Pos)
    {
        if (capacity() <= _Pos)
            _Xran();    // _Pos off end
        void* val = &m_array[_Pos / _Bitsperword];
        return *static_cast<atomic_type*>(val);
    }
    const atomic_type& x_getnode(size_t _Pos) const
    {
        if (capacity() <= _Pos)
            _Xran();    // _Pos off end
        const void* val = &m_array[_Pos / _Bitsperword];
        return *static_cast<const atomic_type*>(val);
    }

    bool _Subscript(size_t _Pos) const
    {    // subscript nonmutable sequence
        return ((x_getnode(_Pos)
            & ((_Ty)1 << _Pos % _Bitsperword)) != 0);
    }
    [[noreturn]] void _Xran() const
    {
        throw std::out_of_range("invalid CAtomicBitSet position");
    }

};



END_NCBI_SCOPE

#endif
