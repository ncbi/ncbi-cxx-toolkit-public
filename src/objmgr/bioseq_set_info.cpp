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


CBioseq_set_Info::CBioseq_set_Info(const CBioseq_set_Info& info)
    : m_Bioseq_set_Id(-1)
{
    x_SetObject(info);
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
    //ds.x_MapSeq_entry(*this);
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
    //ds.x_UnmapSeq_entry(*this);
    TParent::x_DSDetachContents(ds);
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


void CBioseq_set_Info::x_DoUpdate(TNeedUpdateFlags flags)
{
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


void CBioseq_set_Info::x_SetObject(const CBioseq_set_Info& info)
{
    _ASSERT(!m_Object);
    m_Object = sx_ShallowCopy(info.x_GetObject());
    if ( info.IsSetSeq_set() ) {
        _ASSERT(m_Object->GetSeq_set().size() == info.m_Seq_set.size());
        ITERATE ( TSeq_set, it, info.m_Seq_set ) {
            CRef<CSeq_entry_Info> info2(new CSeq_entry_Info(**it));
            m_Seq_set.push_back(info2);
            x_AttachEntry(info2);
        }
    }
    if ( info.IsSetAnnot() ) {
        x_SetAnnot(info);
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
        obj->SetDescr(const_cast<TDescr&>(src.GetDescr()));
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
                                                   int index)
{
    CRef<CSeq_entry_Info> info(new CSeq_entry_Info(entry));
    AddEntry(info, index);
    return info;
}


void CBioseq_set_Info::AddEntry(CRef<CSeq_entry_Info> info, int index)
{
    _ASSERT(!info->HasParent_Info());
    CBioseq_set::TSeq_set& obj_seq_set = m_Object->SetSeq_set();

    CRef<CSeq_entry> obj(&info->x_GetObject());

    _ASSERT(obj_seq_set.size() == m_Seq_set.size());
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
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        (*it)->x_UpdateAnnotIndex(tse);
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.8  2004/07/12 16:57:32  vasilche
 * Fixed loading of split Seq-descr and Seq-data objects.
 * They are loaded correctly now when GetCompleteXxx() method is called.
 *
 * Revision 1.7  2004/05/21 21:42:12  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.6  2004/05/06 17:32:37  grichenk
 * Added CanGetXXXX() methods
 *
 * Revision 1.5  2004/03/31 17:08:07  vasilche
 * Implemented ConvertSeqToSet and ConvertSetToSeq.
 *
 * Revision 1.4  2004/03/24 20:05:17  vasilche
 * Fixed compilation error on Sun.
 *
 * Revision 1.3  2004/03/24 18:57:35  vasilche
 * Added include <algorithm> for find().
 *
 * Revision 1.2  2004/03/24 18:30:29  vasilche
 * Fixed edit API.
 * Every *_Info object has its own shallow copy of original object.
 *
 * Revision 1.1  2004/03/16 15:47:27  vasilche
 * Added CBioseq_set_Handle and set of EditHandles
 *
 * Revision 1.10  2004/02/03 19:02:18  vasilche
 * Fixed broken 'dirty annot index' state after RemoveEntry().
 *
 * Revision 1.9  2004/02/02 14:46:44  vasilche
 * Several performance fixed - do not iterate whole tse set in CDataSource.
 *
 * Revision 1.8  2004/01/29 19:33:07  vasilche
 * Fixed coredump on WorkShop when invalid Seq-entry is added to CScope.
 *
 * Revision 1.7  2004/01/22 20:10:40  vasilche
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
 * Revision 1.6  2003/12/18 16:38:07  grichenk
 * Added CScope::RemoveEntry()
 *
 * Revision 1.5  2003/12/11 17:02:50  grichenk
 * Fixed CRef resetting in constructors.
 *
 * Revision 1.4  2003/11/19 22:18:03  grichenk
 * All exceptions are now CException-derived. Catch "exception" rather
 * than "runtime_error".
 *
 * Revision 1.3  2003/09/30 16:22:03  vasilche
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
