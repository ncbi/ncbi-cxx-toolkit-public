#ifndef OBJECTS_OBJMGR___SEQ_MAP__HPP
#define OBJECTS_OBJMGR___SEQ_MAP__HPP

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
*           Aleksey Grichenko
*           Michael Kimelman
*           Andrei Gourianov
*           Eugene Vasilchenko
*
* File Description:
*   CSeqMap -- formal sequence map to describe sequence parts in general,
*   i.e. location and type only, without providing real data
*
*/

#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <corelib/ncbimtx.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


/** @addtogroup ObjectManagerSequenceRep
 *
 * @{
 */


class CBioseq;
class CDelta_seq;
class CSeq_loc;
class CSeq_point;
class CSeq_interval;
class CSeq_loc_mix;
class CSeq_loc_equiv;
class CSeq_literal;
class CSeq_data;
class CPacked_seqint;
class CPacked_seqpnt;
class CTSE_Chunk_Info;

// Provided for compatibility with old code; new code should just use TSeqPos.
typedef TSeqPos TSeqPosition;
typedef TSeqPos TSeqLength;

class CScope;
class CBioseq_Handle;
class CBioseq_Info;
class CSeqMap_CI;
class CSeqMap_CI_SegmentInfo;
class CSeqMap_Delta_seqs;
struct SSeqMapSelector;


/////////////////////////////////////////////////////////////////////////////
///
///  CSeqMap --
///
///  Formal sequence map -- to describe sequence parts in general --
///  location and type only, without providing real data

class NCBI_XOBJMGR_EXPORT CSeqMap : public CObject
{
public:
    // SeqMap segment type
    enum ESegmentType {
        eSeqGap,              ///< gap
        eSeqData,             ///< real sequence data
        eSeqSubMap,           ///< sub seqmap
        eSeqRef,              ///< reference to Bioseq
        eSeqEnd,
        eSeqChunk
    };

    typedef CSeq_inst::TMol TMol;
    typedef CSeqMap_CI const_iterator;
    
    ~CSeqMap(void);

    size_t GetSegmentsCount(void) const;

    TSeqPos GetLength(CScope* scope) const;
    TMol GetMol(void) const;

    // new interface
    /// STL style methods
    const_iterator begin(CScope* scope) const;
    const_iterator end(CScope* scope) const;

    /// NCBI style methods
    CSeqMap_CI Begin(CScope* scope) const;
    CSeqMap_CI End(CScope* scope) const;
    /// Find segment containing the position
    CSeqMap_CI FindSegment(TSeqPos pos, CScope* scope) const;

    /// Segment type flags
    enum EFlags {
        fFindData       = (1<<0),
        fFindGap        = (1<<1),
        fFindLeafRef    = (1<<2),
        fFindInnerRef   = (1<<3),
        fFindExactLevel = (1<<4),
        fIgnoreUnresolved = (1<<5),
        fByFeaturePolicy= (1<<6),
        fFindRef        = (fFindLeafRef | fFindInnerRef),
        fFindAny        = fFindData | fFindGap | fFindRef,
        fFindAnyLeaf    = fFindData | fFindGap | fFindLeafRef,
        fDefaultFlags   = fFindAnyLeaf
    };
    typedef int TFlags;

    CSeqMap_CI BeginResolved(CScope* scope) const;
    CSeqMap_CI BeginResolved(CScope*                scope,
                             const SSeqMapSelector& selector) const;
    CSeqMap_CI EndResolved(CScope* scope) const;
    CSeqMap_CI EndResolved(CScope*                scope,
                           const SSeqMapSelector& selector) const;
    CSeqMap_CI FindResolved(CScope*                scope,
                            TSeqPos                pos,
                            const SSeqMapSelector& selector) const;

    /// Iterate segments in the range with specified strand coordinates
    CSeqMap_CI ResolvedRangeIterator(CScope* scope,
                                     TSeqPos from,
                                     TSeqPos length,
                                     ENa_strand strand = eNa_strand_plus,
                                     size_t maxResolve = size_t(-1),
                                     TFlags flags = fDefaultFlags) const;

