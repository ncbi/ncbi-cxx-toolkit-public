/*
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
* Author:  Alexey Dobronadezhdin
*
* File Description:
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <serial/iterator.hpp>

#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/seqport_util.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_ext.hpp>
#include <objects/seq/Delta_ext.hpp>
#include <objects/seq/Delta_seq.hpp>
#include <objects/seq/Seq_literal.hpp>

#include <objects/submit/Submit_block.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seqblock/EMBL_block.hpp>


#include <util/sequtil/sequtil.hpp>
#include <util/sequtil/sequtil_convert.hpp>

#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Genetic_code.hpp>

#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/biblio/Cit_sub.hpp>

#include <objects/seq/Seq_gap.hpp>

#include <objtools/cleanup/cleanup.hpp>
#include <objtools/edit/gaps_edit.hpp>

#include "wgs_utils.hpp"
#include "wgs_sub.hpp"
#include "wgs_seqentryinfo.hpp"
#include "wgs_params.hpp"
#include "wgs_asn.hpp"
#include "wgs_tax.hpp"
#include "wgs_med.hpp"

namespace wgsparse
{

static void RemoveDatesFromDescrs(CSeq_descr::Tdata& descrs, bool remove_creation, bool remove_update)
{
    for (auto descr = descrs.begin(); descr != descrs.end();) {

        if (((*descr)->IsCreate_date() && remove_creation) ||
            ((*descr)->IsUpdate_date() && remove_update)) {
            descr = descrs.erase(descr);
        }
        else
            ++descr;
    }
}

static void RemoveDates(CSeq_entry& entry, bool remove_creation, bool remove_update)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {
        RemoveDatesFromDescrs(descrs->Set(), remove_creation, remove_update);
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveDates(*cur_entry, remove_creation, remove_update);
        }
    }
}


static void RemovePubs(CSeq_entry& entry, const list<string>& common_pubs, CPubCollection& all_pubs, const CDate_std* date)
{
    if (common_pubs.empty()) {
        return;
    }

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto cur_descr = descrs->Set().begin(); cur_descr != descrs->Set().end();) {

            bool removed = false;
            if ((*cur_descr)->IsPub()) {

                string pubdesc_key;

                /*if (date || GetParams().GetSource() != eNCBI) {
                    pubdesc_key = CPubCollection::GetPubdescKeyForCitSub((*cur_descr)->SetPub(), date);
                }*/

                if (pubdesc_key.empty()) {
                    pubdesc_key = ToStringKey((*cur_descr)->GetPub());
                }

                const CPubInfo& pub_info = all_pubs.GetPubInfo(pubdesc_key);
                if (pub_info.m_desc.NotEmpty()) {
                    cur_descr = descrs->Set().erase(cur_descr);
                    removed = true;
                }
            }

            if (!removed) {
                ++cur_descr;
            }
        }
    }
}

static bool ContainsLocals(const CBioseq::TId& ids)
{
    return find_if(ids.begin(), ids.end(), [](const CRef<CSeq_id>& id) { return id->IsLocal(); }) != ids.end();
}

static bool HasLocals(const CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsSetId()) {
        return ContainsLocals(entry.GetSeq().GetId());
    }
    else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (HasLocals(*cur_entry)) {
                return true;
            }
        }
    }
    return false;
}

static void CollectObjectIds(const CSeq_entry& entry, map<string, string>& ids)
{
    if (entry.IsSeq()) {

        bool nuc = entry.GetSeq().IsNa();

        if (entry.GetSeq().IsSetId()) {
            for (auto& id : entry.GetSeq().GetId()) {
                if (IsLocalOrGeneralId(*id)) {
                    string id_str = GetLocalOrGeneralIdStr(*id),
                           dbname;

                    if (nuc && !GetParams().IsChromosomal()) {
                        dbname = GetParams().GetProjAccVerStr();
                    }
                    else {
                        dbname = GetParams().GetProjAccStr();
                    }

                    ids[id_str] = dbname;
                }
            }
        }
    }
    else if (entry.IsSet()) {

        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                CollectObjectIds(*cur_entry, ids);
            }
        }
    }
}

static void FixDbNameInObject(CSerialObject& obj, const map<string, string>& ids)
{
    for (CTypeIterator<CSeq_id> id(obj); id; ++id) {
        if (id->IsGeneral()) {

            CDbtag& dbtag = id->SetGeneral();
            if (dbtag.IsSetDb() && dbtag.IsSetTag()) {

                string cur_id = GetIdStr(dbtag.GetTag());
                auto dbname = ids.find(cur_id);

                if (dbname != ids.end()) {
                    dbtag.SetDb(dbname->second);
                }
            }
        }
        else if (id->IsLocal()) {

            string cur_id = GetIdStr(id->GetLocal());
            auto dbname = ids.find(cur_id);

            if (dbname != ids.end()) {
                CDbtag& dbtag = id->SetGeneral();
                dbtag.SetDb(dbname->second);
                dbtag.SetTag().SetStr(cur_id);
            }
        }
    }
}

static void FixDbName(CSeq_entry& entry, map<string, string>& ids)
{
    CollectObjectIds(entry, ids);

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs) {
        FixDbNameInObject(*descrs, ids);
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        for (auto& annot : *annots) {
            FixDbNameInObject(*annot, ids);
        }
    }

    if (entry.IsSeq()) {

        if (entry.GetSeq().IsSetInst()) {
            FixDbNameInObject(entry.SetSeq().SetInst(), ids);
        }

        if (entry.GetSeq().IsSetId()) {
            for (auto& id : entry.SetSeq().SetId()) {
                FixDbNameInObject(*id, ids);
            }
        }
    }
    
    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {
        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            FixDbName(*cur_entry, ids);
        }
    }
}

static void FixOrigProtTransValue(string& val)
{
    size_t pos = val.rfind('|');
    string no_prefix_val = pos == string::npos ? val : val.substr(pos + 1);

    val = "gnl|" + GetParams().GetProjPrefix() + GetParams().GetIdPrefix() + '|' + no_prefix_val;
}

