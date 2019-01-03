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
* Authors:  Justin Foley
*
* File Description:
*
* ===========================================================================
*/
#include <ncbi_pch.hpp>
#include <corelib/ncbistd.hpp>
#include <objects/seqset/Bioseq_set.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_descr.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seq/MolInfo.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_hist.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/Dbtag.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gene_ref.hpp>
#include <objects/seqfeat/Prot_ref.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqblock/GB_block.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/pub/Pub.hpp>

#include <objtools/logging/message.hpp>
#include <objtools/logging/listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <objtools/readers/mod_error.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cassert>

#include "mod_info.hpp"
#include "descr_mod_apply.hpp"
#include "feature_mod_apply.hpp"

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CModValueAndAttrib::CModValueAndAttrib(const string& value) : mValue(value) {}


CModValueAndAttrib::CModValueAndAttrib(const char* value) : mValue(value) {}


void CModValueAndAttrib::SetValue(const string& value) 
{
    mValue = value;
}


void CModValueAndAttrib::SetAttrib(const string& attrib)
{
    mAttrib = attrib;
}


bool CModValueAndAttrib::IsSetAttrib(void) const
{
    return !(mAttrib.empty());
}

const string& CModValueAndAttrib::GetValue(void) const
{
    return mValue;
}


CModData::CModData(const string& name) : mName(name) {}

CModData::CModData(const string& name, const string& value) : mName(name), mValue(value) {}


void CModData::SetName(const string& name) 
{
    mValue = name;
}


void CModData::SetValue(const string& value) 
{
    mValue = value;
}


void CModData::SetAttrib(const string& attrib)
{
    mAttrib = attrib;
}


bool CModData::IsSetAttrib(void) const
{
    return !(mAttrib.empty());
}


const string& CModData::GetName(void) const
{
    return mName;
}


const string& CModData::GetValue(void) const
{
    return mValue;
}


const string& CModData::GetAttrib(void) const
{
    return mAttrib;
}


const string& CModValueAndAttrib::GetAttrib(void) const
{
    return mAttrib;
}


static bool s_IsUserType(const CUser_object& user_object, const string& type)
{
    return (user_object.IsSetType() &&
            user_object.GetType().IsStr() &&
            user_object.GetType().GetStr() == type);
}


void CModParser::Apply(const CBioseq& bioseq, TMods& mods) 
{
    x_ImportSeqInst(bioseq.GetInst(), mods);

    x_ImportDescriptors(bioseq, mods);

    if (bioseq.IsSetAnnot()) {
        for (const auto& pAnnot : bioseq.GetAnnot()) {
            if (pAnnot) {
                x_ImportFeatureModifiers(*pAnnot, mods);
            }
        }
    }
}


void CModParser::x_ImportDescriptors(const CBioseq& bioseq, TMods& mods)
{
    if (bioseq.GetParentSet() &&
        bioseq.GetParentSet()->IsSetClass() &&
        bioseq.GetParentSet()->GetClass() == CBioseq_set::eClass_nuc_prot &&
        bioseq.GetParentSet()->IsSetDescr()) 
    {
        for (const auto& pDesc : bioseq.GetParentSet()->GetDescr().Get()) {
            if (pDesc) {
                x_ImportDesc(*pDesc, mods);
            }
        } 

        // MolInfo is on the sequence itself
        if (bioseq.IsSetDescr()) {
            for (const auto& pDesc : bioseq.GetDescr().Get()) {
                if (pDesc && 
                    pDesc->IsMolinfo()) {
                    x_ImportMolInfo(pDesc->GetMolinfo(), mods);
                }
            }
        }
    }
    else 
    if (bioseq.IsSetDescr()) {
        for (const auto& pDesc : bioseq.GetDescr().Get()) {
            if (pDesc) {
                x_ImportDesc(*pDesc, mods);
            }
        }
    }
}


