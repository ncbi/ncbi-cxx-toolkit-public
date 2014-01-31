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
#include <objects/seq/Seq_data.hpp>
#include <util/range.hpp>
#include <util/sequtil/sequtil.hpp>

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
    bool IsSetData(void) const;
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
    friend class CSeqMap;
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

    typedef CRange<TSeqPos> TRange;
    /// Set range for iterator - CRange<> version
    SSeqMapSelector& SetRange(const TRange& range)
        {
            m_Position = range.GetFrom();
            m_Length = range.GetLength();
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
    SSeqMapSelector& SetLinkUsedTSE(vector<CTSE_Handle>& used_tses)
        {
            m_LinkUsedTSE = true;
            m_UsedTSEs = &used_tses;
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

    SSeqMapSelector& SetByFeaturePolicy(void)
        {
            m_Flags |= CSeqMap::fByFeaturePolicy;
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

    void AddUsedTSE(const CTSE_Handle& tse) const;

private:
    friend class CSeqMap;
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
    // keep all used TSEs which can not be linked
    vector<CTSE_Handle>* m_UsedTSEs;
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

    /// return the depth of current segment
    size_t       GetDepth(void) const;

    /// return position of current segment in sequence
    TSeqPos      GetPosition(void) const;
    /// return length of current segment
    TSeqPos      GetLength(void) const;
    /// return true if current segment is a gap of unknown length
    bool         IsUnknownLength(void) const;
    /// return end position of current segment in sequence (exclusive)
    TSeqPos      GetEndPosition(void) const;

    CSeqMap::ESegmentType GetType(void) const;
    bool IsSetData(void) const;
    /// will allow only regular data segments (whole, plus strand)
    const CSeq_data& GetData(void) const;
    /// will allow any data segments, user should check for position and strand
    const CSeq_data& GetRefData(void) const;

    /// return CSeq_literal with gap data, or null if either the segment
    /// is not a gap, or an unspecified gap
    CConstRef<CSeq_literal> GetRefGapLiteral(void) const;

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
    friend class CSeqMap_I;

    typedef CSeqMap_CI_SegmentInfo TSegmentInfo;

    CSeqMap_CI(const CSeqMap_CI& base,
               const CSeqMap& seqmap, size_t index,
               TSeqPos pos);

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

    bool x_SettleNext(void);
    bool x_SettlePrev(void);

    void x_Select(const CConstRef<CSeqMap>& seqMap,
                  const SSeqMapSelector& selector,
                  TSeqPos pos);

    typedef vector<TSegmentInfo> TStack;

    // scope for length resolution
    CHeapScope           m_Scope;
    // position stack
    TStack               m_Stack;
    // iterator parameters
    SSeqMapSelector      m_Selector;
    // search range
    TSeqPos              m_SearchPos;
    TSeqPos              m_SearchEnd;

protected:
    void x_UpdateLength(void);
};


class CBioseq_EditHandle;

/// Non-const iterator over CSeqMap (allows to edit the sequence).
class NCBI_XOBJMGR_EXPORT CSeqMap_I : public CSeqMap_CI
{
public:
    CSeqMap_I(void);
    CSeqMap_I(const CBioseq_EditHandle& bioseq,
              const SSeqMapSelector&    selector,
              TSeqPos                   pos = 0);
    CSeqMap_I(const CBioseq_EditHandle& bioseq,
              const SSeqMapSelector&    selector,
              const CRange<TSeqPos>&    range);
    CSeqMap_I(CRef<CSeqMap>&         seqmap,
              CScope*                scope,
              const SSeqMapSelector& selector,
              TSeqPos                pos = 0);
    CSeqMap_I(CRef<CSeqMap>&         seqmap,
              CScope*                scope,
              const SSeqMapSelector& selector,
              const CRange<TSeqPos>& range);

    ~CSeqMap_I(void);

    /// Change current segment to gap.
    void SetGap(TSeqPos length, CSeq_data* gap_data = 0);
    /// Change current segment to reference.
    void SetRef(const CSeq_id_Handle& ref_id,
                TSeqPos               ref_pos,
                TSeqPos               ref_length,
                bool                  ref_minus_strand = false);
    /// Change current segment to data.
    void SetSeq_data(TSeqPos length, CSeq_data& data);

    /// Insert gap. On return the iterator points to the new segment.
    CSeqMap_I& InsertGap(TSeqPos length, CSeq_data* gap_data = 0);
    /// Insert reference. On return the iterator points to the new segment.
    CSeqMap_I& InsertRef(const CSeq_id_Handle& ref_id,
                         TSeqPos               ref_pos,
                         TSeqPos               ref_length,
                         bool                  ref_minus_strand = false);
    /// Insert data. On return the iterator points to the new segment.
    CSeqMap_I& InsertData(TSeqPos length, CSeq_data& data);
    /// Insert data segment using the sequence string and the selected coding.
    CSeqMap_I& InsertData(const string&       buffer,
                          CSeqUtil::ECoding   buffer_coding,
                          CSeq_data::E_Choice seq_data_coding);

    /// Remove current segment. On return the iterator points to the next
    /// segment or becomes invalid.
    CSeqMap_I& Remove(void);

    /// Get current sequence as a string with the selected encoding.
    /// NOTE: Some seq-data types (Gap) and encodings (Ncbipna, Ncbipaa)
    /// are not supported.
    void GetSequence(string&           buffer,
                     CSeqUtil::ECoding buffer_coding) const;
    /// Set sequence data. The buffer is converted from buffer_coding
    /// to seq_data_coding and put into the new seq-data.
    /// NOTE: Some seq-data types (Gap) and encodings (Ncbipna, Ncbipaa)
    /// are not supported.
    void SetSequence(const string&       buffer,
                     CSeqUtil::ECoding   buffer_coding,
                     CSeq_data::E_Choice seq_data_coding);

private:
    static SSeqMapSelector sx_AdjustSelector(const SSeqMapSelector& selector);

    CRef<CSeqMap> m_SeqMap;
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


inline
bool CSeqMap_CI_SegmentInfo::IsSetData(void) const
{
    return InRange() && x_GetSegment().IsSetData();
}


/////////////////////////////////////////////////////////////////////
//  CSeqMap_CI


inline
size_t CSeqMap_CI::GetDepth(void) const
{
    return m_Stack.size();
}


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
bool CSeqMap_CI::IsSetData(void) const
{
    return x_GetSegmentInfo().IsSetData();
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
        (GetPosition() == seg.GetPosition() && 
         (m_Stack.size() < seg.m_Stack.size() ||
          (m_Stack.size() == seg.m_Stack.size() &&
           x_GetIndex() < seg.x_GetIndex())));
}


inline
bool CSeqMap_CI::operator>(const CSeqMap_CI& seg) const
{
    return
        GetPosition() > seg.GetPosition() ||
        (GetPosition() == seg.GetPosition() && 
         (m_Stack.size() > seg.m_Stack.size() ||
          (m_Stack.size() == seg.m_Stack.size() &&
           x_GetIndex() > seg.x_GetIndex())));
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


/////////////////////////////////////////////////////////////////////
//  CSeqMap_I


/* @} */


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR___SEQ_MAP_CI__HPP
