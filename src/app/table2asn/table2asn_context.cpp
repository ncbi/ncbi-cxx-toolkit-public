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
#include <objtools/edit/remote_updater.hpp>

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

#include <objects/seqfeat/Gb_qual.hpp>

#include <objmgr/seqdesc_ci.hpp>

#include <objmgr/scope.hpp>

#include "table2asn_context.hpp"

#include <common/test_assert.h>  /* This header must go last */

BEGIN_NCBI_SCOPE

USING_SCOPE(objects);

namespace
{

bool x_ApplyCreateDate(CSeq_entry& entry)
{
    CAutoAddDesc create_date_desc(entry.SetDescr(), CSeqdesc::e_Create_date);
    if (create_date_desc.IsNull())
    {   
        CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
        create_date_desc.Set().SetCreate_date(*date);
        return false; // no need update_date    
    }
    else
        return true; // need update_date
}


};


CTable2AsnContext::CTable2AsnContext():
    m_output(0),
    m_delay_genprodset(false),
    m_copy_genid_to_note(false),
    m_save_bioseq_set(false),
    m_flipped_struc_cmt(false),
    m_RemoteTaxonomyLookup(false),
    m_RemotePubLookup(false),
    m_HandleAsSet(false),
    m_ecoset(false),
    m_GenomicProductSet(false),
    m_SetIDFromFile(false),
    m_NucProtSet(false),
    m_taxid(0),
    m_avoid_orf_lookup(false),
    m_gapNmin(0),
    m_gap_Unknown_length(0),
    m_minimal_sequence_length(0),
    m_gap_type(-1),
    m_fcs_trim(false),
    m_avoid_submit_block(false),
    m_split_log_files(false),
    m_optmap_use_locations(false),
    m_postprocess_pubs(false),
    m_handle_as_aa(false),
    m_handle_as_nuc(false),
    m_feature_links(0),
    m_use_hypothetic_protein(false),
    m_eukariote(false),
    m_logger(0)
{
}

CTable2AsnContext::~CTable2AsnContext()
{
}

void CTable2AsnContext::AddUserTrack(CSeq_descr& SD, const string& type, const string& lbl, const string& data)
{
    if (data.empty())
        return;

    CRef<CUser_field> uf(new CUser_field);
    uf->SetLabel().SetStr(lbl);
    uf->SetNum(1);
    uf->SetData().SetStr(data);
    SetUserObject(SD, type).SetData().push_back(uf);
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

    CRef<CSeqdesc> user_desc(new CSeqdesc());
    user_desc->Select(CSeqdesc::e_User);
    user_desc->SetUser(*uo);

    descr.Set().push_back(user_desc);
    return *uo;
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
        if (m_HandleAsSet)
        {
            NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
            {
                need_update |= x_ApplyCreateDate(**it);
            }
        }
        else
        {
            need_update |= x_ApplyCreateDate(entry);
        }
        break;
    default:
        break;
    }
    return need_update;
}


