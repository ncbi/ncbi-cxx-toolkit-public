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
#include <objmgr/scope.hpp>
#include <objects/seq/seq_id_handle.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CScope;
class CSeqMap;
class CSeq_entry;

class NCBI_XOBJMGR_EXPORT CSeqMap_CI_SegmentInfo
{
public:
    CSeqMap_CI_SegmentInfo(void);
    CSeqMap_CI_SegmentInfo(const CConstRef<CSeqMap>& seqMap, size_t index);

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


struct NCBI_XOBJMGR_EXPORT SSeqMapSelector
{
    typedef CSeqMap::TFlags TFlags;

    SSeqMapSelector()
        : m_Position(0), m_Length(kInvalidSeqPos),
          m_MaxResolveCount(0),
          m_Flags(CSeqMap::fDefaultFlags)
        {
        }

    SSeqMapSelector& SetPosition(TSeqPos pos)
        {
            m_Position = pos;
            return *this;
        }

    SSeqMapSelector& SetRange(TSeqPos start, TSeqPos length)
        {
            m_Position = start;
            m_Length = length;
            return *this;
        }

    SSeqMapSelector& SetResolveCount(size_t res_cnt)
        {
            m_MaxResolveCount = res_cnt;
            return *this;
        }

    SSeqMapSelector& SetLimitTSE(const CSeq_entry* tse);
    SSeqMapSelector& SetLimitTSE(const CSeq_entry_Handle& tse);

    SSeqMapSelector& SetFlags(TFlags flags)
        {
            m_Flags = flags;
            return *this;
        }

    bool CanResolve(void) const
        {
            return m_MaxResolveCount > 0;
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

    // position of segment in whole sequence in residues
    TSeqPos             m_Position;
    // length of current segment
    TSeqPos             m_Length;
    // maximum resolution level
    size_t              m_MaxResolveCount;
    // limit search to single TSE
    CConstRef<CSeq_entry> m_TSE;
    CConstRef<CObject>  m_TSE_Info;
    // return all intermediate resolved sequences
    TFlags              m_Flags;
};


// iterator through CSeqMap
class NCBI_XOBJMGR_EXPORT CSeqMap_CI
{
public:
    typedef SSeqMapSelector::TFlags TFlags;

    CSeqMap_CI(void);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqmap,
               CScope* scope,
               TSeqPos position,
               size_t maxResolveCount = 0,
               TFlags flags = CSeqMap::fDefaultFlags);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqMap,
               CScope* scope,
               TSeqPos position,
               ENa_strand strand,
               size_t maxResolveCount,
               TFlags flags = CSeqMap::fDefaultFlags);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqmap,
               CScope* scope,
               SSeqMapSelector& selector);
    CSeqMap_CI(const CConstRef<CSeqMap>& seqmap,
               CScope* scope,
               SSeqMapSelector& selector,
               ENa_strand strand);

    ~CSeqMap_CI(void);

    bool IsInvalid(void) const;
    operator bool(void) const;

    bool operator==(const CSeqMap_CI& seg) const;
    bool operator!=(const CSeqMap_CI& seg) const;
    bool operator< (const CSeqMap_CI& seg) const;
    bool operator> (const CSeqMap_CI& seg) const;
    bool operator<=(const CSeqMap_CI& seg) const;
    bool operator>=(const CSeqMap_CI& seg) const;

    // go to next/next segment, return false if no more segments
    // if no_resolve_current == true, do not resolve current segment
    bool Next(bool resolveExternal = true);
    bool Prev(void);

    TFlags GetFlags(void) const;
    void SetFlags(TFlags flags);

    CSeqMap_CI& operator++(void);
    CSeqMap_CI& operator--(void);

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
    void x_Push(const CConstRef<CSeqMap>& seqMap,
                TSeqPos from, TSeqPos length, bool minusStrand, TSeqPos pos);
    bool x_Pop(void);

    bool x_Next(bool resolveExternal);
    bool x_Next(void);
    bool x_Prev(void);

    bool x_TopNext(void);
    bool x_TopPrev(void);

    bool x_SettleNext(void);
    bool x_SettlePrev(void);

    typedef vector<TSegmentInfo> TStack;

    // position stack
    TStack               m_Stack;
    // scope for length resolution
    CHeapScope           m_Scope;
    // iterator parameters
    SSeqMapSelector      m_Selector;
};


#include <objmgr/seq_map_ci.inl>

END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
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
