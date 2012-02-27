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


class IAlnSegment
{
public:
    typedef IAlnExplorer::TSignedRange TSignedRange;

    typedef unsigned TSegTypeFlags; // binary OR of ESegTypeFlags
    enum ESegTypeFlags  {
        fAligned   = 1 << 0,
        fGap       = 1 << 1,
        fReversed  = 1 << 2,
        fIndel     = 1 << 3,
        fUnaligned = 1 << 4,
        fInvalid   = (TSegTypeFlags) 0x80000000,

        fSegTypeMask = fAligned | fGap | fIndel | fUnaligned
    };

    virtual ~IAlnSegment()  {}

    virtual TSegTypeFlags GetType() const = 0;
    virtual const TSignedRange& GetAlnRange() const = 0;
    virtual const TSignedRange& GetRange() const = 0;

    inline bool IsInvalidType() const { return (GetType() & fInvalid) != 0; }
    inline bool IsAligned() const { return (GetType() & fAligned) != 0; }
    inline bool IsGap() const { return (GetType() & fGap) != 0; }
    inline bool IsReversed() const { return (GetType() & fReversed) != 0; }
};


class IAlnSegmentIterator
{
public:
    typedef IAlnSegment value_type;

    enum EFlags {
        eAllSegments,
        eSkipGaps,
        eInsertsOnly,
        eSkipInserts
    };

public:
    virtual ~IAlnSegmentIterator() {};

    virtual IAlnSegmentIterator* Clone() const = 0;

    // returns true if iterator points to a valid segment
    virtual operator bool() const = 0;

    virtual IAlnSegmentIterator& operator++() = 0;

    virtual bool operator==(const IAlnSegmentIterator& it) const = 0;
    virtual bool operator!=(const IAlnSegmentIterator& it) const = 0;

    virtual const value_type& operator*() const = 0;
    virtual const value_type* operator->() const = 0;
};


END_NCBI_SCOPE

#endif  // OBJTOOLS_ALNMGR___ALN_EXPLORER__HPP
