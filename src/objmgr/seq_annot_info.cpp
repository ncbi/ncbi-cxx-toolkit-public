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
*   CSeq_annot_Info info -- entry for data source information about Seq-annot
*
*/

#define ANNOT_EDIT_COPY 1

#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_SeqAnnot

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


struct SLocColumns
{
    SLocColumns(const char* field_name,
                CSeqTable_column_info::C_Field::EField_int base_value)
        : field_name(field_name),
          base_value(base_value),
          is_set(false),
          is_real_loc(false),
          is_simple(false),
          is_probably_simple(false),
          is_simple_point(false),
          is_simple_interval(false),
          is_simple_whole(false),
          col_loc(0), col_id(0), col_gi(0),
          col_from(0), col_to(0), col_strand(0)
        {
        }

    bool AddColumn(const CSeqTable_column& col);
    
    void SetColumn(const CSeqTable_column*& ptr,
                   const CSeqTable_column& col);

    void ParseDefaults(void);

    void UpdateSeq_loc(size_t row,
                       CRef<CSeq_loc>& seq_loc,
                       CRef<CSeq_point>& seq_pnt,
                       CRef<CSeq_interval>& seq_int) const;

    const CSeq_loc& GetLoc(size_t row) const;
    const CSeq_id& GetId(size_t row) const;
    CSeq_id_Handle GetIdHandle(size_t row) const;
    CRange<TSeqPos> GetRange(size_t row) const;
    ENa_strand GetStrand(size_t row) const;

    CTempString field_name;
    int base_value;
    bool is_set, is_real_loc;
    bool is_simple, is_probably_simple;
    bool is_simple_point, is_simple_interval, is_simple_whole;
    const CSeqTable_column* col_loc;
    const CSeqTable_column* col_id;
    const CSeqTable_column* col_gi;
    const CSeqTable_column* col_from;
    const CSeqTable_column* col_to;
    const CSeqTable_column* col_strand;
    typedef vector<const CSeqTable_column*> TExtraCols;
    TExtraCols extra_cols;

    CSeq_id_Handle default_id_handle;
};


class CSeq_annot_Table_Info : public CObject
{
public:
    CSeq_annot_Table_Info(const CSeq_table& feat_table);

    void UpdateSeq_feat(size_t row,
                        CRef<CSeq_feat>& seq_feat,
                        CRef<CSeq_point>& seq_pnt,
                        CRef<CSeq_interval>& seq_int) const;

    SLocColumns loc, prod;
    const CSeqTable_column* col_partial;
    typedef vector<const CSeqTable_column*> TExtraCols;
    TExtraCols extra_cols;
};


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot& annot)
{
    x_SetObject(annot);
}


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot)
{
    x_SetSNP_annot_Info(snp_annot);
}


CSeq_annot_Info::CSeq_annot_Info(const CSeq_annot_Info& info,
                                 TObjectCopyMap* copy_map)
    : TParent(info, copy_map)
{
    x_SetObject(info, copy_map);
}


CSeq_annot_Info::~CSeq_annot_Info(void)
{
}


const CBioseq_Base_Info& CSeq_annot_Info::GetParentBioseq_Base_Info(void) const
{
    return static_cast<const CBioseq_Base_Info&>(GetBaseParent_Info());
}


CBioseq_Base_Info& CSeq_annot_Info::GetParentBioseq_Base_Info(void)
{
    return static_cast<CBioseq_Base_Info&>(GetBaseParent_Info());
}


const CSeq_entry_Info& CSeq_annot_Info::GetParentSeq_entry_Info(void) const
{
    return GetParentBioseq_Base_Info().GetParentSeq_entry_Info();
}


CSeq_entry_Info& CSeq_annot_Info::GetParentSeq_entry_Info(void)
{
    return GetParentBioseq_Base_Info().GetParentSeq_entry_Info();
}


void CSeq_annot_Info::x_ParentAttach(CBioseq_Base_Info& parent)
{
    x_BaseParentAttach(parent);
}


void CSeq_annot_Info::x_ParentDetach(CBioseq_Base_Info& parent)
{
    x_BaseParentDetach(parent);
}


void CSeq_annot_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    x_DSMapObject(m_Object, ds);
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DSAttach(ds);
    }
}


void CSeq_annot_Info::x_DSDetachContents(CDataSource& ds)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DSDetach(ds);
    }
    x_DSUnmapObject(m_Object, ds);
    TParent::x_DSDetachContents(ds);
}


void CSeq_annot_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
}


void CSeq_annot_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
}


void CSeq_annot_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    CRef<CSeq_annot_SNP_Info> snp_info = tse.x_GetSNP_Info(m_Object);
    if ( snp_info ) {
        _ASSERT(!m_SNP_Info);
        m_SNP_Info = snp_info;
        snp_info->x_ParentAttach(*this);
        _ASSERT(&snp_info->GetParentSeq_annot_Info() == this);
        x_AttachObject(*snp_info);
    }
    TParent::x_TSEAttachContents(tse);
    if ( m_SNP_Info ) {
        m_SNP_Info->x_TSEAttach(tse);
    }
}


void CSeq_annot_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_TSEDetach(tse);
    }
    if ( !x_DirtyAnnotIndex() ) {
        x_UnmapAnnotObjects(tse);
        m_ObjectIndex.Clear();
        x_SetDirtyAnnotIndex();
    }
    TParent::x_TSEDetachContents(tse);
}


const CAnnotName& CSeq_annot_Info::GetName(void) const
{
    return m_Name.IsNamed()? m_Name: 
        HasTSE_Info() ? GetTSE_Info().GetName() : m_Name;
}


void CSeq_annot_Info::x_UpdateName(void)
{
    if ( m_Object->IsSetDesc() ) {
        string name;
        ITERATE( CSeq_annot::TDesc::Tdata, it, m_Object->GetDesc().Get() ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                m_Name.SetNamed(desc.GetName());
                return;
            }
        }
    }
    m_Name.SetUnnamed();
}


CSeq_annot_Info::TAnnotIndex
CSeq_annot_Info::x_GetSNPFeatCount(void) const
{
    return x_GetSNP_annot_Info().size();
}


CSeq_annot_Info::TAnnotIndex
CSeq_annot_Info::x_GetAnnotCount(void) const
{
    return GetAnnotObjectInfos().size();
}


CConstRef<CSeq_annot> CSeq_annot_Info::GetCompleteSeq_annot(void) const
{
    x_UpdateComplete();
    return GetSeq_annotCore();
}


CConstRef<CSeq_annot> CSeq_annot_Info::GetSeq_annotCore(void) const
{
    x_UpdateCore();
    return m_Object;
}


void CSeq_annot_Info::x_SetObject(TObject& obj)
{
    _ASSERT(!m_SNP_Info && !m_Object);
    m_Object.Reset(&obj);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    x_UpdateName();
    x_InitAnnotList();
    x_SetDirtyAnnotIndex();
}


namespace {
    CRef<CSeq_annot> sx_ShallowCopy(const CSeq_annot& src)
    {
        CRef<CSeq_annot> obj(new CSeq_annot);
#if ANNOT_EDIT_COPY
        obj->Assign(src);
#else
        if ( src.IsSetId() ) {
            obj->SetId() = src.GetId();
        }
        if ( src.IsSetDb() ) {
            obj->SetDb(src.GetDb());
        }
        if ( src.IsSetName() ) {
            obj->SetName(src.GetName());
        }
        if ( src.IsSetDesc() ) {
            obj->SetDesc().Set() = src.GetDesc().Get();
        }
#endif
        return obj;
    }
}


void CSeq_annot_Info::x_SetObject(const CSeq_annot_Info& info,
                                  TObjectCopyMap* copy_map)
{
    _ASSERT(!m_SNP_Info && !m_Object);
    m_Object = sx_ShallowCopy(info.x_GetObject());
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    m_Name = info.m_Name;
    if ( info.m_SNP_Info ) {
        m_SNP_Info.Reset(new CSeq_annot_SNP_Info(*info.m_SNP_Info));
        m_SNP_Info->x_ParentAttach(*this);
        x_AttachObject(*m_SNP_Info);
    }
#if ANNOT_EDIT_COPY
    x_InitAnnotList();
#else
    x_InitAnnotList(info);
#endif
    x_SetDirtyAnnotIndex();
}


