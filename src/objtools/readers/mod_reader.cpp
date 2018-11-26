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

#include <objtools/logging/listener.hpp>
#include <objtools/readers/mod_reader.hpp>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "mod_info.hpp"

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
        _ASSERT(s_GenomeEnumToString.find(e_genome) != s_GenomeEnumToString.end());
        const auto& genome_string = s_GenomeEnumToString[e_genome]; 
        mods.emplace("location", genome_string);
    }

    if (biosource.IsSetOrigin()) {
        auto e_origin = static_cast<CBioSource::EOrigin>(biosource.GetOrigin());
        _ASSERT(s_OriginEnumToString.find(e_origin) != s_OriginEnumToString.end());
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
            mods.emplace("EC-number", ec_number);
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






const CModHandler::TNameMap CModHandler::sm_NameMap = 
{{"top","topology"},
 {"mol","molecule"},
 {"mol-type", "moltype"},
 {"fwd-pcr-primer-name", "fwd-primer-name"},
 {"fwd-pcr-primer-seq"," fwd-primer-seq"},
 {"rev-pcr-primer-name", "rev-primer-name"},
 {"rev-pcr-primer-seq", "rev-primer-seq"},
 {"org", "organism"},
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


void CModHandler::AddMod(const string& name,
                         const string& value,
                         EHandleExisting handle_existing)
{
    const auto& canonical_name = x_GetCanonicalName(name);

    if (m_Mods.find(canonical_name) != m_Mods.end()) {
        if (handle_existing == ePreserve) { // Do nothing
            return;
        }
        if (handle_existing == eReplace) {
            m_Mods.erase(canonical_name);    
        }
        else { 
            const auto allow_multiple_values = 
                (sm_MultipleValuesForbidden.find(canonical_name) == 
                sm_MultipleValuesForbidden.end());

            if (allow_multiple_values == false) {
                if (handle_existing == eAppendPreserve) { // Do nothing
                    return;
                }
                // handle_existing == eAppendReplace
                m_Mods.erase(canonical_name);
            }
        }
    }

    m_Mods.emplace(canonical_name, value);
}


void CModHandler::AddMod(const string& name,
                         const CModValueAndAttrib& value_attrib,
                         EHandleExisting handle_existing)
{
    const auto& canonical_name = x_GetCanonicalName(name);

    if (m_Mods.find(canonical_name) != m_Mods.end()) {
        if (handle_existing == ePreserve) { // Do nothing
            return;
        }
        if (handle_existing == eReplace) {
            m_Mods.erase(canonical_name);    
        }
        else { 
            const auto allow_multiple_values = 
                (sm_MultipleValuesForbidden.find(canonical_name) == 
                sm_MultipleValuesForbidden.end());

            if (allow_multiple_values == false) {
                if (handle_existing == eAppendPreserve) { // Do nothing
                    return;
                }
                // handle_existing == eAppendReplace
                m_Mods.erase(canonical_name);
            }
        }
    }
    m_Mods.emplace(canonical_name, value_attrib);
}


void CModHandler::AddMods(const TMods& mods,
                          EHandleExisting handle_existing)
{
    unordered_set<string> previous_mods;
    for (const auto& mod : mods) {
        const auto& canonical_name = x_GetCanonicalName(mod.first);
        const auto allow_multiple_values = 
            (sm_MultipleValuesForbidden.find(canonical_name) == 
            sm_MultipleValuesForbidden.end());

        const auto first_mod_instance = previous_mods.insert(canonical_name).second;

        if (first_mod_instance ||
            allow_multiple_values) {
            if (allow_multiple_values) {
                if ((handle_existing == eAppendPreserve) ||
                    (handle_existing == eAppendReplace) ) {
                    m_Mods.emplace(canonical_name, mod.second);
                }
                else 
                if (m_Mods.find(canonical_name) == m_Mods.end()) {
                    m_Mods.emplace(canonical_name, mod.second);
                }
                else
                if ((handle_existing == eReplace) &&
                     first_mod_instance) {
                    m_Mods.erase(canonical_name);
                    m_Mods.emplace(canonical_name, mod.second);
                }
                else
                if (!first_mod_instance) {
                    m_Mods.emplace(canonical_name, mod.second);
                }
            }
            else { // first_mod_instance
                if (m_Mods.find(canonical_name) != m_Mods.end()) {
                    m_Mods.erase(canonical_name);
                }
                m_Mods.emplace(canonical_name, mod.second);
            }
        }
        else {
            if (m_pMessageListener) {
                // report an error
            }
            else {
                // throw an exception
            }
        }
    }
}


const CModHandler::TMods& CModHandler::GetMods(void) const
{
    return m_Mods;
}


void CModHandler::Clear(void) 
{
    m_Mods.clear();
}


string CModHandler::x_GetCanonicalName(const string& name)
{
    const auto& normalized_name = x_GetNormalizedName(name);
    const auto& it = sm_NameMap.find(normalized_name);
    if (it != sm_NameMap.end()) {
        return it->second;
    }
    return normalized_name;
}


string CModHandler::x_GetNormalizedName(const string& name) 
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


template<typename TMultimap>
struct SRangeGetter 
{
    using TIterator = typename TMultimap::const_iterator;
    using TRange = pair<TIterator,TIterator>;
    using TRanges = list<TRange>;

    static TRanges GetEqualRanges(const TMultimap& mod_map)  
    {
        TRanges ranges;
        if (!mod_map.empty()) {
            auto end_it = mod_map.cend();
            auto current_it = mod_map.cbegin();
            while (current_it != end_it) {
                auto next_it = ++current_it;
                auto current_key = current_it->first;
                while(next_it != end_it &&
                    next_it->first == current_key) {
                    ++next_it;
                }
                ranges.emplace_back(current_it, next_it);
                current_it = next_it;
            }
        }
        return ranges;
    }
};



// Add subtype and subsource references
class CDescrCache 
{
public:

    struct SDescrContainer_Base {
        virtual ~SDescrContainer_Base(void) = default;
        virtual bool IsSet(void) const = 0;
        virtual CSeq_descr& SetDescr(void) = 0;
    };

    template<class TObject>
    class CDescrContainer : public SDescrContainer_Base
    {
    public:
        CDescrContainer(TObject& object) : m_Object(object) {}

        bool IsSet(void) const {
            return m_Object.IsSetDescr();
        }

        CSeq_descr& SetDescr(void) {
            return m_Object.SetDescr();
        }

    private:
        TObject& m_Object;
    };

    using TDescrContainer = SDescrContainer_Base;
    using TSubtype = CBioSource::TSubtype;
    using TOrgMods = COrgName::TMod;
    using TPCRReactionSet = CPCRReactionSet;


    CDescrCache(unique_ptr<TDescrContainer> pDescrContainer);

    CDescrCache(CBioseq& bioseq);

    CUser_object& SetDBLink(void);
    CUser_object& SetTpaAssembly(void);
    CUser_object& SetGenomeProjects(void);

    CGB_block& SetGBblock(void);
    CMolInfo& SetMolInfo(void);
    CBioSource& SetBioSource(void);

    string& SetComment(void);
    CPubdesc& SetPubdesc(void);

    TSubtype& SetSubtype(void);
    TOrgMods& SetOrgMods(void);
    CPCRReactionSet& SetPCR_primers(void);

    
private:
    enum EChoice : size_t {
        eDBLink = 1,
        eTpa = 2,
        eGenomeProjects = 3,
        eMolInfo = 4,
        eGBblock = 5,
        eBioSource = 6,
    };

    void x_SetUserType(const string& type, CUser_object& user_object);

    CSeqdesc& x_SetDescriptor(const EChoice eChoice, 
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create);

    CSeqdesc& x_SetDescriptor(const EChoice eChoice, 
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create,
                              TDescrContainer* pDescrContainer);

    TSubtype* m_pSubtype = nullptr;
    TOrgMods* m_pOrgMods = nullptr;
    CPCRReactionSet* m_pPCRReactionSet = nullptr;
    bool m_FirstComment = true;
    bool m_FirstPubdesc = true;
    using TMap = unordered_map<EChoice, CRef<CSeqdesc>, hash<underlying_type<EChoice>::type>>;
    TMap m_Cache;


    TDescrContainer* m_pPrimaryContainer;
    unique_ptr<TDescrContainer> m_pDescrContainer;
    unique_ptr<TDescrContainer> m_pNucProtSetContainer;
    unique_ptr<TDescrContainer> m_pBioseqContainer;
};


CDescrCache::CDescrCache(CBioseq& bioseq)
    : m_pBioseqContainer(new CDescrContainer<CBioseq>(bioseq))
{
    auto pParentSet = bioseq.GetParentSet();

    if (pParentSet &&
        pParentSet->IsSetClass() &&
        pParentSet->GetClass() == CBioseq_set::eClass_nuc_prot)
    {
        auto& bioseq_set = const_cast<CBioseq_set&>(*pParentSet);
        m_pNucProtSetContainer.reset(new CDescrContainer<CBioseq_set>(bioseq_set));
        m_pPrimaryContainer = m_pNucProtSetContainer.get();
    }
    else {
        m_pPrimaryContainer = m_pBioseqContainer.get();
    }

    _ASSERT(m_pPrimaryContainer);
}


class CFeatureCache
{
public:
    CFeatureCache(CBioseq& bioseq) : m_Bioseq(bioseq) {}
    virtual ~CFeatureCache() = default;

    CSeq_feat& SetSeq_feat(void);

protected:
    virtual bool VerifyFeature(const CSeq_feat& feat) = 0;
    virtual CRef<CSeqFeatData> CreateSeqFeatData(void) = 0;
private:

    CRef<CSeq_loc> x_GetWholeSeqLoc(void);

    CBioseq& m_Bioseq;
    CRef<CSeq_feat> m_pFeature;

};



CRef<CSeq_loc> CFeatureCache::x_GetWholeSeqLoc(void)
{
    auto pSeqLoc = Ref(new CSeq_loc());
    auto pSeqId = FindBestChoice(m_Bioseq.GetId(), CSeq_id::BestRank);
    if (pSeqId) {
        pSeqLoc->SetWhole(*pSeqId);
    }
    return pSeqLoc;
}


CSeq_feat& CFeatureCache::SetSeq_feat()
{
    if (m_pFeature) {
        return *m_pFeature;
    }

    if (m_Bioseq.IsSetAnnot()) {
        for (auto& pAnnot : m_Bioseq.SetAnnot()) {
            if (pAnnot && pAnnot->IsFtable()) {
                for (auto pSeqfeat : pAnnot->SetData().SetFtable()) {
                    if (pSeqfeat &&
                        VerifyFeature(*pSeqfeat)) {
                        m_pFeature = pSeqfeat;
                        return *m_pFeature;
                    }
                }
            }
        }
    }

    m_pFeature = Ref(new CSeq_feat());
    m_pFeature->SetData(*CreateSeqFeatData());
    m_pFeature->SetLocation(*x_GetWholeSeqLoc());

    auto pAnnot = CRef<CSeq_annot>(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(m_pFeature);
    m_Bioseq.SetAnnot().push_back(pAnnot);
    return *m_pFeature;
}


class CGeneRefCache : public CFeatureCache
{
public:
    CGeneRefCache(CBioseq& bioseq) : CFeatureCache(bioseq) {}
private: 
    bool VerifyFeature(const CSeq_feat& seq_feat) {
        return (seq_feat.IsSetData() &&
                seq_feat.GetData().IsGene());
    }

    CRef<CSeqFeatData> CreateSeqFeatData() {
        auto pData = Ref(new CSeqFeatData());
        pData->SetGene();
        return pData;
    }
};


class CProteinRefCache : public CFeatureCache
{
public:
    CProteinRefCache(CBioseq& bioseq) : CFeatureCache(bioseq) {}
private:
    bool VerifyFeature(const CSeq_feat& seq_feat) {
        return (seq_feat.IsSetData() &&
                seq_feat.GetData().IsProt());
    }

    CRef<CSeqFeatData> CreateSeqFeatData() {
        auto pData = Ref(new CSeqFeatData());
        pData->SetProt();
        return pData;
    }
};


template<class TObject>
class CDescrContainer : public CDescrCache::TDescrContainer
{
public:
    CDescrContainer(TObject& object) : m_Object(object) {}

    bool IsSet(void) const {
        return m_Object.IsSetDescr();
    }

    CSeq_descr& SetDescr(void) {
        return m_Object.SetDescr();
    }

private:
    TObject& m_Object;
};


void CDescrCache::x_SetUserType(const string& type, 
                                     CUser_object& user_object) 
{
    user_object.SetType().SetStr(type);
} 


CPubdesc& CDescrCache::SetPubdesc()
{
    _ASSERT(m_pDescrContainer);

    if (m_FirstPubdesc) {
        if (m_pDescrContainer->IsSet()) {
            m_pDescrContainer->SetDescr().Set().remove_if([](const CRef<CSeqdesc>& pDesc) { return (pDesc && pDesc->IsPub()); });
        }
        m_FirstPubdesc = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_pDescrContainer->SetDescr().Set().push_back(pDesc);
    return pDesc->SetPub();
}


string& CDescrCache::SetComment()
{
    _ASSERT(m_pDescrContainer);

    if (m_FirstComment) {
        if (m_pDescrContainer->IsSet()) {
            m_pDescrContainer->SetDescr().Set().remove_if([](const CRef<CSeqdesc>& pDesc) { return (pDesc && pDesc->IsComment()); });
        }
        m_FirstComment = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_pDescrContainer->SetDescr().Set().push_back(pDesc);
    return pDesc->SetComment();
}


CUser_object& CDescrCache::SetDBLink() 
{
    return x_SetDescriptor(eDBLink, 
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && desc.GetUser().IsDBLink());
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetUser().SetObjectType(CUser_object::eObjectType_DBLink);
            return pDesc;
        }).SetUser();
}

CUser_object& CDescrCache::SetTpaAssembly()
{
    return x_SetDescriptor(eTpa,
        [this](const CSeqdesc& desc) {
            return (desc.IsUser() && s_IsUserType(desc.GetUser(), "TpaAssembly"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("TpaAssembly", pDesc->SetUser());
            return pDesc;
        }
    ).SetUser();
}

CUser_object& CDescrCache::SetGenomeProjects()
{
        return x_SetDescriptor(eGenomeProjects,
        [this](const CSeqdesc& desc) {
            return (desc.IsUser() && s_IsUserType(desc.GetUser(), "GenomeProjectsDB"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("GenomeProjectsDB", pDesc->SetUser());
            return pDesc;
        }
    ).SetUser();
}


CGB_block& CDescrCache::SetGBblock()
{
    return x_SetDescriptor(eGBblock, 
        [](const CSeqdesc& desc) { 
            return desc.IsGenbank(); 
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetGenbank();
            return pDesc;
        }
    ).SetGenbank();
}


CMolInfo& CDescrCache::SetMolInfo()
{   // MolInfo is a Bioseq descriptor 
    return x_SetDescriptor(eMolInfo,
        [](const CSeqdesc& desc) { 
            return desc.IsMolinfo(); 
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetMolinfo();
            return pDesc;
        },
        m_pBioseqContainer.get()
    ).SetMolinfo();
}


CBioSource& CDescrCache::SetBioSource()
{
    return x_SetDescriptor(eBioSource,
            [](const CSeqdesc& desc) {
                return desc.IsSource();
            },
            []() {
                auto pDesc = Ref(new CSeqdesc());
                pDesc->SetSource();
                return pDesc;
            }
        ).SetSource();
}


CDescrCache::TSubtype& CDescrCache::SetSubtype()
{
    if (!m_pSubtype) {
        m_pSubtype = &(SetBioSource().SetSubtype());
        m_pSubtype->clear();
    }

    return *m_pSubtype;
}


CDescrCache::TOrgMods& CDescrCache::SetOrgMods()
{
    if (!m_pOrgMods) {
        m_pOrgMods = &(SetBioSource().SetOrg().SetOrgname().SetMod());
        m_pOrgMods->clear();
    }

    return *m_pOrgMods;
}


CPCRReactionSet& CDescrCache::SetPCR_primers()
{
    if (!m_pPCRReactionSet) {
        m_pPCRReactionSet = &(SetBioSource().SetPcr_primers());
        m_pPCRReactionSet->Set().clear();   
    }
    return *m_pPCRReactionSet;
}


CSeqdesc& CDescrCache::x_SetDescriptor(const EChoice eChoice, 
                                       function<bool(const CSeqdesc&)> f_verify,
                                       function<CRef<CSeqdesc>(void)> f_create)
{
    return x_SetDescriptor(eChoice, f_verify, f_create, m_pPrimaryContainer);
}


CSeqdesc& CDescrCache::x_SetDescriptor(const EChoice eChoice, 
                                       function<bool(const CSeqdesc&)> f_verify,
                                       function<CRef<CSeqdesc>(void)> f_create,
                                       TDescrContainer* pDescrContainer)
{
    auto it = m_Cache.find(eChoice);
    if (it != m_Cache.end()) {
        return *(it->second);
    }


    // Search for descriptor on Bioseq
    if (pDescrContainer->IsSet()) {
        for (auto& pDesc : pDescrContainer->SetDescr().Set()) {
            if (pDesc.NotEmpty() && f_verify(*pDesc)) {
                m_Cache.insert(make_pair(eChoice, pDesc));
                return *pDesc;
            }
        }
    }

    auto pDesc = f_create();
    m_Cache.insert(make_pair(eChoice, pDesc));
    pDescrContainer->SetDescr().Set().push_back(pDesc);
    return *pDesc;
}


void CModAdder::Apply(const CModHandler& mod_handler,
                           CBioseq& bioseq,
                           IObjtoolsListener* pMessageListener)
{
    const auto& ranges = SRangeGetter<TMods>::GetEqualRanges(mod_handler.GetMods());
    CDescrCache descr_cache(bioseq);

    unique_ptr<CFeatureCache> pFeatureCache;
    if (bioseq.IsNa()) {
        pFeatureCache.reset(new CGeneRefCache(bioseq));
    }
    else {
        pFeatureCache.reset(new CProteinRefCache(bioseq));
    }

    for (const auto& mod_range : ranges) {
        if (x_TryDescriptorMod(mod_range, descr_cache, pMessageListener)) {
            const string& mod_name = x_GetModName(mod_range);
            if (mod_name == "secondary-accession"){
                x_SetHist(mod_range, bioseq.SetInst(), pMessageListener);
            }
            else if (mod_name == "mol-type") {
                // mol-type appears before molecule in the default-ordered 
                // map keys. Therefore, if both mol-type and molecule are 
                // specified, molecule will take precedence over (or, more precisly, overwrite) 
                // the information extracted from mol-type when setting Seq-inst::mol
                x_SetMoleculeFromMolType(mod_range, bioseq.SetInst(), pMessageListener);
            }
            continue;
        }

        if (x_TrySeqInstMod(mod_range, bioseq.SetInst(), pMessageListener)) {
            continue;
        }

        if (x_TryFeatureMod(mod_range, *pFeatureCache, pMessageListener)) {
            continue;
        }
        // Report unrecognised modifier
    }
}


void CModAdder::x_ReportInvalidValue(const string& mod_name,
                                     const string& mod_value,
                                     IObjtoolsListener* pMessageListener)
{
}


const string& CModAdder::x_GetModName(const TRange& mod_range) 
{
    return mod_range.first->first;
}


const string& CModAdder::x_GetModValue(const TRange& mod_range)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    return mod_range.first->second.GetValue();
}


bool CModAdder::x_TrySeqInstMod(const TRange& mod_range, CSeq_inst& seq_inst,
    IObjtoolsListener* pMessageListener)
{
    const auto& mod_name = x_GetModName(mod_range);

    if (mod_name == "strand") {
        x_SetStrand(mod_range, seq_inst, pMessageListener);
        return true;
    }

    if (mod_name == "molecule") {
        x_SetMolecule(mod_range, seq_inst, pMessageListener);
        return true;
    }

    if (mod_name == "topology") {
        x_SetTopology(mod_range, seq_inst, pMessageListener);
        return true;
    }

//   Note that we do not check for the 'secondary-accession' modifier here.
//   secondary-accession also modifies the GB_block descriptor
//   The check for secondary-accession and any resulting call 
//   to x_SetHist is performed before x_TrySeqInstMod 
//   is invoked. 

    return false;
}



bool CModAdder::x_TryDescriptorMod(const TRange& mod_range, CDescrCache& descr_cache, 
        IObjtoolsListener* pMessageListener)
{
     
    if (x_TryBioSourceMod(mod_range, descr_cache, pMessageListener)) {
        return true;
    } 

    {
        static const unordered_map<string,function<void(const TRange&, CDescrCache&)>>
            s_MethodMap = {{"sra", x_SetDBLink},
                           {"bioproject", x_SetDBLink},
                           {"biosample", x_SetDBLink},
                           {"mol-type", x_SetMolInfoType},
                           {"completeness", x_SetMolInfoCompleteness},
                           {"tech", x_SetMolInfoTech},
                           {"primary-accession", x_SetTpaAssembly},
                           {"secondary-accession", x_SetGBblockIds},
                           {"keyword", x_SetGBblockKeywords},
                           {"project", x_SetGenomeProjects},
                           {"comment", x_SetComment},
                           {"pmid", x_SetPMID}
                          };

        const auto& mod_name = x_GetModName(mod_range);
        auto it = s_MethodMap.find(mod_name);
        if (it != s_MethodMap.end()) {
            it->second(mod_range, descr_cache);
            return true;
        }
    }

    return false;
}


bool CModAdder::x_TryBioSourceMod(const TRange& mod_range, CDescrCache& descr_cache,
        IObjtoolsListener* pMessageListener)
{
    const auto& name = x_GetModName(mod_range);
    if (name == "location") {
        _ASSERT(distance(mod_range.first, mod_range.second)==1);
        static const auto s_GenomeStringToEnum = s_GetReverseMap(s_GenomeEnumToString);
        const auto& value = x_GetModValue(mod_range);
        const auto& genome_enum = s_GenomeStringToEnum.at(value); // Need proper error handling here
        descr_cache.SetBioSource().SetGenome(genome_enum);
        return true;
    }

    if (name == "origin") {
        _ASSERT(distance(mod_range.first, mod_range.second)==1);
        static const auto s_OriginStringToEnum = s_GetReverseMap(s_OriginEnumToString);
        const auto& value = x_GetModValue(mod_range);
        const auto& origin_enum = s_OriginStringToEnum.at(value); // Need proper error handling here
        descr_cache.SetBioSource().SetOrigin(origin_enum);
        return true;
    }

    if (name == "focus") {
        _ASSERT(distance(mod_range.first, mod_range.second)==1);
        descr_cache.SetBioSource().SetIs_focus();
        return true;
    }


    { // check to see if this is a subsource mod
         auto it = s_SubSourceStringToEnum.find(name);
         if (it != s_SubSourceStringToEnum.end()) {
            x_SetSubtype(mod_range, descr_cache);
            return true;
         }
    }


    if (x_TryPCRPrimerMod(mod_range, descr_cache, pMessageListener)) {
        return true;
    }


    if (x_TryOrgRefMod(mod_range, descr_cache, pMessageListener)) {
        return true;
    }

    return false;
}



void CModAdder::x_SetPrimerNames(const string& primer_names, CPCRPrimerSet& primer_set)
{
    const auto set_size = primer_set.Get().size();
    vector<string> names;
    NStr::Split(primer_names, ":", names, NStr::fSplit_Tokenize);
    const auto num_names = names.size();

    auto it = primer_set.Set().begin();
    for (auto i=0; i<num_names; ++i) {
        if (NStr::IsBlank(names[i])) {
            continue;
        }
        if (i<set_size) {
            (*it)->SetName().Set(names[i]);
            ++it;
        } 
        else {
            auto pPrimer = Ref(new CPCRPrimer());
            pPrimer->SetName().Set(names[i]);
            primer_set.Set().push_back(move(pPrimer));
        }
    }
}


void CModAdder::x_SetPrimerSeqs(const string& primer_seqs, CPCRPrimerSet& primer_set)
{
    const auto set_size = primer_set.Get().size();
    vector<string> seqs;
    NStr::Split(primer_seqs, ":", seqs, NStr::fSplit_Tokenize);
    const auto num_seqs = seqs.size();

    auto it = primer_set.Set().begin();
    for (auto i=0; i<num_seqs; ++i) {
        if (NStr::IsBlank(seqs[i])) {
            continue;
        }
        if (i<set_size) {
            (*it)->SetSeq().Set(seqs[i]);
            ++it;
        } 
        else {
            auto pPrimer = Ref(new CPCRPrimer());
            pPrimer->SetSeq().Set(seqs[i]);
            primer_set.Set().push_back(move(pPrimer));
        }
    }
}


void CModAdder::x_AppendPrimerNames(const string& mod, vector<string>& reaction_names)
{
    vector<string> names;
    NStr::Split(mod, ":", names, NStr::fSplit_Tokenize);
    reaction_names.insert(reaction_names.end(), names.begin(), names.end());
}


void CModAdder::x_AppendPrimerSeqs(const string& mod, vector<string>& reaction_seqs)
{
    vector<string> seqs;
    NStr::Split(mod, ",", seqs, NStr::fSplit_Tokenize);
    if (seqs.size() > 1) {
        if (seqs.front().front() == '(') {
            seqs.front().erase(0,1);
        }
        if (seqs.back().back() == ')') {
            seqs.back().erase(seqs.back().size()-1,1);
        }
    }
    reaction_seqs.insert(reaction_seqs.end(), seqs.begin(), seqs.end());
}


bool CModAdder::x_TryPCRPrimerMod(const TRange& mod_range, CDescrCache& descr_cache,
        IObjtoolsListener* pMessageListener)
{
    const auto& mod_name = x_GetModName(mod_range);
    
    // Refactor to eliminate duplicated code
    if (mod_name == "fwd-primer-name") {
        vector<string> names;
        for (auto it = mod_range.first; it != mod_range.second; ++it)
        {
            x_AppendPrimerNames(it->second.GetValue(), names);
        }
        auto& pcr_reaction_set = descr_cache.SetPCR_primers();
        auto it = pcr_reaction_set.Set().begin();
        for (const auto& reaction_names : names) {
            if (it == pcr_reaction_set.Set().end()) {
                auto pPCRReaction = Ref(new CPCRReaction());
                x_SetPrimerNames(reaction_names, pPCRReaction->SetForward());
                pcr_reaction_set.Set().push_back(move(pPCRReaction));
            } 
            else { 
                x_SetPrimerNames(reaction_names, (*it++)->SetForward());
            }
        }
        return true;
    }


    if (mod_name == "fwd-primer-seq") {
        vector<string> seqs;
        for (auto it = mod_range.first; it != mod_range.second; ++it)
        {
            x_AppendPrimerSeqs(it->second.GetValue(), seqs);
        }
        auto& pcr_reaction_set = descr_cache.SetPCR_primers();
        auto it = pcr_reaction_set.Set().begin();
        for (const auto& reaction_seqs : seqs) {
            if (it == pcr_reaction_set.Set().end()) {
                auto pPCRReaction = Ref(new CPCRReaction());
                x_SetPrimerSeqs(reaction_seqs, pPCRReaction->SetForward());
                pcr_reaction_set.Set().push_back(move(pPCRReaction));
            } 
            else { 
                x_SetPrimerSeqs(reaction_seqs, (*it++)->SetForward());
            }
        }
        return true;
    }


    if(mod_name == "rev-primer-name") 
    {
        vector<string> names;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            x_AppendPrimerNames(it->second.GetValue(), names);
        }
        if (!names.empty()) {
            auto& pcr_reaction_set = descr_cache.SetPCR_primers();
            const size_t num_reactions = pcr_reaction_set.Get().size();
            const size_t num_names = names.size();
            if (num_names <= num_reactions) {
                auto it = pcr_reaction_set.Set().rbegin();
                for(auto i=num_names-1; i>=0; --i) {
                    x_SetPrimerNames(names[i], (*it++)->SetReverse());
                }
            }
            else {
                auto it = pcr_reaction_set.Set().begin();
                for (auto i=0; i<num_reactions; ++i) {
                     x_SetPrimerNames(names[i], (*it++)->SetReverse());
                }

                for (auto i=num_reactions; i<num_names; ++i) {
                    auto pPCRReaction = Ref(new CPCRReaction());
                    x_SetPrimerNames(names[i], pPCRReaction->SetReverse());
                }
            }
        }
        return true;
    }


    if(mod_name == "rev-primer-seq") 
    {
        vector<string> seqs;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            x_AppendPrimerSeqs(it->second.GetValue(), seqs);
        }
        if (!seqs.empty()) {
            auto& pcr_reaction_set = descr_cache.SetPCR_primers();
            const size_t num_reactions = pcr_reaction_set.Get().size();
            const size_t num_seqs = seqs.size();
            if (num_seqs <= num_reactions) {
                auto it = pcr_reaction_set.Set().rbegin();
                for(auto i=num_seqs-1; i>=0; --i) {
                    x_SetPrimerSeqs(seqs[i], (*it++)->SetReverse());
                }
            }
            else {
                auto it = pcr_reaction_set.Set().begin();
                for (auto i=0; i<num_reactions; ++i) {
                     x_SetPrimerSeqs(seqs[i], (*it++)->SetReverse());
                }

                for (auto i=num_reactions; i<num_seqs; ++i) {
                    auto pPCRReaction = Ref(new CPCRReaction());
                    x_SetPrimerSeqs(seqs[i], pPCRReaction->SetReverse());
                }
            }
        }
        return true;
    }

    return false;
}


bool CModAdder::x_TryOrgRefMod(const TRange& mod_range, CDescrCache& descr_cache,
        IObjtoolsListener* pMessageListener)
{
    const auto& name = x_GetModName(mod_range);
    if (name == "taxname") {
        const auto& value = x_GetModValue(mod_range);
        descr_cache.SetBioSource().SetOrg().SetTaxname(value);
        return true;
    }

    if (name == "common") {
        const auto& value = x_GetModValue(mod_range);
        descr_cache.SetBioSource().SetOrg().SetCommon(value);
        return true;
    }

    if (name == "dbxref") {
        x_SetDBxref(mod_range, descr_cache, pMessageListener);
        return true;
    }

    if (x_TryOrgNameMod(mod_range, descr_cache, pMessageListener)) {
        return true;
    }

    return false;
}


void CModAdder::x_SetDBxref(const TRange& mod_range, CDescrCache& descr_cache,
        IObjtoolsListener* pMessageListener)
{   
   vector<CRef<CDbtag>> dbtags;
   for (auto it = mod_range.first; it != mod_range.second; ++it) {
       const auto& value = it->second.GetValue();

       auto colon_pos = value.find(":");
       string database;
       string tag;
       if (colon_pos < (value.length()-1)) {
           database = value.substr(0, colon_pos);
           tag = value.substr(colon_pos+1);
       }
       else {
            database = value;
            tag = "?";
       }
       auto pDbtag = Ref(new CDbtag());
       pDbtag->SetDb(database);
       pDbtag->SetTag().SetStr(tag);
       dbtags.push_back(move(pDbtag));
   }

   descr_cache.SetBioSource().SetOrg().SetDb() = dbtags;
}


bool CModAdder::x_TryOrgNameMod(const TRange& mod_range, CDescrCache& descr_cache, 
        IObjtoolsListener* pMessageListener)
{
    const auto& name = x_GetModName(mod_range);
    if (name == "lineage") {
        const auto& value = x_GetModValue(mod_range);
        descr_cache.SetBioSource().SetOrg().SetOrgname().SetLineage(value);
        return true;
    }

    if (name == "division") {
        const auto& value = x_GetModValue(mod_range);
        descr_cache.SetBioSource().SetOrg().SetOrgname().SetDiv(value);
        return true;
    }

    // check for gcode, mgcode, pgcode
    using TSetCodeMemFn = void (COrgName::*)(int);
    using TFunction = function<void(COrgName&, int)>;
    static const 
        unordered_map<string, TFunction> 
                s_GetCodeSetterMethods = 
                {{"gcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetGcode))}, 
                 {"mgcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetMgcode))}, 
                 {"pgcode",  TFunction(static_cast<TSetCodeMemFn>(&COrgName::SetPgcode))}};

    auto it = s_GetCodeSetterMethods.find(name);
    if (it != s_GetCodeSetterMethods.end()) {
        const auto& value = x_GetModValue(mod_range);
        try {
            auto code = NStr::StringToInt(value);
            it->second(descr_cache.SetBioSource().SetOrg().SetOrgname(),
                       code);
            return true;
        }
        catch(...)
        {
            x_ReportInvalidValue(name, value, pMessageListener);
            return true;
        }
    }

    { //  check for orgmod
        auto it = s_OrgModStringToEnum.find(name);
        if (it != s_OrgModStringToEnum.end()) {
            x_SetOrgMod(mod_range, descr_cache);
            return true;
        }
    }
    return false;
}


void CModAdder::x_SetSubtype(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(mod_range.first != mod_range.second);
    const auto subtype = s_SubSourceStringToEnum.at(x_GetModName(mod_range));
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        const auto& name = it->second.GetValue();
        auto pSubSource = Ref(new CSubSource(subtype,name));
        if (it->second.IsSetAttrib()) {
            pSubSource->SetAttrib(it->second.GetAttrib());
        }
        descr_cache.SetSubtype().push_back(move(pSubSource));
    }
}


void CModAdder::x_SetOrgMod(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(mod_range.first != mod_range.second);
    const auto& subtype = s_OrgModStringToEnum.at(x_GetModName(mod_range));
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        const auto& subname = it->second.GetValue();
        auto pOrgMod = Ref(new COrgMod(subtype,subname));
        if (it->second.IsSetAttrib()) {
            pOrgMod->SetAttrib(it->second.GetAttrib());
        }
        descr_cache.SetOrgMods().push_back(move(pOrgMod));
    }
}


void CModAdder::x_SetMolInfoType(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = x_GetModValue(mod_range);
    auto it = s_BiomolStringToEnum.find(value);
    if (it != s_BiomolStringToEnum.end()) {
        descr_cache.SetMolInfo().SetBiomol(it->second);
    }
    else {
        const auto& mod_name = x_GetModName(mod_range);
        x_ReportInvalidValue(mod_name, value);
    }
}


void CModAdder::x_SetMolInfoTech(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = x_GetModValue(mod_range);
    auto it = s_TechStringToEnum.find(value);
    if (it != s_TechStringToEnum.end()) {
        descr_cache.SetMolInfo().SetTech(it->second);
    }
    else {
        const auto& mod_name = x_GetModName(mod_range);
        x_ReportInvalidValue(mod_name, value);
    }
}


void CModAdder::x_SetMolInfoCompleteness(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = x_GetModValue(mod_range);
    auto it = s_CompletenessStringToEnum.find(value);
    if (it != s_CompletenessStringToEnum.end()) {
        descr_cache.SetMolInfo().SetCompleteness(it->second);
    }
    else {
        const auto& mod_name = x_GetModName(mod_range);
        x_ReportInvalidValue(mod_name, value);
    }
}


void CModAdder::x_SetTpaAssembly(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<CStringUTF8> accession_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        transform(value_sublist.begin(), value_sublist.end(), back_inserter(accession_list),
                [](const CTempString& val) { return CUtf8::AsUTF8(val, eEncoding_UTF8); });
    }

    auto make_user_field = [](const CStringUTF8& accession) {
        auto pField = Ref(new CUser_field());
        pField->SetLabel().SetId(0);
        auto pSubfield = Ref(new CUser_field());
        pSubfield->SetLabel().SetStr("accession");
        pSubfield->SetData().SetStr(accession);
        pField->SetData().SetFields().push_back(move(pSubfield));
        return pField;
    };

    auto& user = descr_cache.SetTpaAssembly();
    user.SetData().resize(accession_list.size());
    transform(accession_list.begin(), accession_list.end(), user.SetData().begin(), make_user_field);
}


void CModAdder::x_SetGenomeProjects(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<int> id_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;  
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        transform(value_sublist.begin(), value_sublist.end(), back_inserter(id_list), 
                [](const CTempString& val) { return NStr::StringToUInt(val); });
    }

    auto make_user_field = [](const int& id) {
        auto pField = Ref(new CUser_field());
        auto pSubfield = Ref(new CUser_field());
        pField->SetLabel().SetId(0);
        pSubfield->SetLabel().SetStr("ProjectID");
        pField->SetData().SetFields().push_back(pSubfield);
        pSubfield.Reset(new CUser_field());
        pSubfield->SetLabel().SetStr("ParentID");
        pSubfield->SetData().SetInt(0);
        pField->SetData().SetFields().push_back(pSubfield);
        return pField;
    };

    auto& user = descr_cache.SetGenomeProjects();
    user.SetData().resize(id_list.size());
    transform(id_list.begin(), id_list.end(), user.SetData().begin(), make_user_field);
}


void CModAdder::x_SetGBblockIds(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<string> id_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            string value = NStr::TruncateSpaces_Unsafe(val);
            try {
                SSeqIdRange idrange(value);
                id_list.insert(id_list.end(),idrange.begin(), idrange.end());
            }
            catch(...)
            {
                id_list.push_back(value);
            }
        }
    }
    auto& gb_block = descr_cache.SetGBblock();
    gb_block.SetExtra_accessions().assign(id_list.begin(), id_list.end());
}