void CTable2AsnContext::ApplyUpdateDate(objects::CSeq_entry& entry) const
{
    CRef<CDate> date(new CDate(CTime(CTime::eCurrent), CDate::ePrecision_day));
    CAutoAddDesc date_desc(entry.SetDescr(), CSeqdesc::e_Update_date);
    date_desc.Set().SetUpdate_date(*date);
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

void CTable2AsnContext::UpdateSubmitObject(CRef<objects::CSeq_submit>& submit) const
{
    if (!m_HoldUntilPublish.IsEmpty())
    {
        submit->SetSub().SetHup(true);
        CRef<CDate> reldate(new CDate(m_HoldUntilPublish, CDate::ePrecision_day));
        submit->SetSub().SetReldate(*reldate);
    }

    string toolname = CNcbiApplication::GetAppName() + " " + CNcbiApplication::Instance()->GetVersion().Print();
    submit->SetSub().SetSubtype(CSubmit_block::eSubtype_new);
    submit->SetSub().SetTool(toolname);
}

CRef<CSerialObject> CTable2AsnContext::CreateSubmitFromTemplate(CRef<CSeq_entry>& object, CRef<CSeq_submit>& submit) const
{
    if (submit.NotEmpty())
    {
        UpdateSubmitObject(submit);
        return CRef<CSerialObject>(submit);
    }
    else
    if (m_submit_template.NotEmpty())
    {
        CRef<CSeq_submit> submit(new CSeq_submit);
        submit->Assign(*m_submit_template);

        submit->SetData().SetEntrys().clear();
        submit->SetData().SetEntrys().push_back(object);

        UpdateSubmitObject(submit);

        return CRef<CSerialObject>(submit);
    }

    return CRef<CSerialObject>(object);
}

CRef<CSerialObject> CTable2AsnContext::CreateSeqEntryFromTemplate(CRef<CSeq_entry> object) const
{
    if (m_submit_template.NotEmpty())
    {
        if (m_submit_template->IsSetSub() &&
            m_submit_template->GetSub().IsSetCit())
        {
            CRef<CPub> pub(new CPub);
            pub->SetSub().Assign(m_submit_template->GetSub().GetCit());

            CRef<CSeqdesc> pub_desc(new CSeqdesc);
            pub_desc->SetPub().SetPub().Set().push_back(pub);
            object->SetDescr().Set().push_back(pub_desc);
        }

        object->Parentize();
    }
    return CRef<CSerialObject>(object);
}

bool CTable2AsnContext::IsDBLink(const CSeqdesc& desc)
{
    if (desc.IsUser() && desc.GetUser().IsSetType() && desc.GetUser().GetType().IsStr() &&
        NStr::CompareNocase(desc.GetUser().GetType().GetStr().c_str(), "DBLink") == 0)
        return true;
    else
        return false;
}

void CTable2AsnContext::x_MergeSeqDescr(objects::CSeq_descr& dest, const objects::CSeq_descr& src, bool only_set) const
{
    ITERATE(CSeq_descr::Tdata, it, src.Get())
    {
        CRef<CSeqdesc> desc;
        switch ((**it).Which())
        {
        case CSeqdesc::e_Molinfo:
        case CSeqdesc::e_Source:
            if (only_set)
               continue;
            break;
        case CSeqdesc::e_User:
            if (IsDBLink(**it))
            {
               if (only_set)
                    continue;
            }
            else
            if (!only_set)
                continue;
            break;
        case CSeqdesc::e_Pub:
            if (!only_set)
                continue;
            break;
        default:
            if (only_set)
                continue;
            break;
        }

        switch ((**it).Which())
        {
        case CSeqdesc::e_User:
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
    if (m_entry_template.IsNull() || !m_entry_template->IsSetDescr())
        return;

    if (entry.IsSet())// && entry.GetSet().IsSetClass())
    {
        x_MergeSeqDescr(entry.SetDescr(), m_entry_template->GetDescr(), true);

        CSeq_entry_Base::TSet::TSeq_set& data = entry.SetSet().SetSeq_set();
        NON_CONST_ITERATE(CSeq_entry_Base::TSet::TSeq_set, it, data)
        {
            MergeWithTemplate(**it);
        }
    }
    else
    if (entry.IsSeq())
    {
        if (entry.GetParentEntry() == 0)
           x_MergeSeqDescr(entry.SetDescr(), m_entry_template->GetDescr(), true);
        x_MergeSeqDescr(entry.SetDescr(), m_entry_template->GetDescr(), false);
    }
}

void CTable2AsnContext::SetSeqId(CSeq_entry& entry) const
{
    string base;
    CDirEntry::SplitPath(m_current_file, 0, &base, 0);
    CRef<CSeq_id> id(new CSeq_id(string("lcl|") + base));

    CBioseq* bioseq = 0;
    if (entry.IsSeq())
    {
        bioseq = &entry.SetSeq();
    }
    else
    if (entry.IsSet())
    {
        bioseq = &entry.SetSet().SetSeq_set().front()->SetSeq();
    }
    _ASSERT(bioseq);
    bioseq->SetId().clear();
    bioseq->SetId().push_back(id);
    // now it's good to rename features ....
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

void CTable2AsnContext::SmartFeatureAnnotation(CSeq_entry& entry) const
{
    CScope scope(*CObjectManager::GetInstance());
    CSeq_entry_Handle h_entry = scope.AddTopLevelSeqEntry(entry);

    size_t numgene = 0;

    std::vector<CSeq_feat*> cds;
    std::vector<CSeq_feat*> rnas;
    for (CFeat_CI feat_it(h_entry); feat_it; ++feat_it)
    {
        if (!feat_it->IsSetData())
            continue;

        switch (feat_it->GetData().Which())
        {
        case CSeqFeatData::e_Gene:
            numgene++;
            break;
        case CSeqFeatData::e_Cdregion:
            cds.push_back((CSeq_feat*) &feat_it->GetOriginalFeature());
            break;
        case CSeqFeatData::e_Rna:
            rnas.push_back((CSeq_feat*) &feat_it->GetOriginalFeature());
            break;
        default:
            break;
        }
    }
    if (numgene == 0)
        return;

}

void CTable2AsnContext::MakeGenomeCenterId(CSeq_entry_EditHandle& entry_h)
{
    CSeq_entry& entry = (CSeq_entry&)*entry_h.GetCompleteSeq_entry();
    CScope& scope = entry_h.GetScope();
    scope.RemoveTopLevelSeqEntry(entry_h);
    VisitAllBioseqs(entry, &CTable2AsnContext::MakeGenomeCenterId);
    entry_h = scope.AddTopLevelSeqEntry(entry).GetEditHandle();   
}

void CTable2AsnContext::VisitAllBioseqs(objects::CSeq_entry& entry, BioseqVisitorMethod m)
{
    if (entry.IsSeq())
    {
        m(*this, entry.SetSeq());
    }
    else
    if (entry.IsSet()) 
    {
        NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it_se, entry.SetSet().SetSeq_set())
        {
            VisitAllBioseqs(**it_se, m);
        }
    }
}


void CTable2AsnContext::MakeGenomeCenterId(CTable2AsnContext& context, CBioseq& bioseq)
{    
    if (context.m_genome_center_id.empty()) return;

    CTempString db = context.m_genome_center_id;

    NON_CONST_ITERATE(CBioseq::TId, id_it, bioseq.SetId())
    {           
        CSeq_id* seq_id(*id_it);
        if (seq_id == 0) continue;

        const CObject_id* obj_id;
        switch (seq_id->Which())
        {
        case CSeq_id::e_Local:
            obj_id = &seq_id->GetLocal();            
            break;
        case CSeq_id::e_General:
            obj_id = &seq_id->GetGeneral().GetTag();
            break;
        default:
            continue;
        }
        if (obj_id->IsId())
            seq_id->SetGeneral().SetTag().SetId(obj_id->GetId());
        else
        {
            string id = obj_id->GetStr();
            seq_id->SetGeneral().SetTag().SetStr(id);
        }

        seq_id->SetGeneral().SetDb(db);                      
    }
}

void CTable2AsnContext::RenameProteinIdsQuals(CTable2AsnContext& context, CSeq_feat& feature)
{
    if (!feature.IsSetQual())
        return;

    CSeq_feat::TQual& quals = feature.SetQual();
    for (CSeq_feat::TQual::iterator it = quals.begin(); it != quals.end();) // no ++ iterator
    {
        if ((**it).GetQual() == "transcript_id")
        {
            (**it).SetQual("orig_transcript_id");
            it++;
        }
        else
        if ((**it).GetQual() == "protein_id")
        {
            if (feature.IsSetData() && feature.GetData().IsCdregion())
               it = quals.erase(it);
            else
            {
               (**it).SetQual("orig_protein_id");
               it++;
            }
        }
        else
        {
            it++;
        }
    }
    if (quals.empty())
        feature.ResetQual();
}

void CTable2AsnContext::RemoveProteinIdsQuals(CTable2AsnContext& context, CSeq_feat& feature)
{
    if (!feature.IsSetQual())
        return;

    CSeq_feat::TQual& quals = feature.SetQual();
    for (CSeq_feat::TQual::iterator it = quals.begin(); it != quals.end();) // no ++ iterator
    {
        if ((**it).GetQual() == "protein_id" ||
            (**it).GetQual() == "transcript_id")
        {
            it = quals.erase(it);
        }
        else
        {
            it++;
        }
    }
    if (quals.empty())
        feature.ResetQual();
}

void CTable2AsnContext::VisitAllFeatures(CSeq_entry_EditHandle& entry_h, FeatureVisitorMethod m)
{
    for (CFeat_CI feat_it(entry_h); feat_it; ++feat_it)
    {
        m(*this, *(CSeq_feat*)feat_it->GetOriginalSeq_feat().GetPointer());
    }
}

void CTable2AsnContext::UpdateOrgFromTaxon(CTable2AsnContext& context, objects::CSeqdesc& seqdesc)
{
    context.m_remote_updater->UpdateOrgFromTaxon(context.m_logger, seqdesc);
}

void CTable2AsnContext::VisitAllSeqDesc(objects::CSeq_entry_EditHandle& entry_h, SeqdescVisitorMethod m)
{
    for (CBioseq_CI bioseq_it(entry_h); bioseq_it; ++bioseq_it)
    {
        CSeqdesc_CI it(bioseq_it->GetEditHandle(), CSeqdesc::e_not_set, 0);
        for (; it; ++it)
        {
            m(*this, (CSeqdesc&)*it);
        }
    }
}

bool CTable2AsnContext::ApplyCreateUpdateDates(objects::CSeq_entry& entry) const
{
    bool need_update = false;
    switch(entry.Which())
    {
    case CSeq_entry::e_Seq:
        need_update |= x_ApplyCreateDate(entry);
        if (need_update)
        {
            if (entry.GetParentEntry() == 0)
                ApplyUpdateDate(entry);
            else
                CAutoAddDesc::EraseDesc(entry.SetDescr(), CSeqdesc::e_Update_date);
        }
        break;
    case CSeq_entry::e_Set:
        {
            if (entry.GetSet().IsSetClass() &&
                entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot)
            {
                ApplyUpdateDate(entry);
            }
            else
            {
                NON_CONST_ITERATE(CSeq_entry::TSet::TSeq_set, it, entry.SetSet().SetSeq_set())
                {
                    need_update |= ApplyCreateUpdateDates(**it);
                }
                if (need_update)
                    ApplyUpdateDate(entry);
            }
        }
        break;
    default:
        break;
    }
    return need_update;
}

void CTable2AsnContext::ApplyFileTracks(objects::CSeq_entry& entry) const
{
  if (!m_ft_url.empty()) 
    AddUserTrack(entry.SetDescr(), "FileTrack", "Map-FileTrackURL", m_ft_url);
  if (!m_ft_url_mod.empty()) 
    AddUserTrack(entry.SetDescr(), "FileTrack", "BaseModification-FileTrackURL", m_ft_url_mod);

}

END_NCBI_SCOPE
