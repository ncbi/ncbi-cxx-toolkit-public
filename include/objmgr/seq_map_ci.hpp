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

#include <objmgr/seq_map.hpp>
#include <objmgr/impl/heap_scope.hpp>
#include <objmgr/tse_handle.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <util/range.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CSeq_entry;
class CSeq_entry_Handle;


/** @addtogroup ObjectManagerIterators
 *
 * @{
 */


class CScope;
class CSeqMap;
class CSeq_entry;

class NCBI_XOBJMGR_EXPORT CSeqMap_CI_SegmentInfo
{
public:
    CSeqMap_CI_SegmentInfo(void);

    TSeqPos GetRefPosition(void) const;
    bool GetRefMinusStrand(void) const;

    const CSeqMap& x_GetSeqMap(void) const;
    size_t x_GetIndex(void) const;
    const CSeqMap::CSegment& x_GetSegment(void) const;
    const CSeqMap::CSegment& x_GetNextSegment(void) const;

    bool InRange(void) const;
    CSeqMap::ESegmentType GetType(void) const;
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
    CTSE_Handle        m_TSE;
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


/// Selector used in CSeqMap methods returning iterators.
struct NCBI_XOBJMGR_EXPORT SSeqMapSelector
{
    typedef CSeqMap::TFlags TFlags;

    SSeqMapSelector(void);
    SSeqMapSelector(TFlags flags, size_t resolve_count = 0);

    /// Find segment containing the position
    SSeqMapSelector& SetPosition(TSeqPos pos)
        {
            m_Position = pos;
            return *this;
        }

    /// Set range for iterator
    SSeqMapSelector& SetRange(TSeqPos start, TSeqPos length)
        {
            m_Position = start;
            m_Length = length;
            return *this;
        }

    /// Set strand to iterate over
    SSeqMapSelector& SetStrand(ENa_strand strand)
        {
            m_MinusStrand = IsReverse(strand);
            return *this;
        }

    /// Set max depth of resolving seq-map
    SSeqMapSelector& SetResolveCount(size_t res_cnt)
        {
            m_MaxResolveCount = res_cnt;
            return *this;
        }

    SSeqMapSelector& SetLinkUsedTSE(bool link = true)
        {
            m_LinkUsedTSE = link;
            return *this;
        }
    SSeqMapSelector& SetLinkUsedTSE(const CTSE_Handle& top_tse)
        {
            m_LinkUsedTSE = true;
            m_TopTSE = top_tse;
            return *this;
        }

    /// Limit TSE to resolve references
    SSeqMapSelector& SetLimitTSE(const CSeq_entry_Handle& tse);

    /// Select segment type(s)
    SSeqMapSelector& SetFlags(TFlags flags)
        {
            m_Flags = flags;
            return *this;
        }

    size_t GetResolveCount(void) const
        {
            return m_MaxResolveCount;
        }
    bool CanResolve(void) const
        {
            return GetResolveCount() > 0;
        }

    void PushResolve(void)
        {
            _ASSERT(CanResolve());
            --m_MaxResolveCount;
        }
    void PopResolve(void)
        {
            ++m_MaxResolveCount;
            _ASSERT(CanResolve());
        }

private:
    friend class CSeqMap_CI;

    bool x_HasLimitTSE(void) const
        {
            return m_LimitTSE;
        }
    const CTSE_Handle& x_GetLimitTSE(CScope* scope = 0) const;

    // position of segment in whole sequence in residues
    TSeqPos             m_Position;
    // length of current segment
    TSeqPos             m_Length;
    // Requested strand
    bool                m_MinusStrand;
    // Link segment bioseqs to master
    bool                m_LinkUsedTSE;
    CTSE_Handle         m_TopTSE;
    // maximum resolution level
    size_t              m_MaxResolveCount;
    // limit search to single TSE
    CTSE_Handle         m_LimitTSE;
    // return all intermediate resolved sequences
    TFlags              m_Flags;
};


/// Iterator over CSeqMap
class NCBI_XOBJMGR_EXPORT CSeqMap_CI
{
public:
    typedef SSeqMapSelector::TFlags TFlags;

