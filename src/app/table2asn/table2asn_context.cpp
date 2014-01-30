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
* Author:  Sergiy Gotvyanskyy, NCBI
*
* File Description:
*   Context structure holding all table2asn parameters
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <corelib/ncbistd.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/feat_ci.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/feat_ci.hpp>
#include <objmgr/bioseq_ci.hpp>
#include <objects/general/Dbtag.hpp>

#include <objects/submit/Seq_submit.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objtools/readers/source_mod_parser.hpp>
#include <objects/general/Date.hpp>
#include <objtools/readers/message_listener.hpp>

#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>
#include <objects/general/Int_fuzz.hpp>

#include <objects/submit/Submit_block.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/biblio/Cit_sub.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>

#include <objects/submit/Contact_info.hpp>
#include <objects/biblio/Author.hpp>
#include <objects/biblio/Auth_list.hpp>

#include "table2asn_context.hpp"
#include "objtools/edit/dblink_field.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace
{

bool x_ApplyCreateDate(CSeq_entry& entry)
{
    bool need_update = false;
    if (CAutoAddDesc::LocateDesc(entry.SetDescr(), CSeqdesc::e_Create_date).NotNull())
        need_update = true;
    else
    {
        CRef<CSeqdesc> create_date(new CSeqdesc);
        CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
        create_date->SetCreate_date(*date);

        entry.SetDescr().Set().push_back(create_date);
    }
    return need_update;
}

void x_ApplySourceQualifiers(CBioseq& bioseq, CSourceModParser& smp)
{
    CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
    NON_CONST_ITERATE(CSourceModParser::TMods, mod, unused_mods)
    {
        if (NStr::CompareNocase(mod->key, "bioproject")==0)
            edit::CDBLink::SetBioProject(CTable2AsnContext::SetUserObject(bioseq.SetDescr(), "DBLink"), mod->value);
        else
        if (NStr::CompareNocase(mod->key, "biosample")==0)
            edit::CDBLink::SetBioSample(CTable2AsnContext::SetUserObject(bioseq.SetDescr(), "DBLink"), mod->value);
    }
}

void x_ApplySourceQualifiers(objects::CBioseq& bioseq, const string& src_qualifiers)
{
    if (src_qualifiers.empty())
        return;

    CSourceModParser smp;
    if (true)
    {
        smp.ParseTitle(src_qualifiers, CConstRef<CSeq_id>(bioseq.GetFirstId()) );
        smp.ApplyAllMods(bioseq);

        x_ApplySourceQualifiers(bioseq, smp);
#if 0
//        if( TestFlag(fUnknModThrow) ) 
        {
            CSourceModParser::TMods unused_mods = smp.GetMods(CSourceModParser::fUnusedMods);
            if( ! unused_mods.empty() )
            {
                // there are unused mods and user specified to throw if any
                // unused
                CNcbiOstrstream err;
                err << "CFastaReader: Inapplicable or unrecognized modifiers on ";

                // get sequence ID
                const CSeq_id* seq_id = bioseq.GetFirstId();
                if( seq_id ) {
                    err << seq_id->GetSeqIdString();
                } else {
                    // seq-id unknown
                    err << "sequence";
                }

                err << ":";
                ITERATE(CSourceModParser::TMods, mod_iter, unused_mods) {
                    err << " [" << mod_iter->key << "=" << mod_iter->value << ']';
                }
                err << " around line " + NStr::NumericToString(iLineNum);
                NCBI_THROW2(CObjReaderParseException, eUnusedMods,
                    (string)CNcbiOstrstreamToString(err),
                    iLineNum);
            }
        }
#endif

#if 0
        smp.GetLabel(&title, CSourceModParser::fUnusedMods);

        copy( smp.GetBadMods().begin(), smp.GetBadMods().end(),
            inserter(m_BadMods, m_BadMods.begin()) );
        CSourceModParser::TMods unused_mods =
            smp.GetMods(CSourceModParser::fUnusedMods);
        copy( unused_mods.begin(), unused_mods.end(),
            inserter(m_UnusedMods, m_UnusedMods.begin() ) );
#endif
    }

}

};


