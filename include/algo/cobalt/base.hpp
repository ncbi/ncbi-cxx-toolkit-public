#ifndef ALGO_COBALT___BASE__HPP
#define ALGO_COBALT___BASE__HPP

/* $Id$
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: base.hpp

Author: Jason Papadopoulos

Contents: Definitions used by all COBALT aligner components

******************************************************************************/

/// @file base.hpp
/// Definitions used by all COBALT aligner components

#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

/// Basic data type for offsets into a sequence. By
/// convention, all offsets are zero-based
typedef int TOffset;

/// Basic type specifying a range on a sequence
typedef pair<TOffset, TOffset> TOffsetPair;

/// Sufficient extra functionality is needed from CRange
/// that it justifies extending the class
template<class Position>
class CLocalRange : public CRange<Position>
{
public:
    typedef CRange<Position> TParent;                       ///< parent class
    typedef typename TParent::position_type position_type;  ///< underlying type
    typedef CLocalRange<Position> TThisType;                ///< shorthand

    /// default constructor
    CLocalRange() {}

    /// convert offsets into a range
    /// @param from Start offset
    /// @param to End offset
    ///
    CLocalRange(position_type from, position_type to)
        : TParent(from, to) {}

    /// convert parent class to a range
    /// @param range Reference to an object of the parent class
    ///
    CLocalRange(const TParent& range)
        : TParent(range) {}

    /// Test whether 'this' completely envelops
    /// a given sequence range
    /// @param r Range to test for containment
    /// @return true if 'this' envelops r, false otherwise
    ///
    bool Contains(const TThisType& r)
    {
        return !TParent::Empty() && 
               TParent::GetFrom() <= r.GetFrom() &&
               TParent::GetToOpen() >= r.GetFrom() &&
               TParent::GetFrom() <= r.GetToOpen() &&
               TParent::GetToOpen() >= r.GetToOpen();
    }

    /// Test whether 'this' represents a sequence range
    /// strictly less than a given sequence range
    /// @param r Range to test for disjointness
    /// @return true if offsets of 'this' are strictly
    ///         below the offsets of r, false otherwise
    ///
    bool StrictlyBelow(const TThisType& r)
    {
        return TParent::GetToOpen() <= r.TParent::GetFrom();
    }

    /// Initialize an empty range
    ///
    void SetEmpty()
    {
        TParent::Set(TParent::GetPositionMax(), TParent::GetPositionMin());
    }
};

/// define for the fundamental building block
/// of sequence ranges
typedef CLocalRange<TOffset> TRange;

/// The aligner internally works only with the 
/// ncbistdaa alphabet
static const int kAlphabetSize = 28;

END_SCOPE(cobalt)
END_NCBI_SCOPE

#endif // ALGO_COBALT___BASE__HPP
