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
#include <objmgr/impl/bioseq_info.hpp>
#include <objmgr/impl/data_source.hpp>

#include <objects/general/Object_id.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


CSeq_entry_Info::CSeq_entry_Info(void)
    : m_Parent(0),
      m_TSE_Info(0),
      m_Bioseq_set_Id(-1),
      m_DirtyAnnotIndex(false)
{
}


CSeq_entry_Info::CSeq_entry_Info(CSeq_entry& entry, CSeq_entry_Info& parent)
    : m_Parent(&parent),
      m_TSE_Info(&parent.GetTSE_Info()),
      m_Bioseq_set_Id(-1),
      m_DirtyAnnotIndex(false)
{
    _ASSERT(!parent.m_Bioseq);
    {{
        parent.m_Entries.push_back(Ref(this));
    }}
    try {
        x_SetSeq_entry(entry);
    }
    catch ( exception& ) {
        _ASSERT(parent.m_Entries.back().GetPointer() == this);
        parent.m_Entries.back().Release();
        parent.m_Entries.pop_back();
        throw;
    }
}


CSeq_entry_Info::~CSeq_entry_Info(void)
{
}


void CSeq_entry_Info::x_DSAttachThis(void)
{
    GetDataSource().x_MapSeq_entry(*this);
}


void CSeq_entry_Info::x_DSDetachThis(void)
{
    GetDataSource().x_UnmapSeq_entry(*this);
}


void CSeq_entry_Info::x_DSAttach(void)
{
    x_DSAttachThis();
    try {
        x_DSAttachContents();
    }
    catch ( ... ) {
        x_DSDetachThis();
        throw;
    }
}


template<class Iterator>
void x_DSDetachObjects(const Iterator& begin, const Iterator& end)
{
    for ( Iterator iter = begin; iter != end; ++iter ) {
        (*iter)->x_DSDetach();
    }
}


template<class Iterator>
void x_DSAttachObjects(const Iterator& begin, const Iterator& end)
{
    for ( Iterator iter = begin; iter != end; ++iter ) {
        try {
            (*iter)->x_DSAttach();
        }
        catch ( ... ) {
            // rollback
            x_DSDetachObjects(begin, iter);
            throw;
        }
    }
}


template<class Container>
void x_DSAttachObjects(Container& container)
{
    x_DSAttachObjects(container.begin(), container.end());
}


template<class Container>
void x_DSDetachObjects(Container& container)
{
    x_DSDetachObjects(container.begin(), container.end());
}


void CSeq_entry_Info::x_DSAttachContents(void)
{
    x_DSAttachObjects(m_Annots);
    try {
        if ( m_Bioseq ) {
            _ASSERT(m_Entries.empty());
            m_Bioseq->x_DSAttach();
        }
        else {
            x_DSAttachObjects(m_Entries);
        }
    }
    catch ( ... ) {
        x_DSDetachObjects(m_Annots);
        throw;
    }
}


void CSeq_entry_Info::x_DSDetachContents(void)
{
    if ( m_Bioseq ) {
        _ASSERT(m_Entries.empty());
        m_Bioseq->x_DSDetach();
    }
    else {
        x_DSDetachObjects(m_Entries);
    }
    x_DSDetachObjects(m_Annots);
}


void CSeq_entry_Info::x_SetSeq_entry(CSeq_entry& entry)
{
    _ASSERT(!m_Seq_entry);
    m_Seq_entry.Reset(&entry);
    x_TSEAttach();
}


void CSeq_entry_Info::x_TSEAttach(void)
{
    CSeq_entry& entry = GetSeq_entry();
    switch ( entry.Which() ) {
    case CSeq_entry::e_Seq:
        new CBioseq_Info(entry.SetSeq(), *this);
        break;
    case CSeq_entry::e_Set:
        x_TSEAttachBioseq_set(entry.SetSet());
        break;
    default:
        break;
    }
}


void CSeq_entry_Info::x_TSEDetach(void)
{
    x_TSEDetachContents();
    x_TSEDetachThis();
}