void CSeq_annot_Info::x_SetSNP_annot_Info(CSeq_annot_SNP_Info& snp_info)
{
    _ASSERT(!m_SNP_Info && !m_Object && !snp_info.HasParent_Info());
    x_SetObject(snp_info.GetRemainingSeq_annot());
    m_SNP_Info.Reset(&snp_info);
    snp_info.x_ParentAttach(*this);
    _ASSERT(&snp_info.GetParentSeq_annot_Info() == this);
    x_AttachObject(snp_info);
}


void CSeq_annot_Info::x_InitAnnotList(void)
{
    _ASSERT(m_Object);
    _ASSERT(m_ObjectIndex.IsEmpty());

    C_Data& data = m_Object->SetData();
    switch ( data.Which() ) {
    case C_Data::e_Ftable:
        x_InitFeatList(data.SetFtable());
        break;
    case C_Data::e_Align:
        x_InitAlignList(data.SetAlign());
        break;
    case C_Data::e_Graph:
        x_InitGraphList(data.SetGraph());
        break;
    case C_Data::e_Locs:
        x_InitLocsList(data.SetLocs());
        break;
    case C_Data::e_Seq_table:
        x_InitFeatTable(data.SetSeq_table());
        break;
    default:
        break;
    }
}


void CSeq_annot_Info::x_InitFeatList(TFtable& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    NON_CONST_ITERATE ( TFtable, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitAlignList(TAlign& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    NON_CONST_ITERATE ( TAlign, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitGraphList(TGraph& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    NON_CONST_ITERATE ( TGraph, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitLocsList(TLocs& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    NON_CONST_ITERATE ( TLocs, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitFeatTable(TSeq_table& table)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex rows = table.GetNum_rows();
    SAnnotTypeSelector type
        (SAnnotTypeSelector::TFeatType(table.GetFeat_type()));
    if ( table.IsSetFeat_subtype() ) {
        type.SetFeatSubtype
            (SAnnotTypeSelector::TFeatSubtype(table.GetFeat_subtype()));
    }
    for ( TAnnotIndex index = 0; index < rows; ++index ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index, type));
    }
    _ASSERT(size_t(rows) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitAnnotList(const CSeq_annot_Info& info)
{
    _ASSERT(m_Object);
    _ASSERT(m_ObjectIndex.IsEmpty());

    const C_Data& src_data = info.x_GetObject().GetData();
    C_Data& data = m_Object->SetData();
    _ASSERT(data.Which() == C_Data::e_not_set);
    switch ( src_data.Which() ) {
    case C_Data::e_Ftable:
        x_InitFeatList(data.SetFtable(), info);
        break;
    case C_Data::e_Align:
        x_InitAlignList(data.SetAlign(), info);
        break;
    case C_Data::e_Graph:
        x_InitGraphList(data.SetGraph(), info);
        break;
    case C_Data::e_Locs:
        x_InitLocsList(data.SetLocs(), info);
        break;
    case C_Data::e_Ids:
        data.SetIds() = src_data.GetIds();
        break;
    case C_Data::e_Seq_table:
        //data.SetSeq_table(src_data.S
        x_InitFeatTable(data.SetSeq_table());
        break;
    default:
        break;
    }
}


void CSeq_annot_Info::x_InitFeatList(TFtable& objs, const CSeq_annot_Info& info)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    ITERATE ( SAnnotObjectsIndex::TObjectInfos, oit,
              info.m_ObjectIndex.GetInfos() ) {
        if ( oit->IsRemoved() ) {
            m_ObjectIndex.AddInfo(CAnnotObject_Info());
        }
        else {
            m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index, objs,
                                                    oit->GetFeat()));
        }
        ++index;
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitAlignList(TAlign& objs, const CSeq_annot_Info& info)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    ITERATE ( SAnnotObjectsIndex::TObjectInfos, oit, info.m_ObjectIndex.GetInfos() ) {
        if ( oit->IsRemoved() ) {
            m_ObjectIndex.AddInfo(CAnnotObject_Info());
        }
        else {
            m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index, objs,
                                                    oit->GetAlign()));
        }
        ++index;
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitGraphList(TGraph& objs, const CSeq_annot_Info& info)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    ITERATE ( SAnnotObjectsIndex::TObjectInfos, oit, info.m_ObjectIndex.GetInfos() ) {
        if ( oit->IsRemoved() ) {
            m_ObjectIndex.AddInfo(CAnnotObject_Info());
        }
        else {
            m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index, objs,
                                                    oit->GetGraph()));
        }
        ++index;
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitLocsList(TLocs& objs, const CSeq_annot_Info& info)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TAnnotIndex index = 0;
    ITERATE ( SAnnotObjectsIndex::TObjectInfos, oit, info.m_ObjectIndex.GetInfos() ) {
        if ( oit->IsRemoved() ) {
            m_ObjectIndex.AddInfo(CAnnotObject_Info());
        }
        else {
            m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index, objs,
                                                    oit->GetLocs()));
        }
        ++index;
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_DoUpdate(TNeedUpdateFlags /*flags*/)
{
    NCBI_THROW(CObjMgrException, eNotImplemented,
               "CSeq_annot_Info::x_DoUpdate: unimplemented");
}


void CSeq_annot_Info::UpdateAnnotIndex(void) const
{
    if ( x_DirtyAnnotIndex() ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
        _ASSERT(!x_DirtyAnnotIndex());
    }
}


void CSeq_annot_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    x_InitAnnotKeys(tse);

    if ( m_SNP_Info ) {
        m_SNP_Info->x_UpdateAnnotIndex(tse);
    }
    TParent::x_UpdateAnnotIndexContents(tse);
}


void CSeq_annot_Info::x_InitAnnotKeys(CTSE_Info& tse)
{
    if ( m_ObjectIndex.IsIndexed() ) {
        return;
    }
    m_ObjectIndex.SetName(GetName());

    C_Data& data = m_Object->SetData();
    switch ( data.Which() ) {
    case C_Data::e_Ftable:
        x_InitFeatKeys(tse);
        break;
    case C_Data::e_Align:
        x_InitAlignKeys(tse);
        break;
    case C_Data::e_Graph:
        x_InitGraphKeys(tse);
        break;
    case C_Data::e_Locs:
        x_InitLocsKeys(tse);
        break;
    case C_Data::e_Seq_table:
        x_InitFeatTableKeys(tse);
        break;
    default:
        break;
    }

    m_ObjectIndex.PackKeys();
    m_ObjectIndex.SetIndexed();
}


inline
void CSeq_annot_Info::x_Map(const CTSEAnnotObjectMapper& mapper,
                            const SAnnotObject_Key& key,
                            const SAnnotObject_Index& index)
{
    m_ObjectIndex.AddMap(key, index);
    mapper.Map(key, index);
}


void CSeq_annot_Info::x_UpdateObjectKeys(CAnnotObject_Info& info,
                                         size_t keys_begin)
{
    size_t keys_end = m_ObjectIndex.GetKeys().size();
    _ASSERT(keys_begin <= keys_end);
    if ( keys_begin + 1 == keys_end &&
         m_ObjectIndex.GetKey(keys_begin).IsSingle() ) {
        // one simple key, store it inside CAnnotObject_Info
        info.SetKey(m_ObjectIndex.GetKey(keys_begin));
        m_ObjectIndex.RemoveLastMap();
    }
    else {
        info.SetKeys(keys_begin, keys_end);
    }
}


void CSeq_annot_Info::x_InitFeatKeys(CTSE_Info& tse)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetFtable().size());
    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(size_t(object_count*1.1));

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps);

        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            bool multi_id = hrmit->GetMap().size() > 1;
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    CNcbiOstrstream s;
                    s << MSerial_AsnText << *info.GetFeatFast();
                    ERR_POST_X(1, "Empty region in "<<s.rdbuf());
                    continue;
                }
                key.m_Handle = hrit->first;
                index.m_Flags = hr.GetStrandsFlag();
                if ( multi_id ) {
                    index.SetMultiIdFlag();
                }
                if ( info.GetFeatFast()->IsSetPartial() ) {
                    index.SetPartial(info.GetFeatFast()->GetPartial());
                }
                if ( hr.HasGaps() ) {
                    index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    index.m_HandleRange->GetData() = hr;
                    if ( hr.IsCircular() ) {
                        key.m_Range = hr.GetCircularRangeStart();
                        x_Map(mapper, key, index);
                        key.m_Range = hr.GetCircularRangeEnd();
                    }
                }
                else {
                    index.m_HandleRange.Reset();
                }
                x_Map(mapper, key, index);
            }
            ++index.m_AnnotLocationIndex;
        }
        x_UpdateObjectKeys(info, keys_begin);
        x_MapFeatIds(info);
    }
}


