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

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CBioseq_set_Info::CBioseq_set_Info(void)
    : m_Bioseq_set_Id(-1)
{
}


CBioseq_set_Info::CBioseq_set_Info(const TObject& seqset)
    : m_Bioseq_set_Id(-1)
{
    x_SetObject(seqset);
}


CBioseq_set_Info::CBioseq_set_Info(const CBioseq_set_Info& info)
    : TParent(info),
      m_Object(info.m_Object),
      m_Id(info.m_Id),
      m_Coll(info.m_Coll),
      m_Level(info.m_Level),
      m_Release(info.m_Release),
      m_Date(info.m_Date)
{
    ITERATE ( TSeq_set, it, info.m_Seq_set ) {
        x_AttachEntry(Ref(new CSeq_entry_Info(**it)));
    }
}


CBioseq_set_Info::~CBioseq_set_Info(void)
{
}


inline
void CBioseq_set_Info::x_UpdateObject(CConstRef<TObject> obj)
{
    m_Object = obj;
    x_ResetModifiedMembers();
}


inline
void CBioseq_set_Info::x_UpdateModifiedObject(void) const
{
    if ( x_IsModified() ) {
        const_cast<CBioseq_set_Info*>(this)->x_UpdateObject(x_CreateObject());
    }
}


CConstRef<CBioseq_set> CBioseq_set_Info::GetCompleteBioseq_set(void) const
{
    x_UpdateModifiedObject();
    _ASSERT(!x_IsModified());
    return m_Object;
}


CConstRef<CBioseq_set> CBioseq_set_Info::GetBioseq_setCore(void) const
{
    x_UpdateModifiedObject();
    _ASSERT(!x_IsModified());
    return m_Object;
}


const char* CBioseq_set_Info::x_GetTypeName(void) const
{
    return "Bioseq-set";
}


const char* CBioseq_set_Info::x_GetMemberName(TMembers member) const
{
    if ( member & fMember_id ) {
        return "id";
    }
    if ( member & fMember_coll ) {
        return "coll";
    }
    if ( member & fMember_level ) {
        return "level";
    }
    if ( member & fMember_class ) {
        return "class";
    }
    if ( member & fMember_release ) {
        return "release";
    }
    if ( member & fMember_date ) {
        return "date";
    }
    if ( member & fMember_seq_set ) {
        return "seq-set";
    }
    return TParent::x_GetMemberName(member);
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


void CBioseq_set_Info::x_SetObject(const TObject& obj)
{
    _ASSERT(!m_Object);
    m_Object.Reset(&obj);
    if ( obj.IsSetId() ) {
        m_Id.Reset(&obj.GetId());
        x_SetSetMembers(fMember_id);
    }
    if ( obj.IsSetColl() ) {
        m_Coll.Reset(&obj.GetColl());
        x_SetSetMembers(fMember_coll);
    }
    if ( obj.IsSetLevel() ) {
        m_Level = obj.GetLevel();
        x_SetSetMembers(fMember_level);
    }
    if ( obj.IsSetClass() ) {
        m_Class = obj.GetClass();
        x_SetSetMembers(fMember_class);
    }
    if ( obj.IsSetRelease() ) {
        m_Release = obj.GetRelease();
        x_SetSetMembers(fMember_release);
    }
    if ( obj.IsSetDate() ) {
        m_Date.Reset(&obj.GetDate());
        x_SetSetMembers(fMember_date);
    }
    if ( obj.IsSetDescr() ) {
        x_SetDescr(obj.GetDescr());
    }
    if ( obj.IsSetSeq_set() ) {
        ITERATE ( TObject::TSeq_set, it, obj.GetSeq_set() ) {
            x_AttachEntry(Ref(new CSeq_entry_Info(**it)));
        }
        x_SetSetMembers(fMember_seq_set);
    }
    if ( obj.IsSetAnnot() ) {
        x_SetAnnot(obj.GetAnnot());
    }
}


CRef<CBioseq_set> CBioseq_set_Info::x_CreateObject(void) const
{        
    CRef<TObject> obj(new TObject);
    if ( IsSetId() ) {
        obj->SetId(const_cast<TId&>(GetId()));
    }
    if ( IsSetColl() ) {
        obj->SetColl(const_cast<TColl&>(GetColl()));
    }
    if ( IsSetLevel() ) {
        obj->SetLevel(GetLevel());
    }
    if ( IsSetClass() ) {
        obj->SetClass(GetClass());
    }
    if ( IsSetRelease() ) {
        obj->SetRelease(GetRelease());
    }
    if ( IsSetDate() ) {
        obj->SetDate(const_cast<TDate&>(GetDate()));
    }
    if ( IsSetDescr() ) {
        obj->SetDescr(const_cast<TDescr&>(GetDescr()));
    }
    if ( IsSetSeq_set() ) {
        TObject::TSeq_set& seq_set = obj->SetSeq_set();
        if ( x_IsModifiedMember(fMember_seq_set) ) {
            ITERATE ( TSeq_set, it, GetSeq_set() ) {
                CConstRef<CSeq_entry> add = (*it)->GetCompleteSeq_entry();
                seq_set.push_back(Ref(const_cast<CSeq_entry*>(&*add)));
            }
        }
        else {
            seq_set = m_Object->GetSeq_set();
        }
    }
    if ( IsSetAnnot() ) {
        TObject::TAnnot& annot = obj->SetAnnot();
        if ( x_IsModifiedMember(fMember_annot) ) {
            x_FillAnnot(annot);
        }
        else {
            annot = m_Object->GetAnnot();
        }
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


CRef<CSeq_entry_Info> CBioseq_set_Info::x_AddEntry(const CSeq_entry& entry,
                                                   int index)
{
    CRef<CSeq_entry_Info> info(new CSeq_entry_Info(entry));
    x_AddEntry(info, index);
    return info;
}


void CBioseq_set_Info::x_AddEntry(CRef<CSeq_entry_Info> entry, int index)
{
    x_AttachEntry(entry, index);
    x_SetModifiedMember(fMember_seq_set);
}


void CBioseq_set_Info::x_RemoveEntry(CRef<CSeq_entry_Info> entry)
{
    if ( &entry->GetParentBioseq_set_Info() != this ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CBioseq_set_Info::x_RemoveEntry: "
                   "not a parent");
    }
    x_DetachEntry(entry);
    NON_CONST_ITERATE ( TSeq_set, it, m_Seq_set ) {
        if ( *it == entry ) {
            m_Seq_set.erase(it);
            break;
        }
    }
    x_SetModifiedMember(fMember_seq_set);
}


void CBioseq_set_Info::x_AttachEntry(CRef<CSeq_entry_Info> entry, int index)
{
    _ASSERT(!entry->HaveParent_Info());
    if ( size_t(index) > m_Seq_set.size() ) {
        index = m_Seq_set.size();
    }
    m_Seq_set.insert(m_Seq_set.begin()+index, entry);
    entry->x_ParentAttach(*this);
    _ASSERT(&entry->GetParentBioseq_set_Info() == this);
    x_AttachObject(*entry);
}


void CBioseq_set_Info::x_DetachEntry(CRef<CSeq_entry_Info> entry)
{
    _ASSERT(&entry->GetParentBioseq_set_Info() == this);
    x_DetachObject(*entry);
    entry->x_ParentDetach(*this);
    _ASSERT(!entry->HaveParent_Info());
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
