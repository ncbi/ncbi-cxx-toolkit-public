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

#include <map>
#include <unordered_map>
#include <unordered_set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using TModNameSet = unordered_set<string>;

    
template<typename TEnum>
unordered_map<string, TEnum> s_InitModNameToEnumMap(
    const CEnumeratedTypeValues& etv,
    const TModNameSet& skip_enum_names,
    const unordered_map<string, TEnum>&  extra_enum_names_to_vals)

{
    using TModNameEnumMap = unordered_map<string, TEnum>;
    TModNameEnumMap smod_enum_map;
    
    for (const auto& name_val : etv.GetValues()) {
        const auto& enum_name = name_val.first;
        const TEnum& enum_val = static_cast<TEnum>(name_val.second);

        if (skip_enum_names.find(enum_name) != skip_enum_names.end()) 
        {
            continue;
        }
        auto emplace_result = smod_enum_map.emplace(enum_name, enum_val);
        // emplace must succeed
        if (!emplace_result.second) {
            NCBI_USER_THROW_FMT(
                "s_InitModNameToEnumMap " << enum_name);
        }
    }

    for (auto extra_smod_to_enum : extra_enum_names_to_vals) {
        auto emplace_result = smod_enum_map.emplace(extra_smod_to_enum);
        // emplace must succeed
        if (!emplace_result.second) {
            NCBI_USER_THROW_FMT(
                "s_InitModNameToEnumMap " << extra_smod_to_enum.first);
        }
    }
    return smod_enum_map;
}


static unordered_map<string, COrgMod::TSubtype> 
s_InitModNameOrgSubtypeMap(void)
{
    static const unordered_set<string> kDeprecatedOrgSubtypes{
            "dosage", "old-lineage", "old-name"};
    static const unordered_map<string, COrgMod::TSubtype> 
        extra_smod_to_enum_names 
        {{ "subspecies", COrgMod::eSubtype_sub_species},
         {"host",COrgMod::eSubtype_nat_host},
         {"specific-host", COrgMod::eSubtype_nat_host}};
    return s_InitModNameToEnumMap<COrgMod::TSubtype>(
        *COrgMod::GetTypeInfo_enum_ESubtype(),
        kDeprecatedOrgSubtypes,
        extra_smod_to_enum_names
    );
}


static unordered_map<string, CSubSource::TSubtype> 
s_InitModNameSubSrcSubtypeMap(void)
{
    // some are skipped because they're handled specially and some are
    // skipped because they're deprecated
    static const unordered_set<string> skip_enum_names {
        // skip because handled specially elsewhere
        "fwd-primer-seq", "rev-primer-seq",
        "fwd-primer-name", "rev-primer-name",
        // skip because deprecated
        "transposon-name",
        "plastid-name",
        "insertion-seq-name",
    };
    static const unordered_map<string, CSubSource::TSubtype> 
        extra_smod_to_enum_names 
        {{ "sub-clone", CSubSource::eSubtype_subclone },
        { "lat-long",   CSubSource::eSubtype_lat_lon  },
        { "latitude-longitude", CSubSource::eSubtype_lat_lon },
        {  "note",  CSubSource::eSubtype_other  },
        {  "notes", CSubSource::eSubtype_other  }};  
    return s_InitModNameToEnumMap<CSubSource::TSubtype>(
            *CSubSource::GetTypeInfo_enum_ESubtype(),
            skip_enum_names,
            extra_smod_to_enum_names );
}


template<typename T, typename U> // Only works if TUmap values are unique
static unordered_map<U,T> s_GetReverseMap(const unordered_map<T,U>& TUmap) 
{
    unordered_map<U,T> UTmap;
    for (const auto& key_val : TUmap) {
        UTmap.emplace(key_val.second, key_val.first);
    }
    return UTmap;
}


class CModValueAttribs
{
public:
    using TAttribs = map<string, string>;

    CModValueAttribs(const string& value); 
    CModValueAttribs(const char* value);

