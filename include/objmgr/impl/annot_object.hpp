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
* Author: Aleksey Grichenko, Michael Kimelman, Eugene Vasilchenko
*
* File Description:
*   Annotation object wrapper
*
*/

#include <corelib/ncbiobj.hpp>

#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqres/Seq_graph.hpp>
#include <objects/seqalign/Seq_align.hpp>

#include <memory>
#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CHandleRangeMap;
class CSeq_align;
class CSeq_graph;
class CSeq_feat;
class CSeq_entry;
class CSeq_entry_Info;
class CSeq_annot;
class CSeq_annot_Info;
class CTSE_Info;
class CTSE_Chunk_Info;

// General Seq-annot object
class NCBI_XOBJMGR_EXPORT CAnnotObject_Info
{
public:
    typedef CSeq_annot::C_Data::E_Choice TAnnotType;
    typedef CSeqFeatData::E_Choice       TFeatType;
    typedef CSeqFeatData::ESubtype       TFeatSubtype;

    CAnnotObject_Info(void);
    CAnnotObject_Info(CSeq_feat& feat,
                      CSeq_annot_Info& annot);
    CAnnotObject_Info(CSeq_align& align,
                      CSeq_annot_Info& annot);
    CAnnotObject_Info(CSeq_graph& graph,
                      CSeq_annot_Info& annot);
    CAnnotObject_Info(CTSE_Chunk_Info& chunk_info,
                      TAnnotType annot_type,
                      TFeatType feat_type,
                      TFeatSubtype feat_subtype);
    ~CAnnotObject_Info(void);

    CAnnotObject_Info(const CAnnotObject_Info&);
    const CAnnotObject_Info& operator=(const CAnnotObject_Info&);

    // Get Seq-annot, containing the element
    const CSeq_annot& GetSeq_annot(void) const;
    const CSeq_annot_Info& GetSeq_annot_Info(void) const;
    CSeq_annot_Info& GetSeq_annot_Info(void);

    // Get Seq-entry, containing the annotation
    const CSeq_entry& GetSeq_entry(void) const;
    const CSeq_entry_Info& GetSeq_entry_Info(void) const;

    // Get top level Seq-entry, containing the annotation
    const CSeq_entry& GetTSE(void) const;
    const CTSE_Info& GetTSE_Info(void) const;
    CTSE_Info& GetTSE_Info(void);

    // Get CDataSource object
    CDataSource& GetDataSource(void) const;

    TAnnotType Which(void) const;
    TAnnotType GetAnnotType(void) const;
    TFeatType GetFeatType(void) const;
    TFeatSubtype GetFeatSubtype(void) const;

    const CRef<CObject>& GetObject(void) const;
    const CObject* GetObjectPointer(void) const;

    bool IsFeat(void) const;
    const CSeq_feat& GetFeat(void) const;
    const CSeq_feat* GetFeatFast(void) const; // unchecked & unsafe

    bool IsAlign(void) const;
    const CSeq_align& GetAlign(void) const;
    const CSeq_align* GetAlignFast(void) const;

    bool IsGraph(void) const;
    const CSeq_graph& GetGraph(void) const;
    const CSeq_graph* GetGraphFast(void) const; // unchecked & unsafe

    void GetMaps(vector<CHandleRangeMap>& hrmaps) const;

    // split support
    bool IsChunkStub(void) const;
    const CTSE_Chunk_Info& GetChunk_Info(void) const;

private:
    // Constructors used by CAnnotTypes_CI only to create fake annotations
    // for sequence segments. The annot object points to the seq-annot
    // containing the original annotation object.

    void x_ProcessAlign(CHandleRangeMap& hrmap, const CSeq_align& align) const;

    CSeq_annot_Info*             m_Seq_annot_Info; // parent annot
    CRef<CObject>                m_Object;         // annot object itself
    Uint1                        m_AnnotType;      // annot object type
    Uint1                        m_FeatType;       // feature type or e_not_set
    Uint1                        m_FeatSubtype;    // feature subtype
    bool                         m_ChunkStub;      // reference to split chunk
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
CAnnotObject_Info::TAnnotType CAnnotObject_Info::Which(void) const
{
    return TAnnotType(m_AnnotType);
}


inline
CAnnotObject_Info::TAnnotType CAnnotObject_Info::GetAnnotType(void) const
{
    return TAnnotType(m_AnnotType);
}


inline
CAnnotObject_Info::TFeatType CAnnotObject_Info::GetFeatType(void) const
{
    return TFeatType(m_FeatType);
}


inline
CAnnotObject_Info::TFeatSubtype CAnnotObject_Info::GetFeatSubtype(void) const
{
    return TFeatSubtype(m_FeatSubtype);
}


inline
bool CAnnotObject_Info::IsChunkStub(void) const
{
    return m_ChunkStub;
}


inline
const CRef<CObject>& CAnnotObject_Info::GetObject(void) const
{
    return m_Object;
}


inline
const CObject* CAnnotObject_Info::GetObjectPointer(void) const
{
    return m_Object.GetPointer();
}


inline
bool CAnnotObject_Info::IsFeat(void) const
{
    return m_AnnotType == CSeq_annot::C_Data::e_Ftable;
}


inline
bool CAnnotObject_Info::IsAlign(void) const
{
    return m_AnnotType == CSeq_annot::C_Data::e_Align;
}


inline
bool CAnnotObject_Info::IsGraph(void) const
{
    return m_AnnotType == CSeq_annot::C_Data::e_Graph;
}


inline
const CSeq_feat* CAnnotObject_Info::GetFeatFast(void) const
{
    _ASSERT(IsFeat() && !IsChunkStub());
    return static_cast<const CSeq_feat*>(m_Object.GetPointerOrNull());
}


inline
const CSeq_graph* CAnnotObject_Info::GetGraphFast(void) const
{
    _ASSERT(IsGraph() && !IsChunkStub());
    return static_cast<const CSeq_graph*>(m_Object.GetPointerOrNull());
}


inline
const CSeq_align* CAnnotObject_Info::GetAlignFast(void) const
{
    _ASSERT(IsAlign() && !IsChunkStub());
    return static_cast<const CSeq_align*>(m_Object.GetPointerOrNull());
}


inline
const CSeq_annot_Info& CAnnotObject_Info::GetSeq_annot_Info(void) const
{
    return *m_Seq_annot_Info;
}


inline
CSeq_annot_Info& CAnnotObject_Info::GetSeq_annot_Info(void)
{
    return *m_Seq_annot_Info;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.12  2003/09/30 16:22:00  vasilche
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
* Revision 1.11  2003/08/27 14:28:51  vasilche
* Reduce amount of object allocations in feature iteration.
*
* Revision 1.10  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.9  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.8  2003/06/02 16:01:37  dicuccio
* Rearranged include/objects/ subtree.  This includes the following shifts:
*     - include/objects/alnmgr --> include/objtools/alnmgr
*     - include/objects/cddalignview --> include/objtools/cddalignview
*     - include/objects/flat --> include/objtools/flat
*     - include/objects/objmgr/ --> include/objmgr/
*     - include/objects/util/ --> include/objmgr/util/
*     - include/objects/validator --> include/objtools/validator
*
* Revision 1.7  2003/04/24 17:54:13  ucko
* +<memory> (for auto_ptr<>)
*
* Revision 1.6  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.5  2003/03/27 19:40:11  vasilche
* Implemented sorting in CGraph_CI.
* Added Rewind() method to feature/graph/align iterators.
*
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