void CModParser::x_ImportDesc(const CSeqdesc& desc, TMods& mods)
{

    if (desc.IsUser()) { // DBLink, GenomeProjects, TpaAssembly
        x_ImportUserObject(desc.GetUser(), mods);
    }
    else
    if (desc.IsSource()) {
        x_ImportBioSource(desc.GetSource(), mods);
    }
    else
    if (desc.IsGenbank()) {
        x_ImportGBblock(desc.GetGenbank(), mods);
    }
    else 
    if (desc.IsMolinfo()) {
        x_ImportMolInfo(desc.GetMolinfo(), mods);
    }
    else 
    if (desc.IsPub()) {
        x_ImportPMID(desc.GetPub(), mods);
    }
    else
    if (desc.IsComment()) {
        mods.emplace("comment", desc.GetComment());
    }
}


void CModParser::x_ImportSeqInst(const CSeq_inst& seq_inst, TMods& mods)
{
    if (seq_inst.IsSetTopology()) {
        const auto& topology = s_TopologyEnumToString.at(seq_inst.GetTopology());
        mods.emplace("topology", topology);
    }

    if (seq_inst.IsSetMol()) {
        const auto& molecule = s_MolEnumToString.at(seq_inst.GetMol());
        mods.emplace("molecule", molecule);
    }

    if (seq_inst.IsSetStrand()) {
        const auto& strand = s_StrandEnumToString.at(seq_inst.GetStrand());
        mods.emplace("strand", strand);
    }

    if (seq_inst.IsSetHist()) {
        x_ImportHist(seq_inst.GetHist(),mods);
    }
}


void CModParser::x_ImportHist(const CSeq_hist& seq_hist, TMods& mods) 
{
    if (seq_hist.IsSetReplaces() &&
        seq_hist.GetReplaces().IsSetIds()) {
        const bool with_version = true;
        for (const auto& pSeqId : seq_hist.GetReplaces().GetIds()) {
            mods.emplace("secondary-accession", pSeqId->GetSeqIdString(with_version));
        }
    }
}


void CModParser::x_ImportUserObject(const CUser_object& user_object, TMods& mods)
{
    if (user_object.IsDBLink()) {
        x_ImportDBLink(user_object, mods);
    }
    else
    if (s_IsUserType(user_object, "GenomeProjectsDB")) {
        x_ImportGenomeProjects(user_object, mods);
    }
    else
    if (s_IsUserType(user_object, "TpaAssembly")) {
        x_ImportTpaAssembly(user_object, mods);
    }
}


void CModParser::x_ImportBioSource(const CBioSource& biosource, TMods& mods)
{
    if (biosource.IsSetGenome()) {
        auto e_genome = static_cast<CBioSource::EGenome>(biosource.GetGenome());
        assert(s_GenomeEnumToString.find(e_genome) != s_GenomeEnumToString.end());
        const auto& genome_string = s_GenomeEnumToString[e_genome]; 
        mods.emplace("location", genome_string);
    }

    if (biosource.IsSetOrigin()) {
        auto e_origin = static_cast<CBioSource::EOrigin>(biosource.GetOrigin());
        assert(s_OriginEnumToString.find(e_origin) != s_OriginEnumToString.end());
        const auto& origin_string = s_OriginEnumToString[e_origin];
        mods.emplace("origin", origin_string);
    }
    if (biosource.IsSetIs_focus()) {
        mods.emplace("focus", "true");
    }

    // x_ImportPCRPrimer(...);
    //
    if (biosource.IsSetSubtype()) {
        for (const auto& pSubSource : biosource.GetSubtype()) {
            if (pSubSource) {
                x_ImportSubSource(*pSubSource, mods);
            }
        }
    }
    x_ImportOrgRef(biosource.GetOrg(), mods);
}




void CModParser::x_ImportSubSource(const CSubSource& subsource, TMods& mods)
{
    static const auto s_SubSourceEnumToString = s_GetReverseMap(s_SubSourceStringToEnum);
    const auto& subtype = s_SubSourceEnumToString.at(subsource.GetSubtype());
    const auto& name = subsource.GetName();
    mods.emplace(subtype, name);
}


