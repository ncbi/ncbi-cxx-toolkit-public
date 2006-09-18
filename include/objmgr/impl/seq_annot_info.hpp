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
struct STSEAnnotObjectMapper;
class CSeq_annot_SNP_Info;

class NCBI_XOBJMGR_EXPORT CSeq_annot_Info : public CTSE_Info_Object
{
    typedef CTSE_Info_Object TParent;
public:
    // typedefs from CSeq_annot
    typedef CSeq_annot::C_Data  C_Data;
    typedef C_Data::TFtable     TFtable;
    typedef C_Data::TAlign      TAlign;
    typedef C_Data::TGraph      TGraph;
    typedef C_Data::TLocs       TLocs;
    typedef Int4                TIndex;

    explicit CSeq_annot_Info(CSeq_annot& annot);
    explicit CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot);
    explicit CSeq_annot_Info(const CSeq_annot_Info& src,
                             TObjectCopyMap* copy_map);
    ~CSeq_annot_Info(void);

    const CBioseq_Base_Info& GetParentBioseq_Base_Info(void) const;
    CBioseq_Base_Info& GetParentBioseq_Base_Info(void);

    const CSeq_entry_Info& GetParentSeq_entry_Info(void) const;
    CSeq_entry_Info& GetParentSeq_entry_Info(void);

    typedef CSeq_annot TObject;
    CConstRef<TObject> GetCompleteSeq_annot(void) const;
    CConstRef<TObject> GetSeq_annotCore(void) const;
    CConstRef<TObject> GetSeq_annotSkeleton(void) const;

    const CAnnotName& GetName(void) const;

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

    void x_SetObject(TObject& obj);
    void x_SetObject(const CSeq_annot_Info& info, TObjectCopyMap* copy_map);

    void x_SetSNP_annot_Info(CSeq_annot_SNP_Info& snp_info);
    bool x_HasSNP_annot_Info(void) const;
    const CSeq_annot_SNP_Info& x_GetSNP_annot_Info(void) const;

    void x_DoUpdate(TNeedUpdateFlags flags);

    typedef SAnnotObjectsIndex::TObjectInfos TAnnotObjectInfos;
    const TAnnotObjectInfos& GetAnnotObjectInfos(void) const;

    // individual annotation editing API
    void Remove(TIndex index);
    void Replace(TIndex index, const CSeq_feat& new_obj);
    void Replace(TIndex index, const CSeq_align& new_obj);
    void Replace(TIndex index, const CSeq_graph& new_obj);
    TIndex Add(const CSeq_feat& new_obj);
    TIndex Add(const CSeq_align& new_obj);
    TIndex Add(const CSeq_graph& new_obj);

    void Update(TIndex index);

    const CAnnotObject_Info& GetInfo(TIndex index) const;

protected:
    friend class CDataSource;
    friend class CTSE_Info;
    friend class CSeq_entry_Info;

    void x_UpdateName(void);

    void x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds);
    void x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds);

    void x_InitAnnotList(void);
    void x_InitAnnotList(const CSeq_annot_Info& info);

    void x_InitFeatList(TFtable& objs);
    void x_InitAlignList(TAlign& objs);
    void x_InitGraphList(TGraph& objs);
    void x_InitLocsList(TLocs& annot);
    void x_InitFeatList(TFtable& objs, const CSeq_annot_Info& info);
    void x_InitAlignList(TAlign& objs, const CSeq_annot_Info& info);
    void x_InitGraphList(TGraph& objs, const CSeq_annot_Info& info);
    void x_InitLocsList(TLocs& annot, const CSeq_annot_Info& info);

    void x_InitAnnotKeys(CTSE_Info& tse);

    void x_InitFeatKeys(CTSE_Info& tse);
    void x_InitAlignKeys(CTSE_Info& tse);
    void x_InitGraphKeys(CTSE_Info& tse);
    void x_InitLocsKeys(CTSE_Info& tse);

    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);

    void x_UnmapAnnotObject(CAnnotObject_Info& info);
    void x_MapAnnotObject(CAnnotObject_Info& info);
    void x_RemapAnnotObject(CAnnotObject_Info& info);

    void x_Map(const STSEAnnotObjectMapper& mapper,
               const SAnnotObject_Key& key,
               const SAnnotObject_Index& index);

    void x_UpdateObjectKeys(CAnnotObject_Info& info, size_t keys_begin);

    // Seq-annot object
    CRef<TObject>           m_Object;

    // name of Seq-annot
    CAnnotName              m_Name;

    // annot object infos array
    SAnnotObjectsIndex      m_ObjectIndex;

    // SNP annotation table
    CRef<CSeq_annot_SNP_Info> m_SNP_Info;

private:
    CSeq_annot_Info(const CSeq_annot_Info&);
    CSeq_annot_Info& operator=(const CSeq_annot_Info&);
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
bool CSeq_annot_Info::x_HasSNP_annot_Info(void) const
{
    return m_SNP_Info.NotEmpty();
}


inline
const CSeq_annot_SNP_Info& CSeq_annot_Info::x_GetSNP_annot_Info(void) const
{
    return *m_SNP_Info;
}

inline 
CConstRef<CSeq_annot> CSeq_annot_Info::GetSeq_annotSkeleton(void) const
{
    return m_Object;   
}

inline
const CAnnotObject_Info& CSeq_annot_Info::GetInfo(TIndex index) const
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    return GetAnnotObjectInfos()[index];
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.29  2006/09/18 14:29:29  vasilche
* Store annots indexing information to allow reindexing after modification.
*
* Revision 1.28  2005/09/20 15:45:35  vasilche
* Feature editing API.
* Annotation handles remember annotations by index.
*
* Revision 1.27  2005/08/25 14:05:36  didenko
* Restructured TSE loading process
*
* Revision 1.26  2005/08/23 17:04:20  vasilche
* Use CAnnotObject_Info pointer instead of annotation index in annot handles.
*
* Revision 1.25  2005/08/09 17:38:19  vasilche
* Define constructors/destructors in *.cpp file to allow Windows DLL compilation.
*
* Revision 1.24  2005/06/29 16:10:10  vasilche
* Removed declarations of obsolete methods.
*
* Revision 1.23  2005/06/27 18:17:03  vasilche
* Allow getting CBioseq_set_Handle from CBioseq_set.
*
* Revision 1.22  2005/06/22 14:23:48  vasilche
* Added support for original->edited map.
*
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
