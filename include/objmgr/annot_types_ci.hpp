#ifndef ANNOT_TYPES_CI__HPP
#define ANNOT_TYPES_CI__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Object manager iterators
*
*/

#include <corelib/ncbiobj.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/annot_selector.hpp>
#include <objmgr/impl/annot_object.hpp>

#include <objects/seqloc/Na_strand.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <set>
#include <map>
#include <vector>

BEGIN_NCBI_SCOPE

class CReadLockGuard;

BEGIN_SCOPE(objects)

class CScope;
class CScope_Impl;
class CTSE_Info;
class CSeq_loc;
class CSeqMap_CI;
class CSeq_id_Handle;
class CAnnotObject_Info;
class CSeq_loc_Conversion;
class CBioseq_Handle;
class CAnnotTypes_CI;
class CHandleRangeMap;
class CHandleRange;
struct SAnnotObject_Index;
class CSeq_annot_SNP_Info;
struct SSNP_Info;
struct SIdAnnotObjs;
class CSeq_loc_Conversion;
class CAnnotName;

class NCBI_XOBJMGR_EXPORT CAnnotObject_Ref
{
public:
    typedef CRange<TSeqPos> TRange;

    CAnnotObject_Ref(void);
    CAnnotObject_Ref(const CAnnotObject_Info& object);
    CAnnotObject_Ref(const CSeq_annot_SNP_Info& snp_info, TSeqPos index);
    ~CAnnotObject_Ref(void);

    enum EObjectType {
        eType_null,
        eType_Seq_annot_Info,
        eType_Seq_annot_SNP_Info
    };

    EObjectType GetObjectType(void) const;

    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    const CSeq_annot_SNP_Info& GetSeq_annot_SNP_Info(void) const;

    const CAnnotObject_Info& GetAnnotObject_Info(void) const;
    const SSNP_Info& GetSNP_Info(void) const;

    TSeqPos GetFrom(void) const;
    TSeqPos GetToOpen(void) const;
    const TRange& GetTotalRange(void) const;

    bool IsPartial(void) const;
    int GetMappedIndex(void) const;

    const CSeq_annot& GetSeq_annot(void) const;
    bool IsFeat(void) const;
    bool IsSNPFeat(void) const;
    bool IsGraph(void) const;
    bool IsAlign(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_align& GetAlign(void) const;

    bool IsMappedLocation(int index) const;
    const CSeq_loc* GetMappedLocation(void) const;
    Uint4 GetAnnotObjectIndex(void) const;
    bool MappedNeedsUpdate(void) const;
    void UpdateMappedLocation(CRef<CSeq_loc>& loc) const;
    void UpdateMappedLocation(CRef<CSeq_loc>& loc,
                              CRef<CSeq_point>& pnt_ref,
                              CRef<CSeq_interval>& int_ref) const;

    void SetAnnotObjectRange(const TRange& range, int index);
    void SetSNP_Point(const SSNP_Info& snp, CSeq_loc_Conversion* cvt);

    void SetPartial(bool value);
    void SetMappedIndex(int index);

    void ResetLocation(void);
    bool operator<(const CAnnotObject_Ref& ref) const; // sort by object

private:
    friend class CSeq_loc_Conversion;
    friend class CSeq_loc_Conversion_Set;

    CConstRef<CObject>      m_Object;
    CRef<CSeq_loc>          m_MappedLocation; // master sequence coordinates
    TRange                  m_TotalRange;
    Uint4                   m_AnnotObject_Index;
    Uint4                   m_MappedIndex;
    Int1                    m_ObjectType; // EObjectType
    bool                    m_Partial;
    Int1                    m_MappedType;
    Int1                    m_MappedStrand;
};


class CSeq_annot_Handle;


class CAnnotDataCollector;


// Base class for specific annotation iterators
class NCBI_XOBJMGR_EXPORT CAnnotTypes_CI : public SAnnotSelector
{
public:
    CAnnotTypes_CI(void);

