#ifndef SEQ_ANNOT_INFO__HPP
#define SEQ_ANNOT_INFO__HPP

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
* Author: Eugene Vasilchenko
*
* File Description:
*   Seq-annot object information
*
*/

#include <corelib/ncbiobj.hpp>

#include <util/range.hpp>

#include <objmgr/impl/tse_info_object.hpp>
#include <objects/seq/seq_id_handle.hpp>
#include <objmgr/annot_name.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/annot_object_index.hpp>

#include <vector>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

class CDataSource;
class CSeq_annot;
class CSeq_entry;
class CTSE_Info;
class CBioseq_Base_Info;
class CAnnotObject_Info;
struct SAnnotObject_Key;
class CSeq_annot_SNP_Info;

class NCBI_XOBJMGR_EXPORT CSeq_annot_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    explicit CSeq_annot_Info(const CSeq_annot& annot);
    explicit CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot);
    explicit CSeq_annot_Info(const CSeq_annot_Info&);
    ~CSeq_annot_Info(void);

    const CBioseq_Base_Info& GetParentBioseq_Base_Info(void) const;
    CBioseq_Base_Info& GetParentBioseq_Base_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    typedef CSeq_annot TObject;
    CConstRef<TObject> GetCompleteSeq_annot(void) const;
    CConstRef<TObject> GetSeq_annotCore(void) const;

    const CAnnotName& GetName(void) const;

    const CAnnotObject_Info& GetAnnotObject_Info(size_t index) const;
    size_t GetAnnotObjectIndex(const CAnnotObject_Info& info) const;

    // tree initialization
    virtual void x_DSAttachContents(CDataSource& ds);
    virtual void x_DSDetachContents(CDataSource& ds);

    virtual void x_TSEAttachContents(CTSE_Info& tse);
    virtual void x_TSEDetachContents(CTSE_Info& tse);

    void x_ParentAttach(CBioseq_Base_Info& parent);
    void x_ParentDetach(CBioseq_Base_Info& parent);

    //
    void UpdateAnnotIndex(void) const;

    void x_UpdateAnnotIndexContents(CTSE_Info& tse);

    const TObject& x_GetObject(void) const;

    void x_SetObject(const TObject& obj);
    void x_SetObject(const CSeq_annot_Info& info);

    void x_SetSNP_annot_Info(CSeq_annot_SNP_Info& snp_info);
    bool x_HasSNP_annot_Info(void) const;
    const CSeq_annot_SNP_Info& x_GetSNP_annot_Info(void) const;

    void x_DoUpdate(TNeedUpdateFlags flags);
    static CRef<TObject> sx_ShallowCopy(TObject& obj);

    typedef SAnnotObjectsIndex::TObjectInfos TAnnotObjectInfos;
    const TAnnotObjectInfos& GetAnnotObjectInfos(void) const;

protected:
    friend class CDataSource;
    friend class CTSE_Info;
    friend class CSeq_entry_Info;
    friend class CAnnot_Collector;

    void x_UpdateName(void);

    typedef vector< CConstRef<TObject> > TDSMappedObjects;

    virtual void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    virtual void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_InitFeats(const CSeq_annot::C_Data::TFtable& objs);
    void x_InitAligns(const CSeq_annot::C_Data::TAlign& objs);
    void x_InitGraphs(const CSeq_annot::C_Data::TGraph& objs);
    void x_InitLocs(const CSeq_annot& annot); // locs case

    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);

    CSeq_annot_Info& operator=(const CSeq_annot_Info&);

    // Seq-annot object
    CConstRef<TObject>      m_Object;
    TDSMappedObjects        m_DSMappedObjects;

    // name of Seq-annot
    CAnnotName              m_Name;

    // annot object infos array
    SAnnotObjectsIndex      m_ObjectIndex;

    // SNP annotation table
    CRef<CSeq_annot_SNP_Info> m_SNP_Info;
};


