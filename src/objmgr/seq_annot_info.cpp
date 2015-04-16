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

#include <ncbi_pch.hpp>

#define ANNOT_EDIT_COPY 1

#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/bioseq_base_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/tse_chunk_info.hpp>
#include <objmgr/impl/annot_object.hpp>
#include <objmgr/impl/handle_range_map.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/snp_annot_info.hpp>
#include <objmgr/impl/seq_table_info.hpp>
#include <objmgr/objmgr_exception.hpp>
#include <objmgr/error_codes.hpp>
#include <objmgr/annot_selector.hpp>

#include <objects/general/general__.hpp>
#include <objects/seqloc/seqloc__.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Textannot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>
#include <objects/seqtable/seqtable__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <objmgr/seq_feat_handle.hpp>


#define NCBI_USE_ERRCODE_X   ObjMgr_SeqAnnot

BEGIN_NCBI_SCOPE

NCBI_DEFINE_ERR_SUBCODE_X(9);

BEGIN_SCOPE(objects)


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot& annot, int chunk_id)
    : m_ChunkId(chunk_id)
{
    x_SetObject(annot);
}


CSeq_annot_Info::CSeq_annot_Info(CSeq_annot_SNP_Info& snp_annot)
    : m_ChunkId(0)
{
    x_SetSNP_annot_Info(snp_annot);
}


CSeq_annot_Info::CSeq_annot_Info(const CSeq_annot_Info& info,
                                 TObjectCopyMap* copy_map)
    : TParent(info, copy_map),
      m_ChunkId(info.GetChunkId())
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
    SetBioObjectId(tse.x_RegisterBioObject(*this));
    x_UpdateName();
    x_SetDirtyAnnotIndex();
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
    tse.x_UnregisterBioObject(*this);
    SetBioObjectId(CBioObjectId());
}


const CAnnotName& CSeq_annot_Info::GetName(void) const
{
    return m_Name;
}


void CSeq_annot_Info::x_UpdateName(void)
{
    int zoom_level = -1;
    m_Name.SetUnnamed();
    const CSeq_annot& annot = *m_Object;
    if ( annot.IsSetId() ) {
        const CSeq_annot::TId& ids = annot.GetId();
        ITERATE ( CSeq_annot::TId, it, ids ) {
            const CAnnot_id& id = **it;
            if ( id.IsOther() ) {
                const CTextannot_id& text_id = id.GetOther();
                if ( text_id.IsSetAccession() ) {
                    const string& acc = text_id.GetAccession();
                    if ( text_id.IsSetVersion() ) {
                        int ver = text_id.GetVersion();
                        m_Name.SetNamed(acc+'.'+NStr::IntToString(ver));
                    }
                    else {
                        m_Name.SetNamed(acc);
                    }
                    break;
                }
            }
        }
    }
    if ( annot.IsSetDesc() ) {
        const CSeq_annot::TDesc::Tdata& descs = annot.GetDesc().Get();
        ITERATE( CSeq_annot::TDesc::Tdata, it, descs ) {
            const CAnnotdesc& desc = **it;
            if ( desc.Which() == CAnnotdesc::e_Name ) {
                if ( m_Name.IsNamed() ) {
                    continue;
                }
                m_Name.SetNamed(desc.GetName());
            }
            else if ( desc.Which() == CAnnotdesc::e_User ) {
                if ( zoom_level >= 0 ) {
                    continue;
                }
                const CUser_object& user = desc.GetUser();
                const CObject_id& type = user.GetType();
                if ( !type.IsStr() || type.GetStr() != "AnnotationTrack" ) {
                    continue;
                }
                CConstRef<CUser_field> field = user.GetFieldRef("ZoomLevel");
                if ( field && field->GetData().IsInt() ) {
                    zoom_level = field->GetData().GetInt();
                }
            }
        }
    }
    if ( HasTSE_Info() && GetTSE_Info().GetName().IsNamed() ) {
        m_Name = GetTSE_Info().GetName();
    }
    if ( zoom_level >= 0 && m_Name.IsNamed() ) {
        m_Name.SetNamed(m_Name.GetName()+
                        NCBI_ANNOT_TRACK_ZOOM_LEVEL_SUFFIX+
                        NStr::IntToString(zoom_level));
    }
}


