#if 0
#if !defined(OBJECTS_OBJMGR___SEQ_MAP_RCI__INL)
#define OBJECTS_OBJMGR___SEQ_MAP_RCI__INL

/* $Id$
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
 * Author: Eugene Vasilchenko
 *
 * File Description:
 *   Inline methods of CSeqMap class.
 *
 */


/////////////////////////////////////////////////////////////////////
//  CSeqMapRange_CI


inline
bool CSeqMapRange_CI::operator==(const CSeqMapRange_CI& seg) const
{
    return m_Segment == seg.m_Segment;
}


inline
bool CSeqMapRange_CI::operator!=(const CSeqMapRange_CI& seg) const
{
    return m_Segment != seg.m_Segment;
}


inline
bool CSeqMapRange_CI::operator< (const CSeqMapRange_CI& seg) const
{
    return m_Segment < seg.m_Segment;
}


inline
bool CSeqMapRange_CI::operator> (const CSeqMapRange_CI& seg) const
{
    return m_Segment > seg.m_Segment;
}


inline
bool CSeqMapRange_CI::operator<=(const CSeqMapRange_CI& seg) const
{
    return m_Segment <= seg.m_Segment;
}


inline
bool CSeqMapRange_CI::operator>=(const CSeqMapRange_CI& seg) const
{
    return m_Segment >= seg.m_Segment;
}


inline
CScope* CSeqMapRange_CI::GetScope(void) const
{
    return m_Segment.GetScope();
}


inline
bool CSeqMapRange_CI::InRange(void) const
{
    return m_Segment.GetPosition() < m_RangeEndPosition ||
        m_Segment.GetEndPosition() > m_RangePosition;
}


inline
TSeqPos CSeqMapRange_CI::GetSegPosition(void) const
{
    return max(m_RangePosition, m_Segment.GetPosition());
}


inline
TSeqPos CSeqMapRange_CI::GetSegEndPosition(void) const
{
    return min(m_RangeEndPosition, m_Segment.GetEndPosition());
}


inline
TSeqPos CSeqMapRange_CI::GetLength(void) const
{
    return GetSegEndPosition() - GetSegPosition();
}


inline
bool CSeqMapRange_CI::Next(void)
{
    return x_Move(m_MinusStrand);
}


inline
bool CSeqMapRange_CI::Prev(void)
{
    return x_Move(!m_MinusStrand);
}


inline
CSeqMapRange_CI& CSeqMapRange_CI::operator++(void)
{
    Next();
    return *this;
}


inline
CSeqMapRange_CI CSeqMapRange_CI::operator++(int)
{
    CSeqMapRange_CI ret(*this);
    Next();
    return ret;
}


inline
CSeqMapRange_CI& CSeqMapRange_CI::operator--(void)
{
    Prev();
    return *this;
}


inline
CSeqMapRange_CI CSeqMapRange_CI::operator--(int)
{
    CSeqMapRange_CI ret(*this);
    Prev();
    return ret;
}


/////////////////////////////////////////////////////////////////////
//  CSeqMapResolved_CI


inline
TSeqPos CSeqMapResolved_CI::GetPosition(void) const
{
    return m_Position;
}


inline
TSeqPos CSeqMapResolved_CI::GetLength(void) const
{
    return m_Stack.top().GetLength();
}


inline
TSeqPos CSeqMapResolved_CI::GetEndPosition(void) const
{
    return GetPosition() + GetLength();
}


inline
CSeqMap::ESegmentType CSeqMapResolved_CI::GetType(void) const
{
    return m_Stack.top().GetType();
}


inline
const CSeq_data& CSeqMapResolved_CI::GetData(void) const
{
    return m_Stack.top().GetData();
}


inline
bool CSeqMapResolved_CI::x_IsReference(void) const
{
    return m_Stack.top().GetRealType() == CSeqMap::eSeqRef;
}


inline
TSeqPos CSeqMapResolved_CI::GetRefPosition(void) const
{
    return m_Stack.top().GetRefPosition();
}


inline
TSeqPos CSeqMapResolved_CI::GetRefEndPosition(void) const
{
    return m_Stack.top().GetRefEndPosition();
}


inline
bool CSeqMapResolved_CI::GetRefMinusStrand(void) const
{
    return m_Stack.top().GetRefMinusStrand();
}


inline
bool CSeqMapResolved_CI::IsRefData(void) const
{
    return m_Stack.top().IsRefData();
}


inline
const CSeq_data& CSeqMapResolved_CI::GetRefData(void) const
{
    return m_Stack.top().GetRefData();
}


inline
TSeqPos CSeqMapResolved_CI::x_GetTopOffset(void) const
{
    return m_Stack.top().GetPosition();
}


inline
TSeqPos CSeqMapResolved_CI::x_GetUpPosition(void) const
{
    return GetPosition() - x_GetTopOffset();
}


inline
bool CSeqMapResolved_CI::operator==(const CSeqMapResolved_CI& seg) const
{
    return GetPosition() == seg.GetPosition();
}


inline
bool CSeqMapResolved_CI::operator!=(const CSeqMapResolved_CI& seg) const
{
    return GetPosition() != seg.GetPosition();
}


inline
bool CSeqMapResolved_CI::operator< (const CSeqMapResolved_CI& seg) const
{
    return GetPosition() < seg.GetPosition();
}


inline
bool CSeqMapResolved_CI::operator> (const CSeqMapResolved_CI& seg) const
{
    return GetPosition() > seg.GetPosition();
}


inline
bool CSeqMapResolved_CI::operator<=(const CSeqMapResolved_CI& seg) const
{
    return GetPosition() <= seg.GetPosition();
}


inline
bool CSeqMapResolved_CI::operator>=(const CSeqMapResolved_CI& seg) const
{
    return GetPosition() >= seg.GetPosition();
}


inline
CSeqMapResolved_CI& CSeqMapResolved_CI::operator++(void)
{
    Next();
    return *this;
}


inline
CSeqMapResolved_CI CSeqMapResolved_CI::operator++(int)
{
    CSeqMapResolved_CI ret(*this);
    Next();
    return ret;
}


inline
CSeqMapResolved_CI& CSeqMapResolved_CI::operator--(void)
{
    Prev();
    return *this;
}


inline
CSeqMapResolved_CI CSeqMapResolved_CI::operator--(int)
{
    CSeqMapResolved_CI ret(*this);
    Prev();
    return ret;
}



/*
 * ===========================================================================
 * $Log$
 * Revision 1.3  2003/01/23 19:32:02  vasilche
 * Commented out unused code. Prepared for removal.
 *
 * Revision 1.2  2003/01/22 20:11:53  vasilche
 * Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
 * CSeqMap_CI now supports resolution and iteration over sequence range.
 * Added several caches to CScope.
 * Optimized CSeqVector().
 * Added serveral variants of CBioseqHandle::GetSeqVector().
 * Tried to optimize annotations iterator (not much success).
 * Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
 *
 * Revision 1.1  2002/12/26 16:39:22  vasilche
 * Object manager class CSeqMap rewritten.
 *
 *
 * ===========================================================================
 */

#endif
#endif