CTable2AsnContext::CTable2AsnContext():
    m_output(0),
    m_copy_genid_to_note(false),
    m_ProjectVersionNumber(0),
    m_flipped_struc_cmt(false),
    m_RemoteTaxonomyLookup(false),
    m_RemotePubLookup(false),
    m_HandleAsSet(false),
    m_GenomicProductSet(false),
    m_SetIDFromFile(false),
    m_NucProtSet(false),
    m_taxid(0),
    m_avoid_orf_lookup(false),
    m_gapNmin(0),
    m_gap_Unknown_length(0),
    m_minimal_sequence_length(0),
    m_fcs_trim(false),
    m_avoid_submit_block(false)
{
}

CTable2AsnContext::~CTable2AsnContext()
{
}

void CTable2AsnContext::AddUserTrack(CSeq_descr& SD, const string& type, const string& lbl, const string& data) const
{
    if (data.empty())
        return;

    CRef<CUser_field> uf(new CUser_field);
    uf->SetLabel().SetStr(lbl);
    uf->SetNum(1);
    uf->SetData().SetStr(data);
    SetUserObject(SD, type).SetData().push_back(uf);
}

void CTable2AsnContext::ApplySourceQualifiers(CSerialObject& obj, const string& src_qualifiers) const
{
    if (src_qualifiers.empty())
        return;

    CSeq_entry* entry = 0;
    if (obj.GetThisTypeInfo()->IsType(CSeq_entry::GetTypeInfo()))
        entry = (CSeq_entry*)(&obj);
    else
    if (obj.GetThisTypeInfo()->IsType(CSeq_submit::GetTypeInfo()))
    {
        CSeq_submit* submit = (CSeq_submit*)(&obj);
        NON_CONST_ITERATE(CSeq_submit::TData::TEntrys, it, submit->SetData().SetEntrys())
        {
            ApplySourceQualifiers(**it, src_qualifiers);
        }
    }

    if (entry)
        switch(entry->Which())
        {
        case CSeq_entry::e_Seq:
            x_ApplySourceQualifiers(entry->SetSeq(), m_source_qualifiers);
            break;
        case CSeq_entry::e_Set:
            NON_CONST_ITERATE(CBioseq_set_Base::TSeq_set, it, entry->SetSet().SetSeq_set())
            {
                ApplySourceQualifiers(**it, m_source_qualifiers);
            }
            break;
        default:
            break;
        }
}

CUser_object& CTable2AsnContext::SetUserObject(CSeq_descr& descr, const string& type)
{
    CRef<CUser_object> user_obj;
    NON_CONST_ITERATE(CSeq_descr::Tdata, desc_it, descr.Set())
    {
        if ((**desc_it).IsUser() && (**desc_it).GetUser().IsSetType() &&
            (**desc_it).GetUser().GetType().IsStr() &&
            (**desc_it).GetUser().GetType().GetStr() == type)
        {
            return (**desc_it).SetUser();
        }
    }

    CRef<CObject_id> oi(new CObject_id);
    oi->SetStr(type);
    CRef<CUser_object> uo(new CUser_object);
    uo->SetType(*oi);

    CRef<CSeqdesc> sd(new CSeqdesc());
    sd->Select(CSeqdesc::e_User);
    sd->SetUser(*uo);

    descr.Set().push_back(sd);
    return *uo;
}

CBioSource& CTable2AsnContext::SetBioSource_old(CSeq_descr& SD)
{
    CRef<CSeqdesc> source_desc = CAutoAddDesc::LocateDesc(SD, CSeqdesc::e_Source);
    if (source_desc.Empty())
    {
        source_desc.Reset(new CSeqdesc());
        source_desc->Select(CSeqdesc::e_Source);
        SD.Set().push_back(source_desc);
    }
    return source_desc->SetSource();
}

// create-date should go into each bioseq
bool CTable2AsnContext::ApplyCreateDate(CSeq_entry& entry) const
{
    bool need_update = false;
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        need_update |= x_ApplyCreateDate(entry);
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            need_update |= x_ApplyCreateDate(**it);
        }
        break;
    default:
        break;
    }
    return need_update;
}

const CSeqdesc& CAutoAddDesc::Get()
{
    if (m_desc.IsNull())
        m_desc = LocateDesc(*m_descr, m_which);
    return *m_desc;
}

