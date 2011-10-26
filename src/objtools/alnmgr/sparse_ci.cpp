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

#include <ncbi_pch.hpp>

#include <objtools/alnmgr/sparse_ci.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(ncbi::objects);

////////////////////////////////////////////////////////////////////////////////
/// CSparseSegment - IAlnSegment implementation for CAlnMap::CAlnChunk

CSparseSegment::CSparseSegment()
{
}


void CSparseSegment::Init(TSignedSeqPos aln_from, TSignedSeqPos aln_to,
                          TSignedSeqPos from, TSignedSeqPos to, TSegTypeFlags type)
{
    m_AlnRange.Set(aln_from, aln_to);
    m_Range.Set(from, to);
    m_Type = type;
}


CSparseSegment::operator bool() const
{
    return ! IsInvalidType();
}


CSparseSegment::TSegTypeFlags CSparseSegment::GetType() const
{
    return m_Type;
}


const CSparseSegment::TSignedRange& CSparseSegment::GetAlnRange() const
{
    return m_AlnRange;
}


const CSparseSegment::TSignedRange& CSparseSegment::GetRange() const
{
    return m_Range;
}


////////////////////////////////////////////////////////////////////////////////
/// CSparse_CI

inline void CSparse_CI::x_InitSegment()
{
    if( ! (bool)*this)   {
        m_Segment.Init(-1, -1, -1, -1, IAlnSegment::fInvalid); // invalid
    } else {
        IAlnSegment::TSegTypeFlags dir_flag = m_It_1->IsDirect() ? 0 : IAlnSegment::fReversed;

        if(m_It_1 == m_It_2)  {   // aligned segment
            if(m_Clip  &&  (m_It_1 == m_Clip->m_First_It  ||  m_It_1 == m_Clip->m_Last_It_1))  {
                // we need to clip the current segment
                TAlignRange r = *m_It_1;
                CRange<TSignedSeqPos> clip(m_Clip->m_From, m_Clip->m_ToOpen - 1);
                r.IntersectWith(clip);

                m_Segment.Init(r.GetFirstFrom(), r.GetFirstTo(),
                               r.GetSecondFrom(), r.GetSecondTo(),
                               IAlnSegment::fAligned | dir_flag);
            } else {
                const TAlignRange& r = *m_It_1;
                m_Segment.Init(r.GetFirstFrom(), r.GetFirstTo(),
                               r.GetSecondFrom(), r.GetSecondTo(),
                               IAlnSegment::fAligned | dir_flag);
            }
        } else {  // gap
            _ASSERT(m_It_1->IsDirect() == m_It_2->IsDirect());
            TSignedSeqPos from, to;
            if (m_It_1->IsDirect()) {
                from = m_It_2->GetSecondToOpen();
                to = m_It_1->GetSecondFrom() - 1;
            } else {
                from = m_It_1->GetSecondToOpen();
                to = m_It_2->GetSecondFrom() - 1;
            }

            if(m_Clip  &&  (m_It_1 == m_Clip->m_First_It  ||  m_It_1 == m_Clip->m_Last_It_1))  {
                TSignedRange r(m_It_2->GetFirstToOpen(), m_It_1->GetFirstFrom() - 1);
                TSignedRange clip(m_Clip->m_From, m_Clip->m_ToOpen - 1);
                r.IntersectWith(clip);

                m_Segment.Init(r.GetFrom(), r.GetTo(), from, to, IAlnSegment::fGap);
            } else {
                m_Segment.Init(m_It_2->GetFirstToOpen(), m_It_1->GetFirstFrom() - 1,
                               from, to, IAlnSegment::fGap);
            }
        }
    }
}

// assuming clipping range
void CSparse_CI::x_InitIterator()
{
    _ASSERT(m_Coll);
    _ASSERT( !m_Coll->empty() );

    bool first_gap = false, last_gap = false;
    if(m_Clip)   {
        // adjsut starting position
        TSignedSeqPos from = m_Clip->m_From;

        pair<TAlignColl::const_iterator, bool> res = m_Coll->find_2(from);

        // set start position
        if(res.second)  {
            _ASSERT(res.first->FirstContains(from)); // pos inside a segment
            m_Clip->m_First_It = res.first;
        } else {    // pos is in the gap
            m_Clip->m_First_It = res.first;
            if(res.first != m_Coll->begin())    {
                first_gap = true;
            }
        }

        // set end condition
        TSignedSeqPos to = m_Clip->m_ToOpen - 1;
        res = m_Coll->find_2(to);

        if(res.second)  {
            _ASSERT(res.first->FirstContains(to)); // pos inside a segment
            m_Clip->m_Last_It_1 = m_Clip->m_Last_It_2 = res.first;
        } else {
            // pos is in the gap
            if(res.first == m_Coll->end())   {
                _ASSERT(res.first != m_Coll->begin());
                m_Clip->m_Last_It_1 = m_Clip->m_Last_It_2 = res.first - 1;
            } else {
                m_Clip->m_Last_It_1 = m_Clip->m_Last_It_2 = res.first;
                if (res.first != m_Coll->begin()) {
                    --m_Clip->m_Last_It_2;
                    last_gap = true;
                }
            }
        }
        // initialize iterators
        m_It_2 = m_It_1 = m_Clip->m_First_It;
    } else { // no clip
        m_It_1 = m_It_2 = m_Coll->begin();
    }

    // adjsut iterators
    switch(m_Flag)  {
    case eAllSegments:
        if(first_gap)   {
            --m_It_2;
        }
        break;
    case eSkipGaps:
        if(last_gap)    {
            --m_Clip->m_Last_It_1;
        }
        break;
    case eInsertsOnly:
        if(first_gap)   {
            --m_It_2;
        } else {
            ++m_It_1;
        }
        if((bool)*this  &&  ! x_IsInsert()) {
            ++(*this);
        }
        break;
    case eSkipInserts:
        if(first_gap  &&  x_IsInsert()) {
            ++(*this);
        }
    }

    x_InitSegment();
}


