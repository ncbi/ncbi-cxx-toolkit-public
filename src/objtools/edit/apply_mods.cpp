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
*   Class for applying modifiers to ASN.1 objects
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <sstream>
#include <corelib/ncbiutil.hpp>
#include <corelib/ncbi_autoinit.hpp>
#include <util/static_map.hpp>
#include <serial/enumvalues.hpp>

#include <objects/general/Dbtag.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/general/User_field.hpp>
#include <objects/misc/sequence_macros.hpp>
#include <objects/pub/Pub.hpp>
#include <objects/pub/Pub_equiv.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seq/Pubdesc.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/Seq_hist_rec.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seqdesc.hpp>
#include <objects/seqfeat/Org_ref.hpp>
#include <objects/seqfeat/OrgMod.hpp>
#include <objects/seqfeat/OrgName.hpp>
#include <objects/seqfeat/PCRReactionSet.hpp>
#include <objects/seqfeat/PCRReaction.hpp>
#include <objects/seqfeat/PCRPrimer.hpp>
#include <objects/seqfeat/PCRPrimerSet.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/SubSource.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_loc.hpp>

#include <objtools/edit/dblink_field.hpp>
#include <objtools/edit/apply_mods.hpp>

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>
#include <functional>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(edit)

static unordered_map<string, CMolInfo::TBiomol> 
MolInfoToBiomol = {
    {"cRNA",                    CMolInfo::eBiomol_cRNA },   
    {"DNA",                     CMolInfo::eBiomol_genomic},   
    {"Genomic",                 CMolInfo::eBiomol_genomic},   
    {"Genomic DNA",             CMolInfo::eBiomol_genomic},   
    {"Genomic RNA",             CMolInfo::eBiomol_genomic},   
    {"mRNA",                    CMolInfo::eBiomol_mRNA},   
    {"ncRNA",                   CMolInfo::eBiomol_ncRNA},
    {"non-coding RNA",          CMolInfo::eBiomol_ncRNA},   
    {"Other-Genetic",           CMolInfo::eBiomol_other_genetic}, 
    {"Precursor RNA",           CMolInfo::eBiomol_pre_RNA},   
    {"Ribosomal RNA",           CMolInfo::eBiomol_rRNA},   
    {"rRNA",                    CMolInfo::eBiomol_rRNA},   
    {"Transcribed RNA",         CMolInfo::eBiomol_transcribed_RNA},   
    {"Transfer-messenger RNA",  CMolInfo::eBiomol_tmRNA},   
    {"Transfer RNA",            CMolInfo::eBiomol_tRNA},   
    {"tRNA",                    CMolInfo::eBiomol_tRNA},   
};