void CSeq_entry_Info::x_TSEAttachBioseq_set(CBioseq_set& seq_set)
{
    x_TSEAttachBioseq_set_Id(seq_set);
    try {
        NON_CONST_ITERATE ( CBioseq_set::TSeq_set, it, seq_set.SetSeq_set() ) {
            new CSeq_entry_Info(**it, *this);
        }
        if ( seq_set.IsSetAnnot() ) {
            x_TSEAttachSeq_annots(seq_set.SetAnnot());
        }
    }
    catch ( exception& ) {
        x_TSEDetachThis();
        throw;
    }
}


void CSeq_entry_Info::x_TSEDetachContents(void)
{
    if ( m_Bioseq ) {
        m_Bioseq->x_TSEDetach();
    }
    NON_CONST_ITERATE ( TEntries, it, m_Entries ) {
        (*it)->x_TSEDetach();
    }
    NON_CONST_ITERATE ( TAnnots, it, m_Annots ) {
        (*it)->x_TSEDetach();
    }
}


void CSeq_entry_Info::x_TSEDetachThis(void)
{
    x_TSEDetachBioseq_set_Id();
}


void CSeq_entry_Info::x_TSEAttachBioseq_set_Id(const CBioseq_set& seq_set)
{
    _ASSERT(m_Bioseq_set_Id < 0);
    if ( seq_set.IsSetId() ) {
        const CObject_id& object_id = seq_set.GetId();
        if ( object_id.Which() != object_id.e_Id ) {
            return;
        }
        int id = object_id.GetId();
        if ( id < 0 ) {
            return;
        }
        GetTSE_Info().x_SetBioseq_setId(id, this);
        m_Bioseq_set_Id = id;
    }
}


void CSeq_entry_Info::x_TSEDetachBioseq_set_Id(void)
{
    if ( m_Bioseq_set_Id >= 0 ) {
        GetTSE_Info().x_ResetBioseq_setId(m_Bioseq_set_Id, this);
        m_Bioseq_set_Id = -1;
    }
}


void CSeq_entry_Info::x_TSEAttachSeq_annots(TSeq_annots& annots)
{
    NON_CONST_ITERATE( TSeq_annots, it, annots ) {
        new CSeq_annot_Info(**it, *this);
    }
}


void CSeq_entry_Info::x_SetDirtyAnnotIndex(void)
{
    if ( !x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = true;
        if ( m_Parent ) {
            m_Parent->x_SetDirtyAnnotIndex();
        }
        else if ( HaveDataSource() ) {
            _ASSERT(m_TSE_Info == this);
            GetDataSource().x_SetDirtyAnnotIndex(m_TSE_Info);
        }
    }
}


void CSeq_entry_Info::x_ResetDirtyAnnotIndex(void)
{
    if ( x_DirtyAnnotIndex() ) {
        m_DirtyAnnotIndex = false;
        if ( !m_Parent && HaveDataSource() ) {
            _ASSERT(m_TSE_Info == this);
            GetDataSource().x_ResetDirtyAnnotIndex(m_TSE_Info);
        }
    }
}


void CSeq_entry_Info::UpdateAnnotIndex(void) const
{
    if ( x_DirtyAnnotIndex() ) {
        GetTSE_Info().UpdateAnnotIndex(*this);
        _ASSERT(!x_DirtyAnnotIndex());
    }
}


void CSeq_entry_Info::x_UpdateAnnotIndex(void)
{
    if ( x_DirtyAnnotIndex() ) {
        x_UpdateAnnotIndexContents();
        x_ResetDirtyAnnotIndex();
    }
}


void CSeq_entry_Info::x_UpdateAnnotIndexContents(void)
{
    NON_CONST_ITERATE ( TAnnots, it, m_Annots ) {
        (*it)->x_UpdateAnnotIndex();
    }
    NON_CONST_ITERATE ( TEntries, it, m_Entries ) {
        (*it)->x_UpdateAnnotIndex();
    }
}


