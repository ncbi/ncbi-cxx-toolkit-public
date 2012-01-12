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
*   CSeq_entry_Info info -- entry for data source information about Seq-entry
*
*/


#include <ncbi_pch.hpp>
#include <objmgr/impl/seq_entry_info.hpp>
#include <objmgr/impl/tse_info.hpp>
#include <objmgr/impl/seq_annot_info.hpp>
#include <objmgr/impl/bioseq_set_info.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/objmgr_exception.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>

#include <algorithm>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_set_Info::CBioseq_set_Info(void)
    : m_Bioseq_set_Id(-1)
{
}


CBioseq_set_Info::CBioseq_set_Info(TObject& seqset)
    : m_Bioseq_set_Id(-1)
{
    x_SetObject(seqset);
}


CBioseq_set_Info::CBioseq_set_Info(const CBioseq_set_Info& info,
                                   TObjectCopyMap* copy_map)
    : TParent(info, copy_map), m_Bioseq_set_Id(-1)
{
    x_SetObject(info, copy_map);
}


CBioseq_set_Info::~CBioseq_set_Info(void)
{
}


CConstRef<CBioseq_set> CBioseq_set_Info::GetCompleteBioseq_set(void) const
{
    x_UpdateComplete();
    return m_Object;
}


CConstRef<CBioseq_set> CBioseq_set_Info::GetBioseq_setCore(void) const
{
    x_UpdateCore();
    return m_Object;
}


void CBioseq_set_Info::x_DSAttachContents(CDataSource& ds)
{
    TParent::x_DSAttachContents(ds);
    x_DSMapObject(m_Object, ds);
    // members
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_DSAttach(ds);
    }
}


void CBioseq_set_Info::x_DSDetachContents(CDataSource& ds)
{
    // members
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_DSDetach(ds);
    }
    x_DSUnmapObject(m_Object, ds);
    TParent::x_DSDetachContents(ds);
}


void CBioseq_set_Info::x_DSMapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Map(obj, this);
}


void CBioseq_set_Info::x_DSUnmapObject(CConstRef<TObject> obj, CDataSource& ds)
{
    ds.x_Unmap(obj, this);
}


void CBioseq_set_Info::x_TSEAttachContents(CTSE_Info& tse)
{
    TParent::x_TSEAttachContents(tse);
    _ASSERT(m_Bioseq_set_Id < 0);
    if ( IsSetId() ) {
        m_Bioseq_set_Id = x_GetBioseq_set_Id(GetId());
        if ( m_Bioseq_set_Id >= 0 ) {
            tse.x_SetBioseq_setId(m_Bioseq_set_Id, this);
        }
    }
    SetBioObjectId(tse.x_IndexBioseq_set(this));
    // members
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_TSEAttach(tse);
    }
}


void CBioseq_set_Info::x_TSEDetachContents(CTSE_Info& tse)
{
    // members
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_TSEDetach(tse);
    }
    if ( m_Bioseq_set_Id >= 0 ) {
        tse.x_ResetBioseq_setId(m_Bioseq_set_Id, this);
        m_Bioseq_set_Id = -1;
    }
    TParent::x_TSEDetachContents(tse);
}


void CBioseq_set_Info::x_ParentAttach(CSeq_entry_Info& parent)
{
    TParent::x_ParentAttach(parent);
    CSeq_entry& entry = parent.x_GetObject();
    _ASSERT(entry.IsSet() && &entry.GetSet() == m_Object);
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        if ( (*it)->x_GetObject().GetParentEntry() != &entry ) {
            entry.ParentizeOneLevel();
            break;
        }
    }
#ifdef _DEBUG
    TSeq_set::const_iterator it2 = m_Seq_set.begin();
    NON_CONST_ITERATE ( CBioseq_set::TSeq_set, it,
                        entry.SetSet().SetSeq_set() ) {
        _ASSERT(it2 != m_Seq_set.end());
        _ASSERT(&(*it2)->x_GetObject() == *it);
        _ASSERT((*it)->GetParentEntry() == &entry);
        ++it2;
    }
    _ASSERT(it2 == m_Seq_set.end());
#endif
}


void CBioseq_set_Info::x_ParentDetach(CSeq_entry_Info& parent)
{
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_GetObject().ResetParentEntry();
    }
    TParent::x_ParentDetach(parent);
}


void CBioseq_set_Info::x_AddBioseqChunkId(TChunkId id)
{
    m_BioseqChunks.push_back(id);
    x_SetNeedUpdate(fNeedUpdate_bioseq);
}


