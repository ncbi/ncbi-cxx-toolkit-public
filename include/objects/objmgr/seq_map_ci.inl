#if !defined(OBJECTS_OBJMGR___SEQ_MAP_CI__INL)
#define OBJECTS_OBJMGR___SEQ_MAP_CI__INL

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
//  CSeqMap_CI


inline
const CSeqMap* CSeqMap_CI::x_GetSeqMap(void) const
{
    return m_SeqMap;
}


inline
size_t CSeqMap_CI::x_GetIndex(void) const
{
    return m_Index;
}


inline
const CSeqMap::CSegment& CSeqMap_CI::x_GetSegment(void) const
{
    return x_GetSeqMap()->x_GetSegment(x_GetIndex());
}


inline
CScope* CSeqMap_CI::GetScope(void) const
{
    return m_Scope;
}


inline
TSeqPos CSeqMap_CI::GetPosition(void) const
{
    return m_Position;
}


inline
TSeqPos CSeqMap_CI::GetLength(void) const
{
    return x_GetSeqMap()->x_GetSegmentLength(x_GetIndex(), GetScope());
}


inline
TSeqPos CSeqMap_CI::GetEndPosition(void) const
{
    return GetPosition() + GetLength();
}


inline
TSeqPos CSeqMap_CI::GetRefEndPosition(void) const
{
    return GetRefPosition() + GetLength();
}


inline
TSeqPos CSeqMap_CI::x_GetPositionInSeqMap(void) const
{
    return x_GetSeqMap()->x_GetSegmentPosition(x_GetIndex(), GetScope());
}


inline
const CSeqMap* CSeqMap_CI::x_GetParentSeqMap(void) const
{
    return x_GetSeqMap()->m_ParentSeqMap;
}


inline
size_t CSeqMap_CI::x_GetParentIndex(void) const
{
    return x_GetSeqMap()->m_ParentIndex;
}


inline
const CSeqMap* CSeqMap_CI::x_GetSubSeqMap(void) const
{
    return x_GetSeqMap()->x_GetSubSeqMap(x_GetSegment());
}


inline
bool CSeqMap_CI::operator==(const CSeqMap_CI& seg) const
{
    return
        GetPosition() == seg.GetPosition() && x_GetIndex() == seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator!=(const CSeqMap_CI& seg) const
{
    return
        GetPosition() != seg.GetPosition() || x_GetIndex() != seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator<(const CSeqMap_CI& seg) const
{
    return
        GetPosition() < seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && x_GetIndex() < seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator>(const CSeqMap_CI& seg) const
{
    return
        GetPosition() > seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && x_GetIndex() > seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator<=(const CSeqMap_CI& seg) const
{
    return
        GetPosition() < seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && x_GetIndex() <= seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator>=(const CSeqMap_CI& seg) const
{
    return
        GetPosition() > seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && x_GetIndex() >= seg.x_GetIndex();
}


inline
CSeqMap_CI& CSeqMap_CI::operator++(void)
{
    Next();
    return *this;
}


inline
CSeqMap_CI CSeqMap_CI::operator++(int)
{
    CSeqMap_CI old(*this);
    Next();
    return old;
}


inline
CSeqMap_CI& CSeqMap_CI::operator--(void)
{
    Prev();
    return *this;
}


inline
CSeqMap_CI CSeqMap_CI::operator--(int)
{
    CSeqMap_CI old(*this);
    Prev();
    return old;
}


/*
 * ===========================================================================
 * $Log$
 * Revision 1.1  2002/12/26 16:39:22  vasilche
 * Object manager class CSeqMap rewritten.
 *
 *
 * ===========================================================================
 */

#endif