void CSeq_annot_Info::x_InitGraphKeys(CTSE_Info& tse)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetGraph().size());
    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(object_count);

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    CNcbiOstrstream s;
                    s << MSerial_AsnText << *info.GetGraphFast();
                    ERR_POST_X(2, "Empty region in "<<s.rdbuf());
                    continue;
                }
                key.m_Handle = hrit->first;
                if ( hr.HasGaps() ) {
                    index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    index.m_HandleRange->GetData() = hr;
                }
                else {
                    index.m_HandleRange.Reset();
                }

                x_Map(mapper, key, index);
            }
            ++index.m_AnnotLocationIndex;
        }
        x_UpdateObjectKeys(info, keys_begin);
    }
}


void CSeq_annot_Info::x_InitAlignKeys(CTSE_Info& tse)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetAlign().size());
    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(object_count);

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    CNcbiOstrstream s;
                    s << MSerial_AsnText << info.GetAlign();
                    ERR_POST_X(3, "Empty region in "<<s.rdbuf());
                    continue;
                }
                key.m_Handle = hrit->first;
                if ( hr.HasGaps() ) {
                    index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                    index.m_HandleRange->GetData() = hr;
                }
                else {
                    index.m_HandleRange.Reset();
                }

                x_Map(mapper, key, index);
            }
            ++index.m_AnnotLocationIndex;
        }
        x_UpdateObjectKeys(info, keys_begin);
    }
}


void CSeq_annot_Info::x_InitLocsKeys(CTSE_Info& tse)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetLocs().size());
    // Only one referenced location per annot is allowed
    if ( m_ObjectIndex.GetInfos().size() != 1) {
        return;
    }

    CAnnotObject_Info& info = m_ObjectIndex.GetInfos().front();
    if ( info.IsRemoved() ) {
        return;
    }

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    size_t keys_begin = m_ObjectIndex.GetKeys().size();
    index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps);
    index.m_AnnotLocationIndex = 0;

    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                CNcbiOstrstream s;
                s << MSerial_AsnText << info.GetLocs();
                ERR_POST_X(4, "Empty region in "<<s.rdbuf());
                continue;
            }
            key.m_Handle = hrit->first;
            if ( hr.HasGaps() ) {
                index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                index.m_HandleRange->GetData() = hr;
            }
            else {
                index.m_HandleRange.Reset();
            }
            x_Map(mapper, key, index);
        }
    }
    x_UpdateObjectKeys(info, keys_begin);
}


void SLocColumns::SetColumn(const CSeqTable_column*& ptr,
                            const CSeqTable_column& col)
{
    if ( ptr ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "Duplicate "<<field_name<<" column");
    }
    ptr = &col;
    is_set = true;
}


bool SLocColumns::AddColumn(const CSeqTable_column& col)
{
    const CSeqTable_column_info& type = col.GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt() - base_value +
            CSeqTable_column_info::C_Field::eField_int_location;
        if ( field >= CSeqTable_column_info::C_Field::eField_int_product ) {
            return false;
        }
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location:
            SetColumn(col_loc, col);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_id:
            SetColumn(col_id, col);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_gi:
            SetColumn(col_gi, col);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_from:
            SetColumn(col_from, col);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_to:
            SetColumn(col_to, col);
            return true;
        case CSeqTable_column_info::C_Field::eField_int_location_strand:
            SetColumn(col_strand, col);
            return true;
        default:
            extra_cols.push_back(&col);
            return true;
        }
    }
    else {
        CTempString field(type.GetField().GetStr());
        if ( field == field_name ) {
            SetColumn(col_loc, col);
            return true;
        }
        else if ( NStr::StartsWith(field, field_name) &&
                  field[field_name.size()] == '.' ) {
            CTempString extra = field.substr(field_name.size()+1);
            if ( extra == "id" || NStr::EndsWith(extra, ".id") ) {
                SetColumn(col_id, col);
            }
            else if ( extra == "gi" || NStr::EndsWith(extra, ".gi") ) {
                SetColumn(col_gi, col);
            }
            else if ( extra == "pnt.point" || extra == "int.from" ) {
                SetColumn(col_from, col);
            }
            else if ( extra == "int.to" ) {
                SetColumn(col_to, col);
            }
            else if ( extra == "strand" ||
                      NStr::EndsWith(extra, ".strand") ) {
                SetColumn(col_strand, col);
            }
            else {
                extra_cols.push_back(&col);
                is_set = true;
            }
            return true;
        }
        else {
            return false;
        }
    }
}


void SLocColumns::ParseDefaults(void)
{
    if ( !is_set ) {
        return;
    }
    if ( col_loc ) {
        is_real_loc = true;
        if ( col_id || col_gi || col_from || col_to || col_strand ||
             !extra_cols.empty() ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "Conflicting "<<field_name<<" columns");
        }
        return;
    }

    if ( !col_id && !col_gi ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "No "<<field_name<<".id column");
    }
    if ( col_id && col_gi ) {
        NCBI_THROW_FMT(CAnnotException, eBadLocation,
                       "Conflicting "<<field_name<<" columns");
    }
    if ( col_id ) {
        if ( col_id->IsSetDefault() ) {
            default_id_handle = CSeq_id_Handle::GetHandle
                (col_id->GetDefault().GetId());
        }
    }
    if ( col_gi ) {
        if ( col_gi->IsSetDefault() ) {
            default_id_handle = CSeq_id_Handle::GetGiHandle
                (col_gi->GetDefault().GetInt());
        }
    }

    if ( col_to ) {
        // interval
        if ( !col_from ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "column "<<field_name<<".to without "<<
                           field_name<<".from");
        }
        is_simple_interval = true;
    }
    else if ( col_from ) {
        // point
        is_simple_point = true;
    }
    else {
        // whole
        if ( col_strand || !extra_cols.empty() ) {
            NCBI_THROW_FMT(CAnnotException, eBadLocation,
                           "extra columns in whole "<<field_name);
        }
        is_simple_whole = true;
    }
    if ( extra_cols.empty() ) {
        is_simple = true;
    }
    else {
        is_probably_simple = true;
    }
}


static inline
size_t sx_GetRowIndex(const CSeqTable_column& col, size_t row)
{
    if ( !col.IsSetSparse() ) {
        return row;
    }
    const CSeqTable_sparse_index::TIndexes& idx =
        col.GetSparse().GetIndexes();
    CSeqTable_sparse_index::TIndexes::const_iterator iter = 
        lower_bound(idx.begin(), idx.end(), int(row));
    if ( iter != idx.end() && *iter == int(row) ) {
        return iter-idx.begin();
    }
    return size_t(-1); // maximum
}


static inline
bool sx_IsColumnSet(const CSeqTable_column& col, size_t row)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        switch ( col.GetData().Which() ) {
        case CSeqTable_multi_data::e_Int:
            if ( index < col.GetData().GetInt().size() ) {
                return true;
            }
            break;
        case CSeqTable_multi_data::e_Real:
            if ( index < col.GetData().GetReal().size() ) {
                return true;
            }
            break;
        case CSeqTable_multi_data::e_String:
            if ( index < col.GetData().GetString().size() ) {
                return true;
            }
            break;
        default:
            ERR_POST("Bad field data type: "<<col.GetData().Which());
            break;
        }
    }
    return col.IsSetDefault();
}


static inline
bool sx_GetColumnBool(const CSeqTable_column& col, size_t row)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        const CSeqTable_multi_data::TBit& arr = col.GetData().GetBit();
        if ( index < arr.size()*8 ) {
            size_t byte = index / 8;
            size_t bit = index % 8;
            return ((arr[byte]<<bit)&0x80) != 0;
        }
    }
    return col.IsSetDefault() && col.GetDefault().GetBit();
}


static inline
int sx_GetColumnInt(const CSeqTable_column& col, size_t row, int def)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        const CSeqTable_multi_data::TInt& arr = col.GetData().GetInt();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    return col.IsSetDefault()? col.GetDefault().GetInt(): def;
}


static inline
const string& sx_GetColumnString(const CSeqTable_column& col, size_t row)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        const CSeqTable_multi_data::TString& arr = col.GetData().GetString();
        if ( index < arr.size() ) {
            return arr[index];
        }
    }
    return col.GetDefault().GetString();
}


