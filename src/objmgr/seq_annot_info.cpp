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

#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Annot_id.hpp>
#include <objects/seq/Annotdesc.hpp>
#include <objects/seq/Annot_descr.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


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
        /*
        switch ( src.GetData().Which() ) {
        case CSeq_annot::C_Data::e_Ftable:
            obj->SetData().SetFtable() = src.GetData().GetFtable();
            break;
        case CSeq_annot::C_Data::e_Graph:
            obj->SetData().SetGraph() = src.GetData().GetGraph();
            break;
        case CSeq_annot::C_Data::e_Align:
            obj->SetData().SetAlign() = src.GetData().GetAlign();
            break;
        case CSeq_annot::C_Data::e_Ids:
            obj->SetData().SetIds() = src.GetData().GetIds();
            break;
        case CSeq_annot::C_Data::e_Locs:
            obj->SetData().SetLocs() = src.GetData().GetLocs();
            break;
        default:
            break;
        }
        */
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
    x_InitAnnotList(info);
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
    default:
        break;
    }
}


void CSeq_annot_Info::x_InitFeatList(TFtable& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TIndex index = 0;
    NON_CONST_ITERATE ( TFtable, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitAlignList(TAlign& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TIndex index = 0;
    NON_CONST_ITERATE ( TAlign, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitGraphList(TGraph& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TIndex index = 0;
    NON_CONST_ITERATE ( TGraph, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
}


void CSeq_annot_Info::x_InitLocsList(TLocs& objs)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TIndex index = 0;
    NON_CONST_ITERATE ( TLocs, oit, objs ) {
        m_ObjectIndex.AddInfo(CAnnotObject_Info(*this, index++, oit));
    }
    _ASSERT(size_t(index) == m_ObjectIndex.GetInfos().size());
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
    default:
        break;
    }
}


void CSeq_annot_Info::x_InitFeatList(TFtable& objs, const CSeq_annot_Info& info)
{
    _ASSERT(m_ObjectIndex.GetInfos().empty());
    TIndex index = 0;
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
    TIndex index = 0;
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
    TIndex index = 0;
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
    TIndex index = 0;
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
    x_InitAnnotKeys();
    tse.x_MapAnnotObjects(m_ObjectIndex);
    //m_ObjectIndex.DropIndices();

    if ( m_SNP_Info ) {
        m_SNP_Info->x_UpdateAnnotIndex(tse);
    }
    TParent::x_UpdateAnnotIndexContents(tse);
}


void CSeq_annot_Info::x_InitAnnotKeys(void)
{
    C_Data& data = m_Object->SetData();
    switch ( data.Which() ) {
    case C_Data::e_Ftable:
        x_InitFeatKeys();
        break;
    case C_Data::e_Align:
        x_InitAlignKeys();
        break;
    case C_Data::e_Graph:
        x_InitGraphKeys();
        break;
    case C_Data::e_Locs:
        x_InitLocsKeys();
        break;
    default:
        break;
    }

    m_ObjectIndex.PackKeys();
}


void CSeq_annot_Info::x_InitFeatKeys(void)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetFtable().size());
    if ( !m_ObjectIndex.GetKeys().empty() ) {
        return;
    }

    m_ObjectIndex.SetName(GetName());

    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(size_t(object_count*1.1));

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;

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
                    ERR_POST("Empty region in "<<s.rdbuf());
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
                        m_ObjectIndex.AddMap(key, index);
                        key.m_Range = hr.GetCircularRangeEnd();
                        m_ObjectIndex.AddMap(key, index);
                    }
                    else {
                        m_ObjectIndex.AddMap(key, index);
                    }
                }
                else {
                    index.m_HandleRange.Reset();
                    m_ObjectIndex.AddMap(key, index);
                }
            }
            ++index.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_InitGraphKeys(void)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetGraph().size());
    if ( !m_ObjectIndex.GetKeys().empty() ) {
        return;
    }

    m_ObjectIndex.SetName(GetName());

    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(object_count);

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    CNcbiOstrstream s;
                    s << MSerial_AsnText << *info.GetGraphFast();
                    ERR_POST("Empty region in "<<s.rdbuf());
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

                m_ObjectIndex.AddMap(key, index);
            }
            ++index.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_InitAlignKeys(void)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetAlign().size());
    if ( !m_ObjectIndex.GetKeys().empty() ) {
        return;
    }

    m_ObjectIndex.SetName(GetName());

    size_t object_count = m_ObjectIndex.GetInfos().size();
    m_ObjectIndex.ReserveMapSize(object_count);

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    NON_CONST_ITERATE ( SAnnotObjectsIndex::TObjectInfos, it,
                        m_ObjectIndex.GetInfos() ) {
        CAnnotObject_Info& info = *it;
        if ( info.IsRemoved() ) {
            continue;
        }
        key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;

        info.GetMaps(hrmaps);
        index.m_AnnotLocationIndex = 0;

        ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
            ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
                const CHandleRange& hr = hrit->second;
                key.m_Range = hr.GetOverlappingRange();
                if ( key.m_Range.Empty() ) {
                    CNcbiOstrstream s;
                    s << MSerial_AsnText << info.GetAlign();
                    ERR_POST("Empty region in "<<s.rdbuf());
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

                m_ObjectIndex.AddMap(key, index);
            }
            ++index.m_AnnotLocationIndex;
        }
    }
}