CSeqdesc& CAutoAddDesc::Set(bool skip_lookup)
{
    if (!skip_lookup && m_desc.IsNull())
        m_desc = LocateDesc(*m_descr, m_which);
    if (m_desc.IsNull())
    {
        m_desc.Reset(new CSeqdesc);
        m_descr->Set().push_back(m_desc);
    }
    return *m_desc;
}

// update-date should go only to top-level bioseq-set or bioseq
CRef<CSeqdesc> CAutoAddDesc::LocateDesc(CSeq_descr& descr, CSeqdesc::E_Choice which)
{
    NON_CONST_ITERATE(CSeq_descr::Tdata, it, descr.Set())
    {
        if ((**it).Which() == which)
            return *it;
    }

    return CRef<CSeqdesc>();
}



void CTable2AsnContext::ApplyUpdateDate(objects::CSeq_entry& entry) const
{
    CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
    CAutoAddDesc date_desc(entry.SetDescr(), CSeqdesc::e_Update_date);
    date_desc.Set().SetUpdate_date(*date);

    /*
    CRef<CSeqdesc> date_desc = LocateDesc(entry.SetDescr(), CSeqdesc::e_Update_date);

    if (date_desc.IsNull())
    {
        date_desc.Reset(new CSeqdesc);
        date_desc->SetUpdate_date(*date);
        entry.SetDescr().Set().push_back(date_desc);
    }
    else
    {
        date_desc->SetUpdate_date(*date);
    }
    */
}

void CTable2AsnContext::ApplyAccession(objects::CSeq_entry& entry) const
{
    if (m_accession.empty())
        return;

    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
            CRef<CSeq_id> accession_id(new CSeq_id);
            accession_id->SetGenbank().SetAccession(m_accession);
            entry.SetSeq().SetId().clear();
            entry.SetSeq().SetId().push_back(accession_id);
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            ApplyAccession(**it);
        }
        break;
    default:
        break;
    }
}

void CTable2AsnContext::HandleGaps(objects::CSeq_entry& entry) const
{
    if (m_gapNmin==0 && m_gap_Unknown_length > 0)
        return;

    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        {
            if (entry.SetSeq().IsSetInst())
            {
                CSeq_inst& inst = entry.SetSeq().SetInst();
                if (inst.IsSetExt() && inst.SetExt().IsDelta())
                {
                    NON_CONST_ITERATE(CDelta_ext::Tdata, it, inst.SetExt().SetDelta().Set())
                    {
                        if ((**it).IsLiteral())
                        {
                            CDelta_seq::TLiteral& lit = (**it).SetLiteral();
                            if (!lit.IsSetSeq_data() && lit.IsSetLength() && lit.GetLength() == m_gap_Unknown_length)
                            {
                                lit.SetFuzz().SetLim(CInt_fuzz::eLim_unk);
                            }
                        }
                    }
                }
            }
        }
        break;
    case CSeq_entry::e_Set:
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
        {
            HandleGaps(**it);
        }
        break;
    default:
        break;
    }
}

CRef<CSerialObject> CTable2AsnContext::CreateSubmitFromTemplate(CRef<CSeq_entry> object) const
{
    if (m_submit_template.NotEmpty())
    {
        CRef<CSeq_submit> submit(new CSeq_submit);
        submit->Assign(*m_submit_template);
        submit->SetData().SetEntrys().clear();
        submit->SetData().SetEntrys().push_back(object);
        return CRef<CSerialObject>(submit);
    }     

    return CRef<CSerialObject>(object);
}

CRef<CSerialObject> CTable2AsnContext::CreateSeqEntryFromTemplate(CRef<CSeq_entry> object) const
{
    if (m_submit_template.NotEmpty())
    {
        if (m_entry_template.NotEmpty() && m_entry_template->IsSetDescr())
        {
            object->SetDescr().Set().insert(object->SetDescr().Set().end(),
                m_entry_template->GetDescr().Get().begin(),
                m_entry_template->GetDescr().Get().end());
        }

        if (m_submit_template->IsSetSub() &&
            m_submit_template->GetSub().IsSetCit())
        {
            CRef<CPub> pub(new CPub);
            pub->SetSub().Assign(m_submit_template->GetSub().GetCit());
#if 0
            if (!pub->SetSub().IsSetDate())
            {
                CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
                pub->SetSub().SetDate(*date);
            }
#endif

            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetPub().SetPub().Set().push_back(pub);
            object->SetDescr().Set().push_back(desc);
        }

#if 0
        if (m_submit_template->IsSetSub() &&
            m_submit_template->GetSub().IsSetContact() && 
            m_submit_template->GetSub().GetContact().IsSetContact())
        {
            CRef<CAuthor> author(new CAuthor);
            author->Assign(m_submit_template->GetSub().GetContact().GetContact());
            CRef<CPub> pub(new CPub);               
            pub->SetSub().SetAuthors().SetNames().SetStd().push_back(author);

            CRef<CSeqdesc> desc(new CSeqdesc);
            desc->SetPub().SetPub().Set().push_back(pub);
            object->SetDescr().Set().push_back(desc);
        }
#endif

        object->Parentize();
    }
    return CRef<CSerialObject>(object);
}