void CBioseq_set_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
    if (flags & (fNeedUpdate_bioseq|fNeedUpdate_core|fNeedUpdate_children)) {
        x_LoadChunks(m_BioseqChunks);
    }
    if ( flags & (fNeedUpdate_core|fNeedUpdate_children) ) {
        if ( !m_Seq_set.empty() ) {
            const CBioseq_set::TSeq_set& seq_set = m_Object->GetSeq_set();
            _ASSERT(seq_set.size() == m_Seq_set.size());
            CBioseq_set::TSeq_set::const_iterator it2 = seq_set.begin();
            NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
                if ( flags & fNeedUpdate_core ) {
                    (*it)->x_UpdateCore();
                }
                if ( flags & fNeedUpdate_children ) {
                    (*it)->x_Update((flags & fNeedUpdate_children) | 
                                    (flags >> kNeedUpdate_bits));
                }
                _ASSERT(it2->GetPointer() == &(*it)->x_GetObject());
                ++it2;
            }
        }
    }
    TParent::x_DoUpdate(flags);
}


void CBioseq_set_Info::x_SetObject(TObject& obj)
{
    _ASSERT(!m_Object);
    m_Object.Reset(&obj);
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    if ( obj.IsSetSeq_set() ) {
        NON_CONST_ITERATE ( TObject::TSeq_set, it, obj.SetSeq_set() ) {
            CRef<CSeq_entry_Info> info(new CSeq_entry_Info(**it));
            m_Seq_set.push_back(info);
            x_AttachEntry(info);
        }
    }
    if ( obj.IsSetAnnot() ) {
        x_SetAnnot();
    }
}


void CBioseq_set_Info::x_SetObject(const CBioseq_set_Info& info,
                                   TObjectCopyMap* copy_map)
{
    _ASSERT(!m_Object);
    m_Object = sx_ShallowCopy(info.x_GetObject());
    if ( HasDataSource() ) {
        x_DSMapObject(m_Object, GetDataSource());
    }
    if ( info.IsSetSeq_set() ) {
        _ASSERT(m_Object->GetSeq_set().size() == info.m_Seq_set.size());
        m_Object->SetSeq_set().clear();
        ITERATE ( TSeq_set, it, info.m_Seq_set ) {
            AddEntry(Ref(new CSeq_entry_Info(**it, copy_map)));
        }
    }
    if ( info.IsSetAnnot() ) {
        x_SetAnnot(info, copy_map);
    }
}


CRef<CBioseq_set> CBioseq_set_Info::sx_ShallowCopy(const CBioseq_set& src)
{
    CRef<TObject> obj(new TObject);
    if ( src.IsSetId() ) {
        obj->SetId(const_cast<TId&>(src.GetId()));
    }
    if ( src.IsSetColl() ) {
        obj->SetColl(const_cast<TColl&>(src.GetColl()));
    }
    if ( src.IsSetLevel() ) {
        obj->SetLevel(src.GetLevel());
    }
    if ( src.IsSetClass() ) {
        obj->SetClass(src.GetClass());
    }
    if ( src.IsSetRelease() ) {
        obj->SetRelease(src.GetRelease());
    }
    if ( src.IsSetDate() ) {
        obj->SetDate(const_cast<TDate&>(src.GetDate()));
    }
    if ( src.IsSetDescr() ) {
        obj->SetDescr().Set() = src.GetDescr().Get();
    }
    if ( src.IsSetSeq_set() ) {
        obj->SetSeq_set() = src.GetSeq_set();
    }
    if ( src.IsSetAnnot() ) {
        obj->SetAnnot() = src.GetAnnot();
    }
    return obj;
}


int CBioseq_set_Info::x_GetBioseq_set_Id(const CObject_id& object_id)
{
    int ret = -1;
    if ( object_id.Which() == object_id.e_Id ) {
        ret = object_id.GetId();
    }
    return ret;
}


bool CBioseq_set_Info::x_IsSetDescr(void) const
{
    return m_Object->IsSetDescr();
}


bool CBioseq_set_Info::x_CanGetDescr(void) const
{
    return m_Object->CanGetDescr();
}


const CSeq_descr& CBioseq_set_Info::x_GetDescr(void) const
{
    return m_Object->GetDescr();
}


CSeq_descr& CBioseq_set_Info::x_SetDescr(void)
{
    return m_Object->SetDescr();
}


void CBioseq_set_Info::x_SetDescr(TDescr& v)
{
    m_Object->SetDescr(v);
}