void CModAdder::x_SetGBblockKeywords(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<CTempString> value_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        value_list.splice(value_list.end(), value_sublist);
    }
    descr_cache.SetGBblock().SetKeywords().assign(value_list.begin(), value_list.end());
}


void CModAdder::x_SetComment(const TRange& mod_range,
                                  CDescrCache& descr_cache)

{
    for (auto it = mod_range.first; it != mod_range.second; ++it)
    {
        const auto& comment = it->second.GetValue();
        descr_cache.SetComment() = comment;
    }
}


void CModAdder::x_SetPMID(const TRange& mod_range,
                               CDescrCache& descr_cache) 
{
    for (auto it = mod_range.first; it != mod_range.second; ++it)
    {
        const auto& value = it->second.GetValue();
        int pmid;
        try {
            pmid = NStr::StringToInt(value);
        }
        catch(...) {
            // Post error/Throw exception here
        } 
        auto pPub = Ref(new CPub());
        pPub->SetPmid().Set(pmid);
        descr_cache.SetPubdesc()
                   .SetPub()
                   .Set()
                   .push_back(move(pPub));
    }
}


void CModAdder::x_SetDBLink(const TRange& mod_range, 
                                 CDescrCache& descr_cache) 
{
    const auto& name = x_GetModName(mod_range);
    static const unordered_map<string, string> s_NameToLabel =
    {{"sra", "Sequence Read Archive"},
     {"biosample", "BioSample"},
     {"bioproject", "BioProject"}};

    const auto& label = s_NameToLabel.at(name);
    x_SetDBLinkField(label, mod_range, descr_cache);
}