const CSeq_loc& SLocColumns::GetLoc(size_t row) const
{
    _ASSERT(col_loc);
    _ASSERT(!col_loc->IsSetDefault());
    _ASSERT(!col_loc->IsSetSparse());
    return *col_loc->GetData().GetLoc()[row];
}


const CSeq_id& SLocColumns::GetId(size_t row) const
{
    _ASSERT(!col_loc);
    _ASSERT(col_id);
    _ASSERT(!col_id->IsSetSparse());
    if ( col_id->IsSetData() ) {
        const CSeqTable_multi_data::TId& ids = col_id->GetData().GetId();
        if ( row < ids.size() ) {
            return *ids[row];
        }
    }
    return col_id->GetDefault().GetId();
}


CSeq_id_Handle SLocColumns::GetIdHandle(size_t row) const
{
    _ASSERT(!col_loc);
    if ( col_id ) {
        _ASSERT(!col_id->IsSetSparse());
        if ( col_id->IsSetData() ) {
            const CSeqTable_multi_data::TId& ids = col_id->GetData().GetId();
            if ( row < ids.size() ) {
                return CSeq_id_Handle::GetHandle(*ids[row]);
            }
        }
    }
    else {
        _ASSERT(!col_gi->IsSetSparse());
        if ( col_gi->IsSetData() ) {
            int gi = sx_GetColumnInt(*col_gi, row, 0);
            return CSeq_id_Handle::GetGiHandle(gi);
        }
    }
    return default_id_handle;
}


CRange<TSeqPos> SLocColumns::GetRange(size_t row) const
{
    _ASSERT(!col_loc);
    _ASSERT(col_from);
    if ( !col_from ) {
        return CRange<TSeqPos>::GetWhole();
    }
    TSeqPos from = sx_GetColumnInt(*col_from, row, kInvalidSeqPos);
    if ( from == kInvalidSeqPos ) {
        return CRange<TSeqPos>::GetWhole();
    }
    TSeqPos to = col_to? sx_GetColumnInt(*col_to, row, from): from;
    return CRange<TSeqPos>(from, to);
}


ENa_strand SLocColumns::GetStrand(size_t row) const
{
    _ASSERT(!col_loc);
    if ( !col_strand ) {
        return eNa_strand_unknown;
    }
    return ENa_strand(sx_GetColumnInt(*col_strand, row, eNa_strand_unknown));
}


CSeq_annot_Table_Info::CSeq_annot_Table_Info(const CSeq_table& feat_table)
    : loc("loc", CSeqTable_column_info::C_Field::eField_int_location),
      prod("product",  CSeqTable_column_info::C_Field::eField_int_product),
      col_partial(0)
{
    ITERATE ( CSeq_table::TColumns, it, feat_table.GetColumns() ) {
        const CSeqTable_column& col = **it;
        if ( loc.AddColumn(col) || prod.AddColumn(col) ) {
            continue;
        }
        const CSeqTable_column_info& type = col.GetHeader();
        if ( type.GetField().IsInt() ) {
            if ( type.GetField().GetInt() ==
                 CSeqTable_column_info::C_Field::eField_int_partial ) {
                if ( col_partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                col_partial = &col;
            }
            else {
                extra_cols.push_back(&col);
            }
        }
        else {
            CTempString field(type.GetField().GetStr());
            if ( field == "partial" ) {
                if ( col_partial ) {
                    NCBI_THROW_FMT(CAnnotException, eOtherError,
                                   "Duplicate partial column");
                }
                col_partial = &col;
            }
            else {
                extra_cols.push_back(&col);
            }
        }
    }

    loc.ParseDefaults();
    prod.ParseDefaults();
}


static inline
void sx_SetTableKeyAndIndex(size_t row,
                            SAnnotObject_Key& key,
                            SAnnotObject_Index& index,
                            const SLocColumns& loc,
                            const CSeqTable_column* col_partial)
{
    key.m_Handle = loc.GetIdHandle(row);
    key.m_Range = loc.GetRange(row);
    ENa_strand strand = loc.GetStrand(row);
    index.m_Flags = 0;
    if ( strand == eNa_strand_unknown ) {
        index.m_Flags |= index.fStrand_both;
    }
    else {
        if ( IsForward(strand) ) {
            index.m_Flags |= index.fStrand_plus;
        }
        if ( IsReverse(strand) ) {
            index.m_Flags |= index.fStrand_minus;
        }
    }
    if ( col_partial && sx_GetColumnBool(*col_partial, row) ) {
        index.SetPartial(true);
    }
    bool simple = loc.is_simple;
    if ( !simple && loc.is_probably_simple ) {
        simple = true;
        ITERATE ( SLocColumns::TExtraCols, it, loc.extra_cols ) {
            if ( sx_IsColumnSet(**it, row) ) {
                simple = false;
                break;
            }
        }
    }
    if ( simple ) {
        if ( loc.is_simple_interval ) {
            index.SetLocationIsInterval();
        }
        else if ( loc.is_simple_point ) {
            index.SetLocationIsPoint();
        }
        else {
            _ASSERT(loc.is_simple_whole);
            index.SetLocationIsWhole();
        }
    }
}


struct SFeatSetField
{
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_loc& loc, double value) const;
    virtual void Set(CSeq_loc& loc, const string& value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, double value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


void SFeatSetField::Set(CSeq_loc& loc, int value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void SFeatSetField::Set(CSeq_loc& loc, double value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void SFeatSetField::Set(CSeq_loc& loc, const string& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-loc field value: "<<value);
}


void SFeatSetField::Set(CSeq_feat& feat, int value) const
{
    Set(feat, NStr::IntToString(value));
}


void SFeatSetField::Set(CSeq_feat& feat, double value) const
{
    Set(feat, NStr::DoubleToString(value));
}


void SFeatSetField::Set(CSeq_feat& feat, const string& value) const
{
    NCBI_THROW_FMT(CAnnotException, eOtherError,
                   "Incompatible Seq-feat field value: "<<value);
}


struct SFeatSetComment : public SFeatSetField
{
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


void SFeatSetComment::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetComment(value);
}


struct SFeatSetDataImpKey : public SFeatSetField
{
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


void SFeatSetDataImpKey::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetData().SetImp().SetKey(value);
}


struct SFeatSetDataRegion : public SFeatSetField
{
    virtual void Set(CSeq_feat& feat, const string& value) const;
};


void SFeatSetDataRegion::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetData().SetRegion(value);
}


struct SFeatSetLocFuzzFromLim : public SFeatSetField
{
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


void SFeatSetLocFuzzFromLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsPnt() ) {
        loc.SetPnt().SetFuzz().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_from().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void SFeatSetLocFuzzFromLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetLocation(), value);
}


struct SFeatSetLocFuzzToLim : public SFeatSetField
{
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


void SFeatSetLocFuzzToLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_to().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void SFeatSetLocFuzzToLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetLocation(), value);
}


struct SFeatSetProdFuzzFromLim : public SFeatSetField
{
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


void SFeatSetProdFuzzFromLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsPnt() ) {
        loc.SetPnt().SetFuzz().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_from().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void SFeatSetProdFuzzFromLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetProduct(), value);
}


struct SFeatSetProdFuzzToLim : public SFeatSetField
{
    virtual void Set(CSeq_loc& loc, int value) const;
    virtual void Set(CSeq_feat& feat, int value) const;
};


void SFeatSetProdFuzzToLim::Set(CSeq_loc& loc, int value) const
{
    if ( loc.IsInt() ) {
        loc.SetInt().SetFuzz_to().SetLim(CInt_fuzz_Base::ELim(value));
    }
    else {
        NCBI_THROW_FMT(CAnnotException, eOtherError,
                       "Incompatible fuzz field");
    }
}


void SFeatSetProdFuzzToLim::Set(CSeq_feat& feat, int value) const
{
    Set(feat.SetProduct(), value);
}


struct SFeatSetQual : public SFeatSetField
{
    SFeatSetQual(const CTempString& name)
        : name(name)
        {
        }

    virtual void Set(CSeq_feat& feat, const string& value) const;

    CTempString name;
};


void SFeatSetQual::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CGb_qual> qual(new CGb_qual);
    qual->SetQual(name);
    qual->SetVal(value);
    feat.SetQual().push_back(qual);
}


