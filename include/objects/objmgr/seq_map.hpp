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

#include <objects/objmgr/seq_id_handle.hpp>
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

// Provided for compatibility with old code; new code should just use TSeqPos.
typedef TSeqPos TSeqPosition;
typedef TSeqPos TSeqLength;


////////////////////////////////////////////////////////////////////
//  CSeqMap::
//     Formal sequence map -- to describe sequence parts in general --
//     location and type only, without providing real data


class CScope;
class CDataSource;
class CSeqMap_CI;
class CSeqMapResolved_CI;
class CSeqMap_Delta_seqs;

class NCBI_XOBJMGR_EXPORT CSeqMap : public CObject
{
public:
    // typedefs
    enum ESegmentType {
        eSeqData,             // real sequence data (m_Object: CSeq_data)
        eSeqGap,              // gap                (m_Object: null)
        eSeqSubMap,           // sub seqmap         (m_Object: CSeqMap)
        eSeqEnd,
        eSeqRef,              // virtual type
        eSeqRef_id,           // real sequence ref  (m_Object: CSeq_id)
        eSeqRef_point,        // real sequence ref  (m_Object: CSeq_point)
        eSeqRef_interval,     // real sequence ref  (m_Object: CSeq_interval)
        eSeqRef_packed_point  // real sequence ref  (m_Object: null)
    };

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

        // Segment type
        ESegmentType         m_SegType;
        // Relative position of the segment in seqmap
        mutable TSeqPos      m_Position;
        // Length of the segment (kInvalidSeqPos if unresolved)
        mutable TSeqPos      m_Length;

        typedef list<TSeqPos>::iterator TList0_I;
        
        struct ListIterator {
            TList0_I m_I;
            //char                 m_I[sizeof(TList0_I)];
        } m_Iterator;
        
        CRef<CObject>        m_Object;
    };

protected:
    friend class CSegment;
    friend class CSeqMap_SeqPoss;

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
    typedef CSeqMapResolved_CI resolved_const_iterator;
    typedef CSeqMapResolved_CI TResolvedSegment_CI;
    
    ~CSeqMap(void);
    
    // new interface
    // STL style methods
    const_iterator begin(CScope* scope = 0) const;
    const_iterator end(CScope* scope = 0) const;
    const_iterator find(TSeqPos pos, CScope* scope = 0) const;
    // NCBI style methods
    TSegment_CI Begin(CScope* scope = 0) const;
    TSegment_CI End(CScope* scope = 0) const;
    TSegment_CI FindSegment(TSeqPos pos, CScope* scope = 0) const;

    // resolved iterators
    const_iterator begin_resolved(CScope* scope) const;
    pair<const_iterator, TSeqPos> find_resolved(TSeqPos pos,
                                                CScope* scope) const;
    const_iterator end_resolved(CScope* scope) const;
    TSegment_CI BeginResolved(CScope* scope) const;
    pair<TSegment_CI, TSeqPos> FindResolvedSegment(TSeqPos pos,
                                                   CScope* scope) const;
    TSegment_CI EndResolved(CScope* scope) const;
    
    // deprecated interface
    //size_t size(void) const;
    CSeqMap_CI operator[] (size_t seg_idx) const;
    
    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

    static CSeqMap* CreateSeqMapForBioseq(CBioseq& seq,
                                          CDataSource* source = 0);
    static TSeqPos ResolveBioseqLength(const CSeq_id& id, CScope* scope);