void CModAdder::x_SetDBLinkField(const string& label,
                                      const TRange& mod_range,
                                      CDescrCache& descr_cache)
{
    list<CTempString> value_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        value_list.splice(value_list.end(), value_sublist);
    }
}


void CModAdder::x_SetDBLinkFieldVals(const string& label,
                                          const list<CTempString>& vals,
                                          CUser_object& dblink)
{
    if (vals.empty()) {
        return;
    }

    CRef<CUser_field> pField;
    if (dblink.IsSetData()) {
        for (auto pUserField : dblink.SetData()) {
            if (pUserField &&
                pUserField->IsSetLabel() &&
                pUserField->GetLabel().IsStr() &&
                NStr::EqualNocase(pUserField->GetLabel().GetStr(), label)) {
                pField = pUserField;
                break;
            }
        }
    }

    if (!pField) {
        pField = Ref(new CUser_field());
        pField->SetLabel().SetStr() = label;
        dblink.SetData().push_back(pField);
    }

    pField->SetData().SetStrs().assign(vals.begin(), vals.end());
}


void CModAdder::x_SetStrand(const TRange& mod_range, CSeq_inst& seq_inst,
        IObjtoolsListener* pMessageListener)
{
    const auto& value = x_GetModValue(mod_range);
    const auto it = s_StrandStringToEnum.find(value);
    if (it == s_StrandStringToEnum.end()) {
        x_ReportInvalidValue(x_GetModName(mod_range), value, pMessageListener);
        return;
    }
    seq_inst.SetStrand(it->second);
}