void CSeq_annot_Info::x_InitLocsKeys(void)
{
    _ASSERT(m_ObjectIndex.GetInfos().size() >= m_Object->GetData().GetLocs().size());
    if ( !m_ObjectIndex.GetKeys().empty() ) {
        return;
    }

    // Only one referenced location per annot is allowed
    if ( m_ObjectIndex.GetInfos().size() != 1) {
        return;
    }

    m_ObjectIndex.SetName(GetName());

    CAnnotObject_Info& info = m_ObjectIndex.GetInfos().front();
    if ( info.IsRemoved() ) {
        return;
    }

    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps);
    index.m_AnnotLocationIndex = 0;

    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                CNcbiOstrstream s;
                s << MSerial_AsnText << info.GetLocs();
                ERR_POST("Empty region in "<<s.rdbuf());
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
            m_ObjectIndex.AddMap(key, index);
        }
    }
}


void CSeq_annot_Info::x_MapAnnotObject(CAnnotObject_Info& info)
{
    if ( x_DirtyAnnotIndex() ) {
        return;
    }

    CTSE_Info& tse = GetTSE_Info();
    CDataSource::TAnnotLockWriteGuard guard;
    if (HasDataSource())
        guard.Guard(GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());
    
    // remove annotation from TSE index
    SAnnotObject_Key key;
    SAnnotObject_Index index;
    vector<CHandleRangeMap> hrmaps;

    key.m_AnnotObject_Info = index.m_AnnotObject_Info = &info;

    info.GetMaps(hrmaps);
    index.m_AnnotLocationIndex = 0;
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
                ERR_POST("Empty region in "<<s.rdbuf());
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
                    m_ObjectIndex.AddMap(key, index);
                    key.m_Range = hr.GetCircularRangeEnd();
                }
            }
            else {
                index.m_HandleRange.Reset();
            }
            tse.x_MapAnnotObject(GetName(), key, index);
        }
        ++index.m_AnnotLocationIndex;
    }
}