static void FixOrigProtTransQuals(CSeq_annot::C_Data::TFtable& ftable)
{
    for (auto& feat : ftable) {

        if (feat->IsSetQual()) {

            for (auto& qual : feat->SetQual()) {
                if (qual->IsSetQual() &&
                    (qual->GetQual() == "orig_protein_id" || qual->GetQual() == "orig_transcript_id")) {
                    FixOrigProtTransValue(qual->SetVal());
                }
            }
        }
    }
}

static void FixOrigProtTransIds(CSeq_entry& entry)
{
    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {
        for (auto& annot : *annots) {
            if (annot->IsFtable()) {
                FixOrigProtTransQuals(annot->SetData().SetFtable());
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            FixOrigProtTransIds(*cur_entry);
        }
    }
}

static void CollectAccGenid(const CBioseq::TId& ids, const string& file, list<CIdInfo>& id_infos)
{
    CIdInfo cur_info;
    for (auto& id : ids) {
        if (NeedToProcessId(*id)) {
            const CTextseq_id* text_id = id->GetTextseq_Id();
            if (text_id && text_id->IsSetAccession()) {
                cur_info.m_accession = text_id->GetAccession();
            }
        }
        else if (id->IsGeneral()) {

            if (id->GetGeneral().IsSetTag()) {
                const string& dbtag = GetIdStr(id->GetGeneral().GetTag());
                if (!dbtag.empty()) {
                    cur_info.m_dbtag = dbtag;
                }
            }
        }
    }

    if (!cur_info.m_accession.empty() && !cur_info.m_dbtag.empty()) {
        cur_info.m_file = file;
        id_infos.emplace_front(move(cur_info));
    }
}

static CRef<CSeq_id> CreateNewAccession(int num)
{
    CRef<CSeq_id> ret;

    CRef<CTextseq_id> text_id(new CTextseq_id);
    int ver = GetParams().GetAssemblyVersion();
    text_id->SetAccession(GetParams().GetIdPrefix() + ToStringLeadZeroes(ver, 2) + ToStringLeadZeroes(num, GetMaxAccessionLen(num)));

    auto set_fun = FindSetTextSeqIdFunc(GetParams().GetIdChoice());
    _ASSERT(set_fun != nullptr && "There should be a valid SetTextId function. Validate the ID choice.");

    if (set_fun == nullptr) {
        return ret;
    }

    ret.Reset(new CSeq_id);
    (ret->*set_fun)(*text_id);

    return ret;
}

static void RemovePreviousAccession(const string& new_acc, CBioseq::TId& ids)
{
    for (auto id = ids.begin(); id != ids.end(); ++id) {
        if (HasTextAccession(**id)) {

            const CTextseq_id* text_id = (*id)->GetTextseq_Id();
            _ASSERT(text_id != nullptr);

            if (text_id->GetAccession() != new_acc) {
                ERR_POST_EX(0, 0, Info << "Input Seq-entry already has accession \"" << text_id->GetAccession() << "\". Replaced with \"" << new_acc << "\".");
            }

            ids.erase(id);
            break;
        }
    }
}

static void AssignNucAccession(CSeq_entry& entry, const string& file, list<CIdInfo>& id_infos, map<string, int>& order_of_entries)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        if (GetParams().IsAccessionAssigned()) {

            if (!GetParams().IsTest() && entry.GetSeq().IsSetId()) {
                CollectAccGenid(entry.GetSeq().GetId(), file, id_infos);
            }
            return;
        }

        string entry_key = GetSeqIdKey(entry.GetSeq());
        int next_id = order_of_entries[entry_key];

        CRef<CSeq_id> new_id = CreateNewAccession(next_id);
        if (new_id.NotEmpty()) {

            const string& new_acc = new_id->GetTextseq_Id()->GetAccession();
            ERR_POST_EX(0, 0, Info << "Assigned nucleotide accession \"" << new_acc << "\".");

            RemovePreviousAccession(new_acc, entry.SetSeq().SetId());
            entry.SetSeq().SetId().push_front(new_id);
        }

        if (!GetParams().IsTest() && entry.GetSeq().IsSetId()) {
            CollectAccGenid(entry.GetSeq().GetId(), file, id_infos);
        }

        if (GetParams().GetUpdateMode() != eUpdateScaffoldsNew || GetParams().IsScaffoldTestMode()) {
            return;
        }

        // TODO HTGS stuff
    }
    
    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
                AssignNucAccession(*cur_entry, file, id_infos, order_of_entries);
            }
        }
    }
}

static void FixTechMolInfo(CMolInfo& mol_info, CSeq_inst::EMol& mol)
{
    if (GetParams().IsTsa()) {
        if (GetParams().GetFixTech() & eFixMolBiomol) {

            mol_info.SetTech(CMolInfo::eTech_tsa);
            mol_info.SetBiomol(CMolInfo::eBiomol_transcribed_RNA);

            mol = CSeq_inst::eMol_rna;
        }

        if (GetParams().GetFixTech() & eFixBiomol_mRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_mRNA);
        }
        else if (GetParams().GetFixTech() & eFixBiomol_rRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_rRNA);
        }
        else if (GetParams().GetFixTech() & eFixBiomol_ncRNA) {
            mol_info.SetBiomol(CMolInfo::eBiomol_ncRNA);
        }
    }
    else {
        if (GetParams().GetFixTech() & eFixMolBiomol) {

            mol_info.SetTech(GetParams().IsTls() ? CMolInfo::eTech_targeted : CMolInfo::eTech_wgs);
            mol_info.SetBiomol(CMolInfo::eBiomol_genomic);

            mol = CSeq_inst::eMol_dna;
        }

        if (GetParams().IsWgs()) {

            if (GetParams().GetFixTech() & eFixBiomol_cRNA) {
                mol_info.SetBiomol(CMolInfo::eBiomol_cRNA);
            }

            if (GetParams().GetFixTech() & eFixInstMolRNA) {
                mol = CSeq_inst::eMol_rna;
            }
        }
    }
}

static void FixTech(CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {
        
        CRef<CSeqdesc> mol_info_descr;
        CSeq_descr& descrs = entry.SetSeq().SetDescr();
        
        auto mol_info_it = find_if(descrs.Set().begin(), descrs.Set().end(), [](CRef<CSeqdesc>& descr) { return descr->IsMolinfo(); });
        if (mol_info_it != descrs.Get().end()) {
            mol_info_descr = *mol_info_it;
        }
        else {
            mol_info_descr.Reset(new CSeqdesc);
            descrs.Set().push_back(mol_info_descr);
        }

        FixTechMolInfo(mol_info_descr->SetMolinfo(), entry.SetSeq().SetInst().SetMol());
    }
    
    if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {
            for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
                FixTech(*cur_entry);
            }
        }
    }
}