void CModAdder::x_SetMolecule(const TRange& mod_range, CSeq_inst& seq_inst,
        IObjtoolsListener* pMessageListener)
{
    auto mol = s_MolStringToEnum.at(x_GetModValue(mod_range));
    seq_inst.SetMol(mol);
}


void CModAdder::x_SetMoleculeFromMolType(const TRange& mod_range, CSeq_inst& seq_inst,
        IObjtoolsListener* pMessageListener)
{
    const auto& value = x_GetModValue(mod_range);
    auto it = s_BiomolStringToEnum.find(value);
    if (it == s_BiomolStringToEnum.end()) {
        // No need to report an error here.
        // The error is reported in x_SetMolInfoType
        return; 
    }
    auto mol =  s_BiomolEnumToMolEnum.at(it->second);
    seq_inst.SetMol(mol);
}


void CModAdder::x_SetTopology(const TRange& mod_range, CSeq_inst& seq_inst, 
        IObjtoolsListener* pMessageListener)
{
    auto topology = s_TopologyStringToEnum.at(x_GetModValue(mod_range));
    seq_inst.SetTopology(topology);
}


void CModAdder::x_SetHist(const TRange& mod_range, CSeq_inst& seq_inst,
        IObjtoolsListener* pMessageListener) 
{
    list<string> id_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            string value = NStr::TruncateSpaces_Unsafe(val);
            if (value.length() >= 8 && 
                value.substr(0,8) == "ref_seq|") {
                value.erase(3,4); // ref_seq| -> ref|
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
    }
    list<CRef<CSeq_id>> secondary_ids;
    // try catch statement
    transform(id_list.begin(), id_list.end(), back_inserter(secondary_ids),
            [](const string& id_string) { return Ref(new CSeq_id(id_string)); });

    seq_inst.SetHist().SetReplaces().SetIds() = move(secondary_ids);
}