struct SFeatSetExt : public SFeatSetField
{
    SFeatSetExt(const CTempString& name)
        : name(name)
        {
        }

    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, double value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;

    CTempString name;
};


void SFeatSetExt::Set(CSeq_feat& feat, int value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetInt(value);
    feat.SetExt().SetData().push_back(field);
}


void SFeatSetExt::Set(CSeq_feat& feat, double value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetReal(value);
    feat.SetExt().SetData().push_back(field);
}


void SFeatSetExt::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CUser_field> field(new CUser_field);
    field->SetLabel().SetStr(name);
    field->SetData().SetStr(value);
    feat.SetExt().SetData().push_back(field);
}


struct SFeatSetExtType : public SFeatSetField
{
    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;

    CTempString name;
};


void SFeatSetExtType::Set(CSeq_feat& feat, int value) const
{
    feat.SetExt().SetType().SetId(value);
}


void SFeatSetExtType::Set(CSeq_feat& feat, const string& value) const
{
    feat.SetExt().SetType().SetStr(value);
}


struct SFeatSetDbxref : public SFeatSetField
{
    SFeatSetDbxref(const CTempString& name)
        : name(name)
        {
        }

    virtual void Set(CSeq_feat& feat, int value) const;
    virtual void Set(CSeq_feat& feat, const string& value) const;

    CTempString name;
};


void SFeatSetDbxref::Set(CSeq_feat& feat, int value) const
{
    CRef<CDbtag> dbtag(new CDbtag);
    dbtag->SetDb(name);
    dbtag->SetTag().SetId(value);
    feat.SetDbxref().push_back(dbtag);
}


void SFeatSetDbxref::Set(CSeq_feat& feat, const string& value) const
{
    CRef<CDbtag> dbtag(new CDbtag);
    dbtag->SetDb(name);
    dbtag->SetTag().SetStr(value);
    feat.SetDbxref().push_back(dbtag);
}


