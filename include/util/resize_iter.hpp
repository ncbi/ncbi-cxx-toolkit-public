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
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2001/09/04 14:06:30  ucko
* Add resizing iterators for sequences whose representation uses an
* unnatural unit size -- for instance, ASN.1 octet strings corresponding
* to sequences of 32-bit integers or of packed nucleotides.
*
* ===========================================================================
*/

#include <corelib/ncbistd.hpp>
#include <iterator>

// (BEGIN_NCBI_SCOPE must be followed by END_NCBI_SCOPE later in this file)
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
    bool AtEnd() { return m_RawIterator == m_End; }

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
        : m_RawIterator(it), m_End(end), m_NewSize(new_size), m_BitOffset(0) {}

    CResizingIterator<TSeq, TVal> & operator++(); // prefix
    CResizingIterator<TSeq, TVal> operator++(int); // postfix
    CResizingIterator<TSeq, TVal> operator*()
        { return *this; } // acts as its own proxy type
    // Again, no operator->.

    void operator=(TVal value);
    operator TVal();

    bool AtEnd() { return m_RawIterator == m_End; }

private:
    TRawIterator m_RawIterator;
    TRawIterator m_End;
    size_t       m_NewSize;
    size_t       m_BitOffset;    
};


///////////////////////////////////////////////////////
// All inline function implementations and internal data
// types, etc. are in this file
#include <util/resize_iter.inl>


// (END_NCBI_SCOPE must be preceded by BEGIN_NCBI_SCOPE)
END_NCBI_SCOPE

#endif  /* RESIZE_ITER__HPP */