string CSeq_annot_Info::GetDescription(void) const
{
    string ret;
    if ( GetName().IsNamed() ) {
        ret = GetName().GetName();
    }
    if ( HasTSE_Info() ) {
        if ( !ret.empty() ) {
            ret += '|';
        }
        ret += GetTSE_Info().GetDescription();
    }
    return ret;
}


CSeq_annot_Info::TAnnotIndex
CSeq_annot_Info::x_GetSNPFeatCount(void) const
{
    return TAnnotIndex(x_GetSNP_annot_Info().size());
}


CSeq_annot_Info::TAnnotIndex
CSeq_annot_Info::x_GetAnnotCount(void) const
{
    return TAnnotIndex(GetAnnotObjectInfos().size());
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
    x_InitAnnotList();
    if ( HasTSE_Info() ) {
        x_UpdateName();
        x_SetDirtyAnnotIndex();
    }
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
        CAnnotObject_Info info(*this, index++, oit);
        m_ObjectIndex.AddInfo(info);
    }
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
    if ( !CSeqTableInfo::IsGoodFeatTable(table) ) {
        // index whole Seq-table
        SAnnotTypeSelector type(CSeq_annot::C_Data::e_Seq_table);
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, 0, type));
        _ASSERT(m_ObjectIndex.GetInfos().size() == 1u);
    }
    else {
        // index each row separately
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


void CSeq_annot_Info::x_Map(const CTSEAnnotObjectMapper& mapper,
                            const SAnnotObject_Key& key,
                            const SAnnotObject_Index& index)
{
    if ( key.m_Range.Empty() ) {
        const CAnnotObject_Info& info = *index.m_AnnotObject_Info;
        CNcbiOstrstream s;
        if ( !info.IsRegular() ) {
            s << "unknown object";
        }
        else if ( info.IsFeat() ) {
            s << MSerial_AsnText << info.GetFeat();
        }
        else if ( info.IsGraph() ) {
            s << "graph " << MSerial_AsnText << info.GetGraph().GetLoc();
        }
        else if ( info.IsAlign() ) {
            s << MSerial_AsnText << info.GetAlign();
        }
        else {
            s << "unknown object";
        }
        ERR_POST_X(6, "Failed to parse location of "<<s.rdbuf()<<" in "<<GetDescription());
        return;
    }
    mapper.Map(key, index);
    m_ObjectIndex.AddMap(key, index);
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
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        _ASSERT(info.GetFeatType() == info.GetFeatFast()->GetData().Which());
    }
    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        _ASSERT(info.GetFeatType() == info.GetFeatFast()->GetData().Which());
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps, master);

        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            bool multi_id = hrmit->GetMap().size() > 1;
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    ERR_POST_X(1, "Empty region in "<<GetDescription()<<" "<<
                               MSerial_AsnText<<*info.GetFeatFast());
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
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();
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

        info.GetMaps(hrmaps, master);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    ERR_POST_X(2, "Empty region in "<<GetDescription()<<" "<<
                               MSerial_AsnText<<*info.GetGraphFast());
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
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();
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

        info.GetMaps(hrmaps, master);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    ERR_POST_X(3, "Empty region in "<<GetDescription()<<" "<<
                               MSerial_AsnText<<info.GetAlign());
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
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    size_t keys_begin = m_ObjectIndex.GetKeys().size();
    index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps, master);
    index.m_AnnotLocationIndex = 0;

    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                ERR_POST_X(4, "Empty region in "<<GetDescription()<<" "<<
                           MSerial_AsnText<<info.GetLocs());
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
    m_Table_Info->GetLocation().UpdateSeq_loc(info.GetAnnotIndex(),
                                              seq_loc, seq_pnt, seq_int);
}