    bool HasSegmentOfType(ESegmentType type) const;
    size_t CountSegmentsOfType(ESegmentType type) const;

    bool CanResolveRange(CScope* scope, const SSeqMapSelector& sel) const;
    bool CanResolveRange(CScope* scope,
                         TSeqPos from,
                         TSeqPos length,
                         ENa_strand strand = eNa_strand_plus,
                         size_t maxResolve = size_t(-1),
                         TFlags flags = fDefaultFlags) const;

    // Methods used internally by other OM classes

    static CRef<CSeqMap> CreateSeqMapForBioseq(const CBioseq& seq);
    static CRef<CSeqMap> CreateSeqMapForSeq_loc(const CSeq_loc& loc,
                                                CScope* scope);
    static CConstRef<CSeqMap> GetSeqMapForSeq_loc(const CSeq_loc& loc,
                                                  CScope* scope);
    virtual CRef<CSeqMap> CloneFor(const CBioseq& seq) const;

    // copy map for editing
    CSeqMap(const CSeqMap& sm);

    void SetRegionInChunk(CTSE_Chunk_Info& chunk, TSeqPos pos, TSeqPos length);
    void LoadSeq_data(TSeqPos pos, TSeqPos len, const CSeq_data& data);

    void SetSegmentGap(const CSeqMap_CI& seg,
                       TSeqPos length);
    void SetSegmentGap(const CSeqMap_CI& seg,
                       TSeqPos length,
                       CSeq_data& gap_data);
    void SetSegmentData(const CSeqMap_CI& seg,
                        TSeqPos length,
                        CSeq_data& data);
    void SetSegmentRef(const CSeqMap_CI& seg,
                       TSeqPos length,
                       const CSeq_id_Handle& ref_id,
                       TSeqPos ref_pos,
                       bool ref_minus_strand);
    /// Insert new gap into sequence map.
    /// @param seg
    ///   Iterator pointing to the place where new gap will be inserted.
    ///   Becomes invalid after the call.
    /// @return
    ///   New iterator pointing to the new segment.
    CSeqMap_CI InsertSegmentGap(const CSeqMap_CI& seg,
                                TSeqPos length);
    /// Delete segment from sequence map.
    /// @param seg
    ///   Iterator pointing to the segment to be deleted.
    ///   Becomes invalid after the call.
    /// @return
    ///   New iterator pointing to the next segment.
    CSeqMap_CI RemoveSegment(const CSeqMap_CI& seg);

    void SetRepr(CSeq_inst::TRepr repr);
    void ResetRepr(void);
    void SetMol(CSeq_inst::TMol mol);
    void ResetMol(void);

    /// Returns true if there is zero-length gap at position.
    /// Checks referenced sequences too.
    /// @param pos
    ///   Sequence position to check
    /// @param scope
    ///   Optional scope for segments resolution
    bool HasZeroGapAt(TSeqPos pos, CScope* scope = 0) const;

protected:

    class CSegment;
    class SPosLessSegment;

    friend class CSegment;
    friend class SPosLessSegment;
    friend class CSeqMap_SeqPoss;
    friend class CBioseq_Info;

    class CSegment
    {
    public:
        CSegment(ESegmentType seg_type = eSeqEnd,
                 TSeqPos length = kInvalidSeqPos,
                 bool unknown_len = false);

        // Check if this segment has CSeq_data object (may be gap)
        bool IsSetData(void) const;

        // Relative position of the segment in seqmap
        mutable TSeqPos      m_Position;
        // Length of the segment (kInvalidSeqPos if unresolved)
        mutable TSeqPos      m_Length;
        bool                 m_UnknownLength;

        // Segment type
        char                 m_SegType;
        char                 m_ObjType;

        // reference info, valid for eSeqData, eSeqSubMap, eSeqRef
        bool                 m_RefMinusStrand;
        TSeqPos              m_RefPosition;
        CConstRef<CObject>   m_RefObject; // CSeq_data, CSeqMap, CSeq_id
    };

