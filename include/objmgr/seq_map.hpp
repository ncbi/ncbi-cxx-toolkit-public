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
        fFindData     = (1<<0),
        fFindGap      = (1<<1),
        fFindLeafRef  = (1<<2),
        fFindInnerRef = (1<<3),
        fFindRef      = (fFindLeafRef | fFindInnerRef),
        fFindAny      = fFindData | fFindGap | fFindRef,
        fFindAnyLeaf  = fFindData | fFindGap | fFindLeafRef,
        fDefaultFlags = fFindAnyLeaf
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

    static CConstRef<CSeqMap> CreateSeqMapForBioseq(const CBioseq& seq);
    static CConstRef<CSeqMap> CreateSeqMapForSeq_loc(const CSeq_loc& loc,
                                                     CScope* scope);
    static CConstRef<CSeqMap> CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
                                                    ENa_strand strand);

    static TSeqPos ResolveBioseqLength(const CSeq_id& id, CScope* scope);

    void SetRegionInChunk(CTSE_Chunk_Info& chunk, TSeqPos pos, TSeqPos length);
    void LoadSeq_data(TSeqPos pos, TSeqPos len, const CSeq_data& data);

protected:

    class CSegment;
    class SPosLessSegment;

    friend class CSegment;
    friend class SPosLessSegment;
    friend class CSeqMap_SeqPoss;

    class CSegment
    {
    public:
        CSegment(ESegmentType seg_type = eSeqEnd,
                 TSeqPos length = kInvalidSeqPos,
                 bool unknown_len = false);

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

        typedef list<TSeqPos>::iterator TList0_I;
        TList0_I m_Iterator;
    };

    class SPosLessSegment
    {
    public:
        bool operator()(TSeqPos pos, const CSegment& seg)
            {
                return pos < seg.m_Position + seg.m_Length;
            }
    };

    // 'ctors
    CSeqMap(CSeqMap* parent, size_t index);
    CSeqMap(void);
    CSeqMap(const CSeq_loc& ref);
    CSeqMap(TSeqPos len); // gap

    void x_AddEnd(void);
    CSegment& x_AddSegment(ESegmentType type,
                           TSeqPos      len,
                           bool         unknown_len = false);
    CSegment& x_AddSegment(ESegmentType type, TSeqPos len, const CObject* object);
    CSegment& x_AddSegment(ESegmentType type, const CObject* object,
                           TSeqPos refPos, TSeqPos len,
                           ENa_strand strand = eNa_strand_plus);
    CSegment& x_AddGap(TSeqPos len, bool unknown_len);
    CSegment& x_Add(CSeqMap* submap);
    CSegment& x_Add(const CSeq_data& data, TSeqPos len);
    CSegment& x_Add(const CPacked_seqint& seq);
    CSegment& x_Add(const CPacked_seqpnt& seq);
    CSegment& x_Add(const CSeq_loc_mix& seq);
    CSegment& x_Add(const CSeq_loc_equiv& seq);
    CSegment& x_Add(const CSeq_literal& seq);
    CSegment& x_Add(const CDelta_seq& seq);
    CSegment& x_Add(const CSeq_loc& seq);
    CSegment& x_Add(const CSeq_id& seq);
    CSegment& x_Add(const CSeq_point& seq);
    CSegment& x_Add(const CSeq_interval& seq);
    CSegment& x_AddUnloadedSubMap(TSeqPos len);
    CSegment& x_AddUnloadedSeq_data(TSeqPos len);

private:
    void ResolveAll(void) const;
    
private:
    // Prohibit copy operator and constructor
    CSeqMap(const CSeqMap&);
    CSeqMap& operator= (const CSeqMap&);
    
protected:    
    // interface for iterators
    size_t x_GetSegmentsCount(void) const;
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
    CBioseq_Handle x_GetBioseqHandle(const CSegment& seg, CScope* scope) const;

    CConstRef<CSeqMap> x_GetSubSeqMap(const CSegment& seg, CScope* scope,
                                      bool resolveExternal = false) const;
    virtual const CSeq_data& x_GetSeq_data(const CSegment& seg) const;
    virtual const CSeq_id& x_GetRefSeqid(const CSegment& seg) const;
    virtual TSeqPos x_GetRefPosition(const CSegment& seg) const;
    virtual bool x_GetRefMinusStrand(const CSegment& seg) const;
    
    void x_LoadObject(const CSegment& seg) const;
    const CObject* x_GetObject(const CSegment& seg) const;
    void x_SetObject(const CSegment& seg, const CObject& obj);
    void x_SetChunk(const CSegment& seg, CTSE_Chunk_Info& chunk);

    virtual void x_SetSeq_data(size_t index, CSeq_data& data);
    virtual void x_SetSubSeqMap(size_t index, CSeqMap_Delta_seqs* subMap);

    typedef vector<CSegment> TSegments;
    
    // segments in this seqmap
    vector<CSegment> m_Segments;
    
    // index of last resolved segment position
    mutable size_t   m_Resolved;
    
    CRef<CObject>    m_Delta;

    // Molecule type from seq-inst
    TMol    m_Mol;
    // Sequence length
    mutable TSeqPos m_SeqLength;

    // MT-protection
    mutable CFastMutex   m_SeqMap_Mtx;
    
    friend class CSeqMap_CI;
    friend class CSeqMap_CI_SegmentInfo;
};


/////////////////////////////////////////////////////////////////////
//  CSeqMap: inline methods

inline
size_t CSeqMap::x_GetSegmentsCount(void) const
{
    return m_Segments.size() - 1;
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
        m_SeqLength = x_GetSegmentPosition(x_GetSegmentsCount(), scope);
    }
    return m_SeqLength;
}


inline
CSeqMap::TMol CSeqMap::GetMol(void) const
{
    return m_Mol;
}


/* @} */

END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_OBJMGR___SEQ_MAP__HPP