void CModParser::x_ImportOrgRef(const COrg_ref& org_ref, TMods& mods)
{
    if (org_ref.IsSetTaxname()) {
        mods.emplace("taxname", org_ref.GetTaxname());
    }
    if (org_ref.IsSetCommon()) {
        mods.emplace("common", org_ref.GetCommon());
    }
    if (org_ref.IsSetOrgname()) {
        const auto& org_name = org_ref.GetOrgname();
        x_ImportOrgName(org_name, mods);
    }

    if (org_ref.IsSetDb()) {
        for(const auto& pDbtag : org_ref.GetDb()) {
            string database = pDbtag->GetDb();
            string tag = pDbtag->GetTag().IsStr() ?
                         pDbtag->GetTag().GetStr() :
                         NStr::IntToString(pDbtag->GetTag().GetId());
            mods.emplace("dbxref", database + ":" + tag);
        }
    }
}


void CModParser::x_ImportOrgName(const COrgName& org_name, TMods& mods)
{
    if (org_name.IsSetDiv()) {
        mods.emplace("division", org_name.GetDiv());
    }

    if (org_name.IsSetLineage()) {
        mods.emplace("lineage", org_name.GetLineage());
    }

    if (org_name.IsSetGcode()) {
        mods.emplace("gcode", NStr::IntToString(org_name.GetGcode()));
    }

    if (org_name.IsSetMgcode()) {
        mods.emplace("mgcode", NStr::IntToString(org_name.GetMgcode()));
    }

    if (org_name.IsSetPgcode()) {
        mods.emplace("pgcode", NStr::IntToString(org_name.GetPgcode()));
    }

    if (org_name.IsSetMod()) {
        for(const auto& pOrgMod : org_name.GetMod()) {
            if (pOrgMod) {
                x_ImportOrgMod(*pOrgMod, mods);
            }
        }
    }
}



void CModParser::x_ImportOrgMod(const COrgMod& org_mod, TMods& mods)
{
    static const auto s_OrgModEnumToString = s_GetReverseMap(s_OrgModStringToEnum);
    const auto& subtype = s_OrgModEnumToString.at(org_mod.GetSubtype());
    const auto& subname = org_mod.GetSubname();
    mods.emplace(subtype, subname);
}


void CModParser::x_ImportPMID(const CPubdesc& pub_desc, TMods& mods)
{  
    if (pub_desc.IsSetPub()) {
        for (const auto& pPub : pub_desc.GetPub().Get()) {
            if (pPub && pPub->IsPmid()) {
                const int pmid = pPub->GetPmid();
                mods.emplace("pmid", NStr::IntToString(pmid));
            }
        }
    }
}


void CModParser::x_ImportMolInfo(const CMolInfo& mol_info, TMods& mods)
{
    if (mol_info.IsSetBiomol()) {
        const string& moltype = s_BiomolEnumToString.at(mol_info.GetBiomol());
        mods.emplace("moltype", moltype);
    }
    if (mol_info.IsSetTech()) {
        const string& tech =  s_TechEnumToString.at(mol_info.GetTech());
        mods.emplace("tech", tech);
    }

    if (mol_info.IsSetCompleteness()) {
        const string& completeness = s_CompletenessEnumToString.at(mol_info.GetCompleteness());
        mods.emplace("completeness", completeness);
    }
}


void CModParser::x_ImportDBLink(const CUser_object& user_object, TMods& mods) 
{
    if (!user_object.IsDBLink()) {
        return;
    }

    static const map<string,string> label_to_mod_name =
    {{"BioProject", "bioproject"},
     {"BioSample", "biosample"},
     {"Sequence Read Archive", "sra"}};


    if (user_object.IsSetData()) {
        for (const auto& pUserField : user_object.GetData()) {
            if (pUserField && 
                pUserField->IsSetLabel() &&
                pUserField->GetLabel().IsStr()) {
                const auto& label = pUserField->GetLabel().GetStr();
                const auto it = label_to_mod_name.find(label);
                if (it != label_to_mod_name.end()) {
                    string vals = "";
                    size_t count = 0;
                    for (const auto& val : pUserField->GetData().GetStrs()) {
                        if (count++ > 0) {
                            vals += ",";
                        }
                        vals += val;
                    }
                    mods.emplace(it->second, vals);
                }
            }
        }
    }
}