    class SPosLessSegment
    {
    public:
        bool operator()(TSeqPos pos, const CSegment& seg)
            {
                return pos < seg.m_Position + seg.m_Length;
            }
        bool operator()(const CSegment& seg, TSeqPos pos)
            {
                return seg.m_Position + seg.m_Length < pos;
            }
        bool operator()(const CSegment& seg1, const CSegment& seg2)
            {
                return seg1.m_Position + seg1.m_Length < seg2.m_Position + seg2.m_Length;
            }
    };

    // 'ctors
    CSeqMap(CSeqMap* parent, size_t index);
    CSeqMap(void);
    CSeqMap(const CSeq_loc& ref);
    CSeqMap(TSeqPos len); // gap
    CSeqMap(const CSeq_inst& inst);

    void x_AddEnd(void);
    void x_AddSegment(ESegmentType type,
                      TSeqPos      len,
                      bool         unknown_len = false);
    void x_AddSegment(ESegmentType type, TSeqPos len, const CObject* object);
    void x_AddSegment(ESegmentType type, const CObject* object,
                      TSeqPos refPos, TSeqPos len,
                      ENa_strand strand = eNa_strand_plus);
    void x_AddGap(TSeqPos len, bool unknown_len);
    void x_AddGap(TSeqPos len, bool unknown_len, const CSeq_data& gap_data);
    void x_Add(CSeqMap* submap);
    void x_Add(const CSeq_data& data, TSeqPos len);
    void x_Add(const CPacked_seqint& seq);
    void x_Add(const CPacked_seqpnt& seq);
    void x_Add(const CSeq_loc_mix& seq);
    void x_Add(const CSeq_loc_equiv& seq);
    void x_Add(const CSeq_literal& seq);
    void x_Add(const CDelta_seq& seq);
    void x_Add(const CSeq_loc& seq);
    void x_Add(const CSeq_id& seq);
    void x_Add(const CSeq_point& seq);
    void x_Add(const CSeq_interval& seq);
    void x_AddUnloadedSeq_data(TSeqPos len);

private:
    void ResolveAll(void) const;
    
private:
    // Prohibit copy operator
    CSeqMap& operator= (const CSeqMap&);
    
protected:    
    // interface for iterators
    size_t x_GetLastEndSegmentIndex(void) const;
    size_t x_GetFirstEndSegmentIndex(void) const;

    const CSegment& x_GetSegment(size_t index) const;
    void x_GetSegmentException(size_t index) const;
    CSegment& x_SetSegment(size_t index);

    size_t x_FindSegment(TSeqPos position, CScope* scope) const;
    
    TSeqPos x_GetSegmentLength(size_t index, CScope* scope) const;
    TSeqPos x_GetSegmentPosition(size_t index, CScope* scope) const;
    TSeqPos x_GetSegmentEndPosition(size_t index, CScope* scope) const;
    TSeqPos x_ResolveSegmentLength(size_t index, CScope* scope) const;
    TSeqPos x_ResolveSegmentPosition(size_t index, CScope* scope) const;

    void x_StartEditing(void);
    bool x_IsChanged(void) const;
    void x_SetChanged(size_t index);
    bool x_UpdateSeq_inst(CSeq_inst& inst);
    virtual bool x_DoUpdateSeq_inst(CSeq_inst& inst);

    CBioseq_Handle x_GetBioseqHandle(const CSegment& seg, CScope* scope) const;

    CConstRef<CSeqMap> x_GetSubSeqMap(const CSegment& seg, CScope* scope,
                                      bool resolveExternal = false) const;
    virtual const CSeq_data& x_GetSeq_data(const CSegment& seg) const;
    virtual const CSeq_id& x_GetRefSeqid(const CSegment& seg) const;
    virtual TSeqPos x_GetRefPosition(const CSegment& seg) const;
    virtual bool x_GetRefMinusStrand(const CSegment& seg) const;
    
    void x_LoadObject(const CSegment& seg) const;
    CRef<CTSE_Chunk_Info> x_GetChunkToLoad(const CSegment& seg) const;
    const CObject* x_GetObject(const CSegment& seg) const;
    void x_SetObject(CSegment& seg, const CObject& obj);
    void x_SetChunk(CSegment& seg, CTSE_Chunk_Info& chunk);