static bool FixBioSources(CSeq_entry& entry, const CMasterInfo& master_info)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        auto source_it = find_if(descrs->Set().begin(), descrs->Set().end(), [](CRef<CSeqdesc>& descr) { return descr->IsSource(); });

        if (source_it != descrs->Set().end() && !PerformTaxLookup((*source_it)->SetSource(), master_info.m_org_refs, GetParams().IsTaxonomyLookup())) {
            return false;
        }
    }

    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        for (auto feat_table_it : *annots) {

            if (feat_table_it->IsFtable()) {
                auto& feat_table = feat_table_it->SetData().SetFtable();
                auto feat_source_it = find_if(feat_table.begin(), feat_table.end(), [](CRef<CSeq_feat>& feat) { return feat->IsSetData() && feat->GetData().IsBiosrc(); });

                if (feat_source_it != feat_table.end() && PerformTaxLookup((*feat_source_it)->SetData().SetBiosrc(), master_info.m_org_refs, GetParams().IsTaxonomyLookup())) {
                    return false;
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {

            if (!FixBioSources(*cur_entry, master_info)) {
                return false;
            }
        }
    }

    return true;
}

struct TTaxNameInfo
{
    const string* m_taxname;
    const string* m_old_taxname;

    list<string> m_others;

    TTaxNameInfo() : m_taxname(nullptr), m_old_taxname(nullptr) {}
};

static void GetTaxNameInfoFromSource(const CBioSource& bio_src, TTaxNameInfo& info)
{
    const COrg_ref& org = bio_src.GetOrg();
    info.m_taxname = &org.GetTaxname();

    if (org.IsSetCommon()) {
        info.m_others.push_back(org.GetCommon());
    }

    if (org.IsSetOrgname() && org.GetOrgname().IsSetMod()) {
        for (auto& mod : org.GetOrgname().GetMod()) {
            if (mod->IsSetSubname() && mod->IsSetSubtype()) {

                switch (mod->GetSubtype()) {
                    case COrgMod::eSubtype_old_name:
                        info.m_old_taxname = &mod->GetSubname();
                        break;
                    case COrgMod::eSubtype_acronym:
                    case COrgMod::eSubtype_synonym:
                    case COrgMod::eSubtype_anamorph:
                    case COrgMod::eSubtype_teleomorph:
                    case COrgMod::eSubtype_gb_acronym:
                    case COrgMod::eSubtype_gb_anamorph:
                    case COrgMod::eSubtype_gb_synonym:
                        info.m_others.push_back(mod->GetSubname());
                        break;
                }
            }
        }
    }

    if (bio_src.IsSetSubtype()) {
        for (auto& subtype : bio_src.GetSubtype()) {
            if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_other && subtype->IsSetName()) {

                static const char COMMON_PREFIX[] = "common:";
                const string name = subtype->GetName();
                if (NStr::StartsWith(name, COMMON_PREFIX, NStr::eNocase)) {

                    size_t pos = sizeof(COMMON_PREFIX) - 1,
                           len = name.size();
                    while (pos < len && name[pos] == ' ') {
                        ++pos;
                    }
                    if (pos < len) {
                        info.m_others.push_back(name.substr(pos));
                    }
                }
            }
        }
    }
}

static void GetTaxNameInfo(const CSeq_entry& entry, TTaxNameInfo& info)
{
    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs && descrs->IsSet()) {

        auto source_it = find_if(descrs->Get().begin(), descrs->Get().end(),
                                 [](const CRef<CSeqdesc>& descr) { return descr->IsSource() && descr->GetSource().IsSetOrg() && descr->GetSource().GetOrg().IsSetTaxname(); });

        if (source_it != descrs->Get().end()) {
            GetTaxNameInfoFromSource((*source_it)->GetSource(), info);
        }
    }

    if (info.m_taxname == nullptr) {
        if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

            for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
                GetTaxNameInfo(*cur_entry, info);
                if (info.m_taxname) {
                    break;
                }
            }
        }
    }
}

static void RemoveOrganelleName(string& name)
{
    static const string valid_organelle[] = {
        "apicoplast",
        "chloroplast",
        "chromatophore",
        "chromoplast",
        "cyanelle",
        "hydrogenosome",
        "kinetoplast",
        "leucoplast",
        "mitochondrion",
        "nucleomorph",
        "plastid",
        "proplastid"
    };

    for (auto& cur_org_name : valid_organelle) {

        if (NStr::StartsWith(name, cur_org_name, NStr::eNocase)) {

            name = NStr::Sanitize(name.substr(cur_org_name.size()));
            break;
        }
    }
}

static void RemoveOtherTaxnamesInBrackets(const list<string>& taxnames, string& source)
{
    static const string prefix_to_be_removed[] = {
        "acronym:",
        "synonym:",
        "anamorph:",
        "teleomorph:"
    };

    for (auto& taxname : taxnames) {

        for (size_t open_bracket = source.find('('); open_bracket != string::npos; open_bracket = source.find('(', open_bracket + 1)) {

            size_t close_bracket = source.find(')', open_bracket);
            if (close_bracket == string::npos)
                break;

            CTempString in_brackets(source.c_str() + open_bracket + 1, close_bracket - open_bracket - 1);
            for (auto& prefix : prefix_to_be_removed) {
                if (NStr::StartsWith(in_brackets, prefix)) {

                    size_t start = open_bracket + 1 + prefix.size();
                    string cur_taxname = NStr::Sanitize(source.substr(start, close_bracket - start));
                    if (taxname == cur_taxname) {
                        source = NStr::Sanitize(source.substr(0, open_bracket - 1) + source.substr(close_bracket + 1));
                        open_bracket = 0;
                    }
                    break;
                }
            }
        }
    }
}

