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
                 TSignedSeqPos from, TSignedSeqPos to, TSegTypeFlags type);
    virtual operator bool() const;

    virtual TSegTypeFlags GetType() const;
    virtual const TSignedRange&    GetAlnRange() const;
    virtual const TSignedRange&    GetRange() const;

protected:
    TSignedRange  m_AlnRange;
    TSignedRange  m_Range;
    bool          m_Reversed;
    TSegTypeFlags m_Type;
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
               const TSignedRange& range);
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

#endif  // __OBJTOOLS_ALNMGR___SPARSE_ITERATOR__HPP
