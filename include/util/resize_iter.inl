#if defined(RESIZE_ITER__HPP)  &&  !defined(RESIZE_ITER__INL)
#define RESIZE_ITER__INL

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
*
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.2  2001/09/21 22:39:18  ucko
* Fix MSVC build.
*
* Revision 1.1  2001/09/04 14:06:31  ucko
* Add resizing iterators for sequences whose representation uses an
* unnatural unit size -- for instance, ASN.1 octet strings corresponding
* to sequences of 32-bit integers or of packed nucleotides.
*
* ===========================================================================
*/


// Helpers.
// This code contains some heavy bit-fiddling; take care when modifying it.

static const unsigned int kBitsPerByte = 8;


template <class T>
size_t x_BitsPerElement(T*) {
    return kBitsPerByte * sizeof(T);
}


template <class TIterator, class TOut>
TOut ExtractBits(TIterator& start, size_t& bit_offset, size_t bit_count)
{
#ifdef _RWSTD_NO_CLASS_PARTIAL_SPEC // Why does Workshop set this?
    static const size_t kBitsPerElement
        = x_BitsPerElement(__value_type(TIterator()));
#elif defined(NCBI_OS_MSWIN) && !defined(__GNUC__)
    // iterator_traits seems to be broken under MSVC. :-/
    static const size_t kBitsPerElement
        = x_BitsPerElement(_Val_type(TIterator()));    
#else
    static const size_t kBitsPerElement
        = kBitsPerByte * sizeof(iterator_traits<TIterator>::value_type);
#endif

    const TOut kMask = (1 << bit_count) - 1;
    static const TOut kMask2 = (1 << kBitsPerElement) - 1;
    TOut value;

    if (bit_offset + bit_count <= kBitsPerElement) {
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
        for (bit_offset += bit_count - kBitsPerElement;
             bit_offset >= kBitsPerElement;
             bit_offset -= kBitsPerElement) {
            value = (value << kBitsPerElement) | (*++start & kMask2);
        }        
        ++start;
        if (bit_offset)
            value = ((value << bit_offset)
                       | ((*start >> (kBitsPerElement - bit_offset))
                          & ((1 << bit_offset) - 1)));
    }
    return value;
}


template <class TIterator, class TVal, class TElement>
TElement StoreBits(TIterator& start, size_t& bit_offset, size_t bit_count,
                   TElement partial, TVal data) // returns new partial
{
    static const size_t kBitsPerElement = kBitsPerByte * sizeof(TElement);

    if (bit_offset) {
        partial &= (~(TElement)0) << (kBitsPerElement - bit_offset);
    } else {
        partial = 0;
    }

    if (bit_offset + bit_count <= kBitsPerElement) {
        // Everything fits in one element.
        bit_offset += bit_count;
        partial |= data << (kBitsPerElement - bit_offset);
        if (bit_count == kBitsPerElement) {
            *(start++) = partial;
            bit_count = 0;
            partial = 0;
        }
    } else {
        // We need to split it up.
        *(start++) = partial | ((data >> (bit_count + bit_offset
                                          - kBitsPerElement))
                                & ((1 << (kBitsPerElement - bit_offset)) - 1));
        for (bit_offset += bit_count - kBitsPerElement;
             bit_offset >= kBitsPerElement;
             bit_offset -= kBitsPerElement) {
            *(start++) = data >> (bit_offset - kBitsPerElement);
        }
        if (bit_offset) {
            partial = data << (kBitsPerElement - bit_offset);
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
    static const size_t kBitsPerElement = kBitsPerByte * sizeof(TRawValue);

    // We advance the raw iterator past things we read, so only advance
    // it now if we haven't read the current value.
    if (!m_ValueKnown) {
        for (m_BitOffset += m_NewSize;  m_BitOffset >= kBitsPerElement;
             m_BitOffset -= kBitsPerElement) {
            m_RawIterator++;
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
        (m_RawIterator, m_BitOffset, m_NewSize);
}


// CResizingIterator members.


template <class TSeq, class TVal>
CResizingIterator<TSeq, TVal>& CResizingIterator<TSeq, TVal>::operator++()
    // prefix
{
    static const size_t kBitsPerElement = kBitsPerByte * sizeof(TRawValue);

    for (m_BitOffset += m_NewSize;  m_BitOffset >= kBitsPerElement;
         m_BitOffset -= kBitsPerElement) {
        m_RawIterator++;
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

    tmp = StoreBits<TRawIterator, TVal, TRawValue>(it, offset, m_NewSize, *it,
                                                   value);
    *it = tmp;
}


template <class TSeq, class TVal>
CResizingIterator<TSeq, TVal>::operator TVal()
{
    // don't advance iterator in object.
    TRawIterator it = m_RawIterator;
    size_t offset = m_BitOffset;

    return ExtractBits<TRawIterator, TVal>(it, offset, m_NewSize);
}

#endif /* def RESIZE_ITER__HPP  &&  ndef RESIZE_ITER__INL */