void CModParser::x_ImportGenomeProjects(const CUser_object& user_object, TMods& mods)
{
    if (user_object.IsSetData()) {
        string projects = "";
        size_t count = 0;
        for (const auto& pField : user_object.GetData()) {
            if (pField && 
                pField->IsSetData() &&
                pField->GetData().IsFields()) {
                for (const auto& pSubfield : pField->GetData().GetFields()) {
                    if (pSubfield->IsSetLabel() &&
                        pSubfield->GetLabel().IsStr() &&
                        pSubfield->GetLabel().GetStr() == "ProjectID") {
                        if (pSubfield->GetData().IsInt()) {
                            const auto& id_string = 
                                NStr::IntToString(pSubfield->GetData().GetInt());
                            if (count++>0) {
                                projects += ",";
                            }
                            projects += id_string;
                        }
                    }
                }
            }
        }
        if (!projects.empty()) {
            mods.emplace("project", projects);
        }
    }
}


void CModParser::x_ImportTpaAssembly(const CUser_object& user_object, TMods& mods)
{
    // Maybe I don't need to extract primary accessions here
    // because these should also be on the bioseq - need to check this!
}


void CModParser::x_ImportGBblock(const CGB_block& gb_block, TMods& mods)
{
    if (gb_block.IsSetExtra_accessions()) {
        string accessions = "";
        size_t count = 0;
        for (const auto& accession : gb_block.GetExtra_accessions()) {
            if (count++ > 0) {
                accessions += ",";
            }    
            accessions += accession; 
        }
        mods.emplace("secondary-accession", accessions);
    }

    if (gb_block.IsSetKeywords()) {
        string keywords = "";
        size_t count = 0;
        for (const auto& keyword : gb_block.GetKeywords()) {
            if (count++ > 0) {
                keywords += ",";
            }
            keywords += keyword;
        }
        mods.emplace("keyword", keywords);
    }
}


void CModParser::x_ImportGene(const CGene_ref& gene_ref, TMods& mods) 
{
    if (gene_ref.IsSetLocus()) {
        mods.emplace("gene", gene_ref.GetLocus());
    }
    if (gene_ref.IsSetAllele()) {
        mods.emplace("allele", gene_ref.GetAllele());
    }
    if (gene_ref.IsSetSyn()) {
        for (const auto& synonym : gene_ref.GetSyn()) {
            mods.emplace("gene-synonym", synonym);
        }
    }
    if (gene_ref.IsSetLocus_tag()) {
        mods.emplace("locus-tag", gene_ref.GetLocus_tag());
    }
}


void CModParser::x_ImportProtein(const CProt_ref& prot_ref, TMods& mods)
{
    if (prot_ref.IsSetName()) {
        for (const auto& name : prot_ref.GetName()) {
            mods.emplace("protein", name);
        }
    }

    if (prot_ref.IsSetDesc()) {
        mods.emplace("protein-desc", prot_ref.GetDesc());
    }

    if (prot_ref.IsSetEc()) {
        for (const auto& ec_number : prot_ref.GetEc()) {
            mods.emplace("ec-number", ec_number);
        }
    }

    if (prot_ref.IsSetActivity()) {
        for (const auto& activity : prot_ref.GetActivity()) {
            mods.emplace("activity", activity);
        }
    }
}


void CModParser::x_ImportFeatureModifiers(const CSeq_annot& annot, TMods& mods)
{
    if (annot.IsFtable()) {
        for (const auto& pSeqFeat : annot.GetData().GetFtable()) {
            if (pSeqFeat && 
                pSeqFeat->IsSetData()) {

                if (pSeqFeat->GetData().IsGene()) {
                    x_ImportGene(pSeqFeat->GetData().GetGene(), mods);
                }
                else
                if (pSeqFeat->GetData().IsProt()) {
                    x_ImportProtein(pSeqFeat->GetData().GetProt(), mods);
                }
            }
        }
    }
}

