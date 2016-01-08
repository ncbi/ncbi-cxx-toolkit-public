#ifndef RESIZE_ITER__HPP
#define RESIZE_ITER__HPP

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
* Author:  Aaron Ucko
*
* File Description:
*   CConstResizingIterator: handles sequences represented as packed
*   sequences of elements of a different size; for instance, a
*   "vector<char>" might actually hold 2-bit nucleotides or 32-bit integers.
*   Assumes a big-endian representation: the MSBs of the first elements of
*   the input and output sequences line up.
*/

#include <corelib/ncbistd.hpp>
#include <iterator>
#include <limits.h>


/** @addtogroup ResizingIterator
 *
 * @{
 */


BEGIN_NCBI_SCOPE


template <class TSeq, class TOut = int>
class CConstResizingIterator
{
    // Acts like an STL input iterator, with two exceptions:
    //  1. It does not support ->, but TOut should be scalar anyway.
    //  2. It caches the last value read, so might not adequately
    //     reflect changes to the underlying sequence.
    // Also has forward-iterator semantics iff TSeq::const_iterator does.

    typedef typename TSeq::const_iterator TRawIterator;
    typedef typename TSeq::value_type     TRawValue;

public:
    typedef input_iterator_tag iterator_category;
    typedef TOut               value_type;
    typedef size_t             difference_type;
    // no pointer or reference.

    CConstResizingIterator(const TSeq& s, size_t new_size /* in bits */)
        : m_RawIterator(s.begin()), m_End(s.end()), m_NewSize(new_size),
          m_BitOffset(0), m_ValueKnown(false) {}
    CConstResizingIterator(const TRawIterator& it, const TRawIterator& end,
                           size_t new_size)
        : m_RawIterator(it), m_End(end), m_NewSize(new_size), m_BitOffset(0),
          m_ValueKnown(false) {}
    CConstResizingIterator<TSeq, TOut> & operator++(); // prefix
    CConstResizingIterator<TSeq, TOut> operator++(int); // postfix
    TOut operator*();
    // No operator->; see above.
    bool AtEnd() const;

private:
    TRawIterator m_RawIterator;
    TRawIterator m_End;
    size_t       m_NewSize;
    size_t       m_BitOffset;
    TOut         m_Value;
    bool         m_ValueKnown;
    // If m_ValueKnown is true, we have already determined the value at
    // the current position and stored it in m_Value, advancing 
    // m_RawIterator + m_BitOffset along the way.  Otherwise, m_Value still
    // holds the previously accessed value, and m_RawIterator + m_BitOffset
    // points at the beginning of the current value.  This system is
    // necessary to handle multiple dereferences to a value spanning
    // multiple elements of the original sequence.
};


template <class TSeq, class TVal = int>
class CResizingIterator
{
    typedef typename TSeq::iterator   TRawIterator;
    // must be a mutable forward iterator.
    typedef typename TSeq::value_type TRawValue;

public:
    typedef forward_iterator_tag iterator_category;
    typedef TVal                 value_type;
    typedef size_t               difference_type;
    // no pointer or reference.

    CResizingIterator(TSeq& s, size_t new_size)
        : m_RawIterator(s.begin()), m_End(s.end()), m_NewSize(new_size),
          m_BitOffset(0) {}
    CResizingIterator(const TRawIterator& start, const TRawIterator& end,
                      size_t new_size)
        : m_RawIterator(start), m_End(end), m_NewSize(new_size), m_BitOffset(0)
        {}

    CResizingIterator<TSeq, TVal> & operator++(); // prefix
    CResizingIterator<TSeq, TVal> operator++(int); // postfix
    CResizingIterator<TSeq, TVal> operator*()
        { return *this; } // acts as its own proxy type
    // Again, no operator->.

    void operator=(TVal value);
    operator TVal();

    bool AtEnd() const;

private:
    TRawIterator m_RawIterator;
    TRawIterator m_End;
    size_t       m_NewSize;
    size_t       m_BitOffset;    
};


/* @} */


