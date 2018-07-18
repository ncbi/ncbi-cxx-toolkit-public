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

#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/general/Person_id.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/general/User_object.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seqblock/EMBL_block.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seq_inst.hpp>

#include <objects/biblio/Auth_list.hpp>
#include <objects/biblio/Author.hpp>

#include "wgs_utils.hpp"
#include "wgs_asn.hpp"
#include "wgs_params.hpp"


namespace wgsparse
{

static bool IsSpecialId(const CSeq_id& id)
{
    return id.IsDdbj() || id.IsEmbl() || id.IsGenbank();
}

static void MoveSpecialIdToTop(CBioseq::TId& ids)
{
    for (auto id = ids.begin(); id != ids.end(); ++id) {
        if (IsSpecialId(**id)) {
            ids.push_front(*id);
            ids.erase(id);
            break;
        }
    }
}

static bool ToBeRemoved(const CSeq_id& id)
{
    bool ret = false;
    if (id.IsGeneral() && id.GetGeneral().IsSetDb()) {

        const string& db = id.GetGeneral().GetDb();
        ret = db == "TMSMART" || db == "NCBIFILE" || db == "BankIt";
    }

    return ret;
}

static void RemoveSomeIds(CBioseq::TId& ids)
{
    for (auto id = ids.begin(); id != ids.end();) {
        if (ToBeRemoved(**id)) {
            id = ids.erase(id);
        }
        else {
            ++id;
        }
    }
}

static bool ContainsLocalsAndGenerals(const CBioseq::TId& ids)
{
    bool locals = false,
         generals = false;

    for (auto id : ids) {
        if (id->IsLocal()) {
            locals = true;
        }
        if (id->IsGeneral()) {
            generals = true;
        }
    }

    return locals & generals;
}

static void CheckGeneralLocalIds(CBioseq::TId& ids, bool &reject)
{
    string general_id,
           local_id;

    for (auto id = ids.begin(); id != ids.end();) {

        bool removed = false;
        if ((*id)->IsGeneral()) {
            if (general_id.empty() && (*id)->GetGeneral().IsSetTag()) {

                general_id = GetIdStr((*id)->GetGeneral().GetTag());
            }
        }
        else {
            if (local_id.empty() && (*id)->IsLocal()) {

                local_id = GetIdStr((*id)->GetLocal());
                id = ids.erase(id);
                removed = true;
            }
        }

        if (!general_id.empty() && !local_id.empty()) {
            break;
        }

        if (!removed) {
            ++id;
        }
    }

    if (general_id != local_id) {
        ERR_POST_EX(0, 0, Critical << "General and local ids within the same Bioseq are not identical: \"" << general_id << "\" vs \"" << local_id << "\".");
        reject = true;
    }
}

static void FixIds(CRef<CSeq_entry>& entry, bool &reject)
{
    if (entry->IsSeq()) {

        if (entry->GetSeq().IsSetId()) {

            CBioseq::TId& ids = entry->SetSeq().SetId();
            MoveSpecialIdToTop(ids);
            RemoveSomeIds(ids);

            if (ContainsLocalsAndGenerals(ids)) {
                CheckGeneralLocalIds(ids, reject);
            }
        }
    }
    else if (entry->IsSet()) {
        if (entry->GetSet().IsSetSeq_set()) {
            for_each(entry->SetSet().SetSeq_set().begin(), entry->SetSet().SetSeq_set().end(), [&reject](CRef<CSeq_entry>& cur_entry){ FixIds(cur_entry, reject); });
        }
    }
}

static bool NeedToProcess(CBioseq_set::EClass bio_set_class)
{
    static set<CBioseq_set::EClass> NEED_TO_PROCESS = {
        CBioseq_set::eClass_genbank,
        CBioseq_set::eClass_pub_set,
        CBioseq_set::eClass_gen_prod_set,
        CBioseq_set::eClass_wgs_set,
        CBioseq_set::eClass_eco_set,
        CBioseq_set::eClass_phy_set,
        CBioseq_set::eClass_small_genome_set
    };

    return NEED_TO_PROCESS.find(bio_set_class) != NEED_TO_PROCESS.end();
}

static bool DescrProcValid(const CSeqdesc&, bool)
{
    return true;
}

static bool DescrProcUnusual(const CSeqdesc& descr, bool first)
{
    static const map<CSeqdesc::E_Choice, string> UNUSUAL_CHOICE_STR = {
        { CSeqdesc::e_Genbank, "genbank" },
        { CSeqdesc::e_Maploc, "maploc" },
        { CSeqdesc::e_Dbxref, "dbxref" },
        { CSeqdesc::e_Source, "source" }
    };

    if (first) {
        const string& str = UNUSUAL_CHOICE_STR.at(descr.Which());
        LOG_POST_EX(0, 0, "Unusual descriptor of type \"" << str << "\" found on top of GenBank set.");
    }

    return true;
}

static bool DescrProcUnexpected(const CSeqdesc& descr, bool first)
{
    static map<CSeqdesc::E_Choice, string> UNEXPECTED_CHOICE_STR = {
        { CSeqdesc::e_Mol_type, "mol_type" },
        { CSeqdesc::e_Modif, "modif" },
        { CSeqdesc::e_Method, "method" },
        { CSeqdesc::e_Name, "name" },
        { CSeqdesc::e_Org, "org" },
        { CSeqdesc::e_Num, "num" },
        { CSeqdesc::e_Pir, "pir" },
        { CSeqdesc::e_Region, "region" },
        { CSeqdesc::e_Sp, "sp" },
        { CSeqdesc::e_Embl, "embl" },
        { CSeqdesc::e_Prf, "prf" },
        { CSeqdesc::e_Pdb, "pdb" },
        { CSeqdesc::e_Het, "het" },
        { CSeqdesc::e_Molinfo, "molinfo" },
        { CSeqdesc::e_Modelev, "moledev" }
    };

    if (first) {
        const string& str = UNEXPECTED_CHOICE_STR[descr.Which()];
        ERR_POST_EX(0, 0, Warning << "Unexpected descriptor of type \"" << str << "\" found on top of GenBank set. Descriptor dropped.");
    }

    return false;
}

static bool DescrProcUnexpectedWarning(const CSeqdesc& descr, bool first)
{
    static const map<CSeqdesc::E_Choice, string> UNEXPECTED_CHOICE_STR = {
        { CSeqdesc::e_Create_date, "create_date" },
        { CSeqdesc::e_Update_date, "update_date" }
    };

    if (first) {
        const string& str = UNEXPECTED_CHOICE_STR.at(descr.Which());
        LOG_POST_EX(0, 0, Warning << "Unexpected descriptor of type \"" << str << "\" found on top of GenBank set. Descriptor dropped.");
    }

    return true;
}

static bool DescrProcUnexpectedSpecial(const CSeqdesc&, bool first)
{
    if (first) {
        static const string ERR_MSG = "Unexpected descriptor of type \"title\" found on top of GenBank set. Descriptor dropped.";
        if (GetParams().GetSource() == eNCBI && GetParams().IsTls()) {
            LOG_POST_EX(0, 0, Info << ERR_MSG);
        }
        else {
            ERR_POST_EX(0, 0, Warning << ERR_MSG);
        }
    }
    return false;
}

static bool DescrProcUser(const CSeqdesc& descr, bool first)
{
    bool ret = false;

    const CUser_object& user_obj = descr.GetUser();
    if (user_obj.IsSetClass()) {
        if (first) {
            ERR_POST_EX(0, 0, "Unexpected User-object descriptor of class \"" << user_obj.GetClass() << "\" found on top of GenBank set. Descriptor dropped.");
        }
    }
    else if (user_obj.IsSetType() && user_obj.GetType().IsStr()) {

        const string& type = user_obj.GetType().GetStr();
        if (type == "DBLink" || type == "GenomeProjectsDB" || type == "StructuredComment") {
            ret = true;
        }
        else if (first && type != "NcbiCleanup") {

            bool is_error = true;
            if (type == "Submission" && GetParams().GetSource() == eNCBI && GetParams().IsTsa()) {
                is_error = false;
            }

            ERR_POST_EX(0, 0, (is_error ? Error : Info) << "Unexpected User-object descriptor of type \"" << type << "\" found on top of GenBank set. Descriptor dropped.");
        }
    }
    else {
        if (first) {
            ERR_POST_EX(0, 0, "Unexpected User-object descriptor with unknown type or _class found on top of GenBank set. Descriptor dropped.");
        }
    }

    return ret;
}


// returns 'true' id description list is not empty
static bool FixDescr(CSeq_descr& descrs, bool first)
{
    // CR may be hash_map or sorted vector
    typedef bool(*TPocessDescr)(const CSeqdesc&, bool);
    static const map<CSeqdesc::E_Choice, TPocessDescr> DESCR_PROCESS_FUNC = {
        { CSeqdesc::e_Comment, DescrProcValid },
        { CSeqdesc::e_Pub, DescrProcValid },

        { CSeqdesc::e_Genbank, DescrProcUnusual },
        { CSeqdesc::e_Maploc, DescrProcUnusual },
        { CSeqdesc::e_Dbxref, DescrProcUnusual },
        { CSeqdesc::e_Source, DescrProcUnusual },

        { CSeqdesc::e_Mol_type, DescrProcUnexpected },
        { CSeqdesc::e_Modif, DescrProcUnexpected },
        { CSeqdesc::e_Method, DescrProcUnexpected },
        { CSeqdesc::e_Name, DescrProcUnexpected },
        { CSeqdesc::e_Org, DescrProcUnexpected },
        { CSeqdesc::e_Num, DescrProcUnexpected },
        { CSeqdesc::e_Pir, DescrProcUnexpected },
        { CSeqdesc::e_Region, DescrProcUnexpected },
        { CSeqdesc::e_Sp, DescrProcUnexpected },
        { CSeqdesc::e_Embl, DescrProcUnexpected },
        { CSeqdesc::e_Prf, DescrProcUnexpected },
        { CSeqdesc::e_Pdb, DescrProcUnexpected },
        { CSeqdesc::e_Het, DescrProcUnexpected },
        { CSeqdesc::e_Molinfo, DescrProcUnexpected },
        { CSeqdesc::e_Modelev, DescrProcUnexpected },

        { CSeqdesc::e_Create_date, DescrProcUnexpectedWarning },
        { CSeqdesc::e_Update_date, DescrProcUnexpectedWarning },

        { CSeqdesc::e_Title, DescrProcUnexpectedSpecial },

        { CSeqdesc::e_User, DescrProcUser }
    };


    bool ret = false;
    if (descrs.IsSet()) {

        for (auto descr = descrs.Set().begin(); descr != descrs.Set().end();) {

            bool good_descr = false;
            auto proc_func = DESCR_PROCESS_FUNC.find((*descr)->Which());
            if (proc_func != DESCR_PROCESS_FUNC.end()) {

                good_descr = proc_func->second(**descr, first);
                if (good_descr) {
                    ++descr;
                }
            }

            if (!good_descr) {
                descr = descrs.Set().erase(descr);
            }
        }

        ret = !descrs.Get().empty();
    }

    return ret;
}

static void AddDescr(const CSeq_descr& descrs, CSeq_entry& entry)
{
    for (auto descr : descrs.Get()) {
        entry.SetDescr().Set().push_back(descr);
    }
}

static bool ReorganizeEntries(CSeq_submit::C_Data::TEntrys& entries, bool first)
{
    bool ret = true;

    for (auto entry = entries.begin(); entry != entries.end();) {

        if ((*entry)->IsSet() && (*entry)->GetSet().IsSetClass() && NeedToProcess((*entry)->GetSet().GetClass())) {

            CBioseq_set& cur_bioseq_set = (*entry)->SetSet();
            if (cur_bioseq_set.IsSetAnnot()) {
                ret = false;
                cur_bioseq_set.ResetAnnot();
            }

            bool add_descr = false;
            if (cur_bioseq_set.IsSetDescr()) {
                add_descr = FixDescr(cur_bioseq_set.SetDescr(), first);
            }

            if (cur_bioseq_set.IsSetSeq_set()) {

                // Process entries of 'cur_bioseq_set' and put them into 'entries' list

                if (add_descr) {
                    for (auto cur_entry : cur_bioseq_set.SetSeq_set()) {
                        AddDescr(cur_bioseq_set.GetDescr(), *cur_entry);
                    }
                }

                entries.splice(entry, cur_bioseq_set.SetSeq_set());
            }

            entry = entries.erase(entry);
        }
        else
            ++entry;
    }

    return ret;
}

static int GetAccessionVersion(const CSeq_id& id)
{
    int ret = -1;
    if (IsSpecialId(id)) {
        const CTextseq_id* text_id = id.GetTextseq_Id();
        if (text_id != nullptr && text_id->IsSetAccession()) {
            const char* ver = text_id->GetAccession().c_str();
            ret = NStr::StringToInt(ver, NStr::fAllowLeadingSymbols | NStr::fAllowTrailingSymbols);
            if (ret == 0) {
                ret = -1;
            }
        }
    }

    return ret;
}

static int GetAccessionVersion(const CSeq_entry& entry)
{
    int ret = -1;
    if (entry.IsSeq()) {

        if (entry.GetSeq().IsSetId()) {

            for (auto id : entry.GetSeq().GetId()) {
                ret = GetAccessionVersion(*id);
                if (ret > 0) {
                    break;
                }
            }
        }
    }
    else if (entry.IsSet()) {
        if (entry.GetSet().IsSetSeq_set()) {

            for (auto cur_entry : entry.GetSet().GetSeq_set()) {
                ret = GetAccessionVersion(*cur_entry);
                if (ret > 0) {
                    break;
                }
            }
        }
    }

    return ret;
}


static bool NeedToGetAccession()
{
    return GetParams().GetUpdateMode() == eUpdateAssembly && GetParams().IsAccessionAssigned();
}

bool FixSeqSubmit(CRef<CSeq_submit>& seq_submit, int& accession_ver, bool first, bool &reject)
{
    bool ret = true;
    if (seq_submit->IsSetData() && seq_submit->GetData().IsEntrys()) {

        CSeq_submit::C_Data::TEntrys& entries = seq_submit->SetData().SetEntrys();
        for (auto& entry : entries) {
            FixIds(entry, reject);
        }

        if (first && accession_ver < 0 && NeedToGetAccession()) {
            for (auto& entry : entries) {
                accession_ver = GetAccessionVersion(*entry);
                if (accession_ver > 0) {
                    break;
                }
            }
        }

        ret = ReorganizeEntries(entries, first);
    }

    return ret;
}

static EChromosomeSubtypeStatus CheckChromosome(const CSeq_descr& descrs)
{
    if (GetParams().GetScaffoldPrefix().empty() || GetParams().GetScaffoldType() != eRegularChromosomal || GetParams().GetScaffoldType() != eTPAChromosomal)
        return eChromosomeSubtypeValid;

    bool source_present = false;
    bool bacteria_or_archea = false;

    if (descrs.IsSet()) {

        for (auto descr = descrs.Get().begin(); descr != descrs.Get().end(); ++descr) {

            if ((*descr)->IsSource()) {

                source_present = true;

                const CBioSource& source = (*descr)->GetSource();

                if (source.IsSetLineage()) {
                    bacteria_or_archea = HasLineage(source.GetLineage(), "Bacteria") || HasLineage(source.GetLineage(), "Archaea");
                }

                if (source.IsSetSubtype()) {
                    for (auto subsource : source.GetSubtype()) {
                        if (subsource->IsSetSubtype() && subsource->GetSubtype() == CSubSource::eSubtype_chromosome &&
                            subsource->IsSetName() && !subsource->GetName().empty()) {
                            return eChromosomeSubtypeValid;
                        }
                    }
                }
            }
        }
    }

    if (source_present) {
        return bacteria_or_archea ? eChromosomeSubtypeMissingInBacteria : eChromosomeSubtypeMissing;
    }

    return eChromosomeSubtypeValid;
}

static void CheckKeywords(const CSeq_descr& descrs, CSeqEntryInfo& info)
{
    const CGB_block* genbank_block = nullptr;

    if (descrs.IsSet()) {

        auto genbank_it = find_if(descrs.Get().begin(), descrs.Get().end(), [](const CRef<CSeqdesc>& descr) { return descr->IsGenbank(); });
        if (genbank_it != descrs.Get().end()) {
            genbank_block = &(*genbank_it)->GetGenbank();
        }

        if (genbank_block && genbank_block->IsSetKeywords()) {

            if (info.m_keywords_set) {

                if (!info.m_keywords.empty()) {
                    // removes all previously stored keywords which are not present in current CGB_block::keywords
                    set<string> keywords_found;
                    for (auto keyword : genbank_block->GetKeywords()) {
                        if (info.m_keywords.find(keyword) != info.m_keywords.end()) {
                            keywords_found.insert(keyword);
                        }
                    }
                    info.m_keywords.swap(keywords_found);
                }
            }
            else {
                // copies all keywords for the first time
                copy(genbank_block->GetKeywords().begin(), genbank_block->GetKeywords().end(), inserter(info.m_keywords, info.m_keywords.end()));
            }
        }
    }

    info.m_keywords_set = true;

    if (genbank_block && genbank_block->IsSetKeywords()) {

        for (auto keyword : genbank_block->GetKeywords()) {
            if (keyword == "TPA:assembly" || keyword == "TPA:reassembly") {
                info.m_has_tpa_keyword = true;
            }
            else if (keyword == "Targeted") {
                info.m_has_targeted_keyword = true;
            }
            else if (keyword == "GMI") {
                info.m_has_gmi_keyword = true;
            }
        }
    }
}

static bool HasGbBlock(const CSeq_descr& descrs)
{
    bool ret = false;
    if (descrs.IsSet()) {

        auto genbank_it = find_if(descrs.Get().begin(), descrs.Get().end(), [](const CRef<CSeqdesc>& descr) { return descr->IsGenbank(); });
        ret = genbank_it != descrs.Get().end();
    }
    return ret;
}

static EIdProblem LookThroughObjectIds(const CBioseq::TId& ids, string& obj_id_str, string& db_name_str)
{
    for (auto id : ids) {

        if (id->IsGeneral() || id->IsLocal()) {
            if (!obj_id_str.empty()) {
                return eIdManyGeneralIds;
            }

            const CObject_id* obj_id = nullptr;
            if (id->IsGeneral()) {
                const CDbtag& dbtag = id->GetGeneral();
                if (dbtag.IsSetDb()) {
                    db_name_str = dbtag.GetDb();
                }

                if (dbtag.IsSetTag()) {
                    obj_id = &dbtag.GetTag();
                }
            }
            else {
                obj_id = &id->GetLocal();
            }

            if (obj_id) {
                obj_id_str = GetIdStr(*obj_id);
            }
        }
    }

    return obj_id_str.empty() ? eIdNoDbTag : eIdNoProblem;
}

static bool ToBeReplaced(const string& db_name)
{
    return (GetParams().IsChromosomal() && GetParams().GetProjAccStr() != db_name) ||
        (!GetParams().IsChromosomal() && GetParams().GetProjAccVerStr() != db_name);
}

static void CheckDBName(const string& db_name, bool is_nuc, CSeqEntryInfo& info, CSeqEntryCommonInfo& common_info)
{
    const string& proj_acc_str = GetParams().GetProjAccStr();
    const string& proj_acc_ver_str = GetParams().GetProjAccVerStr();

    if (info.m_dbname_problem == eDBNameNoProblem &&
        NStr::StartsWith(db_name, "WGS:") && !NStr::StartsWith(db_name, "WGS:XXXX")
        && !GetParams().IsDblinkOverride()) {

        if ((!is_nuc && db_name != proj_acc_str) ||
            (is_nuc && db_name != proj_acc_str && db_name != proj_acc_ver_str)) {

            info.m_dbname = db_name;
            info.m_dbname_problem = is_nuc ? eDBNameBadNucDB : eDBNameBadProtDB;
        }
    }

    if (!GetParams().IsReplaceDBName()) {
        if (info.m_dbname.empty()) {
            info.m_dbname = db_name;
        }
        else if (info.m_dbname != db_name) {
            info.m_diff_dbname = db_name;
        }
    }
    else if (info.m_dbname_problem == eDBNameNoProblem) {

        if (is_nuc && !common_info.m_nuc_warn && ToBeReplaced(db_name)) {
            ERR_POST_EX(0, 0, Warning << "One or more nucleotide Bioseqs have non standard dbname \"" << db_name << "\", will replace with \"" << (GetParams().IsChromosomal() ? proj_acc_str : proj_acc_ver_str) << "\".");
            common_info.m_nuc_warn = true;
        }

        if (!is_nuc && !common_info.m_prot_warn && db_name != proj_acc_str) {
            ERR_POST_EX(0, 0, Warning << "One or more protein Bioseqs have non standard dbname \"" << db_name << "\", will replace with \"" << proj_acc_str << "\".");
            common_info.m_prot_warn = true;
        }
    }
}

static bool IsBadTech(CMolInfo::TTech tech)
{
    return (GetParams().IsWgs() && tech != CMolInfo::eTech_wgs) ||
        (GetParams().IsTsa() && tech != CMolInfo::eTech_tsa) ||
        (GetParams().IsTls() && tech != CMolInfo::eTech_targeted);
}

static bool IsBadBiomol(CMolInfo::TBiomol biomol)
{
    return (!GetParams().IsTsa() && biomol != CMolInfo::eBiomol_genomic) ||
        (GetParams().IsTsa() && biomol != CMolInfo::eBiomol_mRNA && biomol != CMolInfo::eBiomol_rRNA &&
        biomol != CMolInfo::eBiomol_transcribed_RNA && biomol != CMolInfo::eBiomol_ncRNA);
}

static bool IsBadMol(CSeq_inst::EMol mol)
{
    return (!GetParams().IsTsa() && mol != CSeq_inst::eMol_dna) ||
        (GetParams().IsTsa() && mol != CSeq_inst::eMol_rna);
}

static void CheckMolecule(const CBioseq& bioseq, CSeqEntryInfo& info)
{
    if (bioseq.IsSetDescr() && bioseq.GetDescr().IsSet()) {

        const CSeq_descr::Tdata& descrs = bioseq.GetDescr().Get();
        auto mol_info_it = find_if(descrs.begin(), descrs.end(), [](const CRef<CSeqdesc>& descr) { return descr->IsMolinfo(); });
        if (mol_info_it != descrs.end()) {

            const CMolInfo& mol_info = (*mol_info_it)->GetMolinfo();
            if (GetParams().IsTsa() && GetParams().GetFixTech() == eNoFix && info.m_biomol_state != eBiomolDiffers && mol_info.IsSetBiomol()) {
                if (info.m_biomol_state == eBiomolNotSet) {
                    info.m_biomol_state = eBiomolSet;
                    info.m_biomol = mol_info.GetBiomol();
                }
                else if (info.m_biomol != mol_info.GetBiomol()) {

                    info.m_biomol_state = eBiomolDiffers;
                    ERR_POST_EX(0, 0, "Different Molinfo.biomol values found amongst TSA records. First appearance is in record with id \"" << GetSeqIdStr(bioseq) << "\".");
                }
            }

            CMolInfo::TTech tech = mol_info.IsSetTech() ? mol_info.GetTech() : CMolInfo::eTech_unknown;
            info.m_bad_tech = IsBadTech(tech);

            CMolInfo::TBiomol biomol = mol_info.IsSetBiomol() ? mol_info.GetBiomol() : CMolInfo::eBiomol_unknown;
            info.m_bad_biomol = IsBadBiomol(biomol);
        }
    }

    if (bioseq.IsSetInst() && bioseq.GetInst().IsSetMol()) {
        info.m_bad_mol = IsBadMol(bioseq.GetInst().GetMol());
    }
}

static bool ProcessAccession(const string& accession)
{
    if (GetParams().IsUpdateScaffoldsMode()) {
        if (NStr::StartsWith(accession, GetParams().GetAccession())) {

            auto next_to_zeros = accession.find_first_not_of('0', GetParams().GetAccession().size());
            if (next_to_zeros == string::npos || accession[next_to_zeros] == '-') {
                return false;
            }
        }
    }

    return true;
}

static bool IsProjectAccession(const string& accession, size_t len)
{
    static const size_t MIN_ACCESSION_LEN = 12;
    static const size_t MAX_ACCESSION_LEN = 14;

    if (len < MIN_ACCESSION_LEN || len > MAX_ACCESSION_LEN) {
        return false;
    }

    bool four_letters_two_digits = NStr::IsUpper(CTempString(accession.c_str(), 4)) && isdigit(accession[4]) && isdigit(accession[5]);
    if (four_letters_two_digits) {

        auto next_to_zeros = accession.find_first_not_of('0', 6);
        if (next_to_zeros == string::npos || accession[next_to_zeros] == '-') {
            return true;
        }
    }

    return false;
}

static bool IsBadHistSeqId(const CSeq_id& id)
{
    static const set<CSeq_id::E_Choice> ALLOWED_CHOICE = {
        CSeq_id::e_Genbank,
        CSeq_id::e_Embl,
        CSeq_id::e_Ddbj,
        CSeq_id::e_Other,
        CSeq_id::e_Tpg,
        CSeq_id::e_Tpe,
        CSeq_id::e_Tpd
    };

    if (ALLOWED_CHOICE.find(id.Which()) != ALLOWED_CHOICE.end()) {
        const CTextseq_id* text_id = id.GetTextseq_Id();
        if (text_id && text_id->IsSetAccession()) {
            return true;
        }
    }

    return false;
}

static void CheckSecondaries(const CBioseq& bioseq, CSeqEntryInfo& info)
{
    string cur_id = GetSeqIdStr(bioseq);

    set<string> extra_accessions;

    if (bioseq.IsSetDescr() && bioseq.GetDescr().IsSet()) {

        for (auto descr : bioseq.GetDescr().Get()) {

            const list<string>* extra_acc = nullptr;
            if (descr->IsGenbank()) {

                if (descr->GetGenbank().IsSetExtra_accessions()) {
                    extra_acc = &descr->GetGenbank().GetExtra_accessions();
                }
            }
            else if (descr->IsEmbl()) {
                if (descr->GetEmbl().IsSetExtra_acc()) {
                    extra_acc = &descr->GetEmbl().GetExtra_acc();
                }
            }

            if (extra_acc) {

                for (auto& accession : *extra_acc) {

                    if (ProcessAccession(accession)) {

                        size_t range = accession.find('-');
                        if (range == string::npos) {
                            ERR_POST_EX(0, 0, Error << "Found secondary accession \"" << accession << "\" in record with id \"" << cur_id << "\".");
                        }
                        else {
                            ERR_POST_EX(0, 0, Error << "Found secondary accessions range \"" << accession << "\" in record with id \"" << cur_id << "\".");
                        }

                        if (!IsProjectAccession(accession, range == string::npos ? accession.size() : range)) {
                            if (range == string::npos) {
                                extra_accessions.insert(accession);
                            }
                            else {
                                // Unwrap the range
                                auto digits = find_if(accession.begin(), accession.end(), [](char c) { return isdigit(c); });
                                string prefix(accession.begin(), digits);

                                size_t start = NStr::StringToSizet(CTempString(accession, digits - accession.begin(), range));
                                digits = find_if(accession.begin() + range, accession.end(), [](char c) { return isdigit(c); });
                                size_t end = NStr::StringToSizet(CTempString(accession, digits - accession.begin(), accession.size()));

                                for (size_t cur_num = start; cur_num <= end; ++cur_num) {
                                    extra_accessions.insert(prefix + to_string(cur_num));
                                }
                            }
                        }
                    }
                }
            }
        }

        info.m_secondary_accessions = !extra_accessions.empty();

        set<string> hist_accessions;

        if (bioseq.IsSetInst() && bioseq.GetInst().IsSetHist() && bioseq.GetInst().GetHist().IsSetReplaces() &&
            bioseq.GetInst().GetHist().GetReplaces().IsSetIds()) {

            bool bad_id_found = false;
            for (auto id : bioseq.GetInst().GetHist().GetReplaces().GetIds()) {

                if (IsBadHistSeqId(*id)) {
                    bad_id_found = true;
                    break;
                }

                hist_accessions.insert(id->GetTextseq_Id()->GetAccession());
            }

            if (bad_id_found) {
                ERR_POST_EX(0, 0, Error << "Empty or incorrect sequence identifier provided in Seq-hist block for record \"" << cur_id << "\".");
                info.m_hist_secondary_differs = true;
            }
        }

        if (!info.m_hist_secondary_differs) {
            info.m_hist_secondary_differs = extra_accessions != hist_accessions;
        }
    }
}

static bool IsAllowedSeqId(const CSeq_id& id)
{
    return id.IsDdbj() || id.IsEmbl() || id.IsGenbank() || id.IsTpd() || id.IsTpe() || id.IsTpg();
}

static void CheckSeqIdStatus(const CBioseq::TId& ids, CSeq_id::E_Choice& choice, ESeqIdStatus& state)
{
    _ASSERT(!ids.empty() && "ids should contain at least one item");

    const CRef<CSeq_id>& first_id = ids.front();

    if (ids.size() > 1) {
        state = eSeqIdMultiple;
    }
    else if (choice == CSeq_id::e_not_set) {
        choice = first_id->Which();
    }
    else if (!first_id->IsLocal() && !first_id->IsGeneral()) {

        if (!GetParams().IsAccessionAssigned()) {
            state = eSeqIdIncorrect;
        }

        if (!IsAllowedSeqId(*first_id) || choice != first_id->Which()) {
            state = eSeqIdIncorrect;
        }
    }
    else if (choice != first_id->Which()) {
        state = eSeqIdDifferent;
    }
}

bool IsScaffoldPrefix(const string& accession, size_t prefix_len)
{
    static string scaffold_prefs[] = { "CH", "CM", "DS", "EM", "EN", "EP", "EQ", "FA", "GG", "GJ", "GK", "GL", "JH", "KB", "KD", "KE", "KI", "KK", "KL", "KN", "KQ", "KV", "KZ" };
    static string* end_of_scaffold_prefs = scaffold_prefs + sizeof(scaffold_prefs) / sizeof(scaffold_prefs[0]);

    string prefix = accession.substr(0, prefix_len);
    return find(scaffold_prefs, end_of_scaffold_prefs, prefix) != end_of_scaffold_prefs;
}

static bool IsAccessionValid(const string& accession)
{
    const string& prefix = GetParams().GetUpdateMode() == eUpdateScaffoldsUpd ||
        (GetParams().GetUpdateMode() == eUpdateScaffoldsNew && GetParams().IsAccessionAssigned()) ?
        GetParams().GetScaffoldPrefix() : GetParams().GetIdPrefix();

    size_t prefix_size = prefix.size();
    if (prefix_size > accession.size()) {
        return false;
    }

    if (GetParams().GetUpdateMode() == eUpdateScaffoldsUpd) {

        static const size_t SCAFFOLD_ACCESSION_LEN = 8;
        if (accession.size() != SCAFFOLD_ACCESSION_LEN ||
            (GetParams().GetSource() != eDDBJ && GetParams().GetSource() != eEMBL && !IsScaffoldPrefix(accession, prefix_size))) {
            return false;
        }
    }
    else {

        if (GetParams().GetUpdateMode() == eUpdateScaffoldsUpd ||
            (GetParams().GetUpdateMode() == eUpdateScaffoldsNew && !GetParams().IsAccessionAssigned())) {

            static const size_t MIN_ACCESSION_LEN = 12;
            static const size_t MAX_ACCESSION_LEN = 17;

            if (accession.size() < MIN_ACCESSION_LEN || accession.size() > MAX_ACCESSION_LEN) {
                return false;
            }
        }

        if (GetParams().GetSource() == eNCBI && prefix == GetParams().GetIdPrefix() &&
            !NStr::StartsWith(accession, prefix) && !IsScaffoldPrefix(accession, prefix_size)) {
            return false;
        }
    }

    for (string::const_iterator cur_char = accession.begin() + prefix_size; cur_char != accession.end(); ++cur_char) {
        if (!isdigit(*cur_char)) {
            return false;
        }
    }
    return true;
}

static void CheckNucBioseqs(const CSeq_entry& entry, CSeqEntryInfo& info, CSeqEntryCommonInfo& common_info)
{
    if (entry.IsSet()) {

        const CBioseq_set& bioseq_set = entry.GetSet();
        if (bioseq_set.IsSetClass() && bioseq_set.GetClass() == CBioseq_set::eClass_segset) {
            info.m_seqset = true;
        }

        if (bioseq_set.IsSetDescr()) {
            info.m_chromosome_subtype_status = CheckChromosome(bioseq_set.GetDescr());
        }

        for (auto cur_entry : bioseq_set.GetSeq_set()) {
            CheckNucBioseqs(*cur_entry, info, common_info);
        }
    }
    else {

        const CBioseq& bioseq = entry.GetSeq();
        if (bioseq.IsAa() && GetParams().IsVDBMode()) {
            ++info.m_num_of_prot_seq;
        }

        if (bioseq.IsSetDescr()) {
            info.m_chromosome_subtype_status = CheckChromosome(bioseq.GetDescr());

            if (bioseq.IsNa()) {
                CheckKeywords(bioseq.GetDescr(), info);
            }
            else if (!info.m_has_gb_block) {
                info.m_has_gb_block = HasGbBlock(bioseq.GetDescr());
            }
        }

        if (bioseq.IsSetInst() && bioseq.GetInst().IsSetRepr() && bioseq.GetInst().GetRepr() == CSeq_inst::eRepr_seg) {
            info.m_seqset = true;
            return;
        }

        if (bioseq.IsNa()) {
            ++info.m_num_of_nuc_seq;
            CheckSecondaries(bioseq, info);
            info.m_cur_seqid = GetSeqIdStr(bioseq);
        }

        if (!GetParams().IgnoreGeneralIds()) {

            if (bioseq.IsSetId()) {

                string db_name,
                       obj_id;
                info.m_id_problem = LookThroughObjectIds(bioseq.GetId(), obj_id, db_name);
                if (!db_name.empty()) {
                    CheckDBName(db_name, bioseq.IsNa(), info, common_info);
                }

                if (!obj_id.empty()) {
                    info.m_object_ids.push_front(obj_id);
                }
            }
        }

        if (bioseq.IsNa()) {

            if (GetParams().GetUpdateMode() == eUpdateScaffoldsNew && info.m_seqid_state == eSeqIdOK) {

                if (!bioseq.IsSetId() || bioseq.GetId().empty()) {
                    info.m_id_problem = eIdNoDbTag;
                }
                else {
                    CheckSeqIdStatus(bioseq.GetId(), info.m_seqid_type, info.m_seqid_state);
                }
            }

            if (GetParams().IsAccessionAssigned()) {
                if (bioseq.IsSetId()) {

                    for (auto& id : bioseq.GetId()) {
                        const CTextseq_id* text_id = id->GetTextseq_Id();
                        if (text_id != nullptr && text_id->IsSetAccession()) {

                            ++info.m_num_of_accsessions;

                            if (id->Which() != GetParams().GetIdChoice()) {
                                ERR_POST_EX(0, 0, Error << "One or more records have mismatching accession(s) and Seq-id type(s). First appearance: accession is \"" << text_id->GetAccession() <<
                                            "\", Seq-id type = " << id->Which() << ".");

                                info.m_seqid_state = eSeqIdDifferent;
                                // TODO rejection (originally ERR_POST has 'Reject' message type)
                            }

                            if (info.m_seqid_state != eSeqIdOK) {
                                break;
                            }

                            if (info.m_seqid_type == CSeq_id::e_not_set) {
                                info.m_seqid_type = id->Which();
                            }
                            else if (info.m_seqid_type != id->Which()) {
                                info.m_seqid_state = eSeqIdDifferent;
                            }

                            if (!IsAccessionValid(text_id->GetAccession())) {
                                info.m_bad_accession = true;
                                break;
                            }

                            common_info.m_acc_assigned.push_front(text_id->GetAccession());
                        }
                    }
                }
            }

            CheckMolecule(bioseq, info);
        }
    }
}

bool CheckSeqEntry(const CSeq_entry& entry, const string& file, CSeqEntryInfo& info, CSeqEntryCommonInfo& common_info)
{
    bool ret = true;

    CheckNucBioseqs(entry, info, common_info);

    if (GetParams().IsTsa() && GetParams().GetFixTech() == eNoFix && info.m_biomol_state == eBiomolDiffers) {
        ret = false;
    }

    if (!GetParams().IsTpa() && info.m_has_tpa_keyword) {
        ERR_POST_EX(0, 0, Error << "One or more non-TPA WGS record from \"" << file << "\" has a special TPA keyword present, which is prohibited. Cannot proceed.");
        ret = false;
    }
    else if (GetParams().IsTpa() && !info.m_has_tpa_keyword && GetParams().GetTpaKeyword().empty()) {
        ERR_POST_EX(0, 0, Error << "One or more TRA WGS reassembly record from \"" << file << "\" is missing required TPA keyword. Cannot proceed.");
        ret = false;
    }

    if (info.m_chromosome_subtype_status != eChromosomeSubtypeValid) {

        string err_str = "Empty or missing \"chromosome\" SubSource for one or more chromosomal scaffold(s). File \"" + file + "\".";
        if (info.m_chromosome_subtype_status != eChromosomeSubtypeMissing) {
            err_str += " Cannot proceed.";
            ERR_POST_EX(0, 0, Critical << err_str);
            ret = false;
        }
        else {
            ERR_POST_EX(0, 0, Error << err_str);
        }
    }

    if (info.m_secondary_accessions && !GetParams().IsSecondaryAccsAllowed()) {
        ERR_POST_EX(0, 0, Critical << "Found secondary accessions in Seq-entry from \"" << file << "\", which is prohibited. Cannot proceed.");
        ret = false;
    }

    if (info.m_hist_secondary_differs) {
        ERR_POST_EX(0, 0, Critical << "Mismatching sets of accessions in extra-accessions and Seq-hist blocks are not allowed. File \"" << file << "\". Cannot proceed.");
        ret = false;
    }

    if (info.m_seqset) {
        ERR_POST_EX(0, 0, Critical << "Input Seq-entry from \"" << file << "\" contains segmented set. Cannot proceed.");
        ret = false;
    }

    if (info.m_num_of_nuc_seq > 1) {
        ERR_POST_EX(0, 0, Critical << "Input Seq-entry from \"" << file << "\" contains more than one nucleic Bioseq. Cannot proceed.");
        ret = false;
    }

    bool fix_tech_mol_biomol_not_set = (GetParams().GetFixTech() & eFixMolBiomol) == 0;
    if (fix_tech_mol_biomol_not_set || !GetParams().IsTsa()) {
        
        if (info.m_bad_tech) {
            ERR_POST_EX(0, 0, (fix_tech_mol_biomol_not_set ? Critical : Warning) << "Input Seq-entry with Seq-id \"" << info.m_cur_seqid << "\" has missing or incorrect Molinfo.tech.");
            if (fix_tech_mol_biomol_not_set)
                ret = false;
        }

        if (info.m_bad_mol) {
            ERR_POST_EX(0, 0, (fix_tech_mol_biomol_not_set ? Critical : Warning) << "Input Seq-entry with Seq-id \"" << info.m_cur_seqid << "\" has missing or incorrect Seq-inst.mol.");
            if (fix_tech_mol_biomol_not_set)
                ret = false;
        }
    }

    if (info.m_bad_biomol) {

        bool severity_reject = false,
             report = true;

        if (!GetParams().IsTsa()) {
            severity_reject = fix_tech_mol_biomol_not_set;
        }
        else if (GetParams().GetFixTech() == 0)
            severity_reject = true;
        else
            report = false;

        if (report) {
            ERR_POST_EX(0, 0, (severity_reject ? Critical : Warning) << "Input Seq-entry with Seq-id \"" << info.m_cur_seqid << "\" has missing or incorrect Molinfo.biomol.");
            if (severity_reject)
                ret = false;
        }
    }

    if (!info.m_diff_dbname.empty()) {
        ERR_POST_EX(0, 0, Critical << "Dbname \"" << info.m_diff_dbname << "\" from general id Seq-id differs from the others. Submission = \"" << file << "\".");
        ret = false;
    }

    if (GetParams().GetUpdateMode() == eUpdateScaffoldsNew) {

        if (info.m_seqid_state == eSeqIdMultiple)
        {
            ERR_POST_EX(0, 0, Critical << "One or more scaffolds Bioseqs from submission \"" << file << "\" have more than one Seq-id.");
            ret = false;
        }
        else if (info.m_seqid_state == eSeqIdIncorrect)
        {
            ERR_POST_EX(0, 0, Critical << "One or more scaffolds Bioseqs from submission \"" << file << "\" have incorrect Seq-id type.");
            ret = false;
        }

        if (info.m_id_problem == eIdNoDbTag && GetParams().IgnoreGeneralIds()) {
            ERR_POST_EX(0, 0, Critical << "Required general or local Seq-id from Seq-entry is missing or incorrect. Submission = \"" << file << "\".");
            ret = false;
        }
    }

    if (!GetParams().IgnoreGeneralIds()) {

        if (info.m_dbname_problem != eDBNameNoProblem && GetParams().IsReplaceDBName()) {

            string dbname = info.m_dbname;
            if (dbname.empty()) {
                dbname = "[unknown]";
            }

            if (info.m_dbname_problem == eDBNameBadNucDB && !common_info.m_nuc_warn) {

                string correct_dbname = GetParams().IsChromosomal() ? GetParams().GetProjAccStr() : GetParams().GetProjAccVerStr();
                ERR_POST_EX(0, 0, Critical << "One or more nucleic Bioseqs have incorrect dbname supplied: \"" << dbname << "\". Must be \"" << correct_dbname << "\". Submission = \"" << file << "\".");
                common_info.m_nuc_warn = true;
            }

            if (info.m_dbname_problem == eDBNameBadProtDB && !common_info.m_prot_warn) {
                ERR_POST_EX(0, 0, Critical << "One or more protein Bioseqs have incorrect dbname supplied: \"" << dbname << "\". Must be \"" << GetParams().GetProjAccVerStr() << "\". Submission = \"" << file << "\".");
                common_info.m_prot_warn = true;
            }
            ret = false;
        }

        if (info.m_id_problem == eIdNoDbTag) {
            ERR_POST_EX(0, 0, Critical << "Required general or local Seq-id from Seq-entry is missing or incorrect. Submission = \"" << file << "\".");
            ret = false;
        }

        if (info.m_id_problem == eIdManyGeneralIds) {
            ERR_POST_EX(0, 0, Critical << "One or more Bioseqs from submission \"" << file << "\" have more than one general or local ids.");
            ret = false;
        }

        if (GetParams().GetUpdateMode() == eUpdateScaffoldsNew && info.m_seqid_state == eSeqIdDifferent) {
            ERR_POST_EX(0, 0, Critical << "One or more scaffolds Bioseqs from submission \"" << file << "\" have different Seq-id types.");
            ret = false;
        }
    }

    if (GetParams().IsAccessionAssigned()) {

        if (info.m_seqid_state != eSeqIdOK) {
            if (info.m_seqid_state == eSeqIdDifferent) {
                ERR_POST_EX(0, 0, Critical << "Seq-entry from submission \"" << file << "\" has different types of accessions in Seq-id block.");
            }
            ret = false;
        }

        if (info.m_num_of_accsessions == 0) {
            ERR_POST_EX(0, 0, Error << "Submission \"" << file << "\" is lacking accession.");
            ret = false;
        }
        else if (info.m_num_of_accsessions > 1) {
            ERR_POST_EX(0, 0, Critical << "Seq-entry from submission \"" << file << "\" has more than one accession.");
            ret = false;
        }

        if (info.m_bad_accession) {
            ERR_POST_EX(0, 0, Critical << "Seq-entry from submission \"" << file << "\" has incorrect accession prefix+version.");
            ret = false;
        }
    }

    return ret;
}

static bool GetDate(const CSeq_descr::Tdata& descrs, CSeqdesc::E_Choice choice, CDate& date)
{
    auto descr_date = find_if(descrs.begin(), descrs.end(), [choice](const CRef<CSeqdesc>& desc){ return desc->Which() == choice; });
    if (descr_date != descrs.end()) {
        const CDate& date_found = choice == CSeqdesc::e_Create_date ? (*descr_date)->GetCreate_date() : (*descr_date)->GetUpdate_date();
        date.Assign(date_found);
    }
    return descr_date != descrs.end();
}

static bool LookForDate(const CSeq_entry& entry, CSeqdesc::E_Choice choice, CDate& date)
{
    const CSeq_descr* descrs = nullptr;
    if (GetDescr(entry, descrs) && descrs->IsSet()) {
        if (GetDate(descrs->Get(), choice, date)) {
            return true;
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.GetSet().GetSeq_set()) {
            if (LookForDate(*cur_entry, choice, date)) {
                return true;
            }
        }
    }

    return false;
}

EDateIssues CheckDates(const CSeq_entry& entry, CSeqdesc::E_Choice choice, CDate& date)
{
    EDateIssues ret = eDateMissing;

    CDate cur_date;
    if (LookForDate(entry, choice, cur_date)) {
        ret = eDateNoIssues;

        if (date.Which() == CDate::e_not_set) {
            date.Assign(cur_date);
        }
        else if (date.Compare(cur_date) != CDate::eCompare_same) {
            date.Reset();
            ret = eDateDiff;
        }
    }

    return ret;
}

static void StripAuthorsFromArticle(CCit_art& article)
{
    if (article.IsSetAuthors() && article.GetAuthors().IsSetNames() && article.GetAuthors().GetNames().IsStd()) {

        auto& std_names = article.SetAuthors().SetNames().SetStd();
        for (auto name = std_names.begin(); name != std_names.end();) {
            if ((*name)->IsSetName() && !(*name)->GetName().IsConsortium()) {
                name = std_names.erase(name);
            }
            else {
                ++name;
            }
        }
    }
}

void StripAuthors(CSeq_entry& entry)
{
    if (entry.IsSeq() && !entry.GetSeq().IsNa()) {
        return;
    }

    if (entry.IsSetDescr() && entry.GetDescr().IsSet()) {

        for (auto& descr : entry.SetDescr().Set()) {
            if (descr->IsPub() && descr->GetPub().IsSetPub() && descr->GetPub().GetPub().IsSet()) {

                for (auto& pub : descr->SetPub().SetPub().Set()) {

                    if (pub->IsArticle()) {
                        StripAuthorsFromArticle(pub->SetArticle());
                    }
                }
            }
        }
    }

    if (entry.IsSet() && entry.GetSet().IsSetSeq_set()) {

        for (auto& cur_entry : entry.SetSet().SetSeq_set()) {
            StripAuthors(*cur_entry);
        }
    }
}

}