/*
const char* CModReaderException::GetErrCodeString(void) const
{
    switch(GetErrCode()) {
        case eInvalidValue:            return "eInvalidValue";
        case eMultipleValuesForbidden: return "eMultipleValuesForbidden";
        case eUnknownModifier:         return "eUnknownModifier";
        default:                       return CException::GetErrCodeString();
    }
}
*/


const CModHandler::TNameMap CModHandler::sm_NameMap = 
{{"top","topology"},
 {"mol","molecule"},
 {"moltype", "mol-type"},
 {"fwd-pcr-primer-name", "fwd-primer-name"},
 {"fwd-pcr-primer-names", "fwd-primer-name"},
 {"fwd-primer-names", "fwd-primer-name"},
 {"fwd-pcr-primer-seq","fwd-primer-seq"},
 {"fwd-pcr-primer-seqs","fwd-primer-seq"},
 {"fwd-primer-seqs","fwd-primer-seq"},
 {"rev-pcr-primer-name", "rev-primer-name"},
 {"rev-pcr-primer-names", "rev-primer-name"},
 {"rev-primer-names", "rev-primer-name"},
 {"rev-pcr-primer-seq", "rev-primer-seq"},
 {"rev-pcr-primer-seqs", "rev-primer-seq"},
 {"rev-primer-seqs", "rev-primer-seq"},
 {"org", "taxname"},
 {"organism", "taxname"},
 {"div", "division"},
 {"notes", "note"},
 {"completedness", "completeness"},
 {"gene-syn", "gene-synonym"},
 {"genesyn", "gene-synonym"},
 {"genesynonym", "gene-synonym"},
 {"prot", "protein"},
 {"prot-desc", "protein-desc"},
 {"function", "activity"},
 {"secondary", "secondary-accession"},
 {"secondary-accessions", "secondary-accession"},
 {"keywords", "keyword"},
 {"primary", "primary-accession"},
 {"primary-accessions", "primary-accession"},
 {"projects", "project"},
 {"db-xref", "dbxref"},
 {"pubmed", "pmid"}
};




const CModHandler::TNameSet CModHandler::sm_DeprecatedModifiers
{
    "dosage",
    "transposon-name",
    "plastid-name",
    "insertion-seq-name",
    "old-lineage",
    "old-name"
};


const CModHandler::TNameSet CModHandler::sm_MultipleValuesForbidden = 
{
    "topology", // Seq-inst
    "molecule",
    "strand",
    "gene",  // Gene-ref
    "allele",
    "locus-tag",
    "protein-desc",// Protein-ref
    "mol-type", // MolInfo descriptor
    "tech",
    "completeness",
    "location", // Biosource descriptor
    "origin",
    "focus",
    "taxname", // Biosource - Org-ref
    "common",
    "lineage", // Biosource - Org-ref - OrgName
    "division",
    "gcode",
    "mgcode",
    "pgcode"
};


CModHandler::CModHandler(IObjtoolsListener* listener) 
    : m_pMessageListener(listener) {}





// Replace CModValueAndAttrib class with a CModData
// or CModifier class, which also has a name attribute. 
// Then, AddMods will take a list<CModInfo> 
void CModHandler::AddMods(const TModList& mods,
                          EHandleExisting handle_existing,
                          TModList& rejected_mods)
{
    rejected_mods.clear();

    unordered_set<string> current_set;
    TMods accepted_mods;

    for (const auto& mod : mods) {
        const auto& canonical_name = x_GetCanonicalName(mod.GetName());
        if (x_IsDeprecated(canonical_name)) {
            rejected_mods.push_back(mod);
            string message = mod.GetName() + " is deprecated - ignoring";
            x_PutMessage(message, eDiag_Warning);
            continue;
        }

        const auto allow_multiple_values = x_MultipleValuesAllowed(canonical_name);
        const auto first_occurrence = current_set.insert(canonical_name).second;

        if (!allow_multiple_values &&
            !first_occurrence) {
            rejected_mods.push_back(mod);
            string msg = mod.GetName() + " conflicts with other modifiers in set.";
            if (m_pMessageListener) {
                x_PutMessage(msg, eDiag_Error);
            }   
            else { 
                NCBI_THROW(CModReaderException, eMultipleValuesForbidden, msg);
            }
            continue;
        }
        accepted_mods[canonical_name].push_back(mod);
    }

    x_SaveMods(move(accepted_mods), handle_existing, m_Mods);
}