    // short methods
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_loc& loc,
                   const SAnnotSelector& params);
    CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   const SAnnotSelector& params);
    // Iterate everything from the seq-annot
    CAnnotTypes_CI(const CSeq_annot_Handle& annot,
                   const SAnnotSelector& params);
    // Iterate everything from the seq-entry
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_entry& entry,
                   const SAnnotSelector& params);

    // Search all TSEs in all datasources, by default get annotations defined
    // on segments in the same TSE (eResolve_TSE method).
    CAnnotTypes_CI(CScope& scope,
                   const CSeq_loc& loc,
                   SAnnotSelector selector,
                   EOverlapType overlap_type,
                   EResolveMethod resolve,
                   const CSeq_entry* entry = 0);
    // Search only in TSE, containing the bioseq, by default get annotations
    // defined on segments in the same TSE (eResolve_TSE method).
    // If "entry" is set, search only annotations from the seq-entry specified
    // (but no its sub-entries or parent entry).
    CAnnotTypes_CI(const CBioseq_Handle& bioseq,
                   TSeqPos start, TSeqPos stop,
                   SAnnotSelector selector,
                   EOverlapType overlap_type,
                   EResolveMethod resolve,
                   const CSeq_entry* entry = 0);
    CAnnotTypes_CI(const CAnnotTypes_CI& it);
    virtual ~CAnnotTypes_CI(void);

    CAnnotTypes_CI& operator= (const CAnnotTypes_CI& it);

    // Rewind annot iterator to point to the very first annot object,
    // the same as immediately after construction.
    void Rewind(void);

    typedef CConstRef<CTSE_Info> TTSE_Lock;
    typedef set<TTSE_Lock>       TTSE_LockSet;

    const CSeq_annot& GetSeq_annot(void) const;

    // Get number of annotations
    size_t GetSize(void) const;

protected:
    // Check if a datasource and an annotation are selected.
    bool IsValid(void) const;
    // Move to the next valid position
    void Next(void);
    void Prev(void);
    // Return current annotation
    const CAnnotObject_Ref& Get(void) const;
    CScope& GetScope(void) const;

private:
    typedef vector<CAnnotObject_Ref> TAnnotSet;

    void x_Clear(void);
    void x_Initialize(const CBioseq_Handle& bioseq,
                      TSeqPos start, TSeqPos stop);
    void x_Initialize(const CHandleRangeMap& master_loc);
    void x_Initialize(void);
    void x_GetTSE_Info(void);
    bool x_SearchMapped(const CSeqMap_CI& seg,
                        const CSeq_id_Handle& master_id,
                        CSeq_loc& master_loc_empty,
                        const CHandleRange& master_hr);
    bool x_Search(const CHandleRangeMap& loc,
                  CSeq_loc_Conversion* cvt);
    bool x_Search(const CSeq_id_Handle& id,
                  const CBioseq_Handle& bh,
                  const CHandleRange& hr,
                  CSeq_loc_Conversion* cvt);
    bool x_Search(const TTSE_LockSet& tse_set,
                  const CSeq_id_Handle& id,
                  const CHandleRange& hr,
                  CSeq_loc_Conversion* cvt);
    void x_Search(const CTSE_Info& tse_info,
                  const SIdAnnotObjs* objs,
                  CReadLockGuard& guard,
                  const CAnnotName& name,
                  const CSeq_id_Handle& id,
                  const CHandleRange& hr,
                  CSeq_loc_Conversion* cvt);
    void x_SearchAll(void);
    void x_SearchAll(const CSeq_entry_Info& entry_info);
    void x_SearchAll(const CSeq_annot_Info& annot_info);
    void x_Sort(void);
    
    bool x_AddObjectMapping(CAnnotObject_Ref& object_ref,
                            CSeq_loc_Conversion* cvt);
    bool x_AddObject(CAnnotObject_Ref& object_ref);
    bool x_AddObject(CAnnotObject_Ref& object_ref, CSeq_loc_Conversion* cvt);

    // Release all locked resources TSE etc
    void x_ReleaseAll(void);

    bool x_NeedSNPs(void) const;
    bool x_MatchLimitObject(const CAnnotObject_Info& annot_info) const;
    bool x_MatchType(const CAnnotObject_Info& annot_info) const;
    bool x_MatchRange(const CHandleRange& hr,
                      const CRange<TSeqPos>& range,
                      const SAnnotObject_Index& index) const;
    size_t x_GetAnnotCount(void) const;

    // Set of all the annotations found
    TAnnotSet                     m_AnnotSet;
    // Current annotation
    TAnnotSet::const_iterator     m_CurAnnot;
    // TSE set to keep all the TSEs locked
    TTSE_LockSet                  m_TSE_LockSet;
    CHeapScope                    m_Scope;
    auto_ptr<CAnnotDataCollector> m_DataCollector;
};


