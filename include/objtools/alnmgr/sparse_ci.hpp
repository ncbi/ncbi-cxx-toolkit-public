#ifndef __OBJTOOLS_ALNMGR___SPARSE_CI__HPP
#define __OBJTOOLS_ALNMGR___SPARSE_CI__HPP

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
 * Authors:  Andrey Yazhuk
 *
 * File Description:
 *
 */

#include <corelib/ncbimisc.hpp>

#include <gui/widgets/aln_data/utils.hpp>

#include <objtools/alnmgr/sparse_aln.hpp>

BEGIN_NCBI_SCOPE

////////////////////////////////////////////////////////////////////////////////
/// CSparseSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

class NCBI_XALNMGR_EXPORT CSparseSegment : public  IAlnSegment
{
    friend class CSparseIterator;
public:
    typedef CSparseAln::TAlnRng     TAlignRange;
    typedef CSparseAln::TAlnRngColl TAlnRngColl;

    CSparseSegment();
    void    Init(TSignedSeqPos aln_from, TSignedSeqPos aln_to,
                 TSignedSeqPos from, TSignedSeqPos to, int type);
    virtual operator bool() const;

    virtual int   GetType() const;
    virtual const TSignedRange&    GetAlnRange() const;
    virtual const TSignedRange&    GetRange() const;

protected:
    TSignedRange    m_AlnRange;
    TSignedRange    m_Range;
    bool    m_Reversed;
    int     m_Type;
};


////////////////////////////////////////////////////////////////////////////////
/// CSparseIterator - IAlnSegmentIterator implementation for CAlnMap::CAlnChunkVec

class NCBI_XALNMGR_EXPORT CSparse_CI : public IAlnSegmentIterator
{
public:
    typedef CSparseAln::TAlnRng     TAlignRange;
    typedef CSparseAln::TAlnRngColl TAlignColl;
    typedef CRange<TSignedSeqPos>   TSignedRange;

    CSparse_CI();
    CSparse_CI(const TAlignColl& coll, EFlags flag);
    CSparse_CI(const TAlignColl& coll, EFlags flag,
               const CAlignUtils::TSignedRange& range);
    CSparse_CI(const CSparse_CI& orig);

    virtual ~CSparse_CI();

public:
    virtual IAlnSegmentIterator*    Clone() const;

    // returns true if iterator points to a valid segment
    virtual operator bool() const;

    /// postfix operators are not defined to avoid performance overhead
    virtual IAlnSegmentIterator& operator++();

    virtual bool    operator==(const IAlnSegmentIterator& it) const;
    virtual bool    operator!=(const IAlnSegmentIterator& it) const;

    virtual const value_type&  operator*() const;
    virtual const value_type* operator->() const;

protected:
    inline bool x_Equals(const CSparse_CI& it) const
    {
        return  m_Coll == it.m_Coll  &&  m_Flag == it.m_Flag  &&
                m_It_1 == it.m_It_1  &&  m_It_2 == m_It_2;
    }
    inline void x_InitSegment();

    void x_InitIterator();

    inline bool x_IsInsert() const  {
        return (m_It_1 != m_It_2)  &&  (m_It_1->GetFirstFrom() == m_It_2->GetFirstToOpen());
    }
protected:
    struct  SClip
    {
        typedef TAlignColl::const_iterator const_iterator;

        TSignedSeqPos m_From;
        TSignedSeqPos m_ToOpen;

        const_iterator m_First_It;
        const_iterator m_Last_It_1, m_Last_It_2; // designate last segment to iterate
    };

    const TAlignColl*  m_Coll;
    EFlags  m_Flag;     /// iterating mode

    SClip*  m_Clip; // not NULL if clip is set

    TAlignColl::const_iterator m_It_1, m_It_2;
    /// if m_It1 != m_It2 then the iterator points to a gap between m_It1 and m_It2

    CSparseSegment    m_Segment;
};


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2007/01/04 21:15:48  todorov
 * Fixed some typedefs.
 *
 * Revision 1.2  2006/12/01 17:54:05  todorov
 * + NCBI_XALNMGR_EXPORT
 *
 * Revision 1.1  2006/11/16 13:48:12  todorov
 * Moved over from gui/widgets/aln_data and refactored to adapt to the
 * new aln framework.
 *
 * Revision 1.5  2006/09/05 12:35:26  dicuccio
 * White space changes: tabs -> spaces; trim trailing spaces
 *
 * Revision 1.4  2006/07/03 14:32:43  yazhuk
 * Fixed iteration on inserts
 *
 * Revision 1.3  2005/09/19 12:21:04  dicuccio
 * White space changes: trim trailing white space; use spaces not tabs
 *
 * Revision 1.2  2005/06/27 14:37:04  yazhuk
 * Added destructor, TAlignRangeColl renamed to TAlignColl
 *
 * Revision 1.1  2005/06/13 19:02:43  yazhuk
 * Initial revision
 *
 * Revision 1.2  2005/05/09 18:23:29  ucko
 * CSparseSegment: Accept CConstRef<TChunk> by value, not reference!
 *
 * Revision 1.1  2005/05/09 17:54:07  yazhuk
 * Initial revision
 *
 * ===========================================================================
 */

#endif  // __OBJTOOLS_ALNMGR___SPARSE_ITERATOR__HPP