void CModHandler::x_SaveMods(TMods&& mods, EHandleExisting handle_existing, TMods& dest)
{
    if (handle_existing == eReplace) {
        dest = mods;
    }
    else
    if (handle_existing == ePreserve) {
        dest.insert(make_move_iterator(mods.begin()), 
                    make_move_iterator(mods.end()));
    }
    else
    if (handle_existing == eAppendReplace) {
        for (auto& mod_entry : mods) {
            const auto& canonical_name = mod_entry.first;
            auto& dest_mod_list = dest[canonical_name];
            if (x_MultipleValuesAllowed(canonical_name)){
                dest_mod_list.splice(
                        dest_mod_list.end(),
                        move(mod_entry.second));
            }
            else {
                dest_mod_list = move(mod_entry.second);
            }
        }
    }
    else 
    if (handle_existing == eAppendPreserve) {
        for (auto& mod_entry : mods) {
            const auto& canonical_name = mod_entry.first;
            auto& dest_mod_list = dest[canonical_name];
            if (dest_mod_list.empty()) {
                dest_mod_list = move(mod_entry.second);  
            }
            else 
            if (x_MultipleValuesAllowed(canonical_name)){
                dest_mod_list.splice(
                        dest_mod_list.end(),
                        move(mod_entry.second));
            }
        }
    }
}


bool CModHandler::x_MultipleValuesAllowed(const string& canonical_name)
{
    return (sm_MultipleValuesForbidden.find(canonical_name) ==
            sm_MultipleValuesForbidden.end());
}


const CModHandler::TMods& CModHandler::GetMods(void) const
{
    return m_Mods;
}


void CModHandler::Clear(void) 
{
    m_Mods.clear();
}


string CModHandler::x_GetCanonicalName(const string& name) const
{
    const auto& normalized_name = x_GetNormalizedString(name);
    const auto& it = sm_NameMap.find(normalized_name);
    if (it != sm_NameMap.end()) {
        return it->second;
    }

    return normalized_name;
}


bool CModHandler::x_IsDeprecated(const string& canonical_name)
{
    return (sm_DeprecatedModifiers.find(canonical_name) !=
            sm_DeprecatedModifiers.end());
}


string CModHandler::x_GetNormalizedString(const string& name) const
{
    string normalized_name = name;
    NStr::ToLower(normalized_name);
    NStr::TruncateSpacesInPlace(normalized_name);
    NStr::ReplaceInPlace(normalized_name, "_", "-");
    NStr::ReplaceInPlace(normalized_name, " ", "-");
    auto new_end = unique(normalized_name.begin(), 
                          normalized_name.end(),
                          [](char a, char b) { return ((a==b) && (a==' ')); });

    normalized_name.erase(new_end, normalized_name.end());
    NStr::ReplaceInPlace(normalized_name, " ", "-");

    return normalized_name;
}


void CModHandler::x_PutMessage(const string& message, 
                               EDiagSev severity)
{
    if (!m_pMessageListener || NStr::IsBlank(message)) {
        return;
    }

    m_pMessageListener->PutMessage(
        CObjtoolsMessage(message, severity));
}


void CModAdder::Apply(const CModHandler& mod_handler,
                      CBioseq& bioseq,
                      IObjtoolsListener* pMessageListener,
                      TSkippedMods& skipped_mods)
{
    Apply(mod_handler, bioseq, nullptr, pMessageListener, skipped_mods);
}