void CBioseq_set_Info::x_ResetDescr(void)
{
    m_Object->ResetDescr();
}


CBioseq_set::TAnnot& CBioseq_set_Info::x_SetObjAnnot(void)
{
    return m_Object->SetAnnot();
}


void CBioseq_set_Info::x_ResetObjAnnot(void)
{
    m_Object->ResetAnnot();
}


CRef<CSeq_entry_Info> CBioseq_set_Info::AddEntry(CSeq_entry& entry,
                                                 int index, 
                                                 bool set_uniqid)
{
    CRef<CSeq_entry_Info> info(new CSeq_entry_Info(entry));
    AddEntry(info, index, set_uniqid);
    return info;
}


void CBioseq_set_Info::AddEntry(CRef<CSeq_entry_Info> info, int index, 
                                bool set_uniqid)
{
    _ASSERT(!info->HasParent_Info());
    CBioseq_set::TSeq_set& obj_seq_set = m_Object->SetSeq_set();

    CRef<CSeq_entry> obj(&info->x_GetObject());

    //_ASSERT(obj_seq_set.size() == m_Seq_set.size()); quadratic performance
    if ( size_t(index) >= m_Seq_set.size() ) {
        obj_seq_set.push_back(obj);
        m_Seq_set.push_back(info);
    }
    else {
        CBioseq_set::TSeq_set::iterator obj_it = obj_seq_set.begin();
        for ( int i = 0; i < index; ++i ) {
            ++obj_it;
        }
        obj_seq_set.insert(obj_it, obj);
        m_Seq_set.insert(m_Seq_set.begin()+index, info);
    }
    x_AttachEntry(info);

    if (set_uniqid)
        info->SetBioObjectId(GetTSE_Info().x_RegisterBioObject(*info));
}


void CBioseq_set_Info::RemoveEntry(CRef<CSeq_entry_Info> info)
{
    if ( &info->GetParentBioseq_set_Info() != this ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CBioseq_set_Info::x_RemoveEntry: "
                   "not a parent");
    }

    CRef<CSeq_entry> obj(const_cast<CSeq_entry*>(&info->x_GetObject()));
    CBioseq_set::TSeq_set& obj_seq_set = m_Object->SetSeq_set();
    TSeq_set::iterator info_it =
        find(m_Seq_set.begin(), m_Seq_set.end(), info);
    CBioseq_set::TSeq_set::iterator obj_it =
        find(obj_seq_set.begin(), obj_seq_set.end(), obj);

    _ASSERT(info_it != m_Seq_set.end());
    _ASSERT(obj_it != obj_seq_set.end());

    x_DetachEntry(info);

    m_Seq_set.erase(info_it);
    obj_seq_set.erase(obj_it);
}


int CBioseq_set_Info::GetEntryIndex(const CSeq_entry_Info& info) const
{
    CRef<CSeq_entry_Info> entry(const_cast<CSeq_entry_Info*>(&info));
    int index = 0;
    ITERATE(TSeq_set, it, m_Seq_set) {
        if (*it == entry)
            return index;
        index++;
    }
    return -1;
}


CConstRef<CSeq_entry_Info> CBioseq_set_Info::GetFirstEntry(void) const
{
    if ( m_Seq_set.empty() ) {
        return null;
    }
    return m_Seq_set.front();
}


void CBioseq_set_Info::x_AttachEntry(CRef<CSeq_entry_Info> entry)
{
    _ASSERT(!entry->HasParent_Info());
    entry->x_ParentAttach(*this);
    _ASSERT(&entry->GetParentBioseq_set_Info() == this);
    x_AttachObject(*entry);
}


void CBioseq_set_Info::x_DetachEntry(CRef<CSeq_entry_Info> entry)
{
    _ASSERT(&entry->GetParentBioseq_set_Info() == this);
    x_DetachObject(*entry);
    entry->x_ParentDetach(*this);
    _ASSERT(!entry->HasParent_Info());
}


void CBioseq_set_Info::UpdateAnnotIndex(void) const
{
    if ( x_DirtyAnnotIndex() ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
        _ASSERT(!x_DirtyAnnotIndex());
    }
}


void CBioseq_set_Info::x_UpdateAnnotIndexContents(CTSE_Info& tse)
{
    TParent::x_UpdateAnnotIndexContents(tse);
    for ( size_t i = 0; i < m_Seq_set.size(); ++i ) {
        m_Seq_set[i]->x_UpdateAnnotIndex(tse);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE
