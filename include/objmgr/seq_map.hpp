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

#include <objmgr/seq_id_handle.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objects/seqloc/Na_strand.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <corelib/ncbimtx.hpp>
#include <vector>
#include <list>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

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


////////////////////////////////////////////////////////////////////
//  CSeqMap::
//     Formal sequence map -- to describe sequence parts in general --
//     location and type only, without providing real data


class CScope;
class CBioseq_Handle;
class CSegmentPtr;
class CSeqMap_CI;
class CSeqMap_CI_SegmentInfo;
class CSeqMap_Delta_seqs;

class NCBI_XOBJMGR_EXPORT CSeqMap : public CObject
{
public:
    // typedefs
    enum ESegmentType {
        eSeqGap,              // gap
        eSeqData,             // real sequence data
        eSeqSubMap,           // sub seqmap
        eSeqRef,              // reference to Bioseq
        eSeqEnd,
        eSeqChunk
    };

    typedef CSeq_inst::TMol TMol;

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
                 TSeqPos length = kInvalidSeqPos);

        // Relative position of the segment in seqmap
        mutable TSeqPos      m_Position;
        // Length of the segment (kInvalidSeqPos if unresolved)
        mutable TSeqPos      m_Length;

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

public:
    typedef CSeqMap_CI CSegmentInfo; // for compatibility

    typedef CSeqMap_CI const_iterator;
    typedef CSeqMap_CI TSegment_CI;
    
    ~CSeqMap(void);

    TSeqPos GetLength(CScope* scope = 0) const;
    TMol GetMol(void) const;    

    // new interface
    // STL style methods
    const_iterator begin(CScope* scope = 0) const;
    const_iterator end(CScope* scope = 0) const;
    const_iterator find(TSeqPos pos, CScope* scope = 0) const;
    // NCBI style methods
    TSegment_CI Begin(CScope* scope = 0) const;
    TSegment_CI End(CScope* scope = 0) const;
    TSegment_CI FindSegment(TSeqPos pos, CScope* scope = 0) const;

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

    // resolved iterators
    const_iterator begin_resolved(CScope* scope,
                                  size_t maxResolveCount = size_t(-1),
                                  TFlags flags = fDefaultFlags) const;
    const_iterator find_resolved(TSeqPos pos, CScope* scope,
                                 size_t maxResolveCount = size_t(-1),
                                 TFlags flags = fDefaultFlags) const;
    const_iterator find_resolved(TSeqPos pos, CScope* scope,
                                 ENa_strand strand,
                                 size_t maxResolveCount,
                                 TFlags flags) const;
    const_iterator end_resolved(CScope* scope,
                                size_t maxResolveCount = size_t(-1),
                                TFlags flags = fDefaultFlags) const;

    TSegment_CI BeginResolved(CScope* scope,
                              size_t maxResolveCount = size_t(-1),
                              TFlags flags = fDefaultFlags) const;
    TSegment_CI FindResolved(TSeqPos pos, CScope* scope,
                             size_t maxResolveCount = size_t(-1),
                             TFlags flags = fDefaultFlags) const;
    TSegment_CI FindResolved(TSeqPos pos, CScope* scope,
                             ENa_strand strand,
                             size_t maxResolveCount = size_t(-1),
                             TFlags flags = fDefaultFlags) const;
    TSegment_CI EndResolved(CScope* scope,
                            size_t maxResolveCount = size_t(-1),
                            TFlags flags = fDefaultFlags) const;

    // iterate range with plus strand coordinates
    TSegment_CI ResolvedRangeIterator(CScope* scope,
                                      TSeqPos from,
                                      TSeqPos length,
                                      ENa_strand strand = eNa_strand_plus,
                                      size_t maxResolve = size_t(-1),
                                      TFlags flags = fDefaultFlags) const;
    // iterate range with specified strand coordinates
    TSegment_CI ResolvedRangeIterator(CScope* scope,
                                      ENa_strand strand,
                                      TSeqPos from,
                                      TSeqPos length,
                                      size_t maxResolve = size_t(-1),
                                      TFlags flags = fDefaultFlags) const;
    
    bool CanResolveRange(CScope* scope,
                         TSeqPos from,
                         TSeqPos length,
                         ENa_strand strand = eNa_strand_plus) const;

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    static CConstRef<CSeqMap> CreateSeqMapForBioseq(const CBioseq& seq);
    static CConstRef<CSeqMap> CreateSeqMapForSeq_loc(const CSeq_loc& loc,
                                                     CScope* scope);
    static CConstRef<CSeqMap> CreateSeqMapForStrand(CConstRef<CSeqMap> seqMap,
                                                    ENa_strand strand);

    static TSeqPos ResolveBioseqLength(const CSeq_id& id, CScope* scope);

    void SetRegionInChunk(CTSE_Chunk_Info& chunk, TSeqPos pos, TSeqPos length);
    void LoadSeq_data(TSeqPos pos, TSeqPos len, const CSeq_data& data);