CSparse_CI::CSparse_CI()
:   m_Coll(NULL),
    m_Clip(NULL)
{
    x_InitSegment();
}


CSparse_CI::CSparse_CI(const TAlignColl& coll, EFlags flag)
:   m_Coll(&coll),
    m_Flag(flag),
    m_Clip(NULL)
{
    x_InitIterator();
}


CSparse_CI::CSparse_CI(const TAlignColl& coll, EFlags flag,
                       const TSignedRange& range)
:   m_Coll(&coll),
    m_Flag(flag),
    m_Clip(NULL)
{
    if(m_Coll)  {
        m_Clip = new SClip;
        m_Clip->m_From = range.GetFrom();
        m_Clip->m_ToOpen = range.GetToOpen();
    }

    x_InitIterator();
}


CSparse_CI::CSparse_CI(const CSparse_CI& orig)
:   m_Coll(orig.m_Coll),
    m_Flag(orig.m_Flag),
    m_It_1(orig.m_It_1),
    m_It_2(orig.m_It_2)
{
    if(orig.m_Clip)  {
        m_Clip = new SClip(*orig.m_Clip);
    }
    x_InitSegment();
}


CSparse_CI::~CSparse_CI()
{
    delete m_Clip;
}


IAlnSegmentIterator*    CSparse_CI::Clone() const
{
    return new CSparse_CI(*this);
}


CSparse_CI::operator bool() const
{
    if(m_Coll  &&  m_It_1 >= m_Coll->begin())  {
        if(m_Clip)  {
            return  (m_It_1 <= m_Clip->m_Last_It_1)  &&   (m_It_2 <= m_Clip->m_Last_It_2);
        }
        else {
            return m_It_1 != m_Coll->end();
        }
    }
    return false;
}


IAlnSegmentIterator& CSparse_CI::operator++()
{
    _ASSERT(m_Coll);

    switch(m_Flag)  {
    case eAllSegments:
        if(m_It_1 == m_It_2)   {  // aligned segment - move to gap
            ++m_It_1;
        } else {    // gap - move to the next segment
            ++m_It_2;
        }
        break;
    case eSkipGaps:
        ++m_It_1;
        ++m_It_2;
        break;
    case eInsertsOnly:
        do {
            ++m_It_1;
            ++m_It_2;
        } while( (bool)*this && m_It_1->GetFirstFrom() != m_It_2->GetFirstToOpen() );
        break;
    case eSkipInserts:
        if( m_It_1 == m_It_2 ){  // aligned segment - move to gap
            ++m_It_1;
            if( (bool)*this && m_It_1->GetFirstFrom() == m_It_2->GetFirstToOpen() ){ // insert - skip
                ++m_It_2;
            }
        } else {    // gap - move to the next segment
            ++m_It_2;
        }
        break;
    default:
        _ASSERT(false); // not implemented
    }

    x_InitSegment();
    return *this;
}


bool CSparse_CI::operator==(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CSparse_CI* sparse_it =
            dynamic_cast<const CSparse_CI*>(&it);
        return x_Equals(*sparse_it);
    }
    return false;
}


bool CSparse_CI::operator!=(const IAlnSegmentIterator& it) const
{
    if(typeid(*this) == typeid(it)) {
        const CSparse_CI* sparse_it =
            dynamic_cast<const CSparse_CI*>(&it);
        return ! x_Equals(*sparse_it);
    }
    return true;
}


const CSparse_CI::value_type& CSparse_CI::operator*() const
{
    _ASSERT(*this);
    return m_Segment;
}


const CSparse_CI::value_type* CSparse_CI::operator->() const
{
    _ASSERT(*this);
    return &m_Segment;
}


END_NCBI_SCOPE
