#if 0
#ifndef OBJECTS_OBJMGR___SEQ_MAP_RCI__HPP
#define OBJECTS_OBJMGR___SEQ_MAP_RCI__HPP

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

#include <objects/objmgr/seq_map_ci.hpp>
#include <stack>
//#include <objects/objmgr/small_stack.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CSeq_id;
class CSeq_point;
class CSeq_interval;

class NCBI_XOBJMGR_EXPORT CSeqMapRange_CI
{
public:
    enum EWhole { eWhole };
    CSeqMapRange_CI(void);
    CSeqMapRange_CI(EWhole dummy, const CSeqMap_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EBegin dummy, const CSeqMap_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EBegin dummy, const CSeqMapRange_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EBegin dummy, const CSeq_id_Handle& id,
                    TSeqPos from, TSeqPos to, bool minusStrand, CScope* scope);
    CSeqMapRange_CI(CSeqMap_CI::EEnd dummy, const CSeqMap_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EEnd dummy, const CSeqMapRange_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EEnd dummy, const CSeq_id_Handle& id,
                    TSeqPos from, TSeqPos to, bool minusStrand, CScope* scope);
    CSeqMapRange_CI(CSeqMap_CI::EPosition dummy, TSeqPos position,
                    const CSeqMap_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EPosition dummy, TSeqPos position,
                    const CSeqMapRange_CI& seg);
    CSeqMapRange_CI(CSeqMap_CI::EPosition dummy, TSeqPos position,
                    const CSeq_id_Handle& id,
                    TSeqPos from, TSeqPos to, bool minusStrand, CScope* scope);
#if 0
    CSeqMapRange_CI(const CSeq_id& whole, CScope* scope);
    CSeqMapRange_CI(const CSeq_point& point, CScope* scope);
    CSeqMapRange_CI(const CSeq_interval& interval, CScope* scope);
#endif

    ~CSeqMapRange_CI(void);

    bool operator==(const CSeqMapRange_CI& seg) const;
    bool operator!=(const CSeqMapRange_CI& seg) const;
    bool operator< (const CSeqMapRange_CI& seg) const;
    bool operator> (const CSeqMapRange_CI& seg) const;
    bool operator<=(const CSeqMapRange_CI& seg) const;
    bool operator>=(const CSeqMapRange_CI& seg) const;

    // check if inside the range
    bool InRange(void) const;
    // go to next segment, return false if no more segments
    bool Next(void);
    // go to prev segment, return false if no more segments
    bool Prev(void);

    CSeqMapRange_CI& operator++(void);
    CSeqMapRange_CI operator++(int);
    CSeqMapRange_CI& operator--(void);
    CSeqMapRange_CI operator--(int);

    // return position of current segment in range of sequence
    TSeqPos      GetPosition(void) const;
    // return length of current segment in the range
    TSeqPos      GetLength(void) const;
    // return end position of current segment in the range (exclusive)
    TSeqPos      GetEndPosition(void) const;

    // Current segment will never be Seq_data as it will be presented by
    // SeqRef to current sequence.
    // This is needed to prevent creation of translated CSeq_data objects
    // by iterator.
    CSeqMap::ESegmentType GetType(void) const;
    // This method can return eSeqData.
    CSeqMap::ESegmentType GetRealType(void) const;
    const CSeq_data& GetData(void) const;
    bool ReplacedData(void) const; // return true if GetRealType() != GetType()

    // The following function makes sense only
    // when GetType() have returned eSeqRef
    CSeq_id_Handle GetRefSeqid(void) const;
    TSeqPos GetRefPosition(void) const;
    TSeqPos GetRefEndPosition(void) const;
    bool GetRefMinusStrand(void) const;
    bool IsRefData(void) const;
    const CSeq_data& GetRefData(void) const;