void CSeq_annot_Info::UpdateTableFeatProduct(
    CRef<CSeq_loc>& seq_loc,
    CRef<CSeq_point>& seq_pnt,
    CRef<CSeq_interval>& seq_int,
    const CAnnotObject_Info& info) const
{
    m_Table_Info->GetProduct().UpdateSeq_loc(info.GetAnnotIndex(),
                                             seq_loc, seq_pnt, seq_int);
}


bool CSeq_annot_Info::IsTableFeatPartial(const CAnnotObject_Info& info) const
{
    return m_Table_Info->IsPartial(info.GetAnnotIndex());
}


void CSeq_annot_Info::x_InitFeatTableKeys(CTSE_Info& tse)
{
    const CSeq_table& feat_table = m_Object->GetData().GetSeq_table();
    m_Table_Info = new CSeqTableInfo(feat_table);
    
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();

    if ( !m_Table_Info->IsFeatTable() ) {
        // index whole Seq-table
        m_ObjectIndex.ReserveMapSize(1);
        SAnnotObject_Key key;
        SAnnotObject_Index index;
        CHandleRangeMap hrmap;
        hrmap.SetMasterSeq(master);
        CTSEAnnotObjectMapper mapper(tse, GetName());
        CAnnotObject_Info& info = m_ObjectIndex.GetInfos().front();
        if ( info.IsRemoved() ) {
            return;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;
        index.m_AnnotLocationIndex = 0;
        CConstRef<CSeq_loc> loc = m_Table_Info->GetTableLocation();
        if ( !loc ) {
            return;
        }
        hrmap.AddLocation(*loc);
        bool multi_id = hrmap.GetMap().size() > 1;
        ITERATE ( CHandleRangeMap, hrit, hrmap ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                ERR_POST_X(7, "Empty region in "<<GetDescription()<<" "<<
                           MSerial_AsnText<<*info.GetFeatFast());
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
        x_UpdateObjectKeys(info, keys_begin);
        return;
    }

    size_t object_count = m_ObjectIndex.GetInfos().size();
    _ASSERT(object_count == size_t(feat_table.GetNum_rows()));
    m_ObjectIndex.ReserveMapSize(object_count);

    SAnnotObject_Key key;
    SAnnotObject_Index index;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    SAnnotObjectsIndex::TObjectInfos::iterator it =
        m_ObjectIndex.GetInfos().begin();
    for ( size_t row = 0; row < object_count; ++row, ++it ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        if ( m_Table_Info->RowIsDisabled(row) ) {
            continue;
        }
        size_t keys_begin = m_ObjectIndex.GetKeys().size();
        index.m_AnnotObject_Info = &info;
        bool partial = m_Table_Info->IsPartial(row);
        for ( index.m_AnnotLocationIndex = 0;
              index.m_AnnotLocationIndex < 2;
              index.m_AnnotLocationIndex += 1 ) {
            // index by location, then by product
            const CSeqTableLocColumns& loc =
                index.m_AnnotLocationIndex == 0?
                m_Table_Info->GetLocation():
                m_Table_Info->GetProduct();
            if ( loc.IsSet() ) { // if this Seq-loc field is set
                if ( loc.IsRealLoc() ) { // full Seq-loc object
                    CHandleRangeMap hrmap;
                    hrmap.SetMasterSeq(master);
                    hrmap.AddLocation(*loc.GetLoc(row));
                    bool multi_id = hrmap.GetMap().size() > 1;
                    ITERATE ( CHandleRangeMap, hrit, hrmap ) {
                        const CHandleRange& hr = hrit->second;
                        key.m_Range = hr.GetOverlappingRange();
                        if ( key.m_Range.Empty() ) {
                            ERR_POST_X(8, "Empty region in "<<GetDescription()<<" "<<
                                       MSerial_AsnText<<*info.GetFeatFast());
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
                        index.SetPartial(partial);
                        x_Map(mapper, key, index);
                    }
                }
                else { // simplified Seq-loc object
                    loc.SetTableKeyAndIndex(row, key, index);
                    index.SetPartial(partial);
                    x_Map(mapper, key, index);
                }
            }
        }

        x_UpdateObjectKeys(info, keys_begin);
        //x_MapFeatIds(info);
    }
}


void CSeq_annot_Info::x_MapFeatById(const CFeat_id& id,
                                    CAnnotObject_Info& info,
                                    EFeatIdType id_type)
{
    if ( id.IsLocal() ) {
        GetTSE_Info().x_MapFeatById(id.GetLocal(), info, id_type);
    }
}


void CSeq_annot_Info::x_UnmapFeatById(const CFeat_id& id,
                                      CAnnotObject_Info& info,
                                      EFeatIdType id_type)
{
    if ( id.IsLocal() ) {
        GetTSE_Info().x_UnmapFeatById(id.GetLocal(), info, id_type);
    }
}


void CSeq_annot_Info::x_MapFeatByGene(const CGene_ref& gene,
                                      CAnnotObject_Info& info)
{
    if ( gene.IsSetLocus() ) {
        GetTSE_Info().x_MapFeatByLocus(gene.GetLocus(), false, info);
    }
    else if ( gene.IsSetDesc() ) {
        GetTSE_Info().x_MapFeatByLocus(gene.GetDesc(), false, info);
    }
    if ( gene.IsSetLocus_tag() ) {
        GetTSE_Info().x_MapFeatByLocus(gene.GetLocus_tag(), true, info);
    }
}


void CSeq_annot_Info::x_UnmapFeatByGene(const CGene_ref& gene,
                                        CAnnotObject_Info& info)
{
    if ( gene.IsSetLocus() ) {
        GetTSE_Info().x_UnmapFeatByLocus(gene.GetLocus(), false, info);
    }
    else if ( gene.IsSetDesc() ) {
        GetTSE_Info().x_UnmapFeatByLocus(gene.GetDesc(), false, info);
    }
    if ( gene.IsSetLocus_tag() ) {
        GetTSE_Info().x_UnmapFeatByLocus(gene.GetLocus_tag(), true, info);
    }
}


void CSeq_annot_Info::x_MapFeatIds(CAnnotObject_Info& info)
{
    if ( !info.IsRegular() ) {
        // table?
        return;
    }
    const CSeq_feat& feat = *info.GetFeatFast();
    if ( feat.IsSetId() ) {
        x_MapFeatById(feat.GetId(), info, eFeatId_id);
    }
    if ( feat.IsSetIds() ) {
        ITERATE ( CSeq_feat::TIds, it, feat.GetIds() ) {
            x_MapFeatById(**it, info, eFeatId_id);
        }
    }
    if ( info.GetFeatType() == CSeqFeatData::e_Gene ) {
        x_MapFeatByGene(feat.GetData().GetGene(), info);
    }
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                x_MapFeatById(xref.GetId(), info, eFeatId_xref);
            }
        }
    }
}