/////////////////////////////////////////////////////////////////////////////
// CAnnotObject_Ref
/////////////////////////////////////////////////////////////////////////////

inline
CAnnotObject_Ref::CAnnotObject_Ref(void)
    : m_AnnotObject_Index(0),
      m_MappedIndex(0),
      m_ObjectType(eType_null),
      m_Partial(false),
      m_MappedType(CSeq_loc::e_not_set),
      m_MappedStrand(eNa_strand_unknown)
{
}


inline
CAnnotObject_Ref::~CAnnotObject_Ref(void)
{
}


inline
CAnnotObject_Ref::EObjectType CAnnotObject_Ref::GetObjectType(void) const
{
    return EObjectType(m_ObjectType);
}


inline
TSeqPos CAnnotObject_Ref::GetFrom(void) const
{
    return m_TotalRange.GetFrom();
}


inline
TSeqPos CAnnotObject_Ref::GetToOpen(void) const
{
    return m_TotalRange.GetToOpen();
}


inline
const CAnnotObject_Ref::TRange& CAnnotObject_Ref::GetTotalRange(void) const
{
    return m_TotalRange;
}


inline
Uint4 CAnnotObject_Ref::GetAnnotObjectIndex(void) const
{
    return m_AnnotObject_Index;
}


inline
bool CAnnotObject_Ref::IsPartial(void) const
{
    return m_Partial;
}


inline
bool CAnnotObject_Ref::IsMappedLocation(int index) const
{
    return bool(m_MappedLocation) && m_MappedIndex == Uint4(index);
}


inline
bool CAnnotObject_Ref::MappedNeedsUpdate(void) const
{
    return m_MappedType != CSeq_loc::e_not_set;
}


inline
const CSeq_loc* CAnnotObject_Ref::GetMappedLocation(void) const
{
    return m_MappedLocation.GetPointerOrNull();
}


inline
int CAnnotObject_Ref::GetMappedIndex(void) const
{
    return m_MappedIndex;
}


inline
bool CAnnotObject_Ref::IsFeat(void) const
{
    return m_ObjectType == eType_Seq_annot_SNP_Info ||
        m_ObjectType == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsFeat();
}


inline
bool CAnnotObject_Ref::IsSNPFeat(void) const
{
    return m_ObjectType == eType_Seq_annot_SNP_Info;
}


inline
bool CAnnotObject_Ref::IsGraph(void) const
{
    return m_ObjectType == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsGraph();
}


inline
bool CAnnotObject_Ref::IsAlign(void) const
{
    return m_ObjectType == eType_Seq_annot_Info &&
        GetAnnotObject_Info().IsAlign();
}


inline
const CSeq_feat& CAnnotObject_Ref::GetFeat(void) const
{
    return GetAnnotObject_Info().GetFeat();
}


inline
const CSeq_graph& CAnnotObject_Ref::GetGraph(void) const
{
    return GetAnnotObject_Info().GetGraph();
}


inline
const CSeq_align& CAnnotObject_Ref::GetAlign(void) const
{
    return GetAnnotObject_Info().GetAlign();
}


inline
void CAnnotObject_Ref::SetMappedIndex(int index)
{
    m_MappedIndex = index;
}


inline
void CAnnotObject_Ref::SetAnnotObjectRange(const TRange& range, int index)
{
    m_TotalRange = range;
    m_MappedIndex = index;
}