static void RemoveGbblockSource(CSeq_entry& entry, const TTaxNameInfo& info)
{
    if (entry.IsSeq()) {

        if (entry.GetSeq().IsSetDescr() && entry.GetSeq().GetDescr().IsSet()) {
            for (auto& descr : entry.SetSeq().SetDescr().Set()) {

                if (descr->IsGenbank()) {
                    CGB_block& gb_block = descr->SetGenbank();
                    if (!gb_block.IsSetSource()) {
                        break;
                    }

                    string source = gb_block.GetSource();
                    bool ends_with_dot = source.back() == '.';
                    if (ends_with_dot) {
                        source.pop_back();
                    }

                    RemoveOrganelleName(source);
                    RemoveOtherTaxnamesInBrackets(info.m_others, source);

                    string source_with_dot;
                    if (ends_with_dot) {
                        source_with_dot == source + ".";
                    }

                    if ((info.m_taxname && (source == *info.m_taxname || source_with_dot == *info.m_taxname)) ||
                        (info.m_old_taxname && (source == *info.m_old_taxname || source_with_dot == *info.m_old_taxname))) {
                        gb_block.ResetSource();
                    }

                    break;
                }
            }
        }
    }
    else if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveGbblockSource(*cur_entry, info);
        }
    }
}

void FixGbblockSource(CSeq_entry& entry)
{
    TTaxNameInfo info;
    GetTaxNameInfo(entry, info);

    RemoveGbblockSource(entry, info);
}

static void PackSeqData(CSeq_data::E_Choice code, CSeq_data& seq_data)
{
    const string* seq_str = nullptr;
    const vector<char>* seq_vec = nullptr;

    CSeqUtil::ECoding old_coding = CSeqUtil::e_not_set;
    size_t old_size = 0;

    static const map<CSeq_data::E_Choice, CSeqUtil::ECoding> CHOICE_TO_ENCODING =
    {
        { CSeq_data::e_Iupacaa, CSeqUtil::e_Iupacaa },
        { CSeq_data::e_Ncbi8aa, CSeqUtil::e_Ncbi8aa },
        { CSeq_data::e_Ncbistdaa, CSeqUtil::e_Ncbistdaa }
    };

    switch (code) {
        case CSeq_data::e_Iupacaa:
        case CSeq_data::e_Ncbi8aa:
        case CSeq_data::e_Ncbistdaa:
            seq_str = &seq_data.GetIupacaa().Get();
            old_coding = CHOICE_TO_ENCODING.at(code);
            old_size = seq_str->size();
            break;

        default:; // do nothing
    }

    vector<char> new_seq(old_size);
    size_t new_size = 0;
    if (seq_str != nullptr)
        new_size = CSeqConvert::Convert(seq_str->c_str(), old_coding, 0, static_cast<TSeqPos>(old_size), &new_seq[0], CSeqUtil::e_Ncbieaa);
    else if (seq_vec != nullptr)
        new_size = CSeqConvert::Convert(&(*seq_vec)[0], old_coding, 0, static_cast<TSeqPos>(old_size), &new_seq[0], CSeqUtil::e_Ncbieaa);

    if (!new_seq.empty()) {
        seq_data.SetNcbieaa().Set().assign(new_seq.begin(), new_seq.begin() + new_size);
    }
}

static void RawBioseqPack(CBioseq& bioseq)
{
    if (bioseq.GetInst().IsSetSeq_data()) {
        if (!bioseq.GetInst().IsSetMol() || !bioseq.GetInst().IsNa()) {
            CSeq_data::E_Choice code = bioseq.GetInst().GetSeq_data().Which();
            PackSeqData(code, bioseq.SetInst().SetSeq_data());
        }
        else if (!bioseq.GetInst().GetSeq_data().IsGap()) {
            CSeqportUtil::Pack(&bioseq.SetInst().SetSeq_data());
        }
    }
}

static void DeltaBioseqPack(CBioseq& bioseq)
{
    if (bioseq.GetInst().IsSetExt() && bioseq.GetInst().GetExt().IsDelta()) {
        for (auto& delta: bioseq.SetInst().SetExt().SetDelta().Set())
        {
            if (delta->IsLiteral() && delta->GetLiteral().IsSetSeq_data() && !delta->GetLiteral().GetSeq_data().IsGap()) {
                CSeqportUtil::Pack(&delta->SetLiteral().SetSeq_data());
            }
        }
    }
}

static void PackEntry(CSeq_entry& entry)
{
    for (CTypeIterator<CBioseq> bioseq(Begin(entry)); bioseq; ++bioseq) {
        if (bioseq->IsSetInst() && bioseq->GetInst().IsSetRepr()) {
            CSeq_inst::ERepr repr = bioseq->GetInst().GetRepr();
            if (repr == CSeq_inst::eRepr_raw || repr == CSeq_inst::eRepr_const)
                RawBioseqPack(*bioseq);
            else if (repr == CSeq_inst::eRepr_delta)
                DeltaBioseqPack(*bioseq);
        }
    }
}

static void RemoveBioSources(CSeq_entry& entry)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto descr = descrs->Set().begin(); descr != descrs->Set().end();) {

            if ((*descr)->IsSource()) {
                descr = descrs->Set().erase(descr);
            }
            else {
                ++descr;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveBioSources(*cur_entry);
        }
    }
}

static void PerformCleanup(CSeq_submit::C_Data::TEntrys& entries, bool remove_bio_sources)
{
    CCleanup cleanup;
    for (auto& entry : entries) {
        cleanup.ExtendedCleanup(*entry);

        if (remove_bio_sources) {
            RemoveBioSources(*entry);
        }
    }
}

static string MakeOutputFileName(const string& in_file)
{
    string ret = GetParams().GetOutputDir();

    size_t start = string::npos;
    if (!GetParams().IsPreserveInputPath()) {

        start = GetLastSlashPos(in_file);
        if (start != string::npos) {
            ++start;
        }
    }

    if (start == string::npos) {
        start = 0;
        if (in_file.front() == '/' || in_file.front() == '\\') {
            start = 1;
        }
    }

    ret += '/' + in_file.substr(start);
    return ret;
}