    CSeqMap_CI(void);
    CSeqMap_CI(const CBioseq_Handle&     bioseq,
               const SSeqMapSelector&    selector,
               TSeqPos                   pos = 0);
    CSeqMap_CI(const CBioseq_Handle&     bioseq,
               const SSeqMapSelector&    selector,
               const CRange<TSeqPos>&    range);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqmap,
               CScope*                   scope,
               const SSeqMapSelector&    selector,
               TSeqPos                   pos = 0);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqmap,
               CScope*                   scope,
               const SSeqMapSelector&    selector,
               const CRange<TSeqPos>&    range);

    ~CSeqMap_CI(void);

    bool IsInvalid(void) const;
    bool IsValid(void) const;

    DECLARE_OPERATOR_BOOL(IsValid());

    bool operator==(const CSeqMap_CI& seg) const;
    bool operator!=(const CSeqMap_CI& seg) const;
    bool operator< (const CSeqMap_CI& seg) const;
    bool operator> (const CSeqMap_CI& seg) const;
    bool operator<=(const CSeqMap_CI& seg) const;
    bool operator>=(const CSeqMap_CI& seg) const;

    /// go to next/next segment, return false if no more segments
    /// if no_resolve_current == true, do not resolve current segment
    bool Next(bool resolveExternal = true);
    bool Prev(void);

    TFlags GetFlags(void) const;
    void SetFlags(TFlags flags);

    CSeqMap_CI& operator++(void);
    CSeqMap_CI& operator--(void);

    /// return position of current segment in sequence
    TSeqPos      GetPosition(void) const;
    /// return length of current segment
    TSeqPos      GetLength(void) const;
    /// return true if current segment is a gap of unknown length
    bool         IsUnknownLength(void) const;
    /// return end position of current segment in sequence (exclusive)
    TSeqPos      GetEndPosition(void) const;

    CSeqMap::ESegmentType GetType(void) const;
    /// will allow only regular data segments (whole, plus strand)
    const CSeq_data& GetData(void) const;
    /// will allow any data segments, user should check for position and strand
    const CSeq_data& GetRefData(void) const;

    /// The following function makes sense only
    /// when the segment is a reference to another seq.
    CSeq_id_Handle GetRefSeqid(void) const;
    TSeqPos GetRefPosition(void) const;
    TSeqPos GetRefEndPosition(void) const;
    bool GetRefMinusStrand(void) const;

    CScope* GetScope(void) const;

    const CTSE_Handle& GetUsingTSE(void) const;

private:
    friend class CSeqMap;
    typedef CSeqMap_CI_SegmentInfo TSegmentInfo;

    const TSegmentInfo& x_GetSegmentInfo(void) const;
    TSegmentInfo& x_GetSegmentInfo(void);

    // Check if the current reference can be resolved in the TSE
    // set by selector
    bool x_RefTSEMatch(const CSeqMap::CSegment& seg) const;
    bool x_CanResolve(const CSeqMap::CSegment& seg) const;

    // valid iterator
    const CSeqMap& x_GetSeqMap(void) const;
    size_t x_GetIndex(void) const;
    const CSeqMap::CSegment& x_GetSegment(void) const;

    TSeqPos x_GetTopOffset(void) const;
    void x_Resolve(TSeqPos pos);

    bool x_Found(void) const;

    bool x_Push(TSeqPos offset, bool resolveExternal);
    bool x_Push(TSeqPos offset);
    void x_Push(const CConstRef<CSeqMap>& seqMap, const CTSE_Handle& tse,
                TSeqPos from, TSeqPos length, bool minusStrand, TSeqPos pos);
    bool x_Pop(void);

    bool x_Next(bool resolveExternal);
    bool x_Next(void);
    bool x_Prev(void);

    bool x_TopNext(void);
    bool x_TopPrev(void);

    bool x_SettleNext(TSeqPos end_pos = kInvalidSeqPos);
    bool x_SettlePrev(void);

    void x_Select(const CConstRef<CSeqMap>& seqMap,
                  const SSeqMapSelector& selector,
                  TSeqPos pos,
                  TSeqPos end_pos);

    typedef vector<TSegmentInfo> TStack;

    // scope for length resolution
    CHeapScope           m_Scope;
    // position stack
    TStack               m_Stack;
    // iterator parameters
    SSeqMapSelector      m_Selector;
};


/////////////////////////////////////////////////////////////////////
//  CSeqMap_CI_SegmentInfo


inline
const CSeqMap& CSeqMap_CI_SegmentInfo::x_GetSeqMap(void) const
{
    return *m_SeqMap;
}


inline
size_t CSeqMap_CI_SegmentInfo::x_GetIndex(void) const
{
    return m_Index;
}


inline
const CSeqMap::CSegment& CSeqMap_CI_SegmentInfo::x_GetSegment(void) const
{
    return x_GetSeqMap().x_GetSegment(x_GetIndex());
}