bool CSeq_entry_Info::HaveDataSource(void) const
{
    return GetTSE_Info().HaveDataSource();
}


CDataSource& CSeq_entry_Info::GetDataSource(void) const
{
    return GetTSE_Info().GetDataSource();
}


const CSeq_entry& CSeq_entry_Info::GetTSE(void) const
{
    return GetTSE_Info().GetTSE();
}


CSeq_entry& CSeq_entry_Info::GetTSE(void)
{
    return GetTSE_Info().GetTSE();
}


void CSeq_entry_Info::x_AddAnnot(CSeq_annot& annot)
{
    CSeq_entry& entry = GetSeq_entry();
    switch ( entry.Which() ) {
    case CSeq_entry::e_Set:
        entry.SetSet().SetAnnot().push_back(Ref(&annot));
        break;
    case CSeq_entry::e_Seq:
        entry.SetSeq().SetAnnot().push_back(Ref(&annot));
        break;
    default:
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CSeq_entry_Info::x_AddAnnot: "
                   "entry is not initializated");
    }

    CRef<CSeq_annot_Info> annot_info(new CSeq_annot_Info(annot, *this));
    if ( HaveDataSource() ) {
        annot_info->x_DSAttach();
    }
}


void CSeq_entry_Info::x_RemoveAnnot(CSeq_annot_Info& annot_info)
{
    CSeq_entry& entry = GetSeq_entry();
    CSeq_annot& annot = annot_info.GetSeq_annot();
    switch ( entry.Which() ) {
    case CSeq_entry::e_Set:
        entry.SetSet().SetAnnot().remove(Ref(&annot));
        if ( entry.GetSet().GetAnnot().empty() ) {
            entry.SetSet().ResetAnnot();
        }
        break;
    case CSeq_entry::e_Seq:
        entry.SetSeq().SetAnnot().remove(Ref(&annot));
        if ( entry.GetSeq().GetAnnot().empty() ) {
            entry.SetSeq().ResetAnnot();
        }
        break;
    default:
        NCBI_THROW(CObjMgrException, eModifyDataError,
                   "CSeq_entry_Info::x_RemoveAnnot: "
                   "entry is not initializated");
    }

    annot_info.x_TSEDetach();
    if ( HaveDataSource() ) {
        annot_info.x_DSDetach();
    }

    NON_CONST_ITERATE ( TAnnots, it, m_Annots ) {
        if ( *it == &annot_info ) {
            m_Annots.erase(it);
            break;
        }
    }
}


void CSeq_entry_Info::x_AddEntry(CSeq_entry& child_entry)
{
    CSeq_entry& parent_entry = GetSeq_entry();
    if ( parent_entry.Which() != CSeq_entry::e_Set ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CSeq_entry_Info::x_AddEntry: "
                   "invalid entry type");
    }

    parent_entry.SetSet().SetSeq_set().push_back(Ref(&child_entry));
    parent_entry.Parentize();

    CRef<CSeq_entry_Info> child_info(new CSeq_entry_Info(child_entry, *this));
    if ( HaveDataSource() ) {
        child_info->x_DSAttach();
    }
}


void CSeq_entry_Info::x_RemoveEntry(CSeq_entry_Info& child_info)
{
    CSeq_entry& parent_entry = GetSeq_entry();
    CSeq_entry& child_entry = child_info.GetSeq_entry();
    if ( parent_entry.Which() != CSeq_entry::e_Set ) {
        NCBI_THROW(CObjMgrException, eAddDataError,
                   "CSeq_entry_Info::x_AddEntry: "
                   "invalid entry type");
    }
    parent_entry.SetSet().SetSeq_set().remove(Ref(&child_entry));
    child_entry.ResetParentEntry();

    child_info.x_TSEDetach();
    if ( HaveDataSource() ) {
        child_info.x_DSDetach();
    }

    NON_CONST_ITERATE ( TEntries, it, m_Entries ) {
        if ( *it == &child_info ) {
            m_Entries.erase(it);
            break;
        }
    }
}


END_SCOPE(objects)
END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
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
