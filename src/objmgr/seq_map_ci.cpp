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
* Authors:
*           Eugene Vasilchenko
*
* File Description:
*   Sequence map for the Object Manager. Describes sequence as a set of
*   segments of different types (data, reference, gap or end).
*
*/

#include <objects/objmgr/seq_map_ci.hpp>
#include <objects/objmgr/seq_map.hpp>
#include "data_source.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
// CSeqMap::CSegmentInfo


inline
void CSeqMap_CI::x_Lock(void)
{
    if ( const CSeqMap* seqMap = x_GetSeqMap() ) {
        seqMap->x_Lock();
    }
}

inline
void CSeqMap_CI::x_Unlock(void)
{
    if ( const CSeqMap* seqMap = x_GetSeqMap() ) {
        seqMap->x_Unlock();
    }
}


CSeqMap_CI::CSeqMap_CI(void)
    : m_SeqMap(0),
      m_Index(0),
      m_Position(kInvalidSeqPos),
      m_Scope(0)
{
}

#if 0
CSeqMap_CI::CSeqMap_CI(const CSeqMap* seqmap, CScope* scope,
                       EIndex /*dummy*/, size_t index)
    : m_SeqMap(seqmap),
      m_Index(index),
      m_Position(seqmap->x_GetSegmentPosition(index, scope)),
      m_Scope(scope)
{
    x_Lock();
}
#endif

CSeqMap_CI::CSeqMap_CI(const CSeqMap_CI& info)
    : m_SeqMap(info.x_GetSeqMap()),
      m_Index(info.x_GetIndex()),
      m_Position(info.GetPosition()),
      m_Scope(info.GetScope())
{
    x_Lock();
}


CSeqMap_CI::~CSeqMap_CI(void)
{
    x_Unlock();
}


CSeqMap::ESegmentType CSeqMap_CI::GetType(void) const
{
    CSeqMap::ESegmentType type = x_GetSegment().m_SegType;
    if ( type > CSeqMap::eSeqRef )
        type = CSeqMap::eSeqRef;
    return type;
}


const CSeq_data& CSeqMap_CI::GetData(void) const
{
    return x_GetSeqMap()->x_GetSeq_data(x_GetSegment());
}


CSeq_id_Handle CSeqMap_CI::GetRefSeqid(void) const
{
    _ASSERT(x_GetSeqMap()->m_Source);
    return x_GetSeqMap()->m_Source->GetIdMapper().
        GetHandle(x_GetSeqMap()->x_GetRefSeqid(x_GetSegment()));
}


TSeqPos CSeqMap_CI::GetRefPosition(void) const
{
    return x_GetSeqMap()->x_GetRefPosition(x_GetSegment());
}


bool CSeqMap_CI::GetRefMinusStrand(void) const
{
    return x_GetSeqMap()->x_GetRefMinusStrand(x_GetSegment());
}


CSeqMap_CI&
CSeqMap_CI::operator=(const CSeqMap_CI& info)
{
    if ( this != &info ) {
        x_Unlock();
        m_SeqMap = info.x_GetSeqMap();
        m_Index  = info.x_GetIndex();
        m_Position = info.GetPosition();
        m_Scope = info.GetScope();
        x_Lock();
    }
    return *this;
}


CSeqMap_CI::CSeqMap_CI(const CSeqMap* seqMap, CScope* scope,
                       EPosition /*dummy*/, TSeqPos pos)
    : m_SeqMap(seqMap),
      m_Index(0),
      m_Position(0),
      m_Scope(scope)
{
    x_Lock();
    try {
        x_LocatePosition(pos);
    }
    catch (...) {
        x_Unlock();
        throw;
    }
}


CSeqMap_CI::CSeqMap_CI(const CSeqMap* seqMap, CScope* scope,
                       EBegin /*begin*/)
    : m_SeqMap(seqMap),
      m_Index(0),
      m_Position(0),
      m_Scope(scope)
{
    x_Lock();
    try {
        x_LocateFirst();
    }
    catch (...) {
        x_Unlock();
        throw;
    }
}


CSeqMap_CI::CSeqMap_CI(const CSeqMap* seqMap, CScope* scope,
                       EEnd /*end*/)
    : m_SeqMap(seqMap),
      m_Index(seqMap->x_GetSegmentsCount()),
      m_Position(seqMap->x_GetLength(scope)),
      m_Scope(scope)
{
    x_Lock();
}


void CSeqMap_CI::x_SetIndex(size_t index)
{
    if ( index != x_GetIndex() ) {
        m_Position -= x_GetPositionInSeqMap(); // base seqmap position
        m_Index = index;
        m_Position += x_GetPositionInSeqMap(); // segment position
    }
}


bool CSeqMap_CI::x_Push(void)
{
    if ( const CSeqMap* subSeqMap = x_GetSubSeqMap() ) {
        subSeqMap->x_Lock();
        x_Unlock();
        m_SeqMap = subSeqMap;
        m_Index = 0;
        // position is not changed
        return true;
    }
    return false;
}


bool CSeqMap_CI::x_Pop(void)
{
    if ( const CSeqMap* parentSeqMap = x_GetParentSeqMap() ) {
        size_t parentIndex = x_GetParentIndex();
        x_SetIndex(0);
        parentSeqMap->x_Lock();
        x_Unlock();
        m_SeqMap = parentSeqMap;
        m_Index = parentIndex;
        // position is not changed
        return true;
    }
    return false;
}


void CSeqMap_CI::x_LocatePosition(TSeqPos pos)
{
    do {
        x_SetIndex(x_GetSeqMap()->x_FindSegment(pos-GetPosition(),
                                                GetScope()));
    } while ( x_Push() );
}


void CSeqMap_CI::x_LocateFirst(void)
{
    while ( x_Push() ) {
        x_SetIndex(0);
    }
}


void CSeqMap_CI::x_LocateLast(void)
{
    while ( x_Push() ) {
        x_SetIndex(x_GetSeqMap()->x_GetSegmentsCount()-1);
    }
}


bool CSeqMap_CI::Next(void)
{
    while ( x_GetIndex() + 1 >= x_GetSeqMap()->x_GetSegmentsCount() ) {
        if ( !x_Pop() ) {
            x_SetIndex(x_GetSeqMap()->x_GetSegmentsCount());
            return false;
        }
    }
    x_SetIndex(x_GetIndex() + 1);
    x_LocateFirst();
    return true;
}


bool CSeqMap_CI::Prev(void)
{
    while ( x_GetIndex() <= 0 ) {
        if ( !x_Pop() ) {
            x_SetIndex(0);
            return false;
        }
    }
    x_SetIndex(x_GetIndex() - 1);
    x_LocateLast();
    return true;
}


END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/12/26 16:39:24  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/
