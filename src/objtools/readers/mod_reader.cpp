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

CModValueAttribs::CModValueAttribs(const string& value) : mValue(value) {}


CModValueAttribs::CModValueAttribs(const char* value) : mValue(value) {}


void CModValueAttribs::SetValue(const string& value) 
{
    mValue = value;
}

void CModValueAttribs::AddAttrib(const string& attrib_name, 
        const string& attrib_value)
{
    mAdditionalAttribs.emplace(attrib_name, attrib_value);
}

bool CModValueAttribs::HasAdditionalAttribs(void) const
{
    return !mAdditionalAttribs.empty();
}

const string& CModValueAttribs::GetValue(void) const
{
    return mValue;
}

const CModValueAttribs::TAttribs& CModValueAttribs::GetAdditionalAttribs(void) const
{
    return mAdditionalAttribs;
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
    if (x_IsUserType(user_object, "GenomeProjectsDB")) {
        x_ImportGenomeProjects(user_object, mods);
    }
    else
    if (x_IsUserType(user_object, "TpaAssembly")) {
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


bool CModParser::x_IsUserType(const CUser_object& user_object, const string& type)
{
    return (user_object.IsSetType() &&
            user_object.GetType().IsStr() &&
            user_object.GetType().GetStr() == type);
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

const unordered_set<string> CModHandler::sm_MultipleValuesPermitted =
{ 
    "primary-accession",
    "secondary-accession",
    "dbxref",
    "protein",
    "EC-number",
    "activity",
    "pmid",
    "comment",
    "project",
    "keyword",
    "note",     // Need to check subsources
    "sub-clone",
    "gene_synonym"
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
                (sm_MultipleValuesPermitted.find(canonical_name) != 
                sm_MultipleValuesPermitted.end());

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
                         const CModValueAttribs& value_attribs,
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
                (sm_MultipleValuesPermitted.find(canonical_name) != 
                sm_MultipleValuesPermitted.end());

            if (allow_multiple_values == false) {
                if (handle_existing == eAppendPreserve) { // Do nothing
                    return;
                }
                // handle_existing == eAppendReplace
                m_Mods.erase(canonical_name);
            }
        }
    }
    m_Mods.emplace(canonical_name, value_attribs);
}


void CModHandler::AddMods(const TMods& mods,
                          EHandleExisting handle_existing)
{
    unordered_set<string> previous_mods;
    for (const auto& mod : mods) {
        const auto& canonical_name = x_GetCanonicalName(mod.first);
        const auto allow_multiple_values = 
            (sm_MultipleValuesPermitted.find(canonical_name) != 
            sm_MultipleValuesPermitted.end());

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


void CModAdder::Apply(const TMods& mods, CBioseq& bioseq) {
}


// Add subtype and subsource references
class CDescrCache 
{
public:

    struct SDescrContainer {
        virtual ~SDescrContainer(void) = default;
        virtual bool IsSet(void) const = 0;
        virtual CSeq_descr& SetDescr(void) = 0;
    };

    using TDescrContainer = SDescrContainer;
    using TSubtype = CBioSource::TSubtype;
    using TOrgMods = COrgName::TMod;


    CDescrCache(TDescrContainer& descr_container);

    CSeqdesc& SetDBLink(void);
    CSeqdesc& SetTpaAssembly(void);
    CSeqdesc& SetGenomeProjects(void);
    CSeqdesc& SetGBblock(void);
    CSeqdesc& SetMolInfo(void);
    CSeqdesc& SetBioSource(void);
    CSeqdesc& SetComment(void);
    CSeqdesc& SetPubdesc(void);

    TSubtype& SetSubtype(void);
    TOrgMods& SetOrgMods(void);
    

private:
    enum EChoice : size_t {
        eDBLink = 1,
        eTpa = 2,
        eGenomeProjects = 3,
        eMolInfo = 4,
        eGBblock = 5,
        eBioSource = 6,
    };

    bool x_IsUserType(const CUser_object& user_object, const string& type);
    void x_SetUserType(const string& type, CUser_object& user_object);
    CSeqdesc& x_SetDescriptor(const EChoice eChoice, 
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create);

    TSubtype* m_pSubtype = nullptr;
    TOrgMods* m_pOrgMods = nullptr;
    bool m_FirstComment = true;
    bool m_FirstPubdesc = true;
    using TMap = unordered_map<EChoice, CRef<CSeqdesc>, hash<underlying_type<EChoice>::type>>;
    TMap m_Cache;

    SDescrContainer& m_DescrContainer;
};


class CGeneRefCache 
{
public:
    CGeneRefCache(CBioseq& bioseq);
    CSeq_feat& SetSeq_feat(void);
private:
    CBioseq& m_Bioseq;
    CRef<CSeq_feat> m_pFeature;
};


CGeneRefCache::CGeneRefCache(CBioseq& bioseq) : m_Bioseq(bioseq) {}


CSeq_feat& CGeneRefCache::SetSeq_feat()
{
    if (m_pFeature) {
        return *m_pFeature;
    }

    if (m_Bioseq.IsSetAnnot()) {
        for (auto& pAnnot : m_Bioseq.SetAnnot()) {
            if (pAnnot && pAnnot->IsFtable()) {
                for (auto pSeqfeat : pAnnot->SetData().SetFtable()) {
                    if (pSeqfeat &&
                        pSeqfeat->IsSetData() &&
                        pSeqfeat->GetData().IsGene()) {
                        m_pFeature = pSeqfeat;
                        return *m_pFeature;
                    }
                }
            }
        }
    }

    m_pFeature = CRef<CSeq_feat>(new CSeq_feat());
    m_pFeature->SetData().SetGene();
    auto pAnnot = CRef<CSeq_annot>(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(m_pFeature);
    m_Bioseq.SetAnnot().push_back(pAnnot);
    return *m_pFeature;
}


class CProteinRefCache 
{
public:
    CProteinRefCache(CBioseq& bioseq);
    CSeq_feat& SetSeq_feat(void);
private:
    CBioseq& m_Bioseq;
    CRef<CSeq_feat> m_pFeature;
};


CProteinRefCache::CProteinRefCache(CBioseq& bioseq) : m_Bioseq(bioseq) {}


CSeq_feat& CProteinRefCache::SetSeq_feat()
{
    if (m_pFeature) {
        return *m_pFeature;
    }

    if (m_Bioseq.IsSetAnnot()) {
        for (auto& pAnnot : m_Bioseq.SetAnnot()) {
            if (pAnnot && pAnnot->IsFtable()) {
                for (auto pSeqfeat : pAnnot->SetData().SetFtable()) {
                    if (pSeqfeat &&
                        pSeqfeat->IsSetData() &&
                        pSeqfeat->GetData().IsProt()) {
                        m_pFeature = pSeqfeat;
                        return *m_pFeature;
                    }
                }
            }
        }
    }

    m_pFeature = CRef<CSeq_feat>(new CSeq_feat());
    m_pFeature->SetData().SetProt();
    auto pAnnot = CRef<CSeq_annot>(new CSeq_annot());
    pAnnot->SetData().SetFtable().push_back(m_pFeature);
    m_Bioseq.SetAnnot().push_back(pAnnot);
    return *m_pFeature;
}

template<class TObject>
class CDescrContainer : public CDescrCache::SDescrContainer 
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



bool CDescrCache::x_IsUserType(const CUser_object& user_object, const string& type)
{
    return (user_object.IsSetType() &&
            user_object.GetType().IsStr() &&
            user_object.GetType().GetStr() == type);
}


void CDescrCache::x_SetUserType(const string& type, 
                                     CUser_object& user_object) 
{
    user_object.SetType().SetStr(type);
} 


CDescrCache::CDescrCache(CDescrCache::SDescrContainer& descr_container) : m_DescrContainer(descr_container) {}


CSeqdesc& CDescrCache::SetPubdesc()
{
    if (m_FirstPubdesc) {
        if (m_DescrContainer.IsSet()) {
            m_DescrContainer.SetDescr().Set().remove_if([](const CRef<CSeqdesc>& pDesc) { return (pDesc && pDesc->IsPub()); });
        }
        m_FirstPubdesc = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_DescrContainer.SetDescr().Set().push_back(pDesc);
    pDesc->SetPub();
    return *pDesc;
}


CSeqdesc& CDescrCache::SetComment()
{
    if (m_FirstComment) {
        if (m_DescrContainer.IsSet()) {
            m_DescrContainer.SetDescr().Set().remove_if([](const CRef<CSeqdesc>& pDesc) { return (pDesc && pDesc->IsComment()); });
        }
        m_FirstComment = false;
    }

    auto pDesc = Ref(new CSeqdesc());
    m_DescrContainer.SetDescr().Set().push_back(pDesc);
    pDesc->SetComment();
    return *pDesc;
}


CSeqdesc& CDescrCache::SetDBLink() 
{
    return x_SetDescriptor(eDBLink, 
        [](const CSeqdesc& desc) {
            return (desc.IsUser() && desc.GetUser().IsDBLink());
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetUser().SetObjectType(CUser_object::eObjectType_DBLink);
            return pDesc;
        });
}

CSeqdesc& CDescrCache::SetTpaAssembly()
{
    return x_SetDescriptor(eTpa,
        [this](const CSeqdesc& desc) {
            return (desc.IsUser() && x_IsUserType(desc.GetUser(), "TpaAssembly"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("TpaAssembly", pDesc->SetUser());
            return pDesc;
        }
    );
}

CSeqdesc& CDescrCache::SetGenomeProjects()
{
        return x_SetDescriptor(eGenomeProjects,
        [this](const CSeqdesc& desc) {
            return (desc.IsUser() && x_IsUserType(desc.GetUser(), "GenomeProjectsDB"));
        },
        [this]() {
            auto pDesc = Ref(new CSeqdesc());
            x_SetUserType("GenomeProjectsDB", pDesc->SetUser());
            return pDesc;
        }
    );
}


CSeqdesc& CDescrCache::SetGBblock()
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
    );
}


CSeqdesc& CDescrCache::SetMolInfo()
{
    return x_SetDescriptor(eMolInfo,
        [](const CSeqdesc& desc) { 
            return desc.IsMolinfo(); 
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetMolinfo();
            return pDesc;
        }
    );
}


CSeqdesc& CDescrCache::SetBioSource()
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
        );
}


CDescrCache::TSubtype& CDescrCache::SetSubtype()
{
    if (!m_pSubtype) {
        m_pSubtype = &(SetBioSource().SetSource().SetSubtype());
        m_pSubtype->clear();
    }

    return *m_pSubtype;
}


CDescrCache::TOrgMods& CDescrCache::SetOrgMods()
{
    if (!m_pOrgMods) {
        m_pOrgMods = &(SetBioSource().SetSource().SetOrg().SetOrgname().SetMod());
        m_pOrgMods->clear();
    }

    return *m_pOrgMods;
}


CSeqdesc& CDescrCache::x_SetDescriptor(const EChoice eChoice, 
                                            function<bool(const CSeqdesc&)> f_verify,
                                            function<CRef<CSeqdesc>(void)> f_create)
{
    auto it = m_Cache.find(eChoice);
    if (it != m_Cache.end()) {
        return *(it->second);
    }

    // Search for descriptor on Bioseq
    if (m_DescrContainer.IsSet()) {
        for (auto& pDesc : m_DescrContainer.SetDescr().Set()) {
            if (pDesc.NotEmpty() && f_verify(*pDesc)) {
                m_Cache.insert(make_pair(eChoice, pDesc));
                return *pDesc;
            }
        }
    }

    auto pDesc = f_create();
    m_Cache.insert(make_pair(eChoice, pDesc));
    m_DescrContainer.SetDescr().Set().push_back(pDesc);
    return *pDesc;
}


class CModAdder_Impl 
{
public:
    using TMods = CModAdder::TMods;
    using TRange = SRangeGetter<TMods>::TRange;

    static void Apply(const TMods& mods, CBioseq& bioseq) {
        const auto& ranges = SRangeGetter<TMods>::GetEqualRanges(mods);
        for (const auto& mod_range : ranges) {

        }
    }

private:

    static bool x_SetDescriptorMod(const TRange& mod_range, CDescrCache& descr_cache);
//    static bool x_SetBioSourceMod(const TRange& mod_range, CDescrCache& descr_cache);

    static void x_SetStrand(const TRange& mod_range, CSeq_inst& seq_inst);
    static void x_SetMolecule(const TRange& mod_range, CSeq_inst& seq_inst);
    static void x_SetTopology(const TRange& mod_range, CSeq_inst& seq_inst);
    static void x_SetHist(const TRange& mod_range, CSeq_inst& seq_inst);

    static void x_SetDBLink(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGBblockIds(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGBblockKeywords(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetGenomeProjects(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetTpaAssembly(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetComment(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetPMID(const TRange& mod_range, CDescrCache& descr_cache);

    static void x_SetDBLinkField(const string& label, const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetDBLinkFieldVals(const string& label, const list<CTempString>& vals, CSeqdesc& dblink_desc);

    static void x_SetMolInfoType(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetMolInfoTech(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetMolInfoCompleteness(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetSubtype(const TRange& mod_range, CDescrCache& descr_cache);
    static void x_SetOrgMod(const TRange& mod_range, CDescrCache& descr_cache);

    static void x_ReportError(void){} // Need to fill this in
    static void x_ReportBadValue(const string& mod_name, const string& mod_value) {}
};



bool CModAdder_Impl::x_SetDescriptorMod(const TRange& mod_range, CDescrCache& descr_cache)
{
     
    const auto& mod_name = mod_range.first->first;
    
    { // check to see if this is a subsource mod
         auto it = s_SubSourceStringToEnum.find(mod_name);
         if (it != s_SubSourceStringToEnum.end()) {
            x_SetSubtype(mod_range, descr_cache);
            return true;
         }
    }

    { // check for orgmod
        auto it = s_OrgModStringToEnum.find(mod_name);
        if (it != s_OrgModStringToEnum.end()) {
            x_SetOrgMod(mod_range, descr_cache);
            return true;
        }
    }


    {
        static const unordered_map<string,function<void(const TRange&, CDescrCache&)>>
            s_MethodMap = {{"sra", x_SetDBLink},
                           {"bioproject", x_SetDBLink},
                           {"biosample", x_SetDBLink},
                           {"primary-accession", x_SetTpaAssembly},
                           {"secondary-accession", x_SetGBblockIds},
                           {"keyword", x_SetGBblockKeywords},
                           {"project", x_SetGenomeProjects},
                           {"comment", x_SetComment},
                           {"pmid", x_SetPMID}
                          };

            auto it = s_MethodMap.find(mod_name);
            if (it != s_MethodMap.end()) {
                it->second(mod_range, descr_cache);
                return true;
        }
    }

    return false;
}


void CModAdder_Impl::x_SetSubtype(const TRange& mod_range, CDescrCache& descr_cache)
{
    if (mod_range.first == mod_range.second) {
        return;
    }
    const auto subtype = s_SubSourceStringToEnum.at(mod_range.first->first);
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        const auto& name = it->second.GetValue();
        auto pSubSource = Ref(new CSubSource(subtype,name));
        descr_cache.SetSubtype().push_back(move(pSubSource));
    }
}


void CModAdder_Impl::x_SetOrgMod(const TRange& mod_range, CDescrCache& descr_cache)
{
    if (mod_range.first == mod_range.second) {
        return ;
    }
    const auto& subtype = s_OrgModStringToEnum.at(mod_range.first->first);
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        const auto& subname = it->second.GetValue();
        auto pOrgMod = Ref(new COrgMod(subtype,subname));
        descr_cache.SetOrgMods().push_back(move(pOrgMod));
    }
}


void CModAdder_Impl::x_SetMolInfoType(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = mod_range.first->second.GetValue();
    auto it = s_BiomolStringToEnum.find(value);
    if (it != s_BiomolStringToEnum.end()) {
        descr_cache.SetMolInfo().SetMolinfo().SetBiomol(it->second);
    }
    else {
        const auto& mod_name = mod_range.first->first;
        x_ReportBadValue(mod_name, value);
    }
}


void CModAdder_Impl::x_SetMolInfoTech(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = mod_range.first->second.GetValue();
    auto it = s_TechStringToEnum.find(value);
    if (it != s_TechStringToEnum.end()) {
        descr_cache.SetMolInfo().SetMolinfo().SetTech(it->second);
    }
    else {
        const auto& mod_name = mod_range.first->first;
        x_ReportBadValue(mod_name, value);
    }
}


void CModAdder_Impl::x_SetMolInfoCompleteness(const TRange& mod_range, CDescrCache& descr_cache)
{
    _ASSERT(distance(mod_range.first, mod_range.second)==1);
    const auto& value = mod_range.first->second.GetValue();
    auto it = s_CompletenessStringToEnum.find(value);
    if (it != s_CompletenessStringToEnum.end()) {
        descr_cache.SetMolInfo().SetMolinfo().SetCompleteness(it->second);
    }
    else {
        const auto& mod_name = mod_range.first->first;
        x_ReportBadValue(mod_name, value);
    }
}


void CModAdder_Impl::x_SetTpaAssembly(const TRange& mod_range, CDescrCache& descr_cache)
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

    auto& user = descr_cache.SetTpaAssembly().SetUser();
    user.SetData().resize(accession_list.size());
    transform(accession_list.begin(), accession_list.end(), user.SetData().begin(), make_user_field);
}


void CModAdder_Impl::x_SetGenomeProjects(const TRange& mod_range, CDescrCache& descr_cache)
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

    auto& user = descr_cache.SetGenomeProjects().SetUser();
    user.SetData().resize(id_list.size());
    transform(id_list.begin(), id_list.end(), user.SetData().begin(), make_user_field);
}


void CModAdder_Impl::x_SetGBblockIds(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<CTempString> id_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        for (const auto& val : value_sublist) {
            try {
                SSeqIdRange idrange(val);
                id_list.insert(id_list.end(),idrange.begin(), idrange.end());
            }
            catch(...)
            {
                id_list.push_back(val);
            }
        }
    }
    auto& genbank = descr_cache.SetGBblock().SetGenbank();
    genbank.SetExtra_accessions().assign(id_list.begin(), id_list.end());
}


void CModAdder_Impl::x_SetGBblockKeywords(const TRange& mod_range, CDescrCache& descr_cache)
{
    list<CTempString> value_list;
    for (auto it = mod_range.first; it != mod_range.second; ++it) {
        list<CTempString> value_sublist;
        const auto& vals = it->second.GetValue();
        NStr::Split(vals, ",", value_sublist, NStr::fSplit_Tokenize);
        value_list.splice(value_list.end(), value_sublist);
    }
    descr_cache.SetGBblock().SetGenbank().SetKeywords().assign(value_list.begin(), value_list.end());
}


void CModAdder_Impl::x_SetComment(const TRange& mod_range,
                                  CDescrCache& descr_cache)

{
    for (auto it = mod_range.first; it != mod_range.second; ++it)
    {
        const auto& comment = it->second.GetValue();
        descr_cache.SetComment().SetComment(comment);
    }
}


void CModAdder_Impl::x_SetPMID(const TRange& mod_range,
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
                   .SetPub()
                   .Set()
                   .push_back(move(pPub));
    }
}


void CModAdder_Impl::x_SetDBLink(const TRange& mod_range, 
                                 CDescrCache& descr_cache) 
{
    const auto& name = mod_range.first->first;
    static const unordered_map<string, string> s_NameToLabel =
    {{"sra", "Sequence Read Archive"},
     {"biosample", "BioSample"},
     {"bioproject", "BioProject"}};

    const auto& label = s_NameToLabel.at(name);
    x_SetDBLinkField(label, mod_range, descr_cache);
}



void CModAdder_Impl::x_SetDBLinkField(const string& label,
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


void CModAdder_Impl::x_SetDBLinkFieldVals(const string& label,
                                          const list<CTempString>& vals,
                                          CSeqdesc& dblink_desc)
{
    if (vals.empty()) {
        return;
    }

    auto& user_obj = dblink_desc.SetUser();
    CRef<CUser_field> pField;
    if (user_obj.IsSetData()) {
        for (auto pUserField : user_obj.SetData()) {
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
        user_obj.SetData().push_back(pField);
    }

    pField->SetData().SetStrs().assign(vals.begin(), vals.end());
}


void CModAdder_Impl::x_SetStrand(const TRange& mod_range, CSeq_inst& seq_inst)
{
    auto strand = s_StrandStringToEnum.at(mod_range.first->second.GetValue());
    seq_inst.SetStrand(strand);
}


void CModAdder_Impl::x_SetMolecule(const TRange& mod_range, CSeq_inst& seq_inst)
{
    auto mol = s_MolStringToEnum.at(mod_range.first->second.GetValue());
    seq_inst.SetMol(mol);
}


void CModAdder_Impl::x_SetTopology(const TRange& mod_range, CSeq_inst& seq_inst)
{
    auto topology = s_TopologyStringToEnum.at(mod_range.first->second.GetValue());
    seq_inst.SetTopology(topology);
}


void CModAdder_Impl::x_SetHist(const TRange& mod_range, CSeq_inst& seq_inst) 
{
}


END_SCOPE(objects)
END_NCBI_SCOPE



