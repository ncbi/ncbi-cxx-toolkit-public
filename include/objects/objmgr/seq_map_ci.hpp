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

class NCBI_XOBJMGR_EXPORT CSeqMap_CI_SegmentInfo
{
public:
    CSeqMap_CI_SegmentInfo(void);
    CSeqMap_CI_SegmentInfo(CConstRef<CSeqMap> seqMap, size_t index);

    TSeqPos GetRefPosition(void) const;
    bool GetRefMinusStrand(void) const;

    const CSeqMap& x_GetSeqMap(void) const;
    size_t x_GetIndex(void) const;
    const CSeqMap::CSegment& x_GetSegment(void) const;
    const CSeqMap::CSegment& x_GetNextSegment(void) const;
    bool x_Move(bool minusStrand, CScope* scope);

    TSeqPos x_GetLevelRealPos(void) const;
    TSeqPos x_GetLevelRealEnd(void) const;
    TSeqPos x_GetLevelPos(void) const;
    TSeqPos x_GetLevelEnd(void) const;
    TSeqPos x_GetSkipBefore(void) const;
    TSeqPos x_GetSkipAfter(void) const;
    TSeqPos x_CalcLength(void) const;
    TSeqPos x_GetTopOffset(void) const;

private:

    // seqmap
    CConstRef<CSeqMap> m_SeqMap;
    // index of segment in seqmap
    size_t             m_Index;
    // position inside m_SeqMap
    // m_RangeEnd >= m_RangePos
    TSeqPos            m_LevelRangePos;
    TSeqPos            m_LevelRangeEnd;
    bool               m_MinusStrand;

    friend class CSeqMap_CI;
};

// iterator through CSeqMap
class NCBI_XOBJMGR_EXPORT CSeqMap_CI
{
public:
    // selector enums for CSeqMap_CI constructors
    enum EBegin    { eBegin    };
    enum EEnd      { eEnd      };
    enum EIndex    { eIndex    };
    enum EPosition { ePosition };

    CSeqMap_CI(void);
    CSeqMap_CI(CConstRef<CSeqMap> seqmap, CScope* scope,
               EIndex byIndex, size_t index,
               size_t maxResolveCount = 0);
    CSeqMap_CI(CConstRef<CSeqMap> seqmap, CScope* scope,
               EPosition byPosition, TSeqPos position,
               size_t maxResolveCount = 0);
    CSeqMap_CI(CConstRef<CSeqMap> seqmap, CScope* scope,
               EBegin toBegin,
               size_t maxResolveCount = 0);
    CSeqMap_CI(CConstRef<CSeqMap> seqmap, CScope* scope,
               EEnd toEnd,
               size_t maxResolveCount = 0);
    CSeqMap_CI(CConstRef<CSeqMap> seqmap, CScope* scope,
               TSeqPos start, TSeqPos length, ENa_strand strand,
               EBegin toBegin,
               size_t maxResolveCount = 0);
    ~CSeqMap_CI(void);

    operator bool(void) const;

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
    // will allow only regular data segments (whole, plus strand)
    const CSeq_data& GetData(void) const;
    // will allow any data segments, user should check for position and strand
    const CSeq_data& GetRefData(void) const;

    // The following function makes sense only
    // when the segment is a reference to another seq.
    CSeq_id_Handle GetRefSeqid(void) const;
    TSeqPos GetRefPosition(void) const;
    TSeqPos GetRefEndPosition(void) const;
    bool GetRefMinusStrand(void) const;

    CScope* GetScope(void) const;

    CConstRef<CSeqMap> x_GetSubSeqMap(bool resolveExternal) const;
    CConstRef<CSeqMap> x_GetSubSeqMap(void) const;

private:
    typedef CSeqMap_CI_SegmentInfo TSegmentInfo;

    const TSegmentInfo& x_GetSegmentInfo(void) const;
    TSegmentInfo& x_GetSegmentInfo(void);

    // valid iterator
    const CSeqMap& x_GetSeqMap(void) const;
    size_t x_GetIndex(void) const;
    const CSeqMap::CSegment& x_GetSegment(void) const;

    TSeqPos x_GetTopOffset(void) const;
    void x_Resolve(TSeqPos pos);
    bool x_Push(TSeqPos offset);
    void x_Push(CConstRef<CSeqMap> seqMap,
                TSeqPos from, TSeqPos length, bool minusStrand, TSeqPos pos);
    bool x_Pop(void);
    bool x_TopNext(void);
    bool x_TopPrev(void);
    bool x_StopOnIntermediateSegments(void) const;

    typedef vector<TSegmentInfo> TStack;

    // position stack
    TStack         m_Stack;
    // position of segment in whole sequence in residues
    TSeqPos        m_Position;
    // length of current segment
    TSeqPos        m_Length;
    // scope for length resolution
    CScope*        m_Scope;
    // maximum resolution level
    size_t         m_MaxResolveCount;
};


#include <objects/objmgr/seq_map_ci.inl>

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.3  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.2  2002/12/26 20:51:36  dicuccio
* Added Win32 export specifier
*
* Revision 1.1  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP_CI__HPP
