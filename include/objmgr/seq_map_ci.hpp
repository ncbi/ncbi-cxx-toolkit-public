#ifndef OBJECTS_OBJMGR___SEQ_MAP_CI__HPP
#define OBJECTS_OBJMGR___SEQ_MAP_CI__HPP

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
*   CSeqMap -- formal sequence map to describe sequence parts in general,
*   i.e. location and type only, without providing real data
*
*/

#include <objects/objmgr/seq_map.hpp>
#include <objects/objmgr/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeqMap;

// iterator through CSeqMap
class CSeqMap_CI
{
public:
    // selector enums for CSeqMap_CI constructors
    enum EBegin    { eBegin    };
    enum EEnd      { eEnd      };
    enum EIndex    { eIndex    };
    enum EPosition { ePosition };

    CSeqMap_CI(void);
    CSeqMap_CI(const CSeqMap* seqmap, CScope* scope, EIndex dummy,
               size_t index);
    CSeqMap_CI(const CSeqMap* seqmap, CScope* scope, EPosition dummy,
               TSeqPos position);
    CSeqMap_CI(const CSeqMap* seqmap, CScope* scope, EBegin dummy);
    CSeqMap_CI(const CSeqMap* seqmap, CScope* scope, EEnd dummy);
    CSeqMap_CI(const CSeqMap_CI& info);
    ~CSeqMap_CI(void);

    CSeqMap_CI& operator=(const CSeqMap_CI& info);

    bool operator==(const CSeqMap_CI& seg) const;
    bool operator!=(const CSeqMap_CI& seg) const;
    bool operator< (const CSeqMap_CI& seg) const;
    bool operator> (const CSeqMap_CI& seg) const;
    bool operator<=(const CSeqMap_CI& seg) const;
    bool operator>=(const CSeqMap_CI& seg) const;

    // go to next segment, return false if no more segments
    bool Next(void);
    // go to prev segment, return false if no more segments
    bool Prev(void);

    CSeqMap_CI& operator++(void);
    CSeqMap_CI operator++(int);
    CSeqMap_CI& operator--(void);
    CSeqMap_CI operator--(int);

    // return position of current segment in sequence
    TSeqPos      GetPosition(void) const;
    // return length of current segment
    TSeqPos      GetLength(void) const;
    // return end position of current segment in sequence (exclusive)
    TSeqPos      GetEndPosition(void) const;

    CSeqMap::ESegmentType GetType(void) const;
    const CSeq_data& GetData(void) const;

    // The following function makes sense only
    // when the segment is a reference to another seq.
    CSeq_id_Handle GetRefSeqid(void) const;
    TSeqPos GetRefPosition(void) const;
    TSeqPos GetRefEndPosition(void) const;
    bool GetRefMinusStrand(void) const;

    CScope* GetScope(void) const;

private:
    // valid iterator
    const CSeqMap* x_GetSeqMap(void) const;
    size_t x_GetIndex(void) const;
    void x_SetIndex(size_t index);
    const CSeqMap::CSegment& x_GetSegment(void) const;
    const CSeqMap* x_GetSubSeqMap(void) const;
    const CSeqMap* x_GetParentSeqMap(void) const;
    size_t x_GetParentIndex(void) const;
    TSeqPos x_GetPositionInSeqMap(void) const;

    void x_Lock(void);
    void x_Unlock(void);

    bool x_Push(void);
    bool x_Pop(void);
    void x_LocatePosition(TSeqPos pos);
    void x_LocateFirst(void);
    void x_LocateLast(void);

    // current seqmap
    const CSeqMap* m_SeqMap;
    // index of segment in current seqmap
    size_t         m_Index;
    // current position of segment in whole sequence
    TSeqPos        m_Position;
    // scope for length resolution
    CScope*        m_Scope;
};


#include <objects/objmgr/seq_map_ci.inl>

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.1  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP_CI__HPP