protected:
    // 'ctors
    CSeqMap(CSeqMap* parent, size_t index);
    CSeqMap(CDataSource* source = 0);
    CSeqMap(CSeq_data& data, TSeqPos len, CDataSource* source = 0);
    CSeqMap(CSeq_loc& ref, CDataSource* source = 0);
    CSeqMap(TSeqPos len, CDataSource* source = 0); // gap

    void x_AddEnd(void);
    CSegment& x_AddSegment(ESegmentType type, TSeqPos len);
    CSegment& x_AddSegment(ESegmentType type, TSeqPos len, CObject* object);
    CSegment& x_AddGap(TSeqPos len);
    CSegment& x_Add(CSeqMap* submap);
    CSegment& x_Add(CSeq_data& data, TSeqPos len);
    CSegment& x_Add(CPacked_seqint& seq);
    CSegment& x_Add(CPacked_seqpnt& seq);
    CSegment& x_Add(CSeq_loc_mix& seq);
    CSegment& x_Add(CSeq_loc_equiv& seq);
    CSegment& x_Add(CSeq_literal& seq);
    CSegment& x_Add(CDelta_seq& seq);
    CSegment& x_Add(CSeq_loc& seq);
    CSegment& x_Add(CSeq_id& seq);
    CSegment& x_Add(CSeq_point& seq);
    CSegment& x_Add(CSeq_interval& seq);
    CSegment& x_AddUnloadedSubMap(TSeqPos len);
    CSegment& x_AddUnloadedSeq_data(TSeqPos len);

private:
    void ResolveAll(void) const;
    
    void x_SetParent(CSeqMap* parent, size_t index);

private:
    // Prohibit copy operator and constructor
#ifdef NCBI_OS_MSWIN
    CSeqMap(const CSeqMap&) { THROW1_TRACE(runtime_error, "unimplemented"); }
    CSeqMap& operator= (const CSeqMap&) { THROW1_TRACE(runtime_error, "unimplemented"); }
#else
    CSeqMap(const CSeqMap&);
    CSeqMap& operator= (const CSeqMap&);
#endif
    
protected:    
    // interface for iterators
    TSeqPos x_GetLength(CScope* scope) const;
    size_t x_GetSegmentsCount(void) const;
    const CSegment& x_GetSegment(size_t index) const;
    CSegment& x_SetSegment(size_t index);
    size_t x_FindSegment(TSeqPos position, CScope* scope) const;
    
    TSeqPos x_GetSegmentLength(size_t index, CScope* scope) const;
    TSeqPos x_GetSegmentPosition(size_t index, CScope* scope) const;
    TSeqPos x_ResolveSegmentLength(const CSegment& seg, CScope* scope) const;

    const CSeqMap* x_GetSubSeqMap(const CSegment& seg) const;
    CSeqMap* x_GetSubSeqMap(CSegment& seg);
    virtual const CSeq_data& x_GetSeq_data(const CSegment& seg) const;
    virtual const CSeq_id& x_GetRefSeqid(const CSegment& seg) const;
    virtual TSeqPos x_GetRefPosition(const CSegment& seg) const;
    virtual bool x_GetRefMinusStrand(const CSegment& seg) const;
    
    void x_LoadObject(const CSegment& seg) const;
    const CObject* x_GetObject(const CSegment& seg) const;
    CObject* x_GetObject(CSegment& seg);
    virtual void x_SetSeq_data(size_t index, CSeq_data& data);
    virtual void x_SetSubSeqMap(size_t index, CSeqMap_Delta_seqs* subMap);

    void x_Lock(void) const;
    void x_Unlock(void) const;
    
    typedef vector<CSegment> TSegments;
    
    // parent CSeqMap
    const CSeqMap*   m_ParentSeqMap;
    // index of first segment in parent sequence
    size_t           m_ParentIndex;
    
    // segments in this seqmap
    vector<CSegment> m_Segments;
    
    // index of last resolved segment position
    mutable size_t   m_Resolved;
    
    CRef<CObject>    m_Delta;
    CDataSource*     m_Source;
    
    mutable CAtomicCounter m_LockCounter; // usage lock counter

    // MT-protection
    mutable CFastMutex   m_SeqMap_Mtx;
    
    friend class CSeqMap_CI;
    friend class CDataSource;
};


#include <objects/objmgr/seq_map.inl>


END_SCOPE(objects)
END_NCBI_SCOPE

#include <objects/objmgr/seq_map_ci.hpp>

/*
* ---------------------------------------------------------------------------
* $Log$
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