static
void sx_SetFeatField(CSeq_loc& loc, const SFeatSetField& field,
                     const CSeqTable_column& col, size_t row)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        if ( index != size_t(-1) ) {
            switch ( col.GetData().Which() ) {
            case CSeqTable_multi_data::e_Int:
                if ( index < col.GetData().GetInt().size() ) {
                    field.Set(loc, col.GetData().GetInt()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_Real:
                if ( index < col.GetData().GetReal().size() ) {
                    field.Set(loc, col.GetData().GetReal()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_String:
                if ( index < col.GetData().GetString().size() ) {
                    field.Set(loc, col.GetData().GetString()[index]);
                    return;
                }
                break;
            default:
                ERR_POST("Bad field data type: "<<col.GetData().Which());
                return;
            }
        }
    }
    if ( col.IsSetDefault() ) {
        switch ( col.GetDefault().Which() ) {
        case CSeqTable_single_data::e_Int:
            field.Set(loc, col.GetDefault().GetInt());
            return;
        case CSeqTable_single_data::e_Real:
            field.Set(loc, col.GetDefault().GetReal());
            return;
        case CSeqTable_single_data::e_String:
            field.Set(loc, col.GetDefault().GetString());
            return;
        default:
            ERR_POST("Bad field data type: "<<col.GetDefault().Which());
            return;
        }
    }
}


static
void sx_SetFeatField(CSeq_feat& feat, const SFeatSetField& field,
                     const CSeqTable_column& col, size_t row)
{
    if ( col.IsSetData() ) {
        size_t index = sx_GetRowIndex(col, row);
        if ( index != size_t(-1) ) {
            switch ( col.GetData().Which() ) {
            case CSeqTable_multi_data::e_Int:
                if ( index < col.GetData().GetInt().size() ) {
                    field.Set(feat, col.GetData().GetInt()[index]);
                    return;
                }
                break;
            case CSeqTable_multi_data::e_Real:
                if ( index < col.GetData().GetReal().size() ) {
                    field.Set(feat, col.GetData().GetReal()[index]);
                    return;
                }
                return;
            case CSeqTable_multi_data::e_String:
                if ( index < col.GetData().GetString().size() ) {
                    field.Set(feat, col.GetData().GetString()[index]);
                    return;
                }
                break;
            default:
                ERR_POST("Bad field data type: "<<col.GetData().Which());
                return;
            }
        }
    }
    if ( col.IsSetDefault() ) {
        switch ( col.GetDefault().Which() ) {
        case CSeqTable_single_data::e_Int:
            field.Set(feat, col.GetDefault().GetInt());
            return;
        case CSeqTable_single_data::e_Real:
            field.Set(feat, col.GetDefault().GetReal());
            return;
        case CSeqTable_single_data::e_String:
            field.Set(feat, col.GetDefault().GetString());
            return;
        default:
            ERR_POST("Bad field data type: "<<col.GetDefault().Which());
            return;
        }
    }
}


static
void sx_UpdateField(CSeq_loc& loc, const CSeqTable_column& col, size_t row)
{
    const CSeqTable_column_info& type = col.GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt();
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_from_lim:
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_from_lim:
            sx_SetFeatField(loc, SFeatSetLocFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_to_lim:
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_to_lim:
            sx_SetFeatField(loc, SFeatSetLocFuzzToLim(), col, row);
            break;
        default:
            ERR_POST("SeqTable-column-info.field.int = "<<field);
            break;
        }
    }
    else {
        CTempString field = type.GetField().GetStr();
        if ( NStr::StartsWith(field, "location.") ) {
            field = field.substr(9);
        }
        else if ( NStr::StartsWith(field, "product.") ) {
            field = field.substr(8);
        }
        else {
            ERR_POST("SeqTable-column-info.field.str = "<<field);
        }
        ERR_POST("SeqTable-column-info.field.str = "<<field);
    }
}


static
void sx_UpdateField(CSeq_feat& feat, const CSeqTable_column& col, size_t row)
{
    const CSeqTable_column_info& type = col.GetHeader();
    if ( type.GetField().IsInt() ) {
        int field = type.GetField().GetInt();
        switch ( field ) {
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_from_lim:
            sx_SetFeatField(feat, SFeatSetLocFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_location_fuzz_to_lim:
            sx_SetFeatField(feat, SFeatSetLocFuzzToLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_from_lim:
            sx_SetFeatField(feat, SFeatSetProdFuzzFromLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_product_fuzz_to_lim:
            sx_SetFeatField(feat, SFeatSetProdFuzzToLim(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_comment:
            sx_SetFeatField(feat, SFeatSetComment(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_data_imp_key:
            sx_SetFeatField(feat, SFeatSetDataImpKey(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_data_region:
            sx_SetFeatField(feat, SFeatSetDataRegion(), col, row);
            break;
        case CSeqTable_column_info::C_Field::eField_int_ext_type:
            sx_SetFeatField(feat, SFeatSetExtType(), col, row);
            break;
        default:
            ERR_POST("SeqTable-column-info.field.int = "<<field);
            break;
        }
    }
    else {
        CTempString field = type.GetField().GetStr();
        if ( field[0] == 'E' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, SFeatSetExt(field.substr(2)), col, row);
        }
        else if ( field[0] == 'D' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, SFeatSetDbxref(field.substr(2)), col, row);
        }
        else if ( field[0] == 'Q' && field.size() > 1 && field[1] == '.' ) {
            sx_SetFeatField(feat, SFeatSetQual(field.substr(2)), col, row);
        }
        else {
            ERR_POST("SeqTable-column-info.field.str = "<<field);
        }
    }
}


static
void sx_UpdateFields(CSeq_loc& loc,
                     const SLocColumns::TExtraCols& cols,
                     size_t row)
{
    ITERATE ( SLocColumns::TExtraCols, it, cols ) {
        sx_UpdateField(loc, **it, row);
    }
}


static
void sx_UpdateFields(CSeq_feat& feat,
                     const CSeq_annot_Table_Info::TExtraCols& cols,
                     size_t row)
{
    ITERATE ( CSeq_annot_Table_Info::TExtraCols, it, cols ) {
        sx_UpdateField(feat, **it, row);
    }
}


void SLocColumns::UpdateSeq_loc(size_t row,
                                CRef<CSeq_loc>& seq_loc,
                                CRef<CSeq_point>& seq_pnt,
                                CRef<CSeq_interval>& seq_int) const
{
    _ASSERT(is_set);
    if ( col_loc ) {
        seq_loc = &const_cast<CSeq_loc&>(GetLoc(row));
        return;
    }
    if ( !seq_loc ) {
        seq_loc = new CSeq_loc();
    }
    CSeq_loc& loc = *seq_loc;

    CSeq_id* id = 0;
    int gi = 0;
    if ( col_id ) {
        id = &const_cast<CSeq_id&>(GetId(row));
    }
    else {
        _ASSERT(col_gi);
        gi = sx_GetColumnInt(*col_gi, row, 0);
    }

    TSeqPos from = !col_from? kInvalidSeqPos:
        sx_GetColumnInt(*col_from, row, kInvalidSeqPos);
    if ( from == kInvalidSeqPos ) {
        // whole
        if ( id ) {
            loc.SetWhole(*id);
        }
        else {
            loc.SetWhole().SetGi(gi);
        }
    }
    else {
        TSeqPos to = !col_to? kInvalidSeqPos:
            sx_GetColumnInt(*col_to, row, kInvalidSeqPos);

        int strand = !col_strand? -1: sx_GetColumnInt(*col_strand, row, -1);
        if ( to == kInvalidSeqPos ) {
            // point
            if ( !seq_pnt ) {
                seq_pnt = new CSeq_point();
            }
            else {
                seq_pnt->Reset();
            }
            CSeq_point& point = *seq_pnt;
            if ( id ) {
                point.SetId(*id);
            }
            else {
                point.SetId().SetGi(gi);
            }
            point.SetPoint(from);
            if ( strand >= 0 ) {
                point.SetStrand(ENa_strand(strand));
            }
            loc.SetPnt(point);
        }
        else {
            // interval
            if ( !seq_int ) {
                seq_int = new CSeq_interval();
            }
            else {
                seq_int->Reset();
            }
            CSeq_interval& interval = *seq_int;
            if ( id ) {
                interval.SetId(*id);
            }
            else {
                interval.SetId().SetGi(gi);
            }
            interval.SetFrom(from);
            interval.SetTo(to);
            if ( strand >= 0 ) {
                interval.SetStrand(ENa_strand(strand));
            }
            loc.SetInt(interval);
        }
    }
    sx_UpdateFields(loc, extra_cols, row);
}


void CSeq_annot_Table_Info::UpdateSeq_feat(size_t row,
                                           CRef<CSeq_feat>& seq_feat,
                                           CRef<CSeq_point>& seq_pnt,
                                           CRef<CSeq_interval>& seq_int) const
{
    if ( !seq_feat ) {
        seq_feat = new CSeq_feat;
    }
    else {
        seq_feat->Reset();
    }
    CSeq_feat& feat = *seq_feat;
    if ( loc.is_set ) {
        CRef<CSeq_loc> seq_loc;
        if ( feat.IsSetLocation() ) {
            seq_loc = &feat.SetLocation();
        }
        loc.UpdateSeq_loc(row, seq_loc, seq_pnt, seq_int);
        feat.SetLocation(*seq_loc);
    }
    if ( prod.is_set ) {
        CRef<CSeq_loc> seq_loc;
        CRef<CSeq_point> seq_pnt;
        CRef<CSeq_interval> seq_int;
        if ( feat.IsSetProduct() ) {
            seq_loc = &feat.SetProduct();
        }
        prod.UpdateSeq_loc(row, seq_loc, seq_pnt, seq_int);
        feat.SetProduct(*seq_loc);
    }
    if ( col_partial && sx_GetColumnBool(*col_partial, row) ) {
        feat.SetPartial(true);
    }
    sx_UpdateFields(feat, extra_cols, row);
}


void CSeq_annot_Info::UpdateTableFeat(CRef<CSeq_feat>& seq_feat,
                                      CRef<CSeq_point>& seq_pnt,
                                      CRef<CSeq_interval>& seq_int,
                                      const CAnnotObject_Info& info) const
{
    m_Table_Info->UpdateSeq_feat(info.GetAnnotIndex(),
                                 seq_feat, seq_pnt, seq_int);
}


void CSeq_annot_Info::UpdateTableFeatLocation(
    CRef<CSeq_loc>& seq_loc,
    CRef<CSeq_point>& seq_pnt,
    CRef<CSeq_interval>& seq_int,
    const CAnnotObject_Info& info) const
{
    m_Table_Info->loc.UpdateSeq_loc(info.GetAnnotIndex(),
                                    seq_loc, seq_pnt, seq_int);
}


void CSeq_annot_Info::UpdateTableFeatProduct(
    CRef<CSeq_loc>& seq_loc,
    CRef<CSeq_point>& seq_pnt,
    CRef<CSeq_interval>& seq_int,
    const CAnnotObject_Info& info) const
{
    m_Table_Info->prod.UpdateSeq_loc(info.GetAnnotIndex(),
                                     seq_loc, seq_pnt, seq_int);
}


bool CSeq_annot_Info::IsTableFeatPartial(const CAnnotObject_Info& info) const
{
    return m_Table_Info->col_partial &&
        sx_GetColumnBool(*m_Table_Info->col_partial, info.GetAnnotIndex());
}


void CSeq_annot_Info::x_InitFeatTableKeys(CTSE_Info& tse)
{
    const CSeq_table& feat_table = m_Object->GetData().GetSeq_table();
    size_t object_count = m_ObjectIndex.GetInfos().size();
    _ASSERT(object_count == size_t(feat_table.GetNum_rows()));
    m_ObjectIndex.ReserveMapSize(object_count);

    m_Table_Info = new CSeq_annot_Table_Info(feat_table);

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    SAnnotObjectsIndex::TObjectInfos::iterator it =
        m_ObjectIndex.GetInfos().begin();
    for ( size_t row = 0; row < object_count; ++row, ++it ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;

        if ( m_Table_Info->loc.is_set ) {
            index.m_AnnotLocationIndex = 0;
            if ( m_Table_Info->loc.is_real_loc ) {
                CHandleRangeMap hrmap;
                hrmap.AddLocation(m_Table_Info->loc.GetLoc(row));
                bool multi_id = hrmap.GetMap().size() > 1;
                ITERATE ( CHandleRangeMap, hrit, hrmap ) {
                    const CHandleRange& hr = hrit->second;
                    key.m_Range = hr.GetOverlappingRange();
                    if ( key.m_Range.Empty() ) {
                        CNcbiOstrstream s;
                        s << MSerial_AsnText << *info.GetFeatFast();
                        ERR_POST_X(7, "Empty region in "<<s.rdbuf());
                        continue;
                    }
                    key.m_Handle = hrit->first;
                    index.m_Flags = hr.GetStrandsFlag();
                    if ( multi_id ) {
                        index.SetMultiIdFlag();
                    }
                    if ( hr.HasGaps() ) {
                        index.m_HandleRange = new CObjectFor<CHandleRange>;
                        index.m_HandleRange->GetData() = hr;
                        if ( hr.IsCircular() ) {
                            key.m_Range = hr.GetCircularRangeStart();
                            x_Map(mapper, key, index);
                            key.m_Range = hr.GetCircularRangeEnd();
                        }
                    }
                    else {
                        index.m_HandleRange.Reset();
                    }
                    x_Map(mapper, key, index);
                }
            }
            else {
                sx_SetTableKeyAndIndex(row, key, index,
                                       m_Table_Info->loc,
                                       m_Table_Info->col_partial);
                x_Map(mapper, key, index);
            }
        }
        
        if ( m_Table_Info->prod.is_set ) {
            index.m_AnnotLocationIndex = 1;
            if ( m_Table_Info->prod.is_real_loc ) {
                CHandleRangeMap hrmap;
                hrmap.AddLocation(m_Table_Info->prod.GetLoc(row));
                bool multi_id = hrmap.GetMap().size() > 1;
                ITERATE ( CHandleRangeMap, hrit, hrmap ) {
                    const CHandleRange& hr = hrit->second;
                    key.m_Range = hr.GetOverlappingRange();
                    if ( key.m_Range.Empty() ) {
                        CNcbiOstrstream s;
                        s << MSerial_AsnText << *info.GetFeatFast();
                        ERR_POST_X(8, "Empty region in "<<s.rdbuf());
                        continue;
                    }
                    key.m_Handle = hrit->first;
                    index.m_Flags = hr.GetStrandsFlag();
                    if ( multi_id ) {
                        index.SetMultiIdFlag();
                    }
                    if ( hr.HasGaps() ) {
                        index.m_HandleRange = new CObjectFor<CHandleRange>;
                        index.m_HandleRange->GetData() = hr;
                        if ( hr.IsCircular() ) {
                            key.m_Range = hr.GetCircularRangeStart();
                            x_Map(mapper, key, index);
                            key.m_Range = hr.GetCircularRangeEnd();
                        }
                    }
                    else {
                        index.m_HandleRange.Reset();
                    }
                    x_Map(mapper, key, index);
                }
            }
            else {
                sx_SetTableKeyAndIndex(row, key, index,
                                       m_Table_Info->prod,
                                       m_Table_Info->col_partial);
                x_Map(mapper, key, index);
            }
        }

        x_UpdateObjectKeys(info, keys_begin);
        //x_MapFeatIds(info);
    }
}


void CSeq_annot_Info::x_MapFeatById(const CFeat_id& id,
                                    CAnnotObject_Info& info,
                                    bool xref)
{
    if ( id.IsLocal() ) {
        const CObject_id& obj_id = id.GetLocal();
        if ( obj_id.IsId() ) {
            GetTSE_Info().x_MapFeatById(obj_id.GetId(), info, xref);
        }
    }
}


void CSeq_annot_Info::x_UnmapFeatById(const CFeat_id& id,
                                      CAnnotObject_Info& info,
                                      bool xref)
{
    if ( id.IsLocal() ) {
        const CObject_id& obj_id = id.GetLocal();
        if ( obj_id.IsId() ) {
            GetTSE_Info().x_UnmapFeatById(obj_id.GetId(), info, xref);
        }
    }
}


void CSeq_annot_Info::x_MapFeatIds(CAnnotObject_Info& info)
{
    const CSeq_feat& feat = *info.GetFeatFast();
    if ( feat.IsSetId() ) {
        x_MapFeatById(feat.GetId(), info, true);
    }
    if ( feat.IsSetIds() ) {
        ITERATE ( CSeq_feat::TIds, it, feat.GetIds() ) {
            x_MapFeatById(**it, info, true);
        }
    }
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                x_MapFeatById(xref.GetId(), info, false);
            }
        }
    }
}


void CSeq_annot_Info::x_UnmapFeatIds(CAnnotObject_Info& info)
{
    const CSeq_feat& feat = *info.GetFeatFast();
    if ( feat.IsSetId() ) {
        x_UnmapFeatById(feat.GetId(), info, true);
    }
    if ( feat.IsSetIds() ) {
        ITERATE ( CSeq_feat::TIds, it, feat.GetIds() ) {
            x_UnmapFeatById(**it, info, true);
        }
    }
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                x_UnmapFeatById(xref.GetId(), info, false);
            }
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObject(CAnnotObject_Info& info)
{
    if ( x_DirtyAnnotIndex() ) {
        return;
    }

    CTSE_Info& tse = GetTSE_Info();
    CDataSource::TAnnotLockWriteGuard guard(eEmptyGuard);
    if (HasDataSource())
        guard.Guard(GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());
    
    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps);
    index.m_AnnotLocationIndex = 0;
    size_t keys_begin = m_ObjectIndex.GetKeys().size();
    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        bool multi_id = hrmit->GetMap().size() > 1;
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                CNcbiOstrstream s;
                const CSerialObject* obj = 0;
                obj = dynamic_cast<const CSerialObject*>(info.GetObjectPointer());
                if ( obj ) {
                    s << MSerial_AsnText << *obj;
                }
                else {
                    s << "unknown annotation";
                }
                ERR_POST_X(5, "Empty region in "<<s.rdbuf());
                continue;
            }
            key.m_Handle = hrit->first;
            index.m_Flags = hr.GetStrandsFlag();
            if ( multi_id ) {
                index.SetMultiIdFlag();
            }
            if ( hr.HasGaps() ) {
                index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                index.m_HandleRange->GetData() = hr;
                if ( hr.IsCircular() ) {
                    key.m_Range = hr.GetCircularRangeStart();
                    x_Map(mapper, key, index);
                    key.m_Range = hr.GetCircularRangeEnd();
                }
            }
            else {
                index.m_HandleRange.Reset();
            }
            x_Map(mapper, key, index);
        }
        ++index.m_AnnotLocationIndex;
    }
    x_UpdateObjectKeys(info, keys_begin);
    if ( info.IsFeat() ) {
        x_MapFeatIds(info);
    }
}


void CSeq_annot_Info::x_RemapAnnotObject(CAnnotObject_Info& info)
{
    if ( x_DirtyAnnotIndex() ) {
        return;
    }

    x_UnmapAnnotObject(info);
    x_MapAnnotObject(info);

    /*
    if ( info.IsFeat() &&
         info.GetFeatSubtype() != info.GetFeatFast()->GetSubtype() ) {
        x_UnmapAnnotObject(info);
        x_MapAnnotObject(info);
        return;
    }

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps);
    index.m_AnnotLocationIndex = 0;
    size_t keys_begin = m_ObjectIndex.GetKeys().size();
    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        bool multi_id = hrmit->GetMap().size() > 1;
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                CNcbiOstrstream s;
                const CSerialObject* obj = 0;
                obj = dynamic_cast<const CSerialObject*>(info.GetObjectPointer());
                if ( obj ) {
                    s << MSerial_AsnText << *obj;
                }
                else {
                    s << "unknown annotation";
                }
                ERR_POST_X(6, "Empty region in "<<s.rdbuf());
                continue;
            }
            key.m_Handle = hrit->first;
            index.m_Flags = hr.GetStrandsFlag();
            if ( multi_id ) {
                index.SetMultiIdFlag();
            }
            if ( hr.HasGaps() ) {
                index.m_HandleRange.Reset(new CObjectFor<CHandleRange>);
                index.m_HandleRange->GetData() = hr;
                if ( hr.IsCircular() ) {
                    key.m_Range = hr.GetCircularRangeStart();
                    x_Map(mapper, key, index);
                    key.m_Range = hr.GetCircularRangeEnd();
                }
            }
            else {
                index.m_HandleRange.Reset();
            }
            x_Map(mapper, key, index);
        }
        ++index.m_AnnotLocationIndex;
    }
    x_UpdateObjectKeys(info, keys_begin);


    CTSE_Info& tse = GetTSE_Info();
    CDataSource::TAnnotLockWriteGuard guard(eEmptyGuard);
    if (HasDataSource())
        guard.Guard(GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());
    
    CTSEAnnotObjectMapper mapper(tse, GetName());
    // replace annotation indexes in TSE
    
    size_t old_begin, old_end;
    if ( info.HasSingleKey() ) {
        mapper.Unmap(info.GetKey(), info);
        old_begin = old_end = 0;
    }
    else {
        old_begin = info.GetKeysBegin();
        old_end = info.GetKeysEnd()
        for ( size_t i = old_begin; i < old_end; ++i ) {
            mapper.Unmap(m_ObjectIndex.GetKey(i), info);
        }
    }
    if ( new_count == 1 &&
         m_ObjectIndex.GetKey(keys_begin).IsSingle() ) {
        // one simple key, store it inside CAnnotObject_Info
        info.SetKey(m_ObjectIndex.GetKey(keys_begin));
        m_ObjectIndex.RemoveLastMap();
        mapper.Map(info.GetKey(), info);
    }
    else {
        info.SetKeys(keys_begin, keys_end);
        for ( size_t i = keys_begin; i < keys_end; ++i ) {
            mapper.Map(m_ObjectIndex.GetKey(i), info);
        }
    }
    */
}


void CSeq_annot_Info::x_UnmapAnnotObject(CAnnotObject_Info& info)
{
    if ( x_DirtyAnnotIndex() ) {
        return;
    }

    CTSE_Info& tse = GetTSE_Info();
    CDataSource::TAnnotLockWriteGuard guard(eEmptyGuard);
    if (HasDataSource())
        guard.Guard(GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());
    
    CTSEAnnotObjectMapper mapper(tse, GetName());
    
    if ( info.HasSingleKey() ) {
        mapper.Unmap(info.GetKey(), info);
    }
    else {
        for ( size_t i = info.GetKeysBegin(); i < info.GetKeysEnd(); ++i ) {
            mapper.Unmap(m_ObjectIndex.GetKey(i), info);
        }
    }
    info.ResetKey();
    if ( info.IsFeat() ) {
        x_UnmapFeatIds(info);
    }
}


void CSeq_annot_Info::x_UnmapAnnotObjects(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_UnmapAnnotObjects(tse);
    }
    tse.x_UnmapAnnotObjects(m_ObjectIndex);
    m_ObjectIndex.Clear();
}