static bool OutputSubmission(const CBioseq_set& bioseq_set, const string& in_file)
{
    const string& fname = MakeOutputFileName(in_file) + ".bss";
    string dir_name;
    
    size_t delimiter = GetLastSlashPos(fname);
    if (delimiter != string::npos) {
        dir_name = fname.substr(0, delimiter);
    }

    if (!dir_name.empty()) {
        CDir dir(CDir::CreateAbsolutePath(dir_name));
        dir.CreatePath();
    }

    if (!GetParams().IsOverrideExisting() && CFile(fname).Exists()) {
        ERR_POST_EX(0, 0, Error << "File to print out processed submission already exists: \"" << fname << "\". Override is not allowed.");
        return false;
    }

    try {
        CNcbiOfstream out(fname);

        if (GetParams().IsBinaryOutput())
            out << MSerial_AsnBinary << bioseq_set;
        else
            out << MSerial_AsnText << bioseq_set;
    }
    catch (CException& e) {
        ERR_POST_EX(0, 0, Critical << "Failed to save processed submission to file: \"" << fname << "\" [" << e.GetMsg() << "]. Cannot proceed.");
        return false;
    }

    ERR_POST_EX(0, 0, Info << "Processed submission saved in file \"" << fname << "\".");
    return true;
}

static void RemoveKeywords(CSeq_entry& entry, const set<string>& keywords)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        for (auto descr = descrs->Set().begin(); descr != descrs->Set().end();) {

            bool removed = false;
            if ((*descr)->IsGenbank() && (*descr)->GetGenbank().IsSetKeywords()) {

                CGB_block& gb_block = (*descr)->SetGenbank();
                for (auto keyword = gb_block.SetKeywords().begin(); keyword != gb_block.SetKeywords().end();) {
                    if (keywords.find(*keyword) != keywords.end()) {
                        keyword = gb_block.SetKeywords().erase(keyword);
                    }
                    else {
                        ++keyword;
                    }
                }

                if (gb_block.IsEmpty()) {
                    removed = true;
                    descr = descrs->Set().erase(descr);
                }
            }

            if (!removed) {
                ++descr;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveKeywords(*cur_entry, keywords);
        }
    }
}


typedef string (*GetCommentFunc) (const CSeqdesc& );

static string GetCommentString(const CSeqdesc& descr)
{
    return descr.IsComment() ? descr.GetComment() : kEmptyStr;
}

static string GetStructuredCommentString(const CSeqdesc& descr)
{
    if (IsUserObjectOfType(descr, "StructuredComment")) {
        const CUser_object& user_obj = descr.GetUser();
        return ToString(user_obj);
    }

    return kEmptyStr;
}

static void RemoveComments(CSeq_entry& entry, const list<string>& comments, GetCommentFunc getComment)
{
    if (comments.empty()) {
        return;
    }

    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        CDataChecker checker(false, comments);
        for (auto descr = descrs->Set().begin(); descr != descrs->Set().end();) {

            bool removed = false;
            const string& comment = getComment(**descr);
            if (!comment.empty()) {

                if (checker.IsStringPresent(comment)) {
                    descr = descrs->Set().erase(descr);
                    removed = true;
                }
            }

            if (!removed) {
                ++descr;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveComments(*cur_entry, comments, getComment);
        }
    }
}

static void PropagateTPAKeyword(CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsNa()) {

        CSeq_descr& descrs = entry.SetSeq().SetDescr();
        auto gb_block_descr = find_if(descrs.Set().begin(), descrs.Set().end(), [](CRef<CSeqdesc>& descr) { return descr->IsGenbank(); });

        if (gb_block_descr == descrs.Set().end()) {
            CRef<CSeqdesc> descr(new CSeqdesc);
            descr->SetGenbank();
            descrs.Set().push_back(descr);
            --gb_block_descr;
        }

        CGB_block& gb_block = (*gb_block_descr)->SetGenbank();
        for (auto keyword = gb_block.SetKeywords().begin(); keyword != gb_block.SetKeywords().end();) {
            if (*keyword == "TPA" || NStr::StartsWith(*keyword, "TPA:")) {
                keyword = gb_block.SetKeywords().erase(keyword);
            }
            else {
                ++keyword;
            }
        }

        gb_block.SetKeywords().push_back(GetParams().GetTpaKeyword());
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            PropagateTPAKeyword(*cur_entry);
        }
    }
}

