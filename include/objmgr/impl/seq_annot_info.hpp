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
class CTSEAnnotObjectMapper;
class CSeq_annot_SNP_Info;
class CSeqTableInfo;
class CFeat_id;
class CGene_ref;
class CSeq_feat_Handle;

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
    typedef C_Data::TSeq_table  TSeq_table;
    typedef Uint4               TAnnotIndex;

    explicit CSeq_annot_Info(CSeq_annot& annot, int chunk_id = 0);
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

    int GetChunkId(void) const {
        return m_ChunkId;
    }

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

    TAnnotIndex x_GetSNPFeatCount(void) const;
    TAnnotIndex x_GetAnnotCount(void) const;

    void x_DoUpdate(TNeedUpdateFlags flags);

    typedef SAnnotObjectsIndex::TObjectInfos TAnnotObjectInfos;
    const TAnnotObjectInfos& GetAnnotObjectInfos(void) const;

    typedef SAnnotObjectsIndex::TObjectKeys TAnnotObjectKeys;
    const TAnnotObjectKeys& GetAnnotObjectKeys(void) const;
    const SAnnotObject_Key& GetAnnotObjectKey(size_t i) const;

    // individual annotation editing API
    void Remove(TAnnotIndex index);
    void Replace(TAnnotIndex index, const CSeq_feat& new_obj);
    void Replace(TAnnotIndex index, const CSeq_align& new_obj);
    void Replace(TAnnotIndex index, const CSeq_graph& new_obj);
    TAnnotIndex Add(const CSeq_feat& new_obj);
    TAnnotIndex Add(const CSeq_align& new_obj);
    TAnnotIndex Add(const CSeq_graph& new_obj);

    void ReorderFtable(const vector<CSeq_feat_Handle>& feats);

    void Update(TAnnotIndex index);

    void AddFeatId(TAnnotIndex index,
                   const CObject_id& id,
                   EFeatIdType id_type);
    void RemoveFeatId(TAnnotIndex index,
                      const CObject_id& id,
                      EFeatIdType id_type);
    void ClearFeatIds(TAnnotIndex index,
                      EFeatIdType id_type);

    const CAnnotObject_Info& GetInfo(TAnnotIndex index) const;

    const CSeqTableInfo& GetTableInfo(void) const;

    void UpdateTableFeat(CRef<CSeq_feat>& seq_feat,
                         CRef<CSeq_point>& seq_point,
                         CRef<CSeq_interval>& seq_interval,
                         const CAnnotObject_Info& feat_info) const;
    void UpdateTableFeatLocation(CRef<CSeq_loc>& seq_loc,
                                 CRef<CSeq_point>& seq_point,
                                 CRef<CSeq_interval>& seq_interval,
                                 const CAnnotObject_Info& feat_info) const;
    void UpdateTableFeatProduct(CRef<CSeq_loc>& seq_loc,
                                CRef<CSeq_point>& seq_point,
                                CRef<CSeq_interval>& seq_interval,
                                const CAnnotObject_Info& feat_info) const;
    bool IsTableFeatPartial(const CAnnotObject_Info& feat_info) const;

    virtual string GetDescription(void) const;

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
    void x_InitFeatTable(TSeq_table& table);
    void x_InitFeatList(TFtable& objs, const CSeq_annot_Info& info);
    void x_InitAlignList(TAlign& objs, const CSeq_annot_Info& info);
    void x_InitGraphList(TGraph& objs, const CSeq_annot_Info& info);
    void x_InitLocsList(TLocs& annot, const CSeq_annot_Info& info);

    void x_InitAnnotKeys(CTSE_Info& tse);

    void x_InitFeatKeys(CTSE_Info& tse);
    void x_InitAlignKeys(CTSE_Info& tse);
    void x_InitGraphKeys(CTSE_Info& tse);
    void x_InitLocsKeys(CTSE_Info& tse);
    void x_InitFeatTableKeys(CTSE_Info& tse);

    void x_UnmapAnnotObjects(CTSE_Info& tse);
    void x_DropAnnotObjects(CTSE_Info& tse);

    void x_UnmapAnnotObject(CAnnotObject_Info& info);
    void x_MapAnnotObject(CAnnotObject_Info& info);
    void x_RemapAnnotObject(CAnnotObject_Info& info);

    void x_MapFeatIds(CAnnotObject_Info& info);
    void x_UnmapFeatIds(CAnnotObject_Info& info);
    void x_MapFeatById(const CFeat_id& id,
                       CAnnotObject_Info& info,
                       EFeatIdType id_type);
    void x_UnmapFeatById(const CFeat_id& id,
                         CAnnotObject_Info& info,
                         EFeatIdType id_type);
    void x_MapFeatByGene(const CGene_ref& gene, CAnnotObject_Info& info);
    void x_UnmapFeatByGene(const CGene_ref& gene, CAnnotObject_Info& info);

    void x_Map(const CTSEAnnotObjectMapper& mapper,
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

    // Feature table info
    CRef<CSeqTableInfo> m_Table_Info;

    // chunk ID for deterministic ordering
    int m_ChunkId;

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
const CSeq_annot_Info::TAnnotObjectKeys&
CSeq_annot_Info::GetAnnotObjectKeys(void) const
{
    return m_ObjectIndex.GetKeys();
}


inline
const SAnnotObject_Key&
CSeq_annot_Info::GetAnnotObjectKey(size_t i) const
{
    return m_ObjectIndex.GetKey(i);
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
const CSeqTableInfo& CSeq_annot_Info::GetTableInfo(void) const
{
    return *m_Table_Info;
}

inline 
CConstRef<CSeq_annot> CSeq_annot_Info::GetSeq_annotSkeleton(void) const
{
    return m_Object;   
}

inline
const CAnnotObject_Info& CSeq_annot_Info::GetInfo(TAnnotIndex index) const
{
    _ASSERT(index < GetAnnotObjectInfos().size());
    return GetAnnotObjectInfos()[index];
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // SEQ_ANNOT_INFO__HPP