    void SetValue(const string& value);
    void AddAttrib(const string& attrib_name, const string& attrib_value);
    bool HasAdditionalAttribs(void) const;

    const string& GetValue(void) const;
    const TAttribs& GetAdditionalAttribs(void) const;

private:
    string mValue;
    TAttribs mAdditionalAttribs;
};


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


using TMods = multimap<string, CModValueAttribs>;


class CModParser 
{

public:
    using TMods = multimap<string, CModValueAttribs>;

    static void Apply(const CBioseq& bioseq, TMods& mods);
private:
    static void x_ImportSeqInst(const CSeq_inst& seq_inst, TMods& mods);
    static void x_ImportHist(const CSeq_hist& seq_hist, TMods& mods);

    static void x_ImportDescriptors(const CBioseq& bioseq, TMods& mods);
    static void x_ImportDesc(const CSeqdesc& desc, TMods& mods);
    static void x_ImportUserObject(const CUser_object& user_object, TMods& mods);
    static void x_ImportDBLink(const CUser_object& user_object, TMods& mods);
    static void x_ImportGBblock(const CGB_block& gb_block, TMods& mods);
    static void x_ImportGenomeProjects(const CUser_object& user_object, TMods& mods);
    static void x_ImportTpaAssembly(const CUser_object& user_object, TMods& mods);
    static void x_ImportBioSource(const CBioSource& biosource, TMods& mods);
    static void x_ImportMolInfo(const CMolInfo& molinfo, TMods& mods);
    static void x_ImportPMID(const CPubdesc& pub_desc, TMods& mods);
    static void x_ImportOrgRef(const COrg_ref& org_ref, TMods& mods);
    static void x_ImportOrgName(const COrgName& org_name, TMods& mods);
    static void x_ImportOrgMod(const COrgMod& org_mod, TMods& mods);
    static void x_ImportSubSource(const CSubSource& subsource, TMods& mods);
    static bool x_IsUserType(const CUser_object& user_object, const string& type);

