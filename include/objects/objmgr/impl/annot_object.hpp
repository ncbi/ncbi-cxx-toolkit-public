#ifndef ANNOT_OBJECT__HPP
#define ANNOT_OBJECT__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Annotation object wrapper
*
*/

#include <objects/objmgr/impl/handle_range_map.hpp>
#include <objects/objmgr/annot_selector.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqalign/Seq_align.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;

class NCBI_XOBJMGR_EXPORT CSeq_annot_Info : public CObject
{
public:
    CSeq_annot_Info(CDataSource* data_source,
                   const CSeq_annot& annot,
                   const CSeq_entry* entry);
    ~CSeq_annot_Info(void);

    CDataSource*      GetDataSource(void) const;
    const CSeq_annot* GetSeq_annot(void) const;
    const CSeq_entry* GetSeq_entry(void) const;

private:
    CSeq_annot_Info(const CSeq_annot_Info&);
    CSeq_annot_Info& operator=(const CSeq_annot_Info&);

    CDataSource*          m_DataSource;
    CConstRef<CSeq_annot> m_Annot;
    CConstRef<CSeq_entry> m_Entry;
};


// General Seq-annot object
class NCBI_XOBJMGR_EXPORT CAnnotObject_Info : public CObject
{
public:
    CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                      const CSeq_feat&  feat);
    CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                      const CSeq_align& align);
    CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                      const CSeq_graph& graph);
    virtual ~CAnnotObject_Info(void);

    SAnnotSelector::TAnnotChoice Which(void) const;

    const CConstRef<CObject>& GetObject(void) const;

    bool IsFeat(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_feat* GetFeatFast(void) const; // unchecked & unsafe

    bool IsAlign(void) const;
    const CSeq_align& GetAlign(void) const;

    bool IsGraph(void) const;
    const CSeq_graph& GetGraph(void) const;

    const CHandleRangeMap& GetRangeMap(void) const;
    const CHandleRangeMap* GetProductMap(void) const; // may be null

    // Get Seq-annot, containing the element
    const CSeq_annot& GetSeq_annot(void) const;
    // Get Seq-entry, containing the annotation
    const CSeq_entry& GetSeq_entry(void) const;

    virtual void DebugDump(CDebugDumpContext ddc, unsigned int depth) const;

private:
    CAnnotObject_Info(const CAnnotObject_Info&);
    CAnnotObject_Info& operator=(const CAnnotObject_Info&);

    // Constructors used by CAnnotTypes_CI only to create fake annotations
    // for sequence segments. The annot object points to the seq-annot
    // containing the original annotation object.
    friend class CAnnotTypes_CI;
    friend class CAnnotObject_Less;

    CAnnotObject_Info(const CSeq_feat&  feat,
                      const CSeq_annot& annot,
                      const CSeq_entry* entry);
    CAnnotObject_Info(const CSeq_align& align,
                      const CSeq_annot& annot,
                      const CSeq_entry* entry);
    CAnnotObject_Info(const CSeq_graph& graph,
                      const CSeq_annot& annot,
                      const CSeq_entry* entry);

    void x_ProcessAlign(const CSeq_align& align);

    CRef<CSeq_annot_Info>         m_AnnotRef;
    SAnnotSelector::TAnnotChoice m_Choice;
    CConstRef<CObject>           m_Object;
    auto_ptr<CHandleRangeMap>    m_RangeMap;   // may be null for fake objects
    auto_ptr<CHandleRangeMap>    m_ProductMap; // non-null for features with product
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CSeq_annot_Info::CSeq_annot_Info(CDataSource* data_source,
                               const CSeq_annot& annot,
                               const CSeq_entry* entry)
    : m_DataSource(data_source),
      m_Annot(&annot),
      m_Entry(entry)
{
    return;
}


inline
CSeq_annot_Info::~CSeq_annot_Info(void)
{
    return;
}

inline
CDataSource* CSeq_annot_Info::GetDataSource(void) const
{
    return m_DataSource;
}

inline
const CSeq_annot* CSeq_annot_Info::GetSeq_annot(void) const
{
    return m_Annot;
}

inline
const CSeq_entry* CSeq_annot_Info::GetSeq_entry(void) const
{
    return m_Entry;
}


inline
CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                                     const CSeq_feat& feat)
    : m_AnnotRef(&annot_ref),
      m_Choice(CSeq_annot::C_Data::e_Ftable),
      m_Object(&feat),
      m_RangeMap(new CHandleRangeMap()),
      m_ProductMap(0)
{
    m_RangeMap->AddLocation(feat.GetLocation());
    if ( feat.IsSetProduct() ) {
        m_ProductMap.reset(new CHandleRangeMap());
        m_ProductMap->AddLocation(feat.GetProduct());
    }
    return;
}