void CSeq_annot_Info::x_DropAnnotObjects(CTSE_Info& tse)
{
    if ( m_SNP_Info ) {
        m_SNP_Info->x_DropAnnotObjects(tse);
    }
    m_ObjectIndex.Clear();
}


namespace {
    void sx_CheckType(CSeq_annot::C_Data& data,
                      CSeq_annot::C_Data::E_Choice type,
                      const char* error)
    {
        if ( data.Which() != type ) {
            if ( data.Which() == data.e_not_set ) {
                data.Select(type);
            }
            else {
                NCBI_THROW(CObjMgrException, eInvalidHandle, error);
            }
        }
    }

    bool sx_SameLocation(const CSeq_feat& obj1, const CSeq_feat& obj2)
    {
        if ( !obj1.GetLocation().Equals(obj2.GetLocation()) ) {
            return false;
        }
        if ( obj1.IsSetProduct() ) {
            return obj2.IsSetProduct() &&
                obj1.GetProduct().Equals(obj2.GetProduct());
        }
        return !obj2.IsSetProduct();
    }

    inline
    bool sx_SameLocation(const CSeq_align& obj1, const CSeq_align& obj2)
    {
        return obj1.Equals(obj2);
    }

    inline
    bool sx_SameLocation(const CSeq_graph& obj1, const CSeq_graph& obj2)
    {
        return obj1.GetLoc().Equals(obj2.GetLoc());
    }
}