    static void x_ImportFeatureModifiers(const CSeq_annot& annot, TMods& mods);
    static void x_ImportGene(const CGene_ref& gene_ref, TMods& mods);
    static void x_ImportProtein(const CProt_ref& prot_ref, TMods& mods);
};


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
        static const unordered_map<CSeq_inst::ETopology, string, hash<underlying_type<CSeq_inst::ETopology>::type>> 
            topology_enum_to_string = {{CSeq_inst::eTopology_linear, "linear"},
                                       {CSeq_inst::eTopology_circular, "circular"},
                                       {CSeq_inst::eTopology_tandem, "tandem"},
                                       {CSeq_inst::eTopology_other, "other"}};

        const auto& topology = topology_enum_to_string.at(seq_inst.GetTopology());
        mods.emplace("topology", topology);
    }


    if (seq_inst.IsSetMol()) {
        static const unordered_map<CSeq_inst::EMol, string, hash<underlying_type<CSeq_inst::EMol>::type>>
            mol_enum_to_string = {{CSeq_inst::eMol_dna, "dna"},
                                  {CSeq_inst::eMol_rna, "rna"},
                                  {CSeq_inst::eMol_aa, "aa"},
                                  {CSeq_inst::eMol_na, "na"},
                                  {CSeq_inst::eMol_other, "other"}};
        const auto& molecule = mol_enum_to_string.at(seq_inst.GetMol());
        mods.emplace("molecule", molecule);
    }


    if (seq_inst.IsSetStrand()) {
        static const unordered_map<CSeq_inst::EStrand, string, hash<underlying_type<CSeq_inst::EStrand>::type>>
            strand_enum_to_string = {{CSeq_inst::eStrand_ss, "single"},
                                     {CSeq_inst::eStrand_ds, "double"},
                                     {CSeq_inst::eStrand_mixed, "mixed"},
                                     {CSeq_inst::eStrand_other, "other"}};
        const auto& strand = strand_enum_to_string.at(seq_inst.GetStrand());
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

static unordered_map<CBioSource::EGenome, string, hash<underlying_type<CBioSource::EGenome>::type>> genome_enum_to_string 
= { {CBioSource::eGenome_unknown, "unknown"},
    {CBioSource::eGenome_genomic, "genomic" },
    {CBioSource::eGenome_chloroplast, "chloroplast"},
    {CBioSource::eGenome_kinetoplast, "kinetoplast"}, 
    {CBioSource::eGenome_mitochondrion, "mitochondrial"},
    {CBioSource::eGenome_plastid, "plastid"},
    {CBioSource::eGenome_macronuclear, "macronuclear"},
    {CBioSource::eGenome_extrachrom, "extrachromosomal"},
    {CBioSource::eGenome_plasmid, "plasmid"},
    {CBioSource::eGenome_transposon, "transposon"},
    {CBioSource::eGenome_insertion_seq, "insertion sequence"},
    {CBioSource::eGenome_cyanelle, "cyanelle"},
    {CBioSource::eGenome_proviral, "provirus"},
    {CBioSource::eGenome_virion, "virion"},
    {CBioSource::eGenome_nucleomorph, "nucleomorph"},
    {CBioSource::eGenome_apicoplast, "apicoplase"},
    {CBioSource::eGenome_leucoplast, "leucoplast"},
    {CBioSource::eGenome_proplastid, "proplastid"},
    {CBioSource::eGenome_endogenous_virus, "endogenous virus"},
    {CBioSource::eGenome_hydrogenosome, "hydrogenosome"},
    {CBioSource::eGenome_chromosome, "chromosome" },
    {CBioSource::eGenome_chromatophore, "chromatophore"},
    {CBioSource::eGenome_plasmid_in_mitochondrion, "plasmid in mitochondrion"},
    {CBioSource::eGenome_plasmid_in_plastid, "plasmid in plastid"}
};


static unordered_map<CBioSource::EOrigin, string, hash<underlying_type<CBioSource::EOrigin>::type>> origin_enum_to_string
= { {CBioSource::eOrigin_unknown, "unknown"},
    {CBioSource::eOrigin_natural, "natural"},
    {CBioSource::eOrigin_natmut, "natural mutant"},
    {CBioSource::eOrigin_mut, "mutant"},
    {CBioSource::eOrigin_artificial, "artificial"},
    {CBioSource::eOrigin_synthetic, "synthetic"},
    {CBioSource::eOrigin_other, "other"}
};


void CModParser::x_ImportBioSource(const CBioSource& biosource, TMods& mods)
{
    if (biosource.IsSetGenome()) {
        auto e_genome = static_cast<CBioSource::EGenome>(biosource.GetGenome());
        _ASSERT(genome_enum_to_string.find(e_genome) != genome_enum_to_string.end());
        const auto& genome_string = genome_enum_to_string[e_genome]; 
        mods.emplace("location", genome_string);
    }

    if (biosource.IsSetOrigin()) {
        auto e_origin = static_cast<CBioSource::EOrigin>(biosource.GetOrigin());
        _ASSERT(origin_enum_to_string.find(e_origin) != origin_enum_to_string.end());
        const auto& origin_string = origin_enum_to_string[e_origin];
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


static const auto subsource_string_to_enum = s_InitModNameSubSrcSubtypeMap();


void CModParser::x_ImportSubSource(const CSubSource& subsource, TMods& mods)
{
    static const auto subsource_enum_to_string = s_GetReverseMap(subsource_string_to_enum);
    const auto& subtype = subsource_enum_to_string.at(subsource.GetSubtype());
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


static const auto orgmod_string_to_enum = s_InitModNameOrgSubtypeMap();


void CModParser::x_ImportOrgMod(const COrgMod& org_mod, TMods& mods)
{
    static const auto orgmod_enum_to_string = s_GetReverseMap(orgmod_string_to_enum);
    const auto& subtype = orgmod_enum_to_string.at(org_mod.GetSubtype());
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

static const
unordered_map<CMolInfo::TBiomol, string> biomol_enum_to_string =
{{CMolInfo::eBiomol_cRNA, "cRNA"},
 {CMolInfo::eBiomol_genomic, "Genomic"},
 {CMolInfo::eBiomol_mRNA, "mRNA"},
 {CMolInfo::eBiomol_ncRNA, "ncRNA"},
 {CMolInfo::eBiomol_other_genetic, "Other-Genetic"},
 {CMolInfo::eBiomol_pre_RNA, "Precursor RNA"},
 {CMolInfo::eBiomol_rRNA, "rRNA"},
 {CMolInfo::eBiomol_transcribed_RNA, "Transcribed RNA"},
 {CMolInfo::eBiomol_tmRNA, "tmRNA"},
 {CMolInfo::eBiomol_tRNA, "tRNA"}
};

static const 
unordered_map<string, CMolInfo::TTech>
tech_string_to_enum = {
    { "?",                  CMolInfo::eTech_unknown },
    { "barcode",            CMolInfo::eTech_barcode },
    { "both",               CMolInfo::eTech_both },
    { "composite-wgs-htgs", CMolInfo::eTech_composite_wgs_htgs },
    { "concept-trans",      CMolInfo::eTech_concept_trans },
    { "concept-trans-a",    CMolInfo::eTech_concept_trans_a },
    { "derived",            CMolInfo::eTech_derived },
    { "EST",                CMolInfo::eTech_est },
    { "fli cDNA",           CMolInfo::eTech_fli_cdna },
    { "genetic map",        CMolInfo::eTech_genemap },
    { "htc",                CMolInfo::eTech_htc },
    { "htgs 0",             CMolInfo::eTech_htgs_0 },
    { "htgs 1",             CMolInfo::eTech_htgs_1 },
    { "htgs 2",             CMolInfo::eTech_htgs_2 },
    { "htgs 3",             CMolInfo::eTech_htgs_3 },
    { "physical map",       CMolInfo::eTech_physmap },
    { "seq-pept",           CMolInfo::eTech_seq_pept },
    { "seq-pept-homol",     CMolInfo::eTech_seq_pept_homol },
    { "seq-pept-overlap",   CMolInfo::eTech_seq_pept_overlap },
    { "standard",           CMolInfo::eTech_standard },
    { "STS",                CMolInfo::eTech_sts },
    { "survey",             CMolInfo::eTech_survey },
    { "targeted",           CMolInfo::eTech_targeted },
    { "tsa",                CMolInfo::eTech_tsa },
    { "wgs",                CMolInfo::eTech_wgs }
};


static const auto tech_enum_to_string  = s_GetReverseMap(tech_string_to_enum);


static const 
unordered_map<string, CMolInfo::TCompleteness> 
completeness_string_to_enum = {
    { "complete",  CMolInfo::eCompleteness_complete  },
    { "has-left",  CMolInfo::eCompleteness_has_left  },
    { "has-right", CMolInfo::eCompleteness_has_right  },
    { "no-ends",   CMolInfo::eCompleteness_no_ends  },
    { "no-left",   CMolInfo::eCompleteness_no_left  },
    { "no-right",  CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};


static const auto completeness_enum_to_string = s_GetReverseMap(completeness_string_to_enum);


void CModParser::x_ImportMolInfo(const CMolInfo& mol_info, TMods& mods)
{
    if (mol_info.IsSetBiomol()) {
        const string& moltype = biomol_enum_to_string.at(mol_info.GetBiomol());
        mods.emplace("moltype", moltype);
    }
    if (mol_info.IsSetTech()) {
        const string& tech =  tech_enum_to_string.at(mol_info.GetTech());
        mods.emplace("tech", tech);
    }

    if (mol_info.IsSetCompleteness()) {
        const string& completeness = completeness_enum_to_string.at(mol_info.GetCompleteness());
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

END_SCOPE(objects)
END_NCBI_SCOPE