/////////////////////////////////////////////////////////////////////
//
//  Inline methods
//
/////////////////////////////////////////////////////////////////////


inline
const CSeq_annot& CSeq_annot_Info::x_GetObject(void) const
{
    return *m_Object;
}


inline
const CSeq_annot_Info::TAnnotObjectInfos&
CSeq_annot_Info::GetAnnotObjectInfos(void) const
{
    return m_ObjectIndex.GetInfos();
}


inline
const CAnnotObject_Info&
CSeq_annot_Info::GetAnnotObject_Info(size_t index) const
{
    return m_ObjectIndex.GetInfo(index);
}


inline
bool CSeq_annot_Info::x_HasSNP_annot_Info(void) const
{
    return m_SNP_Info.NotEmpty();
}


inline
const CSeq_annot_SNP_Info& CSeq_annot_Info::x_GetSNP_annot_Info(void) const
{
    return *m_SNP_Info;
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.21  2005/04/05 13:40:59  vasilche
* TSE IdFlags are updated within CTSE_Info.
*
* Revision 1.20  2005/03/29 16:04:50  grichenk
* Optimized annotation retrieval (reduced nuber of seq-ids checked)
*
* Revision 1.19  2005/01/12 17:16:14  vasilche
* Avoid performance warning on MSVC.
*
* Revision 1.18  2004/12/22 15:56:27  vasilche
* Use SAnnotObjectsIndex.
*
* Revision 1.17  2004/08/05 18:25:05  vasilche
* CAnnotName and CAnnotTypeSelector are moved in separate headers.
*
* Revision 1.16  2004/07/12 16:57:32  vasilche
* Fixed loading of split Seq-descr and Seq-data objects.
* They are loaded correctly now when GetCompleteXxx() method is called.
*
* Revision 1.15  2004/07/12 15:05:31  grichenk
* Moved seq-id mapper from xobjmgr to seq library
*
* Revision 1.14  2004/06/07 17:01:17  grichenk
* Implemented referencing through locs annotations
*
* Revision 1.13  2004/04/05 15:56:13  grichenk
* Redesigned CAnnotTypes_CI: moved all data and data collecting
* functions to CAnnotDataCollector. CAnnotTypes_CI is no more
* inherited from SAnnotSelector.
*
* Revision 1.12  2004/03/26 19:42:03  vasilche
* Fixed premature deletion of SNP annot info object.
* Removed obsolete references to chunk info.
*
* Revision 1.11  2004/03/24 18:30:29  vasilche
* Fixed edit API.
* Every *_Info object has its own shallow copy of original object.
*
* Revision 1.10  2004/03/16 15:47:27  vasilche
* Added CBioseq_set_Handle and set of EditHandles
*
* Revision 1.9  2003/11/26 17:55:55  vasilche
* Implemented ID2 split in ID1 cache.
* Fixed loading of splitted annotations.
*
* Revision 1.8  2003/10/07 13:43:22  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.7  2003/09/30 16:22:01  vasilche
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
* Revision 1.6  2003/08/27 14:49:19  vasilche
* Added necessary include.
*
* Revision 1.5  2003/08/27 14:28:51  vasilche
* Reduce amount of object allocations in feature iteration.
*
* Revision 1.4  2003/08/14 20:05:18  vasilche
* Simple SNP features are stored as table internally.
* They are recreated when needed using CFeat_CI.
*
* Revision 1.3  2003/08/04 17:02:59  grichenk
* Added constructors to iterate all annotations from a
* seq-entry or seq-annot.
*
* Revision 1.2  2003/07/17 20:07:55  vasilche
* Reduced memory usage by feature indexes.
* SNP data is loaded separately through PUBSEQ_OS.
* String compression for SNP data.
*
* Revision 1.1  2003/04/24 16:12:37  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
*
* ===========================================================================
*/

#endif  // SEQ_ANNOT_INFO__HPP