protected:
    // 'ctors
    CSeqMap(CSeqMap* parent, size_t index);
    CSeqMap(void);
    CSeqMap(const CSeq_data& data, TSeqPos len);
    CSeqMap(const CSeq_loc& ref);
    CSeqMap(TSeqPos len); // gap

    void x_AddEnd(void);
    CSegment& x_AddSegment(ESegmentType type, TSeqPos len);
    CSegment& x_AddSegment(ESegmentType type, TSeqPos len, const CObject* object);
    CSegment& x_AddSegment(ESegmentType type, const CObject* object,
                           TSeqPos refPos, TSeqPos len,
                           ENa_strand strand = eNa_strand_plus);
    CSegment& x_AddGap(TSeqPos len);
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
    const CSegment& x_GetSegment(size_t index) const;
    void x_GetSegmentException(size_t index) const;
    CSegment& x_SetSegment(size_t index);
    size_t x_FindSegment(TSeqPos position, CScope* scope) const;
    
    TSeqPos x_GetSegmentLength(size_t index, CScope* scope) const;
    TSeqPos x_GetSegmentPosition(size_t index, CScope* scope) const;
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


#include <objmgr/seq_map.inl>


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.45  2004/06/15 14:06:49  vasilche
* Added support to load split sequences.
*
* Revision 1.44  2004/03/16 15:47:26  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.43  2003/11/12 16:53:16  grichenk
* Modified CSeqMap to work with const objects (CBioseq, CSeq_loc etc.)
*
* Revision 1.42  2003/09/30 16:21:59  vasilche
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
* Revision 1.41  2003/09/05 17:29:39  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.40  2003/07/17 19:10:27  grichenk
* Added methods for seq-map and seq-vector validation,
* updated demo.
*
* Revision 1.39  2003/06/26 19:47:25  grichenk
* Added sequence length cache
*
* Revision 1.38  2003/06/24 14:22:46  vasilche
* Fixed CSeqMap constructor from CSeq_loc.
*
* Revision 1.37  2003/06/11 19:32:53  grichenk
* Added molecule type caching to CSeqMap, simplified
* coding and sequence type calculations in CSeqVector.
*
* Revision 1.36  2003/06/10 19:06:34  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
* Revision 1.35  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.34  2003/05/20 20:36:13  vasilche
* Added FindResolved() with strand argument.
*
* Revision 1.33  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.32  2003/02/05 15:55:26  vasilche
* Added eSeqEnd segment at the beginning of seq map.
* Added flags to CSeqMap_CI to stop on data, gap, or references.
*
* Revision 1.31  2003/01/28 17:16:22  vasilche
* Added CSeqMap::ResolvedRangeIterator with strand coordinate translation.
*
* Revision 1.30  2003/01/22 20:11:53  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.29  2002/12/30 19:35:47  vasilche
* Removed redundant friend declarations.
*
* Revision 1.28  2002/12/27 21:11:09  kuznets
* Fixed Windows specific syntax problem in class CSeqMap::CSegment member
* m_Iterator was an unnamed structure. For correct MSVC compilation must have
* a type name.
*
* Revision 1.27  2002/12/27 19:32:46  ucko
* Add forward declarations for nested classes so they can stay protected.
*
* Revision 1.26  2002/12/27 19:29:41  vasilche
* Fixed access to protected class on WorkShop.
*
* Revision 1.25  2002/12/27 19:04:46  vasilche
* Fixed segmentation fault on 64 bit platforms.
*
* Revision 1.24  2002/12/26 20:49:28  dicuccio
* Wrapped previous unimplemented ctor in #ifdef for MSWIN only
*
* Revision 1.22  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.21  2002/10/18 19:12:39  grichenk
* Removed mutex pools, converted most static mutexes to non-static.
* Protected CSeqMap::x_Resolve() with mutex. Modified code to prevent
* dead-locks.
*
* Revision 1.20  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.19  2002/06/30 03:27:38  vakatov
* Get rid of warnings, ident the code, move CVS logs to the end of file
*
* Revision 1.18  2002/05/29 21:19:57  gouriano
* added debug dump
*
* Revision 1.17  2002/05/21 18:40:15  grichenk
* Prohibited copy constructor and operator =()
*
* Revision 1.16  2002/05/06 03:30:36  vakatov
* OM/OM1 renaming
*
* Revision 1.15  2002/05/03 21:28:02  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.14  2002/04/30 18:54:50  gouriano
* added GetRefSeqid function
*
* Revision 1.13  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.12  2002/04/05 21:26:17  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.11  2002/04/02 16:40:53  grichenk
* Fixed literal segments handling
*
* Revision 1.10  2002/03/27 15:07:53  grichenk
* Fixed CSeqMap::CSegmentInfo::operator==()
*
* Revision 1.9  2002/03/15 18:10:05  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.8  2002/03/08 21:25:30  gouriano
* fixed errors with unresolvable references
*
* Revision 1.7  2002/02/25 21:05:26  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.6  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.4  2002/01/30 22:08:47  gouriano
* changed CSeqMap interface
*
* Revision 1.3  2002/01/23 21:59:29  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:26:37  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:04:04  gouriano
* restructured objmgr
*
* ===========================================================================
*/

#endif  // OBJECTS_OBJMGR___SEQ_MAP__HPP