void CSeq_annot_Info::x_UnmapFeatIds(CAnnotObject_Info& info)
{
    if ( !info.IsRegular() ) {
        // table?
        return;
    }
    const CSeq_feat& feat = *info.GetFeatFast();
    if ( feat.IsSetId() ) {
        x_UnmapFeatById(feat.GetId(), info, eFeatId_id);
    }
    if ( feat.IsSetIds() ) {
        ITERATE ( CSeq_feat::TIds, it, feat.GetIds() ) {
            x_UnmapFeatById(**it, info, eFeatId_id);
        }
    }
    if ( info.GetFeatType() == CSeqFeatData::e_Gene ) {
        x_UnmapFeatByGene(feat.GetData().GetGene(), info);
    }
    if ( feat.IsSetXref() ) {
        ITERATE ( CSeq_feat::TXref, it, feat.GetXref() ) {
            const CSeqFeatXref& xref = **it;
            if ( xref.IsSetId() ) {
                x_UnmapFeatById(xref.GetId(), info, eFeatId_xref);
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
    CConstRef<CMasterSeqSegments> master = tse.GetMasterSeqSegments();
    vector<CHandleRangeMap> hrmaps;

    CTSEAnnotObjectMapper mapper(tse, GetName());

    index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps, master);
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
                ERR_POST_X(5, "Empty region in " << GetDescription() << " "
                           << (string)CNcbiOstrstreamToString(s));
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

    info.GetMaps(hrmaps, master);
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
                ERR_POST_X(9, "Empty region in "<<GetDescription()<<" "<<s.rdbuf());
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
    if ( m_ObjectIndex.GetInfos().empty() ) {
        return;
    }
    if ( m_ObjectIndex.GetInfos()[0].IsFeat() ) {
        NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, oit, m_ObjectIndex.GetInfos() ) {
            x_UnmapFeatIds(*oit);
        }
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

    template<class Set>
    bool sx_SameSets(const Set& old_set, const Set& new_set)
    {
        typename Set::const_iterator old_it = old_set.begin();
        ITERATE ( typename Set, new_it, new_set ) {
            if ( old_it == old_set.end() ) {
                return false;
            }
            if ( !(*old_it)->Equals(**new_it) ) {
                return false;
            }
            ++old_it;
        }
        return old_it == old_set.end();
    }

    bool sx_SameFeatIndex(const CAnnotObject_Info& info,
                          const CSeq_feat& new_obj)
    {
        if ( info.GetFeatSubtype() != new_obj.GetData().GetSubtype() ) {
            return false;
        }
        const CSeq_feat& old_obj = info.GetFeat();
        if ( info.GetFeatType() == CSeqFeatData::e_Gene ) {
            if ( !old_obj.GetData().GetGene().Equals(
                     new_obj.GetData().GetGene()) ) {
                return false;
            }
        }
        if ( !sx_SameLocation(old_obj, new_obj) ) {
            return false;
        }
        if ( old_obj.IsSetId() != new_obj.IsSetId() ) {
            return false;
        }
        if ( old_obj.IsSetId() ) {
            if ( !old_obj.GetId().Equals(new_obj.GetId()) ) {
                return false;
            }
        }
        if ( old_obj.IsSetIds() != new_obj.IsSetIds() ) {
            return false;
        }
        if ( old_obj.IsSetIds() ) {
            if ( !sx_SameSets(old_obj.GetIds(), new_obj.GetIds()) ) {
                return false;
            }
        }
        if ( old_obj.IsSetXref() != new_obj.IsSetXref() ) {
            return false;
        }
        if ( old_obj.IsSetXref() ) {
            if ( !sx_SameSets(old_obj.GetXref(), new_obj.GetXref()) ) {
                return false;
            }
        }
        return true;
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
    TAnnotIndex index = TAnnotIndex(m_ObjectIndex.GetInfos().size());
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
    sx_CheckType(data, data.e_Align,
                 "Cannot add Seq-align: Seq-annot is not align");
    TAnnotIndex index = TAnnotIndex(m_ObjectIndex.GetInfos().size());
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
    sx_CheckType(data, data.e_Graph,
                 "Cannot add Seq-graph: Seq-annot is not graph");
    TAnnotIndex index = TAnnotIndex(m_ObjectIndex.GetInfos().size());
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
    else if ( sx_SameFeatIndex(info, new_obj) ) {
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


void CSeq_annot_Info::ReorderFtable(const vector<CSeq_feat_Handle>& feats)
{
    C_Data& data = m_Object->SetData();
    if ( !data.IsFtable() ) {
        NCBI_THROW(CObjMgrException, eInvalidHandle,
                   "ReorderFtable: Seq-annot annot is not ftable");
    }
    C_Data::TFtable& cont = data.SetFtable();
    ITERATE ( vector<CSeq_feat_Handle>, it, feats ) {
        if ( &it->x_GetSeq_annot_Info() != this ) {
            continue;
        }
        const CAnnotObject_Info& info = it->x_GetAnnotObject_Info();
        if ( !info.IsRegular() ) {
            continue;
        }
        const_cast<CAnnotObject_Info&>(info).x_MoveToBack(cont);
    }
}


void CSeq_annot_Info::AddFeatId(TAnnotIndex index,
                                const CObject_id& id,
                                EFeatIdType id_type)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    CRef<CSeq_feat> feat(&const_cast<CSeq_feat&>(info.GetFeat()));
    GetTSE_Info().x_MapFeatById(id, info, id_type);
    CRef<CFeat_id> feat_id(new CFeat_id);
    feat_id->SetLocal().Assign(id);
    if ( id_type == eFeatId_xref ) {
        CRef<CSeqFeatXref> feat_xref(new CSeqFeatXref);
        feat_xref->SetId(*feat_id);
        feat->SetXref().push_back(feat_xref);
    }
    else if ( !feat->IsSetId() ) {
        feat->SetId(*feat_id);
    }
    else {
        feat->SetIds().push_back(feat_id);
    }
}


void CSeq_annot_Info::RemoveFeatId(TAnnotIndex index,
                                   const CObject_id& id,
                                   EFeatIdType id_type)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    CRef<CSeq_feat> feat(&const_cast<CSeq_feat&>(info.GetFeat()));
    bool removed_id = false;
    if ( id_type == eFeatId_xref ) {
        if ( feat->IsSetXref() ) {
            NON_CONST_ITERATE ( CSeq_feat::TXref, it, feat->SetXref() ) {
                const CSeqFeatXref& feat_xref = **it;
                if ( !feat_xref.IsSetId() ) {
                    continue;
                }
                const CFeat_id& feat_id = feat_xref.GetId();
                if ( feat_id.IsLocal() && feat_id.GetLocal().Equals(id) ) {
                    feat->SetXref().erase(it);
                    if ( feat->SetXref().empty() ) {
                        feat->ResetXref();
                    }
                    removed_id = true;
                    break;
                }
            }
        }
    }
    else {
        if ( feat->IsSetId() && feat->GetId().IsLocal() &&
             feat->GetId().GetLocal().Equals(id) ) {
            feat->ResetId();
            removed_id = true;
        }
        else if ( feat->IsSetIds() ) {
            NON_CONST_ITERATE ( CSeq_feat::TIds, it, feat->SetIds() ) {
                const CFeat_id& feat_id = **it;
                if ( feat_id.IsLocal() && feat_id.GetLocal().Equals(id) ) {
                    feat->SetIds().erase(it);
                    if ( feat->SetIds().empty() ) {
                        feat->ResetIds();
                    }
                    removed_id = true;
                    break;
                }
            }
        }
    }
    if ( !removed_id ) {
        NCBI_THROW(CAnnotException, eFindFailed,
                   "CSeq_feat_EditHandle::RemoveFeatId: Feat-id not found");
    }
    GetTSE_Info().x_UnmapFeatById(id, info, id_type);
}


void CSeq_annot_Info::ClearFeatIds(TAnnotIndex index,
                                   EFeatIdType id_type)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    CRef<CSeq_feat> feat(&const_cast<CSeq_feat&>(info.GetFeat()));
    if ( id_type == eFeatId_xref ) {
        if ( feat->IsSetXref() ) {
            ERASE_ITERATE ( CSeq_feat::TXref, it, feat->SetXref() ) {
                const CSeqFeatXref& feat_xref = **it;
                if ( !feat_xref.IsSetId() ) {
                    continue;
                }
                const CFeat_id& feat_id = feat_xref.GetId();
                if ( feat_id.IsLocal() ) {
                    GetTSE_Info().x_UnmapFeatById(feat_id.GetLocal(), info, id_type);
                    VECTOR_ERASE(it, feat->SetXref());
                }
            }
            feat->ResetXref();
        }
    }
    else {
        if ( feat->IsSetId() && feat->GetId().IsLocal() ) {
            GetTSE_Info().x_UnmapFeatById(feat->GetId().GetLocal(), info, id_type);
            feat->ResetId();
        }
        else if ( feat->IsSetIds() ) {
            ERASE_ITERATE ( CSeq_feat::TIds, it, feat->SetIds() ) {
                const CFeat_id& feat_id = **it;
                if ( feat_id.IsLocal() ) {
                    GetTSE_Info().x_UnmapFeatById(feat_id.GetLocal(), info, id_type);
                    VECTOR_ERASE(it, feat->SetIds());
                }
            }
            feat->ResetIds();
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