bool CModAdder::x_TryFeatureMod(const TRange& mod_range, CFeatureCache& feat_cache,
        IObjtoolsListener* pMessageListener)
{
    if (dynamic_cast<CGene_ref*>(&feat_cache)) {
        if (x_TryGeneRefMod(mod_range, feat_cache, pMessageListener)) {
            return true;
        }
    }
    else 
    if (x_TryProteinRefMod(mod_range, feat_cache, pMessageListener)) {
        return true;
    }
    return false;
}


bool CModAdder::x_TryGeneRefMod(const TRange& mod_range, CFeatureCache& feat_cache,
        IObjtoolsListener* pMessageListener)
{
    const auto& name = x_GetModName(mod_range);
    if (name == "gene") {
        const auto& value = x_GetModValue(mod_range);
        feat_cache.SetSeq_feat().SetData().SetGene().SetLocus(value);   
        return true;
    }

    if (name == "allele") {
        const auto& value = x_GetModValue(mod_range);
        feat_cache.SetSeq_feat().SetData().SetGene().SetAllele(value);   
        return true;
    }


    if (name == "locus-tag") {
        const auto& value = x_GetModValue(mod_range);
        feat_cache.SetSeq_feat().SetData().SetGene().SetLocus_tag(value);   
        return true;
    }


    if (name == "gene-synonym") {
        CGene_ref::TSyn synonyms;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            synonyms.push_back(it->second.GetValue());
        }
        feat_cache.SetSeq_feat().SetData().SetGene().SetSyn() = move(synonyms);
        return true;
    }
    return false;
}