///////////////////////////////////////////////////////////////////////////
//
// INLINE FUNCTIONS
//
// This code contains some heavy bit-fiddling; take care when modifying it.

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif


template <class T>
size_t xx_BitsPerElement(const T*)
{
    return CHAR_BIT * sizeof(T);
}

template <class TIterator>
size_t x_BitsPerElement(const TIterator&)
{
#ifdef _RWSTD_NO_CLASS_PARTIAL_SPEC
    // Sun cares way too much about backward bug-for-bug compatibility...
    return xx_BitsPerElement(__value_type(TIterator()));
#elif defined(NCBI_COMPILER_MSVC)
    // iterator_traits seems to be broken under MSVC. :-/
    return xx_BitsPerElement(_Val_type(TIterator()));    
#else
    return CHAR_BIT * sizeof(typename iterator_traits<TIterator>::value_type);
#endif    
}


template <class TIterator, class TOut>
TOut ExtractBits(TIterator& start, const TIterator& end,
                 size_t& bit_offset, size_t bit_count)
{
#if 1
    static const size_t kBitsPerElement = x_BitsPerElement(start);
#elif defined(_RWSTD_NO_CLASS_PARTIAL_SPEC)
    static const size_t kBitsPerElement
        = xx_BitsPerElement(__value_type(TIterator()));
#elif defined(NCBI_COMPILER_MSVC)
    static const size_t kBitsPerElement
        = xx_BitsPerElement(_Val_type(TIterator()));    
#else
    static const size_t kBitsPerElement
        = CHAR_BIT * sizeof(typename iterator_traits<TIterator>::value_type);
#endif

    const TOut kMask = (1 << bit_count) - 1;
    static const TOut kMask2 = (1 << kBitsPerElement) - 1;
    TOut value;

    if (start == end) {
        return 0;
    } else if (bit_offset + bit_count <= kBitsPerElement) {
        // the current element contains it all
        bit_offset += bit_count;
        value = (*start >> (kBitsPerElement - bit_offset)) & kMask;
        if (bit_offset == kBitsPerElement) {
            bit_offset = 0;
            ++start;
        }
    } else {
        // We have to deal with multiple elements.
        value = *start & ((1 << (kBitsPerElement - bit_offset)) - 1);
        ++start;
        for (bit_offset += bit_count - kBitsPerElement;
             bit_offset >= kBitsPerElement;
             bit_offset -= kBitsPerElement) {
            value <<= kBitsPerElement;
            if (start != end) {
                value |= *start & kMask2;
                ++start;
            }
        }        
        if (bit_offset) {
            value <<= bit_offset;
            if (start != end) {
                value |= ((*start >> (kBitsPerElement - bit_offset))
                          & ((1 << bit_offset) - 1));
            }
        }
    }
    return value;
}


template <class TIterator, class TVal, class TElement>
TElement StoreBits(TIterator& start, const TIterator& end,
                   size_t& bit_offset, size_t bit_count,
                   TElement partial, TVal data) // returns new partial
{
    static const size_t kBitsPerElement = CHAR_BIT * sizeof(TElement);

    if (bit_offset) {
        partial = (TElement)(partial & 
                      (~(TElement)0) << (kBitsPerElement - bit_offset));
    } else {
        partial = 0;
    }

    if (bit_offset + bit_count <= kBitsPerElement) {
        // Everything fits in one element.
        bit_offset += bit_count;
        partial = (TElement)(partial | 
                      (data << (kBitsPerElement - bit_offset)));
        if (bit_count == kBitsPerElement) {
            *(start++) = partial;
            bit_count = 0;
            partial = 0;
        }
    } else {
        // We need to split it up.
        *(start++) = (TElement)(partial | 
            ((data >> (bit_count + bit_offset - kBitsPerElement))
              & ((1 << (kBitsPerElement - bit_offset)) - 1)) );
        for (bit_offset += bit_count - kBitsPerElement;
             bit_offset >= kBitsPerElement;
             bit_offset -= kBitsPerElement) {
            if (start != end) {
                *(start++) = (TElement)(data >> (bit_offset - kBitsPerElement));
            }
        }
        if (bit_offset) {
            partial = (TElement)(data << (kBitsPerElement - bit_offset));
        } else {
            partial = 0;
        }
    }
    return partial;
}