inline
void CAnnotObject_Ref::ResetLocation(void)
{
    m_TotalRange = TRange::GetEmpty();
    m_MappedType = CSeq_loc::e_not_set;
    m_MappedStrand = eNa_strand_unknown;
}


/////////////////////////////////////////////////////////////////////////////
// CAnnotTypes_CI
/////////////////////////////////////////////////////////////////////////////


inline
bool CAnnotTypes_CI::IsValid(void) const
{
    return m_CurAnnot != m_AnnotSet.end();
}


inline
void CAnnotTypes_CI::Rewind(void)
{
    m_CurAnnot = m_AnnotSet.begin();
}


inline
void CAnnotTypes_CI::Next(void)
{
    ++m_CurAnnot;
}


inline
void CAnnotTypes_CI::Prev(void)
{
    --m_CurAnnot;
}


inline
const CAnnotObject_Ref& CAnnotTypes_CI::Get(void) const
{
    _ASSERT( IsValid() );
    return *m_CurAnnot;
}


inline
const CSeq_annot& CAnnotTypes_CI::GetSeq_annot(void) const
{
    return Get().GetSeq_annot();
}


inline
size_t CAnnotTypes_CI::GetSize(void) const
{
    return m_AnnotSet.size();
}

inline
CScope& CAnnotTypes_CI::GetScope(void) const
{
    return m_Scope;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.59  2003/11/10 18:11:03  grichenk
* Moved CSeq_loc_Conversion_Set to seq_loc_cvt
*
* Revision 1.58  2003/11/04 21:10:00  grichenk
* Optimized feature mapping through multiple segments.
* Fixed problem with CAnnotTypes_CI not releasing scope
* when exception is thrown from constructor.
*
* Revision 1.57  2003/11/04 16:21:36  grichenk
* Updated CAnnotTypes_CI to map whole features instead of splitting
* them by sequence segments.
*
* Revision 1.56  2003/10/27 20:07:10  vasilche
* Started implementation of full annotations' mapping.
*
* Revision 1.55  2003/10/09 20:20:59  vasilche
* Added possibility to include and exclude Seq-annot names to annot iterator.
* Fixed adaptive search. It looked only on selected set of annot names before.
*
* Revision 1.54  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.53  2003/09/30 16:21:59  vasilche
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
* Revision 1.52  2003/09/12 15:50:09  grichenk
* Updated adaptive-depth triggering
*
* Revision 1.51  2003/09/11 17:45:06  grichenk
* Added adaptive-depth option to annot-iterators.
*
* Revision 1.50  2003/09/03 19:59:00  grichenk
* Initialize m_MappedIndex to 0
*
* Revision 1.49  2003/08/27 21:23:35  vasilche
* Fixed warning.
*
* Revision 1.48  2003/08/27 14:28:50  vasilche
* Reduce amount of object allocations in feature iteration.
*
* Revision 1.47  2003/08/22 14:58:55  grichenk
* Added NoMapping flag (to be used by CAnnot_CI for faster fetching).
* Added GetScope().
*
* Revision 1.46  2003/08/15 19:19:15  vasilche
* Fixed memory leak in string packing hooks.
* Fixed processing of 'partial' flag of features.
* Allow table packing of non-point SNP.
* Allow table packing of SNP with long alleles.
*
* Revision 1.45  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.44  2003/08/04 17:02:57  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.43  2003/07/18 19:39:44  vasilche
* Workaround for GCC 3.0.4 bug.
*
* Revision 1.42  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.41  2003/06/25 20:56:29  grichenk
* Added max number of annotations to annot-selector, updated demo.
*
* Revision 1.40  2003/06/02 16:01:36  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.39  2003/04/29 19:51:12  vasilche
* Fixed interaction of Data Loader garbage collector and TSE locking mechanism.
* Made some typedefs more consistent.
*
* Revision 1.38  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.37  2003/03/27 19:40:10  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
* Revision 1.36  2003/03/21 19:22:48  grichenk
* Redesigned TSE locking, replaced CTSE_Lock with CRef<CTSE_Info>.
*
* Revision 1.35  2003/03/18 21:48:27  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.34  2003/03/05 20:56:42  vasilche
* SAnnotSelector now holds all parameters of annotation iterators.
*
* Revision 1.33  2003/02/28 19:27:20  vasilche
* Cleaned Seq_loc conversion class.
*
* Revision 1.32  2003/02/27 20:56:51  vasilche
* Use one method for lookup on main sequence and segments.
*
* Revision 1.31  2003/02/27 14:35:32  vasilche
* Splitted PopulateTSESet() by logically independent parts.
*
* Revision 1.30  2003/02/26 17:54:14  vasilche
* Added cached total range of mapped location.
*
* Revision 1.29  2003/02/24 21:35:21  vasilche
* Reduce checks in CAnnotObject_Ref comparison.
* Fixed compilation errors on MS Windows.
* Removed obsolete file src/objects/objmgr/annot_object.hpp.
*
* Revision 1.28  2003/02/24 18:57:20  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.27  2003/02/13 14:34:31  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.26  2003/02/04 21:44:10  grichenk
* Convert seq-loc instead of seq-annot to the master coordinates
*
* Revision 1.25  2002/12/26 20:51:35  dicuccio
* Added Win32 export specifier
*
* Revision 1.24  2002/12/26 16:39:21  vasilche
* Object manager class CSeqMap rewritten.
*
* Revision 1.23  2002/12/24 15:42:44  grichenk
* CBioseqHandle argument to annotation iterators made const
*
* Revision 1.22  2002/12/06 15:35:57  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.21  2002/11/04 21:28:58  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.20  2002/11/01 20:46:41  grichenk
* Added sorting to set< CRef<CAnnotObject> >
*
* Revision 1.19  2002/10/08 18:57:27  grichenk
* Added feature sorting to the iterator class.
*
* Revision 1.18  2002/07/08 20:50:56  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.17  2002/05/31 17:52:58  grichenk
* Optimized for better performance (CTSE_Info uses atomic counter,
* delayed annotations indexing, no location convertions in
* CAnnot_Types_CI if no references resolution is required etc.)
*
* Revision 1.16  2002/05/24 14:58:53  grichenk
* Fixed Empty() for unsigned intervals
* SerialAssign<>() -> CSerialObject::Assign()
* Improved performance for eResolve_None case
*
* Revision 1.15  2002/05/06 03:30:35  vakatov
* OM/OM1 renaming
*
* Revision 1.14  2002/05/03 21:28:01  ucko
* Introduce T(Signed)SeqPos.
*
* Revision 1.13  2002/04/30 14:30:41  grichenk
* Added eResolve_TSE flag in CAnnot_Types_CI, made it default
*
* Revision 1.12  2002/04/22 20:06:15  grichenk
* Minor changes in private interface
*
* Revision 1.11  2002/04/17 21:11:58  grichenk
* Fixed annotations loading
* Set "partial" flag in features if necessary
* Implemented most seq-loc types in reference resolving methods
* Fixed searching for annotations within a signle TSE
*
* Revision 1.10  2002/04/11 12:07:28  grichenk
* Redesigned CAnnotTypes_CI to resolve segmented sequences correctly.
*
* Revision 1.9  2002/04/05 21:26:16  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.8  2002/03/07 21:25:31  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.7  2002/03/05 16:08:12  grichenk
* Moved TSE-restriction to new constructors
*
* Revision 1.6  2002/03/04 15:07:46  grichenk
* Added "bioseq" argument to CAnnotTypes_CI constructor to iterate
* annotations from a single TSE.
*
* Revision 1.5  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.4  2002/02/15 20:36:29  gouriano
* changed implementation of HandleRangeMap
*
* Revision 1.3  2002/02/07 21:27:33  grichenk
* Redesigned CDataSource indexing: seq-id handle -> TSE -> seq/annot
*
* Revision 1.2  2002/01/16 16:26:35  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:03:59  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // ANNOT_TYPES_CI__HPP