void CModAdder::Apply(const CModHandler& mod_handler,
                      CBioseq& bioseq,
                      const CSeq_loc* pFeatLoc,
                      IObjtoolsListener* pMessageListener,
                      TSkippedMods& skipped_mods)
{
    skipped_mods.clear();

    CDescrModApply descr_mod_apply(bioseq);
    CFeatModApply feat_mod_apply(bioseq);

    for (const auto& mod_entry : mod_handler.GetMods()) {
        try {
            if (descr_mod_apply.Apply(mod_entry)) {
                const string& mod_name = x_GetModName(mod_entry);
                if (mod_name == "secondary-accession"){
                    x_SetHist(mod_entry, bioseq.SetInst());
                }
                else if (mod_name == "mol-type") {
                    // mol-type appears before molecule in the default-ordered 
                    // map keys. Therefore, if both mol-type and molecule are 
                    // specified, molecule will take precedence over (or, more precisly, overwrite) 
                    // the information extracted from mol-type when setting Seq-inst::mol
                    x_SetMoleculeFromMolType(mod_entry, bioseq.SetInst());
                }
                continue;
            }

            if (x_TrySeqInstMod(mod_entry, bioseq.SetInst())) {
                continue;
            }

            if (feat_mod_apply.Apply(mod_entry)) {
                continue;
            }

            // Report unrecognised modifier
            string msg = "Unrecognized modifier: " + x_GetModName(mod_entry) + ".";
            NCBI_THROW(CModReaderException, eUnknownModifier, msg);
        }
        catch(const CModReaderException& e) {
            skipped_mods.insert(skipped_mods.end(),
                    mod_entry.second.begin(),
                    mod_entry.second.end());
            if (pMessageListener) {
                x_PutMessage(e.GetMsg(), eDiag_Warning, pMessageListener);
                continue;
            }
            throw; // rethrow e
        }
    }
}


void CModAdder::x_AssertSingleValue(const TModEntry& mod_entry)
{
    assert(mod_entry.second.size() == 1);
}


void CModAdder::x_ThrowInvalidValue(const CModData& mod_data,
                                    const string& add_msg)
{
    const auto& mod_name = mod_data.GetName();
    const auto& mod_value = mod_data.GetValue();
    string msg = mod_name + " modifier has invalid value: \"" +   mod_value + "\".";
    if (!NStr::IsBlank(add_msg)) {
        msg += " " + add_msg;
    }
    NCBI_THROW(CModReaderException, eInvalidValue, msg);
}


bool CModAdder::x_PutMessage(const string& message,
                            EDiagSev severity,
                            IObjtoolsListener* pMessageListener)
{
    if (!pMessageListener || NStr::IsBlank(message)) 
    {
        return false;
    }

    pMessageListener->PutMessage(
        CObjtoolsMessage(message, severity));

    return true;
}


bool CModAdder::x_PutError(const CModReaderException& exception,
                           IObjtoolsListener* pMessageListener)
{
    return true;
}


const string& CModAdder::x_GetModName(const TModEntry& mod_entry)
{
    return mod_entry.first;
}

const string& CModAdder::x_GetModValue(const TModEntry& mod_entry)
{
    x_AssertSingleValue(mod_entry);
    return mod_entry.second.front().GetValue();
}


bool CModAdder::x_TrySeqInstMod(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& mod_name = x_GetModName(mod_entry);

    if (mod_name == "strand") {
        x_SetStrand(mod_entry, seq_inst);
        return true;
    }

    if (mod_name == "molecule") {
        x_SetMolecule(mod_entry, seq_inst);
        return true;
    }

    if (mod_name == "topology") {
        x_SetTopology(mod_entry, seq_inst);
        return true;
    }

//   Note that we do not check for the 'secondary-accession' modifier here.
//   secondary-accession also modifies the GB_block descriptor
//   The check for secondary-accession and any resulting call 
//   to x_SetHist is performed before x_TrySeqInstMod 
//   is invoked. 

    return false;
}


void CModAdder::x_SetStrand(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_StrandStringToEnum.find(value);
    if (it == s_StrandStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
    }
    seq_inst.SetStrand(it->second);
}


void CModAdder::x_SetMolecule(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_MolStringToEnum.find(value);
    if (it == s_MolStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
    }
    seq_inst.SetMol(it->second);
}