inline
CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                                     const CSeq_align& align)
    : m_AnnotRef(&annot_ref),
      m_Choice(CSeq_annot::C_Data::e_Align),
      m_Object(&align),
      m_RangeMap(new CHandleRangeMap()),
      m_ProductMap(0)
{
    x_ProcessAlign(align);
    return;
}

inline
CAnnotObject_Info::CAnnotObject_Info(CSeq_annot_Info& annot_ref,
                                     const CSeq_graph& graph)
    : m_AnnotRef(&annot_ref),
      m_Choice(CSeq_annot::C_Data::e_Graph),
      m_Object(&graph),
      m_RangeMap(new CHandleRangeMap()),
      m_ProductMap(0)
{
    m_RangeMap->AddLocation(graph.GetLoc());
    return;
}

inline
CAnnotObject_Info::~CAnnotObject_Info(void)
{
    return;
}

inline
SAnnotSelector::TAnnotChoice CAnnotObject_Info::Which(void) const
{
    return m_Choice;
}

inline
const CConstRef<CObject>& CAnnotObject_Info::GetObject(void) const
{
    return m_Object;
}


inline
bool CAnnotObject_Info::IsFeat(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Ftable;
}

inline
const CSeq_feat& CAnnotObject_Info::GetFeat(void) const
{
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_feat*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_feat&>(*m_Object);
#endif
}

inline
const CSeq_feat* CAnnotObject_Info::GetFeatFast(void) const
{
    return static_cast<const CSeq_feat*>(m_Object.GetPointerOrNull());
}

inline
bool CAnnotObject_Info::IsAlign(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Align;
}

inline
const CSeq_align& CAnnotObject_Info::GetAlign(void) const
{
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_align*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_align&>(*m_Object);
#endif
}

inline
bool CAnnotObject_Info::IsGraph(void) const
{
    return m_Choice == CSeq_annot::C_Data::e_Graph;
}

inline
const CSeq_graph& CAnnotObject_Info::GetGraph(void) const
{
#if defined(NCBI_COMPILER_ICC)
    return *dynamic_cast<const CSeq_graph*>(m_Object.GetPointer());
#else
    return dynamic_cast<const CSeq_graph&>(*m_Object);
#endif
}

inline
const CHandleRangeMap& CAnnotObject_Info::GetRangeMap(void) const
{
    return *m_RangeMap;
}

inline
const CHandleRangeMap* CAnnotObject_Info::GetProductMap(void) const
{
    return m_ProductMap.get();
}

inline
const CSeq_annot& CAnnotObject_Info::GetSeq_annot(void) const
{
    return *m_AnnotRef->GetSeq_annot();
}

inline
const CSeq_entry& CAnnotObject_Info::GetSeq_entry(void) const
{
    return *m_AnnotRef->GetSeq_entry();
}


inline
CAnnotObject_Info::CAnnotObject_Info(const CSeq_feat& feat,
                                     const CSeq_annot& annot,
                                     const CSeq_entry* entry)
    : m_AnnotRef(new CSeq_annot_Info(0, annot, entry)),
      m_Choice(CSeq_annot::C_Data::e_Ftable),
      m_Object(&feat),
      m_RangeMap(0),
      m_ProductMap(0)
{
    return;
}