void CSeq_annot_Info::Update(TAnnotIndex index)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    x_RemapAnnotObject(info);
}


void CSeq_annot_Info::Remove(TAnnotIndex index)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    x_UnmapAnnotObject(info);

    // remove annotation from Seq-annot object
    C_Data& data = m_Object->SetData();
    _ASSERT(info.Which() == data.Which());
    switch ( data.Which() ) {
    case C_Data::e_Ftable:
        data.SetFtable().erase(info.x_GetFeatIter());
        break;
    case C_Data::e_Align:
        data.SetAlign().erase(info.x_GetAlignIter());
        break;
    case C_Data::e_Graph:
        data.SetGraph().erase(info.x_GetGraphIter());
        break;
    case C_Data::e_Locs:
        data.SetLocs().erase(info.x_GetLocsIter());
        break;
    default:
        break;
    }

    // mark CAnnotObject_Info as removed
    info.Reset();
    _ASSERT(info.IsRemoved());
}


CSeq_annot_Info::TAnnotIndex CSeq_annot_Info::Add(const CSeq_feat& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not ftable");
    TAnnotIndex index = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.AddInfo(CAnnotObject_Info(*this,
                                            index,
                                            data.SetFtable(),
                                            new_obj));
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos().back();
    _ASSERT(&info == &GetInfo(index));
    _ASSERT(&info.GetFeat() == &new_obj);
    x_MapAnnotObject(info);
    return index;
}


CSeq_annot_Info::TAnnotIndex CSeq_annot_Info::Add(const CSeq_align& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not align");
    TAnnotIndex index = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.AddInfo(CAnnotObject_Info(*this,
                                            index,
                                            data.SetAlign(),
                                            new_obj));
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos().back();
    _ASSERT(&info == &GetInfo(index));
    _ASSERT(&info.GetAlign() == &new_obj);
    x_MapAnnotObject(info);
    return index;
}


CSeq_annot_Info::TAnnotIndex CSeq_annot_Info::Add(const CSeq_graph& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not align");
    TAnnotIndex index = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.AddInfo(CAnnotObject_Info(*this,
                                            index,
                                            data.SetGraph(),
                                            new_obj));
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos().back();
    _ASSERT(&info == &GetInfo(index));
    _ASSERT(&info.GetGraph() == &new_obj);
    x_MapAnnotObject(info);
    return index;
}


void CSeq_annot_Info::Replace(TAnnotIndex index, const CSeq_feat& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot replace Seq-feat: Seq-annot is not ftable");
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    SAnnotObjectsIndex::TObjectInfos::iterator info_iter =
        m_ObjectIndex.GetInfos().begin()+index;
    CAnnotObject_Info& info = *info_iter;
    if ( info.IsRemoved() ) {
        TFtable& cont = data.SetFtable();
        TFtable::iterator cont_iter = cont.end();
        SAnnotObjectsIndex::TObjectInfos::const_iterator it = info_iter;
        SAnnotObjectsIndex::TObjectInfos::const_iterator it_end =
            m_ObjectIndex.GetInfos().end();
        for ( ; it != it_end; ++it ) {
            if ( !it->IsRemoved() ) {
                cont_iter = it->x_GetFeatIter();
                break;
            }
        }
        cont_iter =
            cont.insert(cont_iter, Ref(const_cast<CSeq_feat*>(&new_obj)));
        info = CAnnotObject_Info(*this, index, cont_iter);
        _ASSERT(!info.IsRemoved());
        x_MapAnnotObject(info);
    }
    else if ( info.GetFeatSubtype() == new_obj.GetData().GetSubtype() &&
              sx_SameLocation(info.GetFeat(), new_obj) ) {
        // same index -> just replace
        info.x_SetObject(new_obj);
    }
    else {
        // reindex
        x_UnmapAnnotObject(info);
        info.x_SetObject(new_obj);
        x_MapAnnotObject(info);
    }
}


void CSeq_annot_Info::Replace(TAnnotIndex index, const CSeq_align& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Align,
                 "Cannot replace Seq-align: Seq-annot is not align");
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    SAnnotObjectsIndex::TObjectInfos::iterator info_iter =
        m_ObjectIndex.GetInfos().begin()+index;
    CAnnotObject_Info& info = *info_iter;
    if ( info.IsRemoved() ) {
        TAlign& cont = data.SetAlign();
        TAlign::iterator cont_iter = cont.end();
        SAnnotObjectsIndex::TObjectInfos::const_iterator it = info_iter;
        SAnnotObjectsIndex::TObjectInfos::const_iterator it_end =
            m_ObjectIndex.GetInfos().end();
        for ( ; it != it_end; ++it ) {
            if ( !it->IsRemoved() ) {
                cont_iter = it->x_GetAlignIter();
                break;
            }
        }
        cont_iter =
            cont.insert(cont_iter, Ref(const_cast<CSeq_align*>(&new_obj)));
        info = CAnnotObject_Info(*this, index, cont_iter);
        _ASSERT(!info.IsRemoved());
        x_MapAnnotObject(info);
    }
    else if ( sx_SameLocation(info.GetAlign(), new_obj) ) {
        // same index -> just replace
        info.x_SetObject(new_obj);
    }
    else {
        // reindex
        x_UnmapAnnotObject(info);
        info.x_SetObject(new_obj);
        x_MapAnnotObject(info);
    }
}


void CSeq_annot_Info::Replace(TAnnotIndex index, const CSeq_graph& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Graph,
                 "Cannot replace Seq-graph: Seq-annot is not graph");
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    SAnnotObjectsIndex::TObjectInfos::iterator info_iter =
        m_ObjectIndex.GetInfos().begin()+index;
    CAnnotObject_Info& info = *info_iter;
    if ( info.IsRemoved() ) {
        TGraph& cont = data.SetGraph();
        TGraph::iterator cont_iter = cont.end();
        SAnnotObjectsIndex::TObjectInfos::const_iterator it = info_iter;
        SAnnotObjectsIndex::TObjectInfos::const_iterator it_end =
            m_ObjectIndex.GetInfos().end();
        for ( ; it != it_end; ++it ) {
            if ( !it->IsRemoved() ) {
                cont_iter = it->x_GetGraphIter();
                break;
            }
        }
        cont_iter =
            cont.insert(cont_iter, Ref(const_cast<CSeq_graph*>(&new_obj)));
        info = CAnnotObject_Info(*this, index, cont_iter);
        _ASSERT(!info.IsRemoved());
        x_MapAnnotObject(info);
    }
    else if ( sx_SameLocation(info.GetGraph(), new_obj) ) {
        // same index -> just replace
        info.x_SetObject(new_obj);
    }
    else {
        // reindex
        x_UnmapAnnotObject(info);
        info.x_SetObject(new_obj);
        x_MapAnnotObject(info);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