void CModAdder::x_SetMoleculeFromMolType(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    auto it = s_BiomolStringToEnum.find(value);
    if (it == s_BiomolStringToEnum.end()) {
        // No need to report an error here.
        // The error is reported in x_SetMolInfoType
        return; 
    }
    auto mol =  s_BiomolEnumToMolEnum.at(it->second);
    seq_inst.SetMol(mol);
}


void CModAdder::x_SetTopology(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    const auto& value = x_GetModValue(mod_entry);
    const auto it = s_TopologyStringToEnum.find(value);
    if (it == s_TopologyStringToEnum.end()) {
        x_ThrowInvalidValue(mod_entry.second.front());
    }
    seq_inst.SetTopology(it->second);
}


void CModAdder::x_SetHist(const TModEntry& mod_entry, CSeq_inst& seq_inst)
{
    list<string> id_list;
    for (const auto& mod : mod_entry.second) {
        const auto& vals = mod.GetValue();
        list<CTempString> value_sublist;
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            string value = NStr::TruncateSpaces_Unsafe(val);
            if (value.length() >= 8 && 
                value.substr(0,8) == "ref_seq|") { 
                value.erase(3,4); // ref_seq| -> ref|
            }
            try {
                SSeqIdRange idrange(value);
                id_list.insert(id_list.end(), idrange.begin(), idrange.end());
            }
            catch (...) 
            {
                id_list.push_back(value);
            }
        }
    }

    if (id_list.empty()) {
        return;
    }

    list<CRef<CSeq_id>> secondary_ids;
    // try catch statement
    transform(id_list.begin(), id_list.end(), back_inserter(secondary_ids),
            [](const string& id_string) { return Ref(new CSeq_id(id_string)); });

    seq_inst.SetHist().SetReplaces().SetIds() = move(secondary_ids);
}



void CTitleParser::Apply(const CTempString& title, TModList& mods, string& remainder)
{
    mods.clear();
    remainder.clear();
    size_t start_pos = 0;
    while(start_pos < title.size()) {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = start_pos;
        if (x_FindBrackets(title, lb_pos, end_pos, eq_pos)) {
            if (lb_pos > start_pos) {
                remainder.append(NStr::TruncateSpaces_Unsafe(title.substr(start_pos, lb_pos-start_pos)));
            }
            if (eq_pos < end_pos) {
                CTempString name = NStr::TruncateSpaces_Unsafe(title.substr(lb_pos+1, eq_pos-(lb_pos+1)));
                CTempString value = NStr::TruncateSpaces_Unsafe(title.substr(eq_pos+1, end_pos-(eq_pos+1)));
                mods.emplace_back(name, value);
            }
            start_pos = end_pos+1;
        }
        else {
            return;
        }
    }
}


bool CTitleParser::HasMods(const CTempString& title) 
{
    size_t start_pos = 0;
    while (start_pos < title.size()) {
        size_t lb_pos, end_pos, eq_pos;
        lb_pos = start_pos;
        if (x_FindBrackets(title, lb_pos, end_pos, eq_pos)) {
            if (eq_pos < end_pos) {
                return true;
            }
            start_pos = end_pos+1;
        }
        else {
            return false;
        }
    }
    return false;
}


bool CTitleParser::x_FindBrackets(const CTempString& line, size_t& start, size_t& stop, size_t& eq_pos)
{ // Copied from CSourceModParser
    size_t i = start;
    bool found = false;

    eq_pos = CTempString::npos;
    const char* s = line.data() + start;

    int num_unmatched_left_brackets = 0;
    while (i < line.size())
    {
        switch (*s)
        {
        case '[':
            num_unmatched_left_brackets++;
            if (num_unmatched_left_brackets == 1)
            {
                start = i;
            }
            break;
        case '=':
            if (num_unmatched_left_brackets > 0 && eq_pos == CTempString::npos) {
                eq_pos = i;
            }
            break;
        case ']':
            if (num_unmatched_left_brackets == 1)
            {
                stop = i;
                return true;
            }
            else
            if (num_unmatched_left_brackets == 0) {
                return false;
            }
            else
            {
                num_unmatched_left_brackets--;
            }
        }
        i++; s++;
    }
    return false;
};



END_SCOPE(objects)
END_NCBI_SCOPE