static unordered_map<string, CMolInfo::TTech>
MolInfoToTech = {
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


static unordered_map<string, CMolInfo::TCompleteness> 
MolInfoToCompleteness = {
    { "complete",  CMolInfo::eCompleteness_complete  },
    { "has-left",  CMolInfo::eCompleteness_has_left  },
    { "has-right", CMolInfo::eCompleteness_has_right  },
    { "no-ends",   CMolInfo::eCompleteness_no_ends  },
    { "no-left",   CMolInfo::eCompleteness_no_left  },
    { "no-right",  CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};

using TModNameSet = set<string>;

template<typename TEnum>
map<string, TEnum> s_InitModNameToEnumMap(
    const CEnumeratedTypeValues& etv,
    const TModNameSet& skip_enum_names,
    const map<string, TEnum>&  extra_enum_names_to_vals)

{
    using TModNameEnumMap = map<string, TEnum>;
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


unordered_set<string> s_InitModNames(
        const CEnumeratedTypeValues& etv,
        const unordered_set<string>& deprecated,
        const unordered_set<string>& extra) 
{
    unordered_set<string> mod_names;

    for (const auto& name_val : etv.GetValues()) {
        const auto& enum_name = name_val.first;
        if (deprecated.find(enum_name) != deprecated.end()) {
            mod_names.insert(enum_name);
        }
    }

    for (const auto& name : extra) {
        mod_names.insert(name);
    }

    return mod_names;
}


//using TModNameOrgSubtypeMap = map<CSourceModParser::SMod, COrgMod::ESubtype>;
using TModNameOrgSubtypeMap = map<string, COrgMod::ESubtype>;

TModNameOrgSubtypeMap s_InitModNameOrgSubtypeMap(void)
{
    const TModNameSet kDeprecatedOrgSubtypes{
            "dosage", "old-lineage", "old-name"};
    const map<string, COrgMod::ESubtype> extra_smod_to_enum_names {
        { "subspecies",    COrgMod::eSubtype_sub_species },
        { "host",          COrgMod::eSubtype_nat_host    },
        { "specific-host", COrgMod::eSubtype_nat_host    },
    };

    return s_InitModNameToEnumMap<COrgMod::ESubtype>(
        *COrgMod::GetTypeInfo_enum_ESubtype(),
        kDeprecatedOrgSubtypes,
        extra_smod_to_enum_names
    );
}

//using TModNameSubSrcSubtype = map<CSourceModParser::SMod,CSubSource::ESubtype>;
using TModNameSubSrcSubtype = map<string,CSubSource::ESubtype>;

TModNameSubSrcSubtype s_InitModNameSubSrcSubtypeMap(void)
{
    // some are skipped because they're handled specially and some are
    // skipped because they're deprecated
    TModNameSet skip_enum_names {
        // skip because handled specially elsewhere
        "fwd_primer_seq", "rev_primer_seq",
        "fwd_primer_name", "rev_primer_name",
        "fwd_PCR_primer_seq", "rev_PCR_primer_seq",
        "fwd_PCR_primer_name", "rev_PCR_primer_name",
        // skip because deprecated
        "transposon_name",
        "plastid_name",
        "insertion_seq_name",
    };
    const TModNameSubSrcSubtype extra_smod_to_enum_names {
        { "sub-clone",          CSubSource::eSubtype_subclone },
        { "lat-long",           CSubSource::eSubtype_lat_lon  },
        { "latitude-longitude", CSubSource::eSubtype_lat_lon  },
    };
    return s_InitModNameToEnumMap<CSubSource::ESubtype>(
            *CSubSource::GetTypeInfo_enum_ESubtype(),
            skip_enum_names,
            extra_smod_to_enum_names );
}


struct SModNameInfo {
    using TNames = unordered_set<string>;

    static bool IsSeqInstMod(const string& name)
    {
        return (seq_inst_mods.find(name) != seq_inst_mods.end());
    } 

    static bool IsAnnotMod(const string& name) 
    {
        return (annot_mods.find(name) != annot_mods.end());
    }

    static bool IsOrgRefMod(const string& name) 
    {
        return (orgref_mods.find(name) != orgref_mods.end());
    }

    static bool IsDescrMod(const string& name)
    {
        return (descr_mods.find(name) != descr_mods.end());
    }

    static TNames seq_inst_mods;
    static TNames annot_mods;
    static TNames orgref_mods; // not org_mods
    static TNames subsource_mods;
    static TNames org_mods;
    //static TNames pcr_mods;
    static TNames descr_mods;
};

SModNameInfo::TNames SModNameInfo::seq_inst_mods =    
    { "top", "topology", 
    "molecule", "mol", "moltype", "mol_type",
    "strand", 
    "secondary_accession", "secondary_accessions" };

SModNameInfo::TNames SModNameInfo::annot_mods = 
    { "gene", "allele", "gene_syn", "gene_synonym", "locus_tag"
      "prot", "protein", "prot_desc", "protein_desc", 
      "EC_number", "activity", "function" };

// org mods is handled separately
SModNameInfo::TNames SModNameInfo::orgref_mods = 
    { "division", "div", "lineage", "gcode", 
      "mgcode", "pgcode", "taxid"
    };

// These should be initialized using a map - less chance of error that way
SModNameInfo::TNames SModNameInfo::subsource_mods =
    s_InitModNames(*CSubSource::GetTypeInfo_enum_ESubtype(),
                   {"fwd_primer_seq", "rev_primer_seq", 
                    "fwd_primer_name", "rev_primer_name",
                    "transposon_name", 
                    "plastid_name", 
                    "insertion_seq_name"},
                  {"sub-clone", "lat-long", "latitude-longitude"});
                    

SModNameInfo::TNames SModNameInfo::org_mods = 
    s_InitModNames(*COrgMod::GetTypeInfo_enum_ESubtype(), 
                   {"dosage", "old-lineage", "old-name"},
                   {"subspecies", "host", "specific-host"});

SModNameInfo::TNames SModNameInfo::descr_mods = 
    {"comment", 
     "moltype", "mol_type", "tech", "completeness", "completedness", // MolInfo
     "secondary_accession", "secondary_accessions", "keyword", "keywords", // GBblock
     "primary", "primary_accessions", // TPA
     "PubMed", "PMID" // Publication Mods
    };


//#include "mod_info.hpp" 

struct SModContainer
{
    using TMods = multimap<string, string>;
    using TMod = TMods::value_type;

    void AddMod(const string& name, const string& value) {
        AddMod(TMod(name, value));
    }

    void AddMod(const TMod& name_value) {
        const auto& mod_name = name_value.first;
        if (SModNameInfo::IsDescrMod(mod_name)) {
            descr_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsSeqInstMod(mod_name)) {
            seq_inst_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsAnnotMod(mod_name)) {
            annot_mods.insert(name_value);
        }
    }
        
    TMods seq_inst_mods;
    TMods annot_mods;
    TMods descr_mods;
};


class CDescriptorCache 
{
public:
    CDescriptorCache(CBioseq& bioseq);

    CRef<CSeqdesc> SetDescriptor(const string& descr_name,
                                 function<bool(const CSeqdesc&)> f_verify,
                                 function<CRef<CSeqdesc>(void)> f_create);
private:
    using TMap = unordered_map<string, CRef<CSeqdesc>>;
    TMap m_Cache;
    CBioseq& m_Bioseq;
};


CDescriptorCache::CDescriptorCache(CBioseq& bioseq) : m_Bioseq(bioseq) {}


CRef<CSeqdesc> CDescriptorCache::SetDescriptor(const string& descr_name, 
                                               function<bool(const CSeqdesc&)> f_verify,
                                               function<CRef<CSeqdesc>(void)> f_create)
{

    auto it = m_Cache.find(descr_name);
    if (it != m_Cache.end()) {
        return it->second;
    }

    // Search for descriptor on Bioseq
    if (m_Bioseq.IsSetDescr()) {
        for (auto& pDesc : m_Bioseq.SetDescr().Set()) {
            if (pDesc.NotEmpty() && f_verify(*pDesc)) {
                m_Cache.insert(make_pair(descr_name, pDesc));
                return pDesc;
            }
        }
    }

    // else create Descr - Need a method to initialise the descriptor
    auto pDesc = f_create();
    m_Cache.insert(make_pair(descr_name, pDesc));
    m_Bioseq.SetDescr().Set().push_back(pDesc);
    return pDesc;
}


static bool s_IsModNameMatch(const string& name, const unordered_set<string>& expected) 
{   
    return (expected.find(name) != expected.end());
}

class CModApply_Impl {
public:

    using TMods = SModContainer::TMods;
    using TMod = SModContainer::TMod;

    CModApply_Impl(void) = default;
    ~CModApply_Impl(void);

    void Apply(CBioseq& bioseq);

    void AddMod(const string& name, const string& value) {
        m_Mods.AddMod(name, value);
    }

private:
    void x_ApplySeqInstMods(const TMods& mods, CSeq_inst& inst);
    bool x_AddTopology(const TMod& mod, CSeq_inst& inst);
    bool x_AddMolType(const TMod& mod, CSeq_inst& inst);
    bool x_AddStrand(const TMod& mod, CSeq_inst& inst);
    bool x_AddHist(const TMod& mod, CSeq_inst& inst);
    bool x_CreateGene(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData);
    bool x_CreateProtein(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData);
    void x_AddNonBioSourceDescriptors(const TMods& mods, CBioseq& bioseq);
    bool x_AddPubMods(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddGenomeProjectsDBMods(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddTPAMods(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddDBLinkMods(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddMolInfo_RemoveUsedMods(TMods& mods, CDescriptorCache& desc_cache);
    bool x_AddGBblock_RemoveUsedMods(TMods& mods, CDescriptorCache& desc_cache);
    bool x_AddComment(const TMod& mod, CRef<CSeqdesc>& pDesc);
    bool x_AddBioSourceMods(const TMods& mods, CRef<CSeqdesc>& pDesc);
    SModContainer m_Mods;
};


CModApply_Impl::~CModApply_Impl() = default;


void CModApply_Impl::Apply(CBioseq& bioseq) 
{
    const auto& seq_inst_mods = m_Mods.seq_inst_mods;
    if (!seq_inst_mods.empty()) {
        x_ApplySeqInstMods(seq_inst_mods, bioseq.SetInst());
    }

    const auto& nonbiosrc_descr_mods = m_Mods.descr_mods;
    if (!nonbiosrc_descr_mods.empty()) {
        x_AddNonBioSourceDescriptors(nonbiosrc_descr_mods, bioseq);
    }
}

/*
static void s_SetGenome(const string& genome, CBioSource& biosource) 
{

    if (NStr::EqualNocase(genome, "mitochondrial")) {
        biosource.SetGenome(CBioSource::eGenome_mitochondrion);
    }
    else 
    if (NStr::EqualNocase(genome, "provirus")) {
        biosource.SetGenome(CBioSource::eGenome_proviral);
    }
    else 
    if (NStr::EqualNocase(genome, "extrachromosomal")) {
        biosource.SetGenome(CBioSource::eGenome_extrachrom);
    }
    else
    if (NStr::EqualNocase(genome, "insertion sequence")) 
    {
        biosource.SetGenome(CBioSource::eGenome_insertion_seq);
    }
    else {
        // try to find a valid genome - need to complete this
    }
}
*/

bool CModApply_Impl::x_AddBioSourceMods(const TMods& mods, CRef<CSeqdesc>& pDesc) 
{
/*
    // location
    for (const auto& mod : mods) {
    }
  */  
    // origin

    // PCR primers
    
    // subsource

    // OrgRef
    // orgmods

    return false;
}

// bool CModApply_Impl::x_AddOrgrefMods() // Implement a method for OrgrefMods



void CModApply_Impl::x_AddNonBioSourceDescriptors(const TMods& mods, CBioseq& bioseq)
{
    // Another approach might be to pass an iterator to these methods 
    // and advance the iterator 
    // within the method - I need to think a bit more about this approach

    CDescriptorCache descriptor_cache(bioseq);

    TMods remaining_mods;
    for (const auto& mod : mods) {
        if (x_AddTPAMods(mod, descriptor_cache) ||
            x_AddPubMods(mod, descriptor_cache) ||
            x_AddGenomeProjectsDBMods(mod, descriptor_cache) ||
            x_AddDBLinkMods(mod, descriptor_cache)) {
            continue;
        }
        CRef<CSeqdesc> pDesc(new CSeqdesc());
        if (x_AddComment(mod, pDesc)) {
            bioseq.SetDescr().Set().push_back(pDesc);
        }
        else {
            remaining_mods.insert(mod);
        }
    }

    if (!remaining_mods.empty()) {
        x_AddMolInfo_RemoveUsedMods(remaining_mods, descriptor_cache);
    }

    if (!remaining_mods.empty()) {
        x_AddGBblock_RemoveUsedMods(remaining_mods, descriptor_cache);
    }
}



bool CModApply_Impl::x_AddDBLinkMods(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;

    if (s_IsModNameMatch(name, {"SRA", "bioproject", "biosample"})) { 

        auto pDesc = descriptor_cache.SetDescriptor("DBLink", // use an enum instead of a string
            [](const CSeqdesc& desc) {
                bool result = false;
                if (desc.IsUser() &&
                    desc.GetUser().IsDBLink()) {
                    result = true;
                }
                return result;
            },
            []() {
                auto pDesc = Ref(new CSeqdesc());
                pDesc->SetUser().SetObjectType(CUser_object::eObjectType_DBLink);
                return pDesc;
            });

        if (NStr::EqualNocase(name, "SRA")) {
            CDBLink::SetSRA(pDesc->SetUser(), mod.second, eExistingText_add_qual);
        }
        else if (NStr::EqualNocase(name, "biosample")) {
            CDBLink::SetBioSample(pDesc->SetUser(), mod.second, eExistingText_add_qual);
        }
        else if (NStr::EqualNocase(name, "bioproject")) {
            CDBLink::SetBioSample(pDesc->SetUser(), mod.second, eExistingText_add_qual);
        } // Only options - Add an ASSERT here(?)
        return true;      
    } 

    return false;
}


bool CModApply_Impl::x_AddGenomeProjectsDBMods(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsModNameMatch(name, {"project", "projects"})) {
        auto pDesc = descriptor_cache.SetDescriptor("GenomeProjects", 
            [](const CSeqdesc& desc) {
                return (desc.IsUser() &&
                desc.GetUser().IsSetType() &&
                desc.GetUser().GetType().IsStr() &&
                desc.GetUser().GetType().GetStr() == "GenomeProjectsDB"); },
            []() {
                auto pDesc = Ref(new CSeqdesc());
                pDesc->SetUser().SetType().SetStr("GenomeProjectsDB");
                return pDesc; }
                );

        list<string> ids;
        NStr::Split(mod.second, ",;", ids, NStr::fSplit_Tokenize);
        for (const auto& id_string : ids) {
            const auto id = NStr::StringToUInt(id_string, NStr::fConvErr_NoThrow);
            if (id) {
                auto pField = Ref(new CUser_field());
                auto pSubfield = Ref(new CUser_field());
                pField->SetLabel().SetId(0);
                pSubfield->SetLabel().SetStr("ProjectID");
                pSubfield->SetData().SetInt(id);
                pField->SetData().SetFields().push_back(pSubfield);
                pSubfield.Reset(new CUser_field());
                pSubfield->SetLabel().SetStr("ParentID");
                pSubfield->SetData().SetInt(0);
                pField->SetData().SetFields().push_back(pSubfield);
                pDesc->SetUser().SetData().push_back(move(pField));
            }
        }
        return true;
    }

    return false;
}


bool CModApply_Impl::x_AddComment(const TMod& mod, CRef<CSeqdesc>& pDesc) 
{
    if (NStr::EqualNocase("comment", mod.first)) {
        pDesc->SetComment(mod.second);
        return true;
    }
    return false;
}


bool CModApply_Impl::x_AddMolInfo_RemoveUsedMods(TMods& mods, CDescriptorCache& descriptor_cache) 
{
    bool has_molinfo = false;

    auto mod_it = mods.begin();
    while (mod_it!=mods.end()) {
        const auto& name = mod_it->first;
        const auto& value = mod_it->second;
        if (s_IsModNameMatch(name, {"moltype", "mol_type"})) {
            has_molinfo = true;
            mods.erase(mod_it);
            auto it = MolInfoToBiomol.find(value);
            if (it != MolInfoToBiomol.end()) {
                auto pDesc = descriptor_cache.SetDescriptor("MolInfo", 
                        [](const CSeqdesc& desc) { return desc.IsMolinfo(); },
                        []() { return Ref(new CSeqdesc()); });
                pDesc->SetMolinfo().SetBiomol(it->second);
            }
            else {
                // x_HandleBadMod
            }
        }
        else 
        if (NStr::EqualNocase(name, "tech")) {
            has_molinfo = true;
            mods.erase(mod_it);
            auto it = MolInfoToTech.find(value);
            if (it != MolInfoToTech.end()) {
                auto pDesc = descriptor_cache.SetDescriptor("MolInfo", 
                        [](const CSeqdesc& desc) { return desc.IsMolinfo(); },
                        []() { return Ref(new CSeqdesc()); });
                pDesc->SetMolinfo().SetTech(it->second);
            }
            else {
            // x_HandleBadMod
            }
        }
        else
        if (s_IsModNameMatch(name, {"completeness", "completedness"})) {
            has_molinfo = true;
            mods.erase(mod_it);
            auto it = MolInfoToCompleteness.find(value);
            if (it != MolInfoToCompleteness.end()) {
                auto pDesc = descriptor_cache.SetDescriptor("MolInfo", 
                        [](const CSeqdesc& desc) { return desc.IsMolinfo(); },
                        []() { return Ref(new CSeqdesc()); });
                pDesc->SetMolinfo().SetCompleteness(it->second);
            }
            else {
                // x_HandleBad Mod
            }
        }
        else {
          ++mod_it;
        }
    }
    return has_molinfo;
}


bool CModApply_Impl::x_AddGBblock_RemoveUsedMods(TMods& mods, CDescriptorCache& descriptor_cache)
{
    bool has_gbblock = false;

    auto mod_it = mods.begin();
    while (mod_it!=mods.end()) { // Need to refactor this to avoid duplication
        const auto& name = mod_it->first;
        const auto& value = mod_it->second;
        if (s_IsModNameMatch(name, {"secondary_accession", "secondary_accessions"})) {
            has_gbblock = true;
            auto pDesc = descriptor_cache.SetDescriptor("GBblock", 
                                                        [](const CSeqdesc& desc) { return desc.IsGenbank(); },
                                                        []() { return Ref(new CSeqdesc()); });
            list<CTempString> ranges;
            NStr::Split(value, ",", ranges, NStr::fSplit_Tokenize);
            for (const auto& range : ranges) {
                string s = NStr::TruncateSpaces_Unsafe(range);
                try {
                    SSeqIdRange idrange(s);
                    for (auto it = idrange.begin(); it != idrange.end(); ++it) {
                        pDesc->SetGenbank().SetExtra_accessions().push_back(*it);
                    }
                }
                catch (CSeqIdException&) {
                    pDesc->SetGenbank().SetExtra_accessions().push_back(s); 
                }
            } 
            mod_it = mods.erase(mod_it); // I don't really want to erase mods, I don't think. 
        }                                // Advance iterator instead
        else 
        if (s_IsModNameMatch(name, {"keyword", "keywords"})) {
            has_gbblock = true;
            auto pDesc = descriptor_cache.SetDescriptor("GBblock", 
                                                        [](const CSeqdesc& desc) { return desc.IsGenbank(); },
                                                        []() { return Ref(new CSeqdesc()); });
            list<string> keywords;
            NStr::Split(value, ",;",  keywords, NStr::fSplit_Tokenize);
            for (const auto& keyword : keywords) {
                pDesc->SetGenbank().SetKeywords().push_back(keyword);
            }
            mod_it = mods.erase(mod_it);
        }
        else {
            ++mod_it;
        }
    }
    return has_gbblock;
}


bool CModApply_Impl::x_AddPubMods(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsModNameMatch(name, {"PubMed", "PMID"})) {
        auto pmid = NStr::StringToNumeric<int>(mod.second, NStr::fConvErr_NoThrow); 
        // Need to add proper error handling here
        auto pDesc = descriptor_cache.SetDescriptor("Pub", 
                                                    [](const CSeqdesc& desc) { return desc.IsPub(); },
                                                    []() { return Ref(new CSeqdesc()); }); 

        auto pPub = Ref(new CPub());
        pPub->SetPmid().Set(pmid);
        pDesc->SetPub().SetPub().Set().push_back(pPub);
        return true;
    }
    return false;
}


void CModApply_Impl::x_ApplySeqInstMods(const TMods& mods, CSeq_inst& seq_inst)
{
    for (const auto& mod : mods) {
        if (x_AddTopology(mod, seq_inst) ||
            x_AddMolType(mod, seq_inst) ||
            x_AddStrand(mod, seq_inst) ||
            x_AddHist(mod, seq_inst)) {
            
        }
        else {
            // report an error - unrecognised modifier
        }
    }
}

bool CModApply_Impl::x_AddTopology(const TMod& mod, CSeq_inst& seq_inst)
{
    if (!s_IsModNameMatch(mod.first, {"topology", "top"})) {
        return false;
    }

    if (seq_inst.IsSetTopology()) {
        // Attempting to modify topology. Post a warning or an error.
    }

    if (NStr::EqualNocase(mod.second, "linear")) {
        seq_inst.SetTopology(CSeq_inst::eTopology_linear);
    }
    else 
    if (NStr::EqualNocase(mod.second, "circular")) {
        seq_inst.SetTopology(CSeq_inst::eTopology_circular);
    }
    else {
        // Handle bad mod value
    }
    return true;
}


bool CModApply_Impl::x_AddMolType(const TMod& mod, CSeq_inst& seq_inst)
{
    const auto& name = mod.first;

    bool is_mol = s_IsModNameMatch(name, {"molecule", "mol"});
    bool is_moltype;
    if (!is_mol) {
        is_moltype = s_IsModNameMatch(name, {"moltype", "mol_type"});
    }

    if (!is_mol && !is_moltype) {
        return false;
    }
    
    if (seq_inst.IsSetMol() &&
        seq_inst.IsAa()) {
        return true;        
    }

    const auto& value = mod.second;
    if (is_mol) {
        if (NStr::EqualNocase(value, "dna")) {
            seq_inst.SetMol(CSeq_inst::eMol_dna);
        }
        else if (NStr::EqualNocase(value, "rna")) {
            seq_inst.SetMol(CSeq_inst::eMol_rna);
        }
        else {
            // x_HandleBadModValue
        }
    }
    else { // is_moltype    
    /*
        const auto& it = sc_BiomolMap.find(value);
        if (it == sc_BiomolMap.end()) {
            // Handle Bad Mod
        }
        else {
            seq_inst.SetMol(it->second.m_eMol);
        }
        */
    }

    return true;
}


bool CModApply_Impl::x_AddStrand(const TMod& mod, CSeq_inst& seq_inst)
{
    if (!NStr::EqualNocase(mod.first,"strand")) {
        return false;
    }

    const auto& value = mod.second;
    if (NStr::EqualNocase(value, "single")) {
        seq_inst.SetStrand( CSeq_inst::eStrand_ss );
    } else if (NStr::EqualNocase(value, "double")) {
        seq_inst.SetStrand( CSeq_inst::eStrand_ds );
    } else if (NStr::EqualNocase(value, "mixed")) {
        seq_inst.SetStrand( CSeq_inst::eStrand_mixed );
    } else {
        // x_HandleBadModValue(*mod);
    }
    return true;
}


bool CModApply_Impl::x_AddHist(const TMod& mod, CSeq_inst& seq_inst)
{
    if (!s_IsModNameMatch(mod.first, {"secondary_accession", "secondary_accessions"})) {
        return false;
    }

    list<CTempString> ranges;
    NStr::Split(mod.second, ",", ranges, NStr::fSplit_MergeDelimiters);
    for (const auto& range : ranges) {
        string s = NStr::TruncateSpaces_Unsafe(range);
        try {
            SSeqIdRange idrange(s);
            for (auto it = idrange.begin(); it != idrange.end(); ++it) {
                seq_inst.SetHist().SetReplaces().SetIds().push_back(it.GetID());
            }
        }
        catch (CSeqIdException&) {
           NStr::ReplaceInPlace(s, "ref_seq|", "ref|", 0, 1);
           seq_inst.SetHist().SetReplaces().SetIds().push_back(Ref(new CSeq_id(s))); 
        }
    }

    return true;
}


bool CModApply_Impl::x_CreateGene(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData)
{
    
    for (const auto& mod : mods) {
        const auto& name = mod.first;
        if (NStr::EqualNocase(name, "gene")) {
            pFeatData->SetGene().SetLocus(mod.second);
        }
        else 
        if (NStr::EqualNocase(name, "allele")) {
            pFeatData->SetGene().SetAllele(mod.second);
        }
        else
        if (s_IsModNameMatch(name, {"gene_syn", "gene_synonym"})) {
            pFeatData->SetGene().SetSyn().push_back(mod.second);
        }
        else
        if (s_IsModNameMatch(name, {"locus_tag"})) { // Change s_IsModNameMatch
            pFeatData->SetGene().SetLocus_tag(mod.second);
        }
    }
    return pFeatData.IsInitialized();
}


bool CModApply_Impl::x_CreateProtein(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData) 
{

    for (const auto& mod : mods) {
        const auto& name = mod.first;
        
        if (s_IsModNameMatch(name, {"protein", "prot"})) {
            pFeatData->SetProt().SetName().push_back(mod.second);
        } 
        else 
        if (s_IsModNameMatch(name, {"prot_desc", "protein_desc"})) {
            pFeatData->SetProt().SetDesc(mod.second);
        }
        else 
        if (s_IsModNameMatch(name, {"EC_number"})) {
            pFeatData->SetProt().SetEc().push_back(mod.second);
        }
        else 
        if (s_IsModNameMatch(name, {"activity", "function"})) {
            pFeatData->SetProt().SetActivity().push_back(mod.second);
        }
    }
    return pFeatData.IsInitialized();
}



bool CModApply_Impl::x_AddTPAMods(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsModNameMatch(name, {"primary", "primary_accessions"})) {
        list<string> accession_list;
        NStr::Split(mod.second, ",", accession_list, NStr::fSplit_MergeDelimiters);
       
        auto pDesc = descriptor_cache.SetDescriptor("TPA", 
                [](const CSeqdesc& desc) {
                    return (desc.IsUser() &&
                            desc.GetUser().IsSetType() &&
                            desc.GetUser().GetType().IsStr() &&
                            desc.GetUser().GetType().GetStr() == "TpaAssembly"); 
                    },
                [](){ auto pDesc = Ref(new CSeqdesc()); 
                      pDesc->SetUser().SetType().SetStr("TpaAssembly");
                      return pDesc;
                    }
                );
        auto& user = pDesc->SetUser();
            
        for (const auto& accession : accession_list) {
            auto pField = Ref(new CUser_field());
            pField->SetLabel().SetId(0);
            auto pSubfield  = Ref(new CUser_field());
            pSubfield->SetLabel().SetStr("accession");
            pSubfield->SetData().SetStr(CUtf8::AsUTF8(accession, eEncoding_UTF8));
            pField->SetData().SetFields().push_back(move(pSubfield));
            user.SetData().push_back(move(pField));
        }
        return true;
    }
    return false;
}


CModApply::CModApply() : m_pImpl(new CModApply_Impl()) {}


CModApply::~CModApply() = default;


void CModApply::Apply(CBioseq& bioseq)  
{
    m_pImpl->Apply(bioseq);
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