void CSeq_annot_Info::x_UnmapAnnotObject(CAnnotObject_Info& info)
{
    if ( x_DirtyAnnotIndex() ) {
        return;
    }

    CTSE_Info& tse = GetTSE_Info();
    CDataSource::TAnnotLockWriteGuard guard;
    if (HasDataSource())
        guard.Guard(GetDataSource());
    CTSE_Info::TAnnotLockWriteGuard guard2(tse.GetAnnotLock());
    
    // remove annotation from TSE index
    SAnnotObject_Key key;
    key.m_AnnotObject_Info = &info;
    vector<CHandleRangeMap> hrmaps;
    info.GetMaps(hrmaps);
    ITERATE ( vector<CHandleRangeMap>, hrmit, hrmaps ) {
        ITERATE ( CHandleRangeMap, hrit, *hrmit ) {
            const CHandleRange& hr = hrit->second;
            key.m_Range = hr.GetOverlappingRange();
            if ( key.m_Range.Empty() ) {
                continue;
            }
            key.m_Handle = hrit->first;
            tse.x_UnmapAnnotObject(GetName(), key);
        }
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


void CSeq_annot_Info::Remove(TIndex index)
{
    _ASSERT(size_t(index) < GetAnnotObjectInfos().size());
    CAnnotObject_Info& info = m_ObjectIndex.GetInfos()[index];
    _ASSERT(info.IsRegular());
    _ASSERT(&info.GetSeq_annot_Info() == this);
    x_UnmapAnnotObject(info);

    // everything is const here so there are a lot of const_casts :(

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


CSeq_annot_Info::TIndex CSeq_annot_Info::Add(const CSeq_feat& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not ftable");
    TIndex index = m_ObjectIndex.GetInfos().size();
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


CSeq_annot_Info::TIndex CSeq_annot_Info::Add(const CSeq_align& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not align");
    TIndex index = m_ObjectIndex.GetInfos().size();
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


CSeq_annot_Info::TIndex CSeq_annot_Info::Add(const CSeq_graph& new_obj)
{
    C_Data& data = m_Object->SetData();
    sx_CheckType(data, data.e_Ftable,
                 "Cannot add Seq-feat: Seq-annot is not align");
    TIndex index = m_ObjectIndex.GetInfos().size();
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


void CSeq_annot_Info::Replace(TIndex index, const CSeq_feat& new_obj)
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


void CSeq_annot_Info::Replace(TIndex index, const CSeq_align& new_obj)
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


void CSeq_annot_Info::Replace(TIndex index, const CSeq_graph& new_obj)
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

/*
 * ===========================================================================
 * $Log$
 * Revision 1.41  2006/08/01 22:14:41  vasilche
 * Set name of Seq-annot only if desc.name is present.
 *
 * Revision 1.40  2006/05/05 15:06:38  vasilche
 * Skip empty locations for edited annotations too.
 *
 * Revision 1.39  2006/05/05 14:41:58  vasilche
 * Skip empty locations in indexing of all annotations.
 *
 * Revision 1.38  2006/02/07 00:15:12  katargir
 * Fixed typo in x_InitLocsKeys which caused exception from ASSERT statement
 *
 * Revision 1.37  2006/01/25 18:59:04  didenko
 * Redisgned bio objects edit facility
 *
 * Revision 1.36  2005/09/29 19:34:38  grichenk
 * Report and skip alignments of zero length
 *
 * Revision 1.35  2005/09/20 15:45:36  vasilche
 * Feature editing API.
 * Annotation handles remember annotations by index.
 *
 * Revision 1.34  2005/08/23 17:02:57  vasilche
 * Moved multi id flags from CAnnotObject_Info to SAnnotObject_Index.
 * Used deque<> instead of vector<> for storing CAnnotObject_Info set.
 *
 * Revision 1.33  2005/08/05 15:41:30  vasilche
 * Copy Seq-annot object when detaching from data loader.
 *
 * Revision 1.32  2005/07/14 16:57:47  vasilche
 * Do not forget removing from annot index when detached.
 *
 * Revision 1.31  2005/06/27 18:17:04  vasilche
 * Allow getting CBioseq_set_Handle from CBioseq_set.
 *
 * Revision 1.30  2005/06/22 14:23:48  vasilche
 * Added support for original->edited map.
 *
 * Revision 1.29  2005/04/05 13:40:59  vasilche
 * TSE IdFlags are updated within CTSE_Info.
 *
 * Revision 1.28  2005/03/29 16:04:50  grichenk
 * Optimized annotation retrieval (reduced nuber of seq-ids checked)
 *
 * Revision 1.27  2005/02/16 15:18:58  grichenk
 * Ignore e_Locs annotations with unknown format
 *
 * Revision 1.26  2004/12/22 15:56:27  vasilche
 * Use SAnnotObjectsIndex.
 *
 * Revision 1.25  2004/12/08 16:39:37  grichenk
 * Optimized total ranges in CHandleRange
 *
 * Revision 1.24  2004/09/30 15:04:09  grichenk
 * Removed splitting of total ranges by strand
 *
 * Revision 1.23  2004/08/20 15:08:27  grichenk
 * Fixed strands indexing
 *
 * Revision 1.22  2004/08/17 15:56:08  vasilche
 * Added mapping and unmapping CSeq_annot -> CSeq_annot_Info in x_SetObject().
 *
 * Revision 1.21  2004/08/16 18:00:40  grichenk
 * Added detection of circular locations, improved annotation
 * indexing by strand.
 *
 * Revision 1.20  2004/08/12 14:18:27  vasilche
 * Allow SNP Seq-entry in addition to SNP Seq-annot.
 *
 * Revision 1.19  2004/08/04 14:53:26  vasilche
 * Revamped object manager:
 * 1. Changed TSE locking scheme
 * 2. TSE cache is maintained by CDataSource.
 * 3. CObjectManager::GetInstance() doesn't hold CRef<> on the object manager.
 * 4. Fixed processing of split data.
 *
 * Revision 1.18  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.17  2004/06/07 17:01:17  grichenk
 * Implemented referencing through locs annotations
 *
 * Revision 1.16  2004/05/21 21:42:13  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.15  2004/03/26 19:42:04  vasilche
 * Fixed premature deletion of SNP annot info object.
 * Removed obsolete references to chunk info.
 *
 * Revision 1.14  2004/03/24 18:30:30  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.13  2004/03/16 15:47:28  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.12  2004/01/22 20:10:40  vasilche
 * 1. Splitted ID2 specs to two parts.
 * ID2 now specifies only protocol.
 * Specification of ID2 split data is moved to seqsplit ASN module.
 * For now they are still reside in one resulting library as before - libid2.
 * As the result split specific headers are now in objects/seqsplit.
 * 2. Moved ID2 and ID1 specific code out of object manager.
 * Protocol is processed by corresponding readers.
 * ID2 split parsing is processed by ncbi_xreader library - used by all readers.
 * 3. Updated OBJMGR_LIBS correspondingly.
 *
 * Revision 1.11  2003/11/26 17:56:00  vasilche
 * Implemented ID2 split in ID1 cache.
 * Fixed loading of splitted annotations.
 *
 * Revision 1.10  2003/11/19 22:18:03  grichenk
 * All exceptions are now CException-derived. Catch "exception" rather
 * than "runtime_error".
 *
 * Revision 1.9  2003/10/07 13:43:23  vasilche
 * Added proper handling of named Seq-annots.
 * Added feature search from named Seq-annots.
 * Added configurable adaptive annotation search (default: gene, cds, mrna).
 * Fixed selection of blobs for loading from GenBank.
 * Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
 * Fixed leaked split chunks annotation stubs.
 * Moved some classes definitions in separate *.cpp files.
 *
 * Revision 1.8  2003/09/30 16:22:03  vasilche
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
 * Revision 1.7  2003/08/27 14:29:52  vasilche
 * Reduce object allocations in feature iterator.
 *
 * Revision 1.6  2003/07/18 19:41:46  vasilche
 * Removed unused variable.
 *
 * Revision 1.5  2003/07/17 22:51:31  vasilche
 * Fixed unused variables warnings.
 *
 * Revision 1.4  2003/07/17 20:07:56  vasilche
 * Reduced memory usage by feature indexes.
 * SNP data is loaded separately through PUBSEQ_OS.
 * String compression for SNP data.
 *
 * Revision 1.3  2003/07/09 17:54:29  dicuccio
 * Fixed uninitialized variables in CDataSource and CSeq_annot_Info
 *
 * Revision 1.2  2003/06/02 16:06:38  dicuccio
 * Rearranged src/objects/ subtree.  This includes the following shifts:
 *     - src/objects/asn2asn --> arc/app/asn2asn
 *     - src/objects/testmedline --> src/objects/ncbimime/test
 *     - src/objects/objmgr --> src/objmgr
 *     - src/objects/util --> src/objmgr/util
 *     - src/objects/alnmgr --> src/objtools/alnmgr
 *     - src/objects/flat --> src/objtools/flat
 *     - src/objects/validator --> src/objtools/validator
 *     - src/objects/cddalignview --> src/objtools/cddalignview
 * In addition, libseq now includes six of the objects/seq... libs, and libmmdb
 * replaces the three libmmdb? libs.
 *
 * Revision 1.1  2003/04/24 16:12:38  vasilche
 * Object manager internal structures are splitted more straightforward.
 * Removed excessive header dependencies.
 *
 *
 * ===========================================================================
 */
