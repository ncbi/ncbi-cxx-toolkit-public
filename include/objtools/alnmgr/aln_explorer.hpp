#ifndef OBJTOOLS_ALNMGR___ALN_EXPLORER__HPP
#define OBJTOOLS_ALNMGR___ALN_EXPLORER__HPP
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
* Authors:  Andrey Yazhuk, Kamen Todorov, NCBI
*
* File Description:
*   Abstract alignment explorer interface
*
* ===========================================================================
*/


#include <corelib/ncbistd.hpp>

#include <util/range.hpp>
#include <objmgr/seq_vector.hpp>


BEGIN_NCBI_SCOPE


/// Alignment explorer interface. Base class for CAlnMap and CSparseAln.
/// @sa CAlnMap
/// @sa CSparseAln
class IAlnExplorer
{
public:
    typedef int TNumrow;
    typedef objects::CSeqVector::TResidue TResidue;

    enum EAlignType {
        fDNA        = 0x01,
        fProtein    = 0x02,
        fMixed      = 0x04,
        fHomogenous = fDNA | fProtein,
        fInvalid    = 0x80000000
    };

    /// Position search options.
    enum ESearchDirection {
        eNone,      ///< No search
        eBackwards, ///< Towards lower seq coord (to the left if plus strand, right if minus)
        eForward,   ///< Towards higher seq coord (to the right if plus strand, left if minus)
        eLeft,      ///< Towards lower aln coord (always to the left)
        eRight      ///< Towards higher aln coord (always to the right)
    };

    enum ESortState {
        eUnSorted,
        eAscending,
        eDescending,
        eNotSupported
    };

    typedef CRange<TSeqPos>       TRange;
    typedef CRange<TSignedSeqPos> TSignedRange;
};


/// Alignment segment interface.
/// @sa CAlnChunkSegment
/// @sa CSparseSegment
class IAlnSegment
{
public:
    typedef IAlnExplorer::TSignedRange TSignedRange;

    typedef unsigned TSegTypeFlags; /// binary OR of ESegTypeFlags
    /// Segment type flags.
    enum ESegTypeFlags  {
        fAligned   = 1 << 0, ///< Aligned segment.

        fGap       = 1 << 1, ///< Both anchor row and the selected row are not
                             ///  included in the segment (some other row is
                             ///  present and the alignment range of this
                             ///  segment is not empty).

        fReversed  = 1 << 2, ///< The selected row is reversed (relative to
                             ///  the anchor).

        fIndel     = 1 << 3, ///< Either anchor or the selected row is not
                             ///  present in the segment.

        fUnaligned = 1 << 4, ///< The range on the selected sequence does not
                             ///  participate in the alignment (the alignment
                             ///  range of the segment is empty).

        fInvalid   = (TSegTypeFlags) 0x80000000, ///< The iterator is in bad state.

        fSegTypeMask = fAligned | fGap | fIndel | fUnaligned
    };

    virtual ~IAlnSegment(void) {}

    /// Get current segment type.
    virtual TSegTypeFlags GetType(void) const = 0;

    /// Get alignment range for the segment.
    virtual const TSignedRange& GetAlnRange(void) const = 0;

    /// Get the selected row range.
    virtual const TSignedRange& GetRange(void) const = 0;

    inline bool IsInvalidType(void) const { return (GetType() & fInvalid) != 0; }
    inline bool IsAligned(void) const { return (GetType() & fAligned) != 0; }
    inline bool IsGap(void) const { return (GetType() & fGap) != 0; }
    inline bool IsReversed(void) const { return (GetType() & fReversed) != 0; }
};


/// Alignment segment iterator interface.
/// @sa CAlnVecIterator
/// @sa CSparse_CI
class IAlnSegmentIterator
{
public:
    typedef IAlnSegment value_type;

    /// Iterator options
    enum EFlags {
        eAllSegments, ///< Iterate all segments
        eSkipGaps,    ///< Skip gap segments (show only aligned ranges)
        eInsertsOnly, ///< Iterate only ranges not participating in the
                      ///  alignment (unaligned segments)
        eSkipInserts  ///< Iterate segments where at least some rows are
                      ///  aligned (including gap segments)
    };

    virtual ~IAlnSegmentIterator(void) {}

    /// Create a copy of the iterator.
    virtual IAlnSegmentIterator* Clone(void) const = 0;

    /// Returns true if iterator points to a valid segment.
    virtual operator bool(void) const = 0;

    /// Advance to the next segment.
    virtual IAlnSegmentIterator& operator++(void) = 0;

    /// Compare iterators.
    virtual bool operator==(const IAlnSegmentIterator& it) const = 0;
    virtual bool operator!=(const IAlnSegmentIterator& it) const = 0;

    virtual const value_type& operator*(void) const = 0;
    virtual const value_type* operator->(void) const = 0;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_EXPLORER__HPP