    virtual void x_SetSeq_data(size_t index, CSeq_data& data);
    virtual void x_SetSubSeqMap(size_t index, CSeqMap_Delta_seqs* subMap);

    virtual void x_SetSegmentGap(size_t index,
                                 TSeqPos length,
                                 CSeq_data* gap_data = 0);
    virtual void x_SetSegmentData(size_t index,
                                  TSeqPos length,
                                  CSeq_data& data);
    virtual void x_SetSegmentRef(size_t index,
                                 TSeqPos length,
                                 const CSeq_id& ref_id,
                                 TSeqPos ref_pos,
                                 bool ref_minus_strand);

    CBioseq_Info*    m_Bioseq;

    typedef vector<CSegment> TSegments;
    
    // segments in this seqmap
    vector<CSegment> m_Segments;
    
    // index of last resolved segment position
    mutable size_t   m_Resolved;
    
    // representation object of the sequence
    CRef<CObject>    m_Delta;

    // Molecule type from seq-inst
    TMol    m_Mol;

    // segments' flags
    typedef Uint1 THasSegments;
    mutable THasSegments m_HasSegments;
    // needs to update Seq-inst
    typedef bool TChanged;
    TChanged m_Changed;

    // Sequence length
    mutable TSeqPos m_SeqLength;

    // MT-protection
    mutable CMutex  m_SeqMap_Mtx;
    
    friend class CSeqMap_CI;
    friend class CSeqMap_CI_SegmentInfo;
};


/////////////////////////////////////////////////////////////////////
//  CSeqMap: inline methods

inline
bool CSeqMap::CSegment::IsSetData(void) const
{
    return static_cast<ESegmentType>(m_SegType) == CSeqMap::eSeqData 
        || static_cast<ESegmentType>(m_ObjType) == CSeqMap::eSeqData;
}


inline
size_t CSeqMap::GetSegmentsCount(void) const
{
    return m_Segments.size() - 2;
}


inline
size_t CSeqMap::x_GetLastEndSegmentIndex(void) const
{
    return m_Segments.size() - 1;
}


inline
size_t CSeqMap::x_GetFirstEndSegmentIndex(void) const
{
    return 0;
}


inline
const CSeqMap::CSegment& CSeqMap::x_GetSegment(size_t index) const
{
    _ASSERT(index < m_Segments.size());
    return m_Segments[index];
}


inline
TSeqPos CSeqMap::x_GetSegmentPosition(size_t index, CScope* scope) const
{
    if ( index <= m_Resolved )
        return m_Segments[index].m_Position;
    return x_ResolveSegmentPosition(index, scope);
}


inline
TSeqPos CSeqMap::x_GetSegmentLength(size_t index, CScope* scope) const
{
    TSeqPos length = x_GetSegment(index).m_Length;
    if ( length == kInvalidSeqPos ) {
        length = x_ResolveSegmentLength(index, scope);
    }
    return length;
}


inline
TSeqPos CSeqMap::x_GetSegmentEndPosition(size_t index, CScope* scope) const
{
    return x_GetSegmentPosition(index, scope)+x_GetSegmentLength(index, scope);
}


inline
TSeqPos CSeqMap::GetLength(CScope* scope) const
{
    if (m_SeqLength == kInvalidSeqPos) {
        m_SeqLength = x_GetSegmentPosition(x_GetLastEndSegmentIndex(), scope);
    }
    return m_SeqLength;
}


inline
CSeqMap::TMol CSeqMap::GetMol(void) const
{
    return m_Mol;
}


inline
bool CSeqMap::x_IsChanged(void) const
{
    return m_Changed;
}


inline
bool CSeqMap::x_UpdateSeq_inst(CSeq_inst& inst)
{
    if ( !x_IsChanged() ) {
        return false;
    }
    m_Changed = false;
    return x_DoUpdateSeq_inst(inst);
}


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR___SEQ_MAP__HPP