inline
CSeqMap_CI_SegmentInfo::CSeqMap_CI_SegmentInfo(void)
    : m_Index(kInvalidSeqPos),
      m_LevelRangePos(kInvalidSeqPos), m_LevelRangeEnd(kInvalidSeqPos)
{
}



inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelRealPos(void) const
{
    return x_GetSegment().m_Position;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelRealEnd(void) const
{
    const CSeqMap::CSegment& seg = x_GetSegment();
    return seg.m_Position + seg.m_Length;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelPos(void) const
{
    return max(m_LevelRangePos, x_GetLevelRealPos());
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetLevelEnd(void) const
{
    return min(m_LevelRangeEnd, x_GetLevelRealEnd());
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetSkipBefore(void) const
{
    TSignedSeqPos skip = m_LevelRangePos - x_GetLevelRealPos();
    if ( skip < 0 )
        skip = 0;
    return skip;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_GetSkipAfter(void) const
{
    TSignedSeqPos skip = x_GetLevelRealEnd() - m_LevelRangeEnd;
    if ( skip < 0 )
        skip = 0;
    return skip;
}


inline
TSeqPos CSeqMap_CI_SegmentInfo::x_CalcLength(void) const
{
    return x_GetLevelEnd() - x_GetLevelPos();
}


inline
bool CSeqMap_CI_SegmentInfo::GetRefMinusStrand(void) const
{
    return x_GetSegment().m_RefMinusStrand ^ m_MinusStrand;
}


inline
bool CSeqMap_CI_SegmentInfo::InRange(void) const
{
    const CSeqMap::CSegment& seg = x_GetSegment();
    return seg.m_Position < m_LevelRangeEnd &&
        seg.m_Position + seg.m_Length > m_LevelRangePos;
}


inline
CSeqMap::ESegmentType CSeqMap_CI_SegmentInfo::GetType(void) const
{
    return InRange()?
        CSeqMap::ESegmentType(x_GetSegment().m_SegType): CSeqMap::eSeqEnd;
}


/////////////////////////////////////////////////////////////////////
//  CSeqMap_CI


inline
const CSeqMap_CI::TSegmentInfo& CSeqMap_CI::x_GetSegmentInfo(void) const
{
    return m_Stack.back();
}


inline
CSeqMap_CI::TSegmentInfo& CSeqMap_CI::x_GetSegmentInfo(void)
{
    return m_Stack.back();
}


inline
const CSeqMap& CSeqMap_CI::x_GetSeqMap(void) const
{
    return x_GetSegmentInfo().x_GetSeqMap();
}


inline
size_t CSeqMap_CI::x_GetIndex(void) const
{
    return x_GetSegmentInfo().x_GetIndex();
}


inline
const CSeqMap::CSegment& CSeqMap_CI::x_GetSegment(void) const
{
    return x_GetSegmentInfo().x_GetSegment();
}


inline
CScope* CSeqMap_CI::GetScope(void) const
{
    return m_Scope.GetScopeOrNull();
}


inline
CSeqMap::ESegmentType CSeqMap_CI::GetType(void) const
{
    return x_GetSegmentInfo().GetType();
}


inline
TSeqPos CSeqMap_CI::GetPosition(void) const
{
    return m_Selector.m_Position;
}


inline
TSeqPos CSeqMap_CI::GetLength(void) const
{
    return m_Selector.m_Length;
}


inline
TSeqPos CSeqMap_CI::GetEndPosition(void) const
{
    return m_Selector.m_Position + m_Selector.m_Length;
}


inline
bool CSeqMap_CI::IsInvalid(void) const
{
    return m_Stack.empty();
}


inline
bool CSeqMap_CI::IsValid(void) const
{
    return !m_Stack.empty()  &&  m_Stack.front().InRange()  &&
        m_Stack.front().GetType() != CSeqMap::eSeqEnd;
}


inline
TSeqPos CSeqMap_CI::GetRefPosition(void) const
{
    return x_GetSegmentInfo().GetRefPosition();
}


inline
bool CSeqMap_CI::GetRefMinusStrand(void) const
{
    return x_GetSegmentInfo().GetRefMinusStrand();
}


inline
TSeqPos CSeqMap_CI::GetRefEndPosition(void) const
{
    return GetRefPosition() + GetLength();
}


inline
bool CSeqMap_CI::operator==(const CSeqMap_CI& seg) const
{
    return
        GetPosition() == seg.GetPosition() &&
        m_Stack.size() == seg.m_Stack.size() &&
        x_GetIndex() == seg.x_GetIndex();
}


inline
bool CSeqMap_CI::operator<(const CSeqMap_CI& seg) const
{
    return
        GetPosition() < seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && 
        (m_Stack.size() < seg.m_Stack.size() ||
         m_Stack.size() == seg.m_Stack.size() && x_GetIndex() < seg.x_GetIndex());
}


inline
bool CSeqMap_CI::operator>(const CSeqMap_CI& seg) const
{
    return
        GetPosition() > seg.GetPosition() ||
        GetPosition() == seg.GetPosition() && 
        (m_Stack.size() > seg.m_Stack.size() ||
         m_Stack.size() == seg.m_Stack.size() && x_GetIndex() > seg.x_GetIndex());
}


inline
bool CSeqMap_CI::operator!=(const CSeqMap_CI& seg) const
{
    return !(*this == seg);
}


inline
bool CSeqMap_CI::operator<=(const CSeqMap_CI& seg) const
{
    return !(*this > seg);
}


inline
bool CSeqMap_CI::operator>=(const CSeqMap_CI& seg) const
{
    return !(*this < seg);
}


inline
CSeqMap_CI& CSeqMap_CI::operator++(void)
{
    Next();
    return *this;
}


inline
CSeqMap_CI& CSeqMap_CI::operator--(void)
{
    Prev();
    return *this;
}


inline
CSeqMap_CI::TFlags CSeqMap_CI::GetFlags(void) const
{
    return m_Selector.m_Flags;
}


inline
const CTSE_Handle& CSeqMap_CI::GetUsingTSE(void) const
{
    return x_GetSegmentInfo().m_TSE;
}


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.28  2006/06/01 15:25:33  vasilche
* Fixed limit range test.
*
* Revision 1.27  2006/06/01 13:51:42  vasilche
* Added limiting range argument to CSeqMap_CI constructor.
*
* Revision 1.26  2005/12/15 21:32:51  vasilche
* Fixed argument type EFlags -> TFlags.
*
* Revision 1.25  2005/12/15 19:15:19  vasilche
* Added CSeqMap::fFindExactLevel flag.
*
* Revision 1.24  2005/06/29 16:10:10  vasilche
* Removed declarations of obsolete methods.
*
* Revision 1.23  2005/01/24 17:09:36  vasilche
* Safe boolean operators.
*
* Revision 1.22  2005/01/06 16:41:31  grichenk
* Removed deprecated methods
*
* Revision 1.21  2004/12/22 15:56:16  vasilche
* Added CTSE_Handle.
* Allow used TSE linking.
*
* Revision 1.20  2004/12/14 17:41:02  grichenk
* Reduced number of CSeqMap::FindResolved() methods, simplified
* BeginResolved and EndResolved. Marked old methods as deprecated.
*
* Revision 1.19  2004/11/22 16:04:47  grichenk
* Added IsUnknownLength()
*
* Revision 1.18  2004/10/14 17:43:12  vasilche
* Use one initialization method in all constructors.
*
* Revision 1.17  2004/10/01 19:52:50  kononenk
* Added doxygen formatting
*
* Revision 1.16  2004/09/30 15:03:41  grichenk
* Fixed segments resolving
*
* Revision 1.15  2004/08/04 14:53:26  vasilche
* Revamped object manager:
* 1. Changed TSE locking scheme
* 2. TSE cache is maintained by CDataSource.
* 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
* 4. Fixed processing of split data.
*
* Revision 1.14  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.13  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.12  2004/01/27 17:11:13  ucko
* Remove redundant forward declaration of CTSE_Info
*
* Revision 1.11  2003/11/10 18:12:09  grichenk
* Removed extra EFlags declaration from seq_map_ci.hpp
*
* Revision 1.10  2003/09/30 16:21:59  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.9  2003/08/27 21:24:16  vasilche
* Added CSeqMap_CI::IsInvalid() method.
*
* Revision 1.8  2003/07/14 21:13:22  grichenk
* Added possibility to resolve seq-map iterator withing a single TSE
* and to skip intermediate references during this resolving.
*
* Revision 1.7  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.6  2003/05/20 20:36:13  vasilche
* Added FindResolved() with strand argument.
*
* Revision 1.5  2003/05/20 15:44:37  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.4  2003/02/05 15:55:26  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
* Revision 1.3  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseq_Handle::GetSeqVector().
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