// CConstResizingIterator members.


template <class TSeq, class TOut>
CConstResizingIterator<TSeq, TOut> &
CConstResizingIterator<TSeq, TOut>::operator++() // prefix
{
    static const size_t kBitsPerElement = CHAR_BIT * sizeof(TRawValue);

    // We advance the raw iterator past things we read, so only advance
    // it now if we haven't read the current value.
    if (!m_ValueKnown) {
        for (m_BitOffset += m_NewSize;
             m_BitOffset >= kBitsPerElement  &&  m_RawIterator != m_End;
             m_BitOffset -= kBitsPerElement) {
            ++m_RawIterator;
        }
    }
    m_ValueKnown = false;
    return *this;
}


template <class TSeq, class TOut>
CConstResizingIterator<TSeq, TOut>
CConstResizingIterator<TSeq, TOut>::operator++(int) // postfix
{
    CConstResizingIterator<TSeq, TOut> copy(*this);
    ++(*this);
    return copy;
}


template <class TSeq, class TOut>
TOut CConstResizingIterator<TSeq, TOut>::operator*()
{
    if (m_ValueKnown)
        return m_Value;

    m_ValueKnown = true;
    return m_Value = ExtractBits<TRawIterator, TOut>
        (m_RawIterator, m_End, m_BitOffset, m_NewSize);
}



template <class TSeq, class TOut>
bool CConstResizingIterator<TSeq, TOut>::AtEnd() const
{
    size_t avail = 0, goal = m_BitOffset + m_NewSize;
    for (TRawIterator it2 = m_RawIterator;  it2 != m_End  &&  avail < goal;
         ++it2) {
        avail += x_BitsPerElement(m_RawIterator);
    }
    return avail < goal;
}


// CResizingIterator members.


template <class TSeq, class TVal>
CResizingIterator<TSeq, TVal>& CResizingIterator<TSeq, TVal>::operator++()
    // prefix
{
    static const size_t kBitsPerElement = CHAR_BIT * sizeof(TRawValue);

    for (m_BitOffset += m_NewSize;
         m_BitOffset >= kBitsPerElement  &&  m_RawIterator != m_End;
         m_BitOffset -= kBitsPerElement) {
        ++m_RawIterator;
    }
    return *this;
}


template <class TSeq, class TVal>
CResizingIterator<TSeq, TVal> CResizingIterator<TSeq, TVal>::operator++(int)
    // postfix
{
    CResizingIterator<TSeq, TVal> copy(*this);
    ++(*this);
    return copy;
}


template <class TSeq, class TVal>
void CResizingIterator<TSeq, TVal>::operator=(TVal value)
{
    // don't advance iterator in object.
    TRawIterator it = m_RawIterator;
    size_t offset = m_BitOffset;
    TRawValue tmp;

    tmp = StoreBits<TRawIterator, TVal, TRawValue>
        (it, m_End, offset, m_NewSize, *it, value);
    if (offset > 0  &&  it != m_End) {
        *it = tmp;
    }
}


template <class TSeq, class TVal>
CResizingIterator<TSeq, TVal>::operator TVal()
{
    // don't advance iterator in object.
    TRawIterator it = m_RawIterator;
    size_t offset = m_BitOffset;

    return ExtractBits<TRawIterator, TVal>(it, m_End, offset, m_NewSize);
}


template <class TSeq, class TVal>
bool CResizingIterator<TSeq, TVal>::AtEnd() const
{
    size_t avail = 0, goal = m_BitOffset + m_NewSize;
    for (TRawIterator it2 = m_RawIterator;  it2 != m_End  &&  avail < goal;
         ++it2) {
        avail += x_BitsPerElement(m_RawIterator);
    }
    return avail < goal;
}


END_NCBI_SCOPE

#endif  /* RESIZE_ITER__HPP */