void CTable2AsnContext::MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src) const
{
    ITERATE(CSeq_descr::Tdata, it, src.Get())
    {
        CRef<CSeqdesc> desc;
        switch ((**it).Which())
        {
        case CSeqdesc::e_Pub:
            break;
        default:
            desc = CAutoAddDesc::LocateDesc(dest, (**it).Which());
        }
        if (desc.Empty())
        {
            desc.Reset(new CSeqdesc);
            dest.Set().push_back(desc);
        }
        desc->Assign(**it);
    }
}

void CTable2AsnContext::MergeWithTemplate(CSeq_entry& entry) const
{
    if (entry.IsSet())
    {
        CSeq_entry_Base::TSet::TSeq_set& data = entry.SetSet().SetSeq_set();
        NON_CONST_ITERATE(CSeq_entry_Base::TSet::TSeq_set, it, data)
        {     
            MergeWithTemplate(**it);
        }
    }
    else
    if (entry.IsSeq())
    {
        MergeSeqDescr(entry.SetDescr(), m_entry_template->GetDescr());
    }
}

void CTable2AsnContext::SetSeqId(CSeq_entry& entry) const
{
    if (entry.IsSeq())
    {
        string base;
        CDirEntry::SplitPath(m_current_file, 0, &base, 0);
        CRef<CSeq_id> id(new CSeq_id(string("lcl|") + base));
        entry.SetSeq().SetId().clear();
        entry.SetSeq().SetId().push_back(id);
        // now it's good to rename features ....
    }
}

void CTable2AsnContext::CopyFeatureIdsToComments(CSeq_entry& entry) const
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle h_entry = scope.AddTopLevelSeqEntry(entry);
    for (CFeat_CI feat_it(h_entry, SAnnotSelector(CSeqFeatData::e_Rna) ); feat_it; ++feat_it)
    {
       //cout << "has feature" << endl;    
    }
}

void CTable2AsnContext::SetSeqId(CSeq_entry& entry) const
{
    if (entry.IsSeq())
    {
        string base;
        CDirEntry::SplitPath(m_current_file, 0, &base, 0);
        CRef<CSeq_id> id(new CSeq_id(string("lcl|") + base));
        entry.SetSeq().SetId().clear();
        entry.SetSeq().SetId().push_back(id);
        // now it's good to rename features ....
    }
}

void CTable2AsnContext::CopyFeatureIdsToComments(CSeq_entry& entry) const
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle h_entry = scope.AddTopLevelSeqEntry(entry);
    for (CBioseq_CI bioseq_it(h_entry); bioseq_it; ++bioseq_it)
    {
        for (CFeat_CI feat_it(*bioseq_it, SAnnotSelector(CSeqFeatData::e_Rna) ); feat_it; ++feat_it)
        {
            ITERATE(CBioseq::TId, id_it, bioseq_it->GetBioseqCore()->GetId())
            {
                if (!(**id_it).IsGeneral()) continue;

                const string& dbtag = (**id_it).GetGeneral().GetDb();
                if (NStr::Compare(dbtag, "TMSMART") == 0) continue;
                if (NStr::Compare(dbtag, "NCBIFILE") == 0) continue;

                CSeq_feat& feature = (CSeq_feat&) feat_it->GetOriginalFeature();

                if (!feature.IsSetComment())
                    feature.SetComment("");

                string& comment = feature.SetComment();
                if (!comment.empty())
                    comment += "; ";
                (**id_it).GetLabel(&comment);
           }
        }
    }
}


END_NCBI_SCOPE