bool CModAdder::x_TryProteinRefMod(const TRange& mod_range, CFeatureCache& feat_cache,
        IObjtoolsListener* pMessageListener)
{
    const auto& mod_name = x_GetModName(mod_range);
    if (mod_name == "protein-desc") {
        const auto& value = x_GetModValue(mod_range);
        feat_cache.SetSeq_feat().SetData().SetProt().SetDesc(value);
        return true;
    }

    if (mod_name == "protein") {
        CProt_ref::TName names;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            names.push_back(it->second.GetValue()); 
        }
        feat_cache.SetSeq_feat().SetData().SetProt().SetName() = move(names);
        return true;
    }

    if (mod_name == "EC-number") {
        CProt_ref::TEc ec_numbers;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            ec_numbers.push_back(it->second.GetValue());
        }
        feat_cache.SetSeq_feat().SetData().SetProt().SetEc() = move(ec_numbers);
        return true;
    }

    if (mod_name == "activity") {
        CProt_ref::TActivity activity;
        for (auto it = mod_range.first; it != mod_range.second; ++it) {
            activity.push_back(it->second.GetValue());
        }
        feat_cache.SetSeq_feat().SetData().SetProt().SetActivity() = move(activity);
        return true;
    }
    return false;
}


END_SCOPE(objects)
END_NCBI_SCOPE