static void CleanupProtGbblock(CSeq_entry& entry)
{
    if (entry.IsSeq() && entry.GetSeq().IsAa() && entry.GetSeq().IsSetDescr()) {

        CSeq_descr& descrs = entry.SetSeq().SetDescr();

        for (auto descr = descrs.Set().begin(); descr != descrs.Set().end();) {

            bool removed = false;
            if ((*descr)->IsGenbank()) {

                CGB_block& gb_block = (*descr)->SetGenbank();
                if (gb_block.IsSetExtra_accessions()) {

                    // remove everything but extra accessions
                    list<string> extra_accs;
                    extra_accs.swap(gb_block.SetExtra_accessions());
                    gb_block.Reset();
                    gb_block.SetExtra_accessions().swap(extra_accs);
                }
                else {
                    removed = true;
                    descr = descrs.Set().erase(descr);
                }
            }

            if (!removed) {
                ++descr;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            CleanupProtGbblock(*cur_entry);
        }
    }
}

static void RemoveDblinkGPID(CSeq_entry& entry, size_t& dblink_order_num)
{
    CSeq_descr* descrs = nullptr;
    if (GetNonConstDescr(entry, descrs) && descrs && descrs->IsSet()) {

        size_t order = 0;
        for (auto descr = descrs->Set().begin(); descr != descrs->Set().end();) {

            ++order;
            if (IsUserObjectOfType(**descr, "GenomeProjectsDB") || IsUserObjectOfType(**descr, "DBLink")) {
                descr = descrs->Set().erase(descr);
                if (dblink_order_num == 0) {
                    dblink_order_num = order;
                }
            }
            else {
                ++descr;
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RemoveDblinkGPID(*cur_entry, dblink_order_num);
        }
    }
}

static string GetAssignedAccessionFromIds(const CBioseq::TId& ids)
{
    for (auto& id : ids) {
        if (id->Which() == GetParams().GetIdChoice() && id->GetTextseq_Id() && id->GetTextseq_Id()->IsSetAccession()) {
            return id->GetTextseq_Id()->GetAccession();
        }
    }

    return "";
}

static string GetAssignedAccession(const CSeq_entry& entry)
{
    string ret;

    if (entry.IsSet()) {

        if (entry.GetSet().IsSetSeq_set()) {
            auto& seq_set = entry.GetSet().GetSeq_set();
            auto nuc_seq = find_if(seq_set.begin(), seq_set.end(), [](const CRef<CSeq_entry>& cur_entry) { return cur_entry->IsSeq() && cur_entry->GetSeq().IsSetId(); });

            if (nuc_seq != seq_set.end()) {
                ret = GetAssignedAccessionFromIds((*nuc_seq)->GetSeq().GetId());
            }
        }
    }
    else if (entry.IsSeq()) {
        if (entry.GetSeq().IsNa() && entry.GetSeq().IsSetId()) {
            ret = GetAssignedAccessionFromIds(entry.GetSeq().GetId());
        }
    }

    return ret;
}

static void SeqToDelta(CSeq_entry& entry, TSeqPos gap_size)
{
    if (gap_size) {
        CGapsEditor gaps_editor(CSeq_gap::eType_unknown, CGapsEditor::TEvidenceSet(), gap_size, 0);
        gaps_editor.ConvertNs2Gaps(entry);
    }
}

static void FixCDRegionGeneticCode(CCdregion& cdregion)
{
    static const CGenetic_code::C_E::TId DEFAULT_GENCODE = 1;

    CGenetic_code::Tdata& genetic_codes = cdregion.SetCode().Set();

    bool found = false;
    for (auto gencode : genetic_codes) {
        if (gencode->IsId()) {

            gencode->SetId(DEFAULT_GENCODE);
            found = true;
            break;
        }
    }

    if (!found) {
        CRef<CGenetic_code::C_E> gencode_id(new CGenetic_code::C_E);
        gencode_id->SetId(DEFAULT_GENCODE);
        genetic_codes.push_back(gencode_id);
    }
}

static void FixGeneticCode(CSeq_entry& entry)
{
    CBioseq::TAnnot* annots = nullptr;
    if (GetNonConstAnnot(entry, annots) && annots) {

        for (auto feat_table_it: *annots) {

            if (feat_table_it->IsFtable()) {
                auto& feat_table = feat_table_it->SetData().SetFtable();
                for (auto& feat : feat_table) {
                    if (feat->IsSetData() && feat->GetData().IsCdregion()) {
                        FixCDRegionGeneticCode(feat->SetData().SetCdregion());
                    }
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            FixGeneticCode(*cur_entry);
        }
    }
}

static bool NeedToAddExtraAccessions(const CSeq_entry& entry)
{
    return entry.IsSeq() && entry.GetSeq().IsNa() && entry.GetSeq().IsSetInst() && entry.GetSeq().GetInst().IsSetRepr() && entry.GetSeq().GetInst().GetRepr() == CSeq_inst::eRepr_delta;
}

class CBlockAdaptor
{
public:
    virtual bool ToBeProcessed(const CRef<CSeqdesc>&) const = 0;

    virtual bool IsSetExtraAccessions(const CRef<CSeqdesc>& descr) const = 0;
    virtual list<string>& SetExtraAccessions(CRef<CSeqdesc>& descr) const = 0;
};

class CGBBlockAdaptor : public CBlockAdaptor
{
public:

    CGBBlockAdaptor() {}

    virtual bool ToBeProcessed(const CRef<CSeqdesc>& descr) const
    {
        return descr.NotEmpty() && descr->IsGenbank();
    }

    virtual bool IsSetExtraAccessions(const CRef<CSeqdesc>& descr) const
    {
        return descr.NotEmpty() && descr->IsGenbank() && descr->GetGenbank().IsSetExtra_accessions();
    }

    virtual list<string>& SetExtraAccessions(CRef<CSeqdesc>& descr) const
    {
        _ASSERT(descr.NotEmpty() && descr->IsGenbank() && "Should be a valid GB-block");
        return descr->SetGenbank().SetExtra_accessions();
    }
};

class CEMBLBlockAdaptor : public CBlockAdaptor
{
public:

    CEMBLBlockAdaptor() {}

    virtual bool ToBeProcessed(const CRef<CSeqdesc>& descr) const
    {
        return descr.NotEmpty() && descr->IsEmbl();
    }

    virtual bool IsSetExtraAccessions(const CRef<CSeqdesc>& descr) const
    {
        return descr.NotEmpty() && descr->IsEmbl() && descr->GetEmbl().IsSetExtra_acc();
    }

    virtual list<string>& SetExtraAccessions(CRef<CSeqdesc>& descr) const
    {
        _ASSERT(descr.NotEmpty() && descr->IsEmbl() && "Should be a valid EMBL-block");
        return descr->SetEmbl().SetExtra_acc();
    }
};


static void AddMasterToSecondary(CSeq_entry& entry, const CBlockAdaptor& block)
{
    if (NeedToAddExtraAccessions(entry)) {

        CRef<CSeqdesc> block_descr;

        if (entry.IsSetDescr() && entry.GetDescr().IsSet()) {
            auto descr_it = find_if(entry.SetDescr().Set().begin(), entry.SetDescr().Set().end(), [&block](const CRef<CSeqdesc>& descr) { return block.ToBeProcessed(descr); });
            if (descr_it != entry.SetDescr().Set().end()) {
                block_descr = *descr_it;
            }
        }

        if (!block_descr.Empty()) {
            block_descr.Reset(new CSeqdesc);
            entry.SetDescr().Set().push_back(block_descr);
        }

        const string& accession = GetParams().GetAccession();
        if (block.IsSetExtraAccessions(block_descr)) {
            auto same = find_if(block.SetExtraAccessions(block_descr).begin(), block.SetExtraAccessions(block_descr).end(),
                                [&accession](const string& cur_accession) { return accession == cur_accession; });
            if (same != block.SetExtraAccessions(block_descr).end()) {
                return;
            }
        }

        block.SetExtraAccessions(block_descr).push_back(accession);
    }
    
    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            AddMasterToSecondary(*cur_entry, block);
        }
    }
}

static bool NeedToAddDbLink(const CSeq_entry& entry)
{
    return entry.IsSeq() || (entry.IsSet() && entry.GetSet().IsSetClass() && entry.GetSet().GetClass() == CBioseq_set::eClass_nuc_prot);
}

static bool AddDblink(CSeq_entry& entry, const CUser_object& dblink, size_t dblink_order_num)
{
    if (NeedToAddDbLink(entry)) {

        CRef<CSeqdesc> descr(new CSeqdesc);
        descr->SetUser().Assign(dblink);

        auto insertion_point = entry.SetDescr().Set().end();
        if (dblink_order_num) {

            insertion_point = entry.SetDescr().Set().begin();
            while (--dblink_order_num) {
                ++insertion_point;
            }
        }

        entry.SetDescr().Set().insert(insertion_point, descr);
        return true;
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            if (AddDblink(*cur_entry, dblink, dblink_order_num)) {
                return true;
            }
        }
    }

    return false;
}

static bool NeedToProcessTitles(const CSeq_entry& entry)
{
    return entry.IsSet() ||
        (entry.IsSeq() && entry.GetSeq().IsNa() && (!entry.GetSeq().IsSetInst() || !entry.GetSeq().GetInst().IsSetRepr() || entry.GetSeq().GetInst().GetRepr() != CSeq_inst::eRepr_seg));
}

static void GetSourceInfo(const CBioSource& source, string& organism, string& chromosome, bool get_organism, bool get_chromosome)
{
    if (get_organism && source.IsSetOrg() && source.GetOrg().IsSetTaxname()) {
        organism = source.GetOrg().GetTaxname();
    }

    if (get_chromosome && source.IsSetSubtype()) {

        for (auto& subtype : source.GetSubtype()) {
            if (subtype->IsSetSubtype() && subtype->GetSubtype() == CSubSource::eSubtype_chromosome && subtype->IsSetName()) {
                chromosome = subtype->GetName();
                break;
            }
        }
    }
}


struct CTitleInfo
{
    string m_chromosome,
           m_organism;
};

static void ReplaceInTitle(string& title, const string& what, string& change)
{
    if (!change.empty()) {
        change += ' ';
    }

    NStr::ReplaceInPlace(title, what, change);

    if (change.empty()) {
        ERR_POST_EX(0, 0, Error << "Could not get " << what << "to include to the title.");
    }
}

static void ReplaceSeqId(const CBioseq& bioseq, string& title)
{
    if (bioseq.IsSetId()) {
        
        auto& ids = bioseq.GetId();
        auto general_id = find_if(ids.begin(), ids.end(), [](const CRef<CSeq_id>& id) { return id->IsGeneral(); });
        if (general_id != ids.end()) {
            string seq_id = GetLocalOrGeneralIdStr(**general_id);
            ReplaceInTitle(title, "[Seq-id] ", seq_id);
        }
    }
}

static void RepTitles(CSeq_entry& entry, CTitleInfo& title_info)
{
    if (NeedToProcessTitles(entry)) {

        const string& title = GetParams().GetNewNucTitle();
        bool has_organism = title.find("[Organism]") != string::npos,
             has_chromosome = title.find("[Chromosome]") != string::npos;

        if (entry.IsSetDescr() && entry.GetDescr().IsSet()) {
            auto& descrs = entry.SetDescr().Set();
            for (auto descr = descrs.begin(); descr != descrs.end();) {
                if ((*descr)->IsTitle()) {
                    descr = descrs.erase(descr);
                }
                else {
                    if ((*descr)->IsSource() && entry.IsSet()) {
                        GetSourceInfo((*descr)->GetSource(), title_info.m_organism, title_info.m_chromosome, has_organism, has_chromosome);
                    }
                    ++descr;
                }
            }
        }

        if (entry.IsSeq() && !title.empty()) {

            CRef<CSeqdesc> title_descr(new CSeqdesc);
            title_descr->SetTitle(title);

            string& new_title = title_descr->SetTitle();
            if (title.find("[Seq-id]") != string::npos) {
                ReplaceSeqId(entry.GetSeq(), new_title);
            }

            if (!has_organism && !has_chromosome) {
                return;
            }

            string organism,
                   chromosome;
            if (entry.IsSetDescr() && entry.GetDescr().IsSet()) {
                const CSeq_descr::Tdata& descrs = entry.GetDescr().Get();
                auto source = find_if(descrs.begin(), descrs.end(), [](const CRef<CSeqdesc>& descr) { return descr->IsSource(); });
                if (source != descrs.end()) {
                    GetSourceInfo((*source)->GetSource(), organism, chromosome, has_organism, has_chromosome);
                }
            }

            if (has_organism) {
                if (title_info.m_organism.empty()) {
                    title_info.m_organism = organism;
                }

                if (organism.empty()) {
                    organism = title_info.m_organism;
                }

                ReplaceInTitle(new_title, "[Organism] ", organism);
            }

            if (has_chromosome) {
                if (title_info.m_chromosome.empty()) {
                    title_info.m_chromosome = chromosome;
                }

                if (chromosome.empty()) {
                    chromosome = title_info.m_chromosome;
                }

                ReplaceInTitle(new_title, "[Chromosome] ", chromosome);
            }

            entry.SetSeq().SetDescr().Set().push_front(title_descr);
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            RepTitles(*cur_entry, title_info);
        }
    }
}

bool ParseSubmissions(CMasterInfo& master_info)
{
    const list<string>& files = GetParams().GetInputFiles();

    bool ret = true;

    for (auto& file : files) {
        CNcbiIfstream in(file);

        if (!in) {
            ERR_POST_EX(0, 0, "Failed to open submission \"" << file << "\" for reading. Cannot proceed.");
            ret = false;
            break;
        }

        if (master_info.m_input_type == eUnknownType) {
            GetInputTypeFromFile(in, master_info.m_input_type);
        }

        CRef<CBioseq_set> bioseq_set;

        bool first = true;
        while (in && !in.eof()) {
            CRef<CSeq_submit> seq_submit = GetSeqSubmit(in, master_info.m_input_type);
            if (seq_submit.Empty()) {

                if (first) {
                    ERR_POST_EX(0, 0, "Failed to read " << GetSeqSubmitTypeName(master_info.m_input_type) << " from file \"" << file << "\". Cannot proceed.");
                    ret = false;
                }
                break;
            }

            first = false;
            FixSeqSubmit(seq_submit, master_info.m_accession_ver, false, master_info.m_reject);

            CRef<CSeqdesc> cit_sub_descr;
            if (seq_submit->IsSetSub() && seq_submit->GetSub().IsSetCit()) {

                if (GetParams().IsSetSubmissionDate()) {
                    seq_submit->SetSub().SetCit().SetDate().SetStd().Assign(GetParams().GetSubmissionDate());
                }

                if (!master_info.m_got_cit_sub && (master_info.m_input_type != eSeqSubmit || GetParams().GetSource() == eNCBI)) {
                    const CContact_info* contact = nullptr;
                    if (seq_submit->GetSub().IsSetContact()) {
                        contact = &seq_submit->GetSub().GetContact();
                    }

                    cit_sub_descr = CreateCitSub(seq_submit->GetSub().GetCit(), contact);
                }
            }

            if (seq_submit->IsSetData() && seq_submit->GetData().IsEntrys()) {

                map<string, string> ids;
                for (auto& entry : seq_submit->SetData().SetEntrys()) {

                    if (master_info.m_need_to_remove_dates) {
                        RemoveDates(*entry, master_info.m_creation_date_present, master_info.m_update_date_present);
                    }

                    if (GetParams().IsVDBMode() && !master_info.m_keywords.empty()) {
                        RemoveKeywords(*entry, master_info.m_keywords);
                    }

                    if (!GetParams().GetTpaKeyword().empty()) {
                        PropagateTPAKeyword(*entry);
                    }

                    if (master_info.m_has_gb_block) {
                        CleanupProtGbblock(*entry);
                    }

                    if (GetParams().GetUpdateMode() != eUpdatePartial) {
                        RemoveComments(*entry, master_info.m_common_comments, GetCommentString);
                        RemoveComments(*entry, master_info.m_common_structured_comments, GetStructuredCommentString);
                        RemovePubs(*entry, master_info.m_common_pubs, master_info.m_pubs, nullptr);
                    }

                    if (cit_sub_descr.NotEmpty()) {
                        entry->SetDescr().Set().push_front(cit_sub_descr);
                    }

                    if (GetParams().IsReplaceDBName() || HasLocals(*entry)) {
                        FixDbName(*entry, ids);
                        FixOrigProtTransIds(*entry);
                    }

                    if (!GetParams().IsVDBMode()) {
                        AssignNucAccession(*entry, file, master_info.m_id_infos, master_info.m_order_of_entries);
                    }

                    if (GetParams().IsUpdateScaffoldsMode()) {

                        if (GetParams().GetSource() == eEMBL) {
                            AddMasterToSecondary(*entry, CEMBLBlockAdaptor());
                        }
                        else {
                            AddMasterToSecondary(*entry, CGBBlockAdaptor());
                        }
                    }

                    if (GetParams().IsForcedGencode()) {
                        FixGeneticCode(*entry);
                    }

                    if (GetParams().GetFixTech() && (GetParams().IsTsa() || (GetParams().GetFixTech() | eFixMolBiomol))) {
                        FixTech(*entry);
                    }

                    SeqToDelta(*entry, GetParams().GetGapSize());

                    if (master_info.m_reject) {
                        break;
                    }

                    bool lookup_succeed = FixBioSources(*entry, master_info);

                    size_t dblink_order_num = 0;
                    if (master_info.m_gpid || master_info.m_dblink_state == eDblinkNoProblem) {
                        RemoveDblinkGPID(*entry, dblink_order_num);
                    }

                    if (GetParams().IsUpdateScaffoldsMode() && master_info.m_dblink_state == eDblinkNoProblem) {
                        AddDblink(*entry, *master_info.m_dblink, dblink_order_num);
                    }

                    if (!lookup_succeed) {
                        ERR_POST_EX(0, 0, Critical << "Taxonomy lookup failed on submission \"" << file << "\". Cannot proceed.");
                        break;
                    }

                    if (GetParams().IsStripAuthors()) {
                        StripAuthors(*entry);
                    }

                    if (!GetParams().GetNewNucTitle().empty()) {
                        CTitleInfo title_info;
                        RepTitles(*entry, title_info);
                    }

                    FixGbblockSource(*entry);
                    PackEntry(*entry);

                } // for each entry

                bool remove_bio_sources = GetParams().IsVDBMode() && master_info.m_biosource.NotEmpty() && master_info.m_same_org && master_info.m_same_biosource;
                PerformCleanup(seq_submit->SetData().SetEntrys(), remove_bio_sources);

                if (bioseq_set.Empty()) {
                    bioseq_set.Reset(new CBioseq_set);
                    bioseq_set->SetClass(CBioseq_set::eClass_genbank);
                }

                bioseq_set->SetSeq_set().splice(bioseq_set->SetSeq_set().end(), seq_submit->SetData().SetEntrys());
            }
        }

        if (GetParams().GetUpdateMode() == eUpdatePartial) {
            for (auto& cur_entry : bioseq_set->SetSeq_set()) {
                RemoveComments(*cur_entry, master_info.m_common_comments, GetCommentString);
                RemoveComments(*cur_entry, master_info.m_common_structured_comments, GetStructuredCommentString);

                const CDate_std* submission_date = GetParams().IsSetSubmissionDate() ? &GetParams().GetSubmissionDate() : nullptr;
                RemovePubs(*cur_entry, master_info.m_common_pubs, master_info.m_pubs, submission_date);
            }
        }

        if (!GetParams().IsVDBMode()) {

            auto sort_by_accessions_func = [](const CRef<CSeq_entry>& a, const CRef<CSeq_entry>& b)
                {
                    //const string & sa = GetAssignedAccession(*a);
                    //const string & sb = GetAssignedAccession(*b);

                    return GetAssignedAccession(*a) > GetAssignedAccession(*b);
                };

            bioseq_set->SetSeq_set().sort(sort_by_accessions_func);
        }

        if (GetParams().IsTest()) {
            ret = true;
        }
        else {
            ret = OutputSubmission(*bioseq_set, file);
        }

        if (!ret) {
            ERR_POST_EX(0, 0, Error << "Failed to save processed submission \"" << file << "\" to file.");
            break;
        }
    }

    return ret;
}

}