inline
CAnnotObject_Info::CAnnotObject_Info(const CSeq_align& align,
                                     const CSeq_annot& annot,
                                     const CSeq_entry* entry)
    : m_AnnotRef(new CSeq_annot_Info(0, annot, entry)),
      m_Choice(CSeq_annot::C_Data::e_Align),
      m_Object(&align),
      m_RangeMap(0),
      m_ProductMap(0)
{
    return;
}

inline
CAnnotObject_Info::CAnnotObject_Info(const CSeq_graph& graph,
                                     const CSeq_annot& annot,
                                     const CSeq_entry* entry)
    : m_AnnotRef(new CSeq_annot_Info(0, annot, entry)),
      m_Choice(CSeq_annot::C_Data::e_Graph),
      m_Object(&graph),
      m_RangeMap(0),
      m_ProductMap(0)
{
    return;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.4  2003/03/18 21:48:28  grichenk
* Removed obsolete class CAnnot_CI
*
* Revision 1.3  2003/02/25 14:48:07  vasilche
* Added Win32 export modifier to object manager classes.
*
* Revision 1.2  2003/02/24 21:35:22  vasilche
* Reduce checks in CAnnotObject_Ref comparison.
* Fixed compilation errors on MS Windows.
* Removed obsolete file src/objects/objmgr/annot_object.hpp.
*
* Revision 1.1  2003/02/24 18:57:21  vasilche
* Make feature gathering in one linear pass using CSeqMap iterator.
* Do not use feture index by sub locations.
* Sort features at the end of gathering in one vector.
* Extracted some internal structures and classes in separate header.
* Delay creation of mapped features.
*
* Revision 1.16  2003/02/13 14:34:34  grichenk
* Renamed CAnnotObject -> CAnnotObject_Info
* + CSeq_annot_Info and CAnnotObject_Ref
* Moved some members of CAnnotObject to CSeq_annot_Info
* and CAnnotObject_Ref.
* Added feat/align/graph to CAnnotObject_Info map
* to CDataSource.
*
* Revision 1.15  2003/02/10 15:53:04  grichenk
* + m_MappedProd, made mapping methods private
*
* Revision 1.14  2003/02/05 17:59:16  dicuccio
* Moved formerly private headers into include/objects/objmgr/impl
*
* Revision 1.13  2003/02/04 21:45:19  grichenk
* Added IsPartial(), IsMapped(), GetMappedLoc()
*
* Revision 1.12  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.11  2002/12/20 20:54:24  grichenk
* Added optional location/product switch to CFeat_CI
*
* Revision 1.10  2002/12/06 15:36:00  grichenk
* Added overlap type for annot-iterators
*
* Revision 1.9  2002/10/09 19:17:41  grichenk
* Fixed ICC problem with dynamic_cast<>
*
* Revision 1.8  2002/07/08 20:51:01  grichenk
* Moved log to the end of file
* Replaced static mutex (in CScope, CDataSource) with the mutex
* pool. Redesigned CDataSource data locking.
*
* Revision 1.7  2002/05/29 21:21:13  gouriano
* added debug dump
*
* Revision 1.6  2002/04/05 21:26:19  grichenk
* Enabled iteration over annotations defined on segments of a
* delta-sequence.
*
* Revision 1.5  2002/03/07 21:25:33  grichenk
* +GetSeq_annot() in annotation iterators
*
* Revision 1.4  2002/02/21 19:27:04  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.3  2002/01/23 21:59:31  grichenk
* Redesigned seq-id handles and mapper
*
* Revision 1.2  2002/01/16 16:25:57  gouriano
* restructured objmgr
*
* Revision 1.1  2002/01/11 19:06:16  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/

#endif  // ANNOT_OBJECT__HPP