    CScope* GetScope(void) const;

private:
    //CBioseq_Handle x_Init(CScope* scope);
    void x_Init(CConstRef<CSeqMap> seqMap, 
                CSeqMap_CI::EBegin toBegin, CScope* scope);
    void x_Init(CConstRef<CSeqMap> seqMap, 
                CSeqMap_CI::EEnd toEnd, CScope* scope);
    void x_Init(CConstRef<CSeqMap> seqMap, 
                CSeqMap_CI::EPosition byPos, TSeqPos pos, CScope* scope);

    TSeqPos GetSegPosition(void) const;
    TSeqPos GetSegEndPosition(void) const;

    bool x_Move(bool minus);

    CSeqMap_CI     m_Segment;
    TSeqPos        m_RangePos;
    TSeqPos        m_RangeEnd;
};

class NCBI_XOBJMGR_EXPORT CSeqMapResolved_CI
{
public:
    CSeqMapResolved_CI(void);
    CSeqMapResolved_CI(const CSeqMap_CI& seg);
    CSeqMapResolved_CI(const CSeqMap_CI& seg, TSeqPos pos);
    CSeqMapResolved_CI(const pair<CSeqMap_CI, TSeqPos> pos);
    //CSeqMapResolved_CI(const CSeqMapResolved_CI& info);
    ~CSeqMapResolved_CI(void);

    //CSeqMapResolved_CI& operator=(const CSeqMapResolved_CI& info);

    bool operator==(const CSeqMapResolved_CI& seg) const;
    bool operator!=(const CSeqMapResolved_CI& seg) const;
    bool operator< (const CSeqMapResolved_CI& seg) const;
    bool operator> (const CSeqMapResolved_CI& seg) const;
    bool operator<=(const CSeqMapResolved_CI& seg) const;
    bool operator>=(const CSeqMapResolved_CI& seg) const;

    // check if pointing to valid segment
    //bool Valid(void) const;
    // go to next segment, return false if no more segments
    bool Next(void);
    // go to prev segment, return false if no more segments
    bool Prev(void);

    CSeqMapResolved_CI& operator++(void);
    CSeqMapResolved_CI operator++(int);
    CSeqMapResolved_CI& operator--(void);
    CSeqMapResolved_CI operator--(int);

    // return position of current segment in sequence
    TSeqPos GetPosition(void) const;
    // return length of current segment
    TSeqPos GetLength(void) const;
    // return end of current segment in sequence (exclusive)
    TSeqPos GetEndPosition(void) const;

    CSeqMap::ESegmentType GetType(void) const;
    const CSeq_data& GetData(void) const;

    // The following function makes sense only
    // when the segment is a reference to another seq.
    CSeq_id_Handle GetRefSeqid(void) const;
    TSeqPos GetRefPosition(void) const;
    TSeqPos GetRefEndPosition(void) const;
    bool GetRefMinusStrand(void) const;
    bool IsRefData(void) const;
    const CSeq_data& GetRefData(void) const;

private:
    void x_Init(const CSeqMap_CI& seg, TSeqPos pos);

    void x_Push(TSeqPos pos);
    bool x_Pop(void);

    bool x_IsReference(void) const;
    TSeqPos x_GetUpPosition(void) const;
    TSeqPos x_GetTopOffset(void) const;
    bool x_TopNext(void);
    bool x_TopPrev(void);

    void x_Resolve(TSeqPos pos);

    typedef stack<CSeqMapRange_CI> TStack;
    //typedef CSmallStack<CSeqMapRange_CI, 3> TStack;

    TSeqPos                m_Position;
    TStack                 m_Stack;
};


#include <objects/objmgr/seq_map_rci.inl>

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/01/23 19:32:02  vasilche
* Commented out unused code. Prepared for removal.
*
* Revision 1.3  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.2  2002/12/26 20:51:20  dicuccio
* Added Win32 export specifier.  Commented out public copy ctor / operator= (no
* implementation and none necessary)
*
* Revision 1.1  2002/12/26 16:39:22  vasilche
* Object manager class CSeqMap rewritten.
*
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP_RCI__HPP
#endif
