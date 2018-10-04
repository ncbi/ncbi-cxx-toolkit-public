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

#include <objtools/edit/apply_mods.hpp>

#include <unordered_set>
#include <unordered_map>
#include <set>
#include <map>
#include <functional>
#include <type_traits>

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
        if (deprecated.find(enum_name) == deprecated.end()) {
            mod_names.insert(enum_name);
        }
    }

    for (const auto& name : extra) {
        mod_names.insert(name);
    }

    return mod_names;
}


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
        { "latitude-longitude", CSubSource::eSubtype_lat_lon },
        {  "note",              CSubSource::eSubtype_other  },
        {  "notes",             CSubSource::eSubtype_other  },
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

    static bool IsStructuredOrgRefMod(const string& name) 
    {
        return (non_orgmod_orgref_mods.find(name) != non_orgmod_orgref_mods.end());
    }

    static bool IsNonBiosourceDescrMod(const string& name)
    {
        return (non_biosource_descr_mods.find(name) != non_biosource_descr_mods.end());
    }

    static bool IsTopLevelBioSourceMod(const string& name) 
    {
        return (NStr::EqualNocase(name, "origin") || 
                NStr::EqualNocase(name, "location") || 
                NStr::EqualNocase(name, "focus"));
    }

    static bool IsPCRPrimerMod(const string& name) {
        return false;
    }

    static bool IsSubSourceMod(const string& name) {
        return (subsource_mods.find(name) != subsource_mods.end());
    }

    static bool IsOrgMod(const string& name) {
        return false;
    }

    static string Canonicalize(const string& mod_name) {
        static const unordered_map<string, string> unique_mod_pseudonyms {
            { "org", "taxname" },
            { "organism", "taxname"},
            { "division", "div" },
            { "mol", "molecule"},
            { "mol_type", "moltype"},
            { "completedness", "completeness"},
            { "top", "topology" },
            { "gene_syn", "gene_synonym"}};

        string canonical_name = mod_name;
        NStr::ToLower(canonical_name);
        NStr::ReplaceInPlace(canonical_name, "-", "_");

        const auto& it = unique_mod_pseudonyms.find(canonical_name);
        if (it != unique_mod_pseudonyms.end()) {
            return it->second;
        }
        return canonical_name;
    }

    static TNames seq_inst_mods;
    static TNames annot_mods;
    static TNames subsource_mods;
    static TNames non_orgmod_orgref_mods; // not org_mods
    static TNames org_mods;
    //static TNames pcr_mods;
    static TNames non_biosource_descr_mods;
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
SModNameInfo::TNames SModNameInfo::non_orgmod_orgref_mods = 
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

SModNameInfo::TNames SModNameInfo::non_biosource_descr_mods = 
    {"comment", 
     "moltype", "mol_type", "tech", "completeness", "completedness", // MolInfo
     "secondary_accession", "secondary_accessions", "keyword", "keywords", // GBblock
     "primary", "primary_accessions", // TPA
     "PubMed", "PMID", // Pub Mods
     "SRA", "biosample", "bioproject",
     "project", "projects"
    };



struct SModContainer
{
    using TMods = multimap<string, string>;
    using TMod = TMods::value_type;

    struct SBiosourceMods { 
        TMods top_level_mods;
        TMods subsource_mods;
        TMods non_orgmod_orgref_mods;
        TMods org_mods;

        bool empty() const {
            return top_level_mods.empty() &&
                   subsource_mods.empty() &&
                   non_orgmod_orgref_mods.empty() &&
                   org_mods.empty();
        }
    };
    using TBiosourceMods = SBiosourceMods;

    void AddMod(const string& name, const string& value) {
        AddMod(TMod(name, value));
    }

    void AddMod(const TMod& name_value) {
        const auto& mod_name = name_value.first;

        // const auto& mod_name = SModInfo::Canonicalize(name_value.first);
        // CheckUniqueness - Need to add a method to do this

        if (SModNameInfo::IsNonBiosourceDescrMod(mod_name)) {
            non_biosource_descr_mods.insert(name_value);
            if (mod_name == "secondary_accession" ||
                mod_name == "secondary_accessions") {
                seq_inst_mods.insert(name_value);
            }
        }
        else 
        if (SModNameInfo::IsSeqInstMod(mod_name)) {
            seq_inst_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsAnnotMod(mod_name)) {
            annot_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsTopLevelBioSourceMod(mod_name)) {
            biosource_mods.top_level_mods.insert(name_value);
        }
        else
        if (SModNameInfo::IsSubSourceMod(mod_name)) {
            biosource_mods.subsource_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsStructuredOrgRefMod(mod_name)) {
            biosource_mods.non_orgmod_orgref_mods.insert(name_value);
        }
        else 
        if (SModNameInfo::IsOrgMod(mod_name)) {
            biosource_mods.org_mods.insert(name_value);
        }
        else {
            // Else report an error - unrecognised modifier
        }
    }

    TMods seq_inst_mods;
    TMods annot_mods;
    TMods non_biosource_descr_mods;
    TBiosourceMods biosource_mods;
    //unordered_set<string> m_ProcessedUniqueModifiers; // Use this to store unique modifiers
};

class CDescriptorCache 
{
public:

    struct SDescrContainer {
        virtual ~SDescrContainer(void) = default;
        virtual bool IsSet(void) const = 0;
        virtual CSeq_descr& SetDescr(void) = 0;
    };

    CDescriptorCache(CBioseq& bioseq);
    CDescriptorCache(CBioseq_set& bioseq_set);

    CSeqdesc& SetDBLink(void);
    CSeqdesc& SetPub(void);
    CSeqdesc& SetTpaAssembly(void);
    CSeqdesc& SetGenomeProjects(void);
    CSeqdesc& SetGBblock(void);
    CSeqdesc& SetMolInfo(void);
private:

    enum EChoice : size_t {
        eDBLink = 1,
        eTpa = 2,
        eGenomeProjects = 3,
        eMolInfo = 4,
        eGBblock = 5,
        ePub = 6,
    };

    bool x_IsUserType(const CUser_object& user_object, const string& type);
    void x_SetUserType(const string& type, CUser_object& user_object);
    CSeqdesc& x_SetDescriptor(const EChoice eChoice, 
                              function<bool(const CSeqdesc&)> f_verify,
                              function<CRef<CSeqdesc>(void)> f_create);

    using TMap = unordered_map<EChoice, CRef<CSeqdesc>, hash<underlying_type<EChoice>::type>>;
    TMap m_Cache;

    unique_ptr<SDescrContainer> m_pDescrContainer;
};


template<class TObject>
class CDescrContainer : public CDescriptorCache::SDescrContainer 
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




bool CDescriptorCache::x_IsUserType(const CUser_object& user_object, const string& type)
{
    return (user_object.IsSetType() &&
            user_object.GetType().IsStr() &&
            user_object.GetType().GetStr() == type);
}


void CDescriptorCache::x_SetUserType(const string& type, 
                                     CUser_object& user_object) 
{
    user_object.SetType().SetStr(type);
} 


CDescriptorCache::CDescriptorCache(CBioseq& bioseq) : m_pDescrContainer(new CDescrContainer<CBioseq>(bioseq)) {}


CDescriptorCache::CDescriptorCache(CBioseq_set& bioseq_set) : m_pDescrContainer(new CDescrContainer<CBioseq_set>(bioseq_set)) {}


CSeqdesc& CDescriptorCache::SetDBLink() 
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

CSeqdesc& CDescriptorCache::SetTpaAssembly()
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

CSeqdesc& CDescriptorCache::SetGenomeProjects()
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

CSeqdesc& CDescriptorCache::SetGBblock()
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

CSeqdesc& CDescriptorCache::SetMolInfo()
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


CSeqdesc& CDescriptorCache::SetPub() 
{
    return x_SetDescriptor(ePub,
        [](const CSeqdesc& desc) {
            return desc.IsPub();
        },
        []() {
            auto pDesc = Ref(new CSeqdesc());
            pDesc->SetPub();
            return pDesc;
        }
    );
}

CSeqdesc& CDescriptorCache::x_SetDescriptor(const EChoice eChoice, 
                                            function<bool(const CSeqdesc&)> f_verify,
                                            function<CRef<CSeqdesc>(void)> f_create)
{
    auto it = m_Cache.find(eChoice);
    if (it != m_Cache.end()) {
        return *(it->second);
    }

    // Search for descriptor on Bioseq
    if (m_pDescrContainer->IsSet()) {
        for (auto& pDesc : m_pDescrContainer->SetDescr().Set()) {
            if (pDesc.NotEmpty() && f_verify(*pDesc)) {
                m_Cache.insert(make_pair(eChoice, pDesc));
                return *pDesc;
            }
        }
    }

    auto pDesc = f_create();
    m_Cache.insert(make_pair(eChoice, pDesc));
    m_pDescrContainer->SetDescr().Set().push_back(pDesc);
    return *pDesc;
}


template<typename T>
static bool s_IsMatch(const string& a, const T& b) 
{
    return  (a == b);
}

template<typename T, typename...Args> 
static bool s_IsMatch(const string& name, const T& first, Args...args)
{
    return (s_IsMatch(name, first) || s_IsMatch(name, args...));
}

class CModApply_Impl {
public:

    using TMods = SModContainer::TMods;
    using TMod = SModContainer::TMod;
    using TBiosourceMods = SModContainer::TBiosourceMods;
    using TUnusedMods = multiset<TMod>;

    CModApply_Impl(const multimap<string, string>& mods);
    ~CModApply_Impl(void);

    void Apply(CBioseq& bioseq);

    void AddMod(const string& name, const string& value) {
        m_Mods.AddMod(name, value);
    }

private:
    // Seq-inst modifiers
    void x_ApplySeqInstMods(const TMods& mods, CSeq_inst& inst);
    bool x_AddTopology(const TMod& mod, CSeq_inst& inst);
    bool x_AddMolType(const TMod& mod, CSeq_inst& inst);
    bool x_AddStrand(const TMod& mod, CSeq_inst& inst);
    bool x_AddHist(const TMod& mod, CSeq_inst& inst);

    // Feature annotation modifiers
    bool x_CreateGene(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData);
    bool x_CreateProtein(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData);

    // Non-biosource descriptor modifiers
    void x_ApplyNonBioSourceDescriptorMods(const TMods& mods, CBioseq& bioseq);
    bool x_AddPubMod(const TMod& mod, CDescriptorCache::SDescrContainer& descr_container);
    bool x_AddGenomeProjectsDBMod(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddTpaAssemblyMod(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddDBLinkMod(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddGBblockMod(const TMod& mod, CDescriptorCache& desc_cache);
    bool x_AddMolInfoMod(const TMod& mod, CDescriptorCache& desc_cache);
    template<typename TObject>
    bool x_AddComment(const TMod& mod, CDescrContainer<TObject>& descr_container);
   
    // Biosource modifiers
    CSeqdesc& x_SetBioSource(CBioseq& bioseq);
    CSeqdesc& x_SetBioSource(CSeq_descr& descr);
    void x_ApplyBioSourceMods(const TBiosourceMods& mods, CBioseq& bioseq);
    bool x_AddBioSourceGenome(const TMod& mod, CBioSource& biosource);
    bool x_AddBioSourceOrigin(const TMod& mod, CBioSource& biosource);
    bool x_AddBioSourceFocus(const TMod& mod, CBioSource& biosource);
    void x_AddSubSourceMods(const TMods& mods, CBioSource& biosource);
    void x_AddOrgRefMods(const TMods& org_mods, 
                         const TMods& other_mods,
                         CBioSource& biosource);
    void x_AddOrgMods(const TMods& org_mods,
                      CBioSource& biosource);
    
    SModContainer m_Mods;
};


CModApply_Impl::CModApply_Impl(const multimap<string, string>& mods) 
{
    for (auto it = mods.cbegin(); 
         it != mods.cend();
         ++it) {
        AddMod(it->first, it->second);
    }

}

CModApply_Impl::~CModApply_Impl() = default;


void CModApply_Impl::Apply(CBioseq& bioseq) 
{
    const auto& seq_inst_mods = m_Mods.seq_inst_mods;
    x_ApplySeqInstMods(seq_inst_mods, bioseq.SetInst());

    const auto& biosource_mods = m_Mods.biosource_mods;
    x_ApplyBioSourceMods(biosource_mods, bioseq);

    const auto& nonbiosrc_descr_mods = m_Mods.non_biosource_descr_mods;
    x_ApplyNonBioSourceDescriptorMods(nonbiosrc_descr_mods, bioseq);

    // const auto& annotation_mods = m_Mods.annotation_mods;
    // x_ApplyAnnotationMods(annotation_mods, bioseq);
}


static CBioSource::EGenome s_GetEGenome(const string& genome) 
{

    if (NStr::EqualNocase(genome, "mitochondrial")) {
        return CBioSource::eGenome_mitochondrion;
    }
    if (NStr::EqualNocase(genome, "provirus")) {
        return CBioSource::eGenome_proviral;
    }
    if (NStr::EqualNocase(genome, "extrachromosomal")) {
        return CBioSource::eGenome_extrachrom;
    }
    if (NStr::EqualNocase(genome, "insertion sequence")) 
    {
        return CBioSource::eGenome_insertion_seq;
    }
    
    return static_cast<CBioSource::EGenome>(CBioSource::GetTypeInfo_enum_EGenome()->FindValue(genome));
}


static CBioSource::EOrigin s_GetEOrigin(const string& origin)
{
    if (NStr::EqualNocase(origin, "natural mutant")) {
        return CBioSource::eOrigin_natmut;
    }

    if (NStr::EqualNocase(origin, "mutant")) {
        return CBioSource::eOrigin_mut;
    }

    return static_cast<CBioSource::EOrigin>(CBioSource::GetTypeInfo_enum_EOrigin()->FindValue(origin));
}


void CModApply_Impl::x_ApplyBioSourceMods(const TBiosourceMods& mods, 
                                          CBioseq& bioseq) 
{
    if (mods.empty()) {
        return;
    }
    auto& descriptor = x_SetBioSource(bioseq);
    auto& biosource  = descriptor.SetSource();

    // Need to reconsider this - b
    for (const auto& mod : mods.top_level_mods) {
        if (x_AddBioSourceGenome(mod, biosource) ||
            x_AddBioSourceOrigin(mod, biosource) ||
            x_AddBioSourceFocus(mod, biosource)) {
            continue;
        }
    }

    // PCR primers
   // x_AddPCRPrimers(mods.pcr_mods, biosource);

    // subsource
    x_AddSubSourceMods(mods.subsource_mods, biosource);

    // Org-ref
    x_AddOrgRefMods(mods.org_mods, 
                    mods.non_orgmod_orgref_mods,
                    biosource);
    return;
}


CSeqdesc& CModApply_Impl::x_SetBioSource(CBioseq& bioseq) 
{   
    // Check to see if sequence is in a nuc-prot-set
    if (bioseq.GetParentSet() && 
        bioseq.GetParentSet()->IsSetClass() &&
        bioseq.GetParentSet()->GetClass() == CBioseq_set::eClass_nuc_prot) 
    {
        auto& bioseq_set = const_cast<CBioseq_set&>(*(bioseq.GetParentSet()));
        return x_SetBioSource(bioseq_set.SetDescr());
    }

    // else
    return x_SetBioSource(bioseq.SetDescr());
}


CSeqdesc& CModApply_Impl::x_SetBioSource(CSeq_descr& descr) 
{
    for (auto& pDesc : descr.Set()) {
        if (pDesc.NotEmpty() && pDesc->IsSource()) {
            return *pDesc;
        }
    }

    auto pDesc = Ref(new CSeqdesc());
    pDesc->SetSource();
    descr.Set().push_back(pDesc);
    return *pDesc;
}



void CModApply_Impl::x_AddSubSourceMods(const TMods& mods, CBioSource& biosource)
{
    for (const auto& mod : mods) {
        const auto& name = mod.first;
        static const auto& subsource_name_to_enum = s_InitModNameSubSrcSubtypeMap();

        const auto& it = subsource_name_to_enum.find(name);
        if (it == subsource_name_to_enum.end()) {
            continue; // Asserttion here - this should never happen
        }
        const auto& e_subtype = it->second;
        const auto& value = mod.second;
        auto pSubsource = Ref(new CSubSource());
        pSubsource->SetSubtype(e_subtype);
        if (NStr::IsBlank(value)) {
            pSubsource->SetName(kEmptyStr);
        }
        else {
            pSubsource->SetName(value);
        }
        biosource.SetSubtype().push_back(move(pSubsource));
    }
}

static void s_SetDBxref(COrg_ref& org_ref,
                        const string& dbxref) 
{
    string database;
    string tag;

    if (!(NStr::SplitInTwo(dbxref, ":", database, tag))) {
        tag = "?";
    }

    auto pDbtag = Ref(new CDbtag());
    pDbtag->SetDb(database);
    pDbtag->SetTag().SetStr(tag);
    org_ref.SetDb().push_back(move(pDbtag));
}

void CModApply_Impl::x_AddOrgRefMods(const TMods& org_mods,
                                     const TMods& other_mods,
                                     CBioSource& biosource) 
{

    if (org_mods.empty() &&
        other_mods.empty()) {
        return;
    }

    // non org mods
    for (const auto& mod : other_mods) {
        const auto& name = mod.first;
        if (s_IsMatch(name, "org", "organism", "taxname")) {
            biosource.SetOrg().SetTaxname(mod.second);
        }
        else
        if (s_IsMatch(name, "div")) {
            biosource.SetOrg().SetOrgname().SetDiv(mod.second);
        }   
        else 
        if (s_IsMatch(name, "lineage")) {
            biosource.SetOrg().SetOrgname().SetLineage(mod.second);
        }
        else
        if (s_IsMatch(name, "gcode")) { // Need error checking
            biosource.SetOrg().SetOrgname().SetGcode(NStr::StringToInt(mod.second, NStr::fConvErr_NoThrow));
        }
        else
        if (s_IsMatch(name, "mgcode")) { // Need error checking
            biosource.SetOrg().SetOrgname().SetMgcode(NStr::StringToInt(mod.second, NStr::fConvErr_NoThrow));
        }
        else 
        if (s_IsMatch(name, "pgcode")) { // Need error checking
            biosource.SetOrg().SetOrgname().SetPgcode(NStr::StringToInt(mod.second, NStr::fConvErr_NoThrow));
        }
        else 
        if (s_IsMatch(name, "dbxref", "db_xref")) {
            s_SetDBxref(biosource.SetOrg(), mod.second);
        }
        else {
            // assertion - this should never happen
        }
    }
    
    x_AddOrgMods(org_mods, biosource);

    // Reset taxid
    if (biosource.GetOrg().GetTaxId()) {
        biosource.SetOrg().SetTaxId(0);
    }
} 


void CModApply_Impl::x_AddOrgMods(const TMods& mods,
                                  CBioSource& biosource)
{
    for (const auto& mod : mods) {
        const auto& name = mod.first;
        static const auto& orgmod_name_to_enum = s_InitModNameOrgSubtypeMap();

        const auto& it = orgmod_name_to_enum.find(name);
        if (it == orgmod_name_to_enum.end()) {
            continue; // assert here - this should never happen
        }

        const auto& e_subtype = it->second;
        const auto& value = mod.second;
        auto pOrgMod = Ref(new COrgMod());
        pOrgMod->SetSubtype(e_subtype);
        pOrgMod->SetSubname(value);
        biosource.SetOrg().SetOrgname().SetMod().push_back(move(pOrgMod));
    }
}


bool CModApply_Impl::x_AddBioSourceGenome(const TMod& mod, CBioSource& biosource) 
{
    const auto& name = mod.first;
    if (name != "location") {
        return false;
    }

    const auto& genome = mod.second;
    try {
        auto e_genome = s_GetEGenome(genome);
        biosource.SetGenome(e_genome);
    }
    catch (...) {
        // Error handling 
    }
    return true;
}


bool CModApply_Impl::x_AddBioSourceOrigin(const TMod& mod, CBioSource& biosource) 
{
    const auto& name = mod.first;
    if (name != "origin") {
        return false;
    }

    const auto& origin = mod.second;
    try {
        auto e_origin = s_GetEOrigin(origin);
        biosource.SetGenome(e_origin);
    }
    catch (...) {
        // Error handling 
    }
    return true;
}

bool CModApply_Impl::x_AddBioSourceFocus(const TMod& mod, CBioSource& biosource) 
{    // This is problematic - should probably drop modifier if it doesn't actually do anything
    if (s_IsMatch(mod.first, "focus")) {
        if (s_IsMatch(mod.second, "TRUE")) {
            biosource.SetIs_focus();
        }
        return true;
    }
    return false;
}


// bool CModApply_Impl::x_AddOrgrefMods() // Implement a method for OrgrefMods



void CModApply_Impl::x_ApplyNonBioSourceDescriptorMods(const TMods& mods, CBioseq& bioseq)
{
    if (mods.empty()) {
        return;
    }

    CDescrContainer<CBioseq> descr_container(bioseq);

    unique_ptr<CDescriptorCache> pDescriptorCache;
    if (bioseq.GetParentSet() &&
        bioseq.GetParentSet()->IsSetClass() &&
        bioseq.GetParentSet()->GetClass() == CBioseq_set::eClass_nuc_prot)
    {
        auto& bioseq_set = const_cast<CBioseq_set&>(*(bioseq.GetParentSet()));
        pDescriptorCache.reset(new CDescriptorCache(bioseq_set));
    }
    else {
        pDescriptorCache.reset(new CDescriptorCache(bioseq));
    }
    CDescriptorCache molinfo_descriptor_cache(bioseq);

    for (const auto& mod : mods) {
        if (x_AddTpaAssemblyMod(mod, *pDescriptorCache) ||
            x_AddPubMod(mod, descr_container) ||
            x_AddGenomeProjectsDBMod(mod, *pDescriptorCache) ||
            x_AddDBLinkMod(mod, *pDescriptorCache) ||
            x_AddGBblockMod(mod, *pDescriptorCache) ||
            x_AddMolInfoMod(mod, molinfo_descriptor_cache) || 
            x_AddComment(mod, descr_container)) {
            continue;
        }
        // Assertion here
    }
}



static void s_SetDBLinkFieldVals(const string& label, 
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
            if (pUserField->IsSetLabel() &&
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

   // pField->SetData().SetStrs().clear(); // RW-518 - clear any preexisting entries
    for (const auto& val : vals) {
        pField->SetData().SetStrs().push_back(val);
    }
    pField->SetNum(pField->GetData().GetStrs().size());
}


static void s_SetDBLinkField(const string& label,
                             const string& vals,
                             CDescriptorCache& descriptor_cache)
{
    list<CTempString> value_list;
    NStr::Split(vals, ",", value_list, NStr::fSplit_MergeDelimiters);
    for (auto& val : value_list) {
        val = NStr::TruncateSpaces_Unsafe(val);
    }
    value_list.remove_if([](const CTempString& val){ return val.empty(); });
    if (value_list.empty()) { // nothing to do
        return;
    }

    s_SetDBLinkFieldVals(label,
                         value_list,
                         descriptor_cache.SetDBLink());
}


bool CModApply_Impl::x_AddDBLinkMod(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsMatch(name, "SRA")) {
        s_SetDBLinkField("Sequence Read Archive", mod.second, descriptor_cache);
        return true;
    }
        
    if (s_IsMatch(name, "biosample")) {
        s_SetDBLinkField("BioSample", mod.second, descriptor_cache);
        return true;
    }
    
    if (s_IsMatch(name, "bioproject")) {
        s_SetDBLinkField("BioProject", mod.second, descriptor_cache);
        return true;
    } 

    return false;
}


bool CModApply_Impl::x_AddGenomeProjectsDBMod(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsMatch(name, "project", "projects")) {
        auto& user = descriptor_cache.SetGenomeProjects().SetUser();

        list<string> ids;
        NStr::Split(mod.second, ",;", ids, NStr::fSplit_Tokenize);
        for (const auto& id_string : ids) { // Need proper error handling
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
                user.SetData().push_back(move(pField));
            }
        }
        return true;
    }

    return false;
}

template<typename TObject>
bool CModApply_Impl::x_AddComment(const TMod& mod, CDescrContainer<TObject>& descr_container) 
{
    if (NStr::EqualNocase("comment", mod.first)) {
        auto pDesc = Ref(new CSeqdesc());
        pDesc->SetComment(mod.second);
        descr_container.SetDescr().Set().push_back(move(pDesc));
        return true;
    }
    return false;
}


bool CModApply_Impl::x_AddMolInfoMod(const TMod& mod, CDescriptorCache& descriptor_cache) 
{
    const auto& name = mod.first;
    if (s_IsMatch(name, "moltype", "mol_type")) {
            auto it = MolInfoToBiomol.find(mod.second);
            if (it != MolInfoToBiomol.end()) {
                auto& descriptor = descriptor_cache.SetMolInfo();
                descriptor.SetMolinfo().SetBiomol(it->second);
            }
            else {
                // x_HandleBadMod
            }
        return true;
    }
        
    if (s_IsMatch(name, "tech")) {
        auto it = MolInfoToTech.find(mod.second);
        if (it != MolInfoToTech.end()) {
            auto& descriptor = descriptor_cache.SetMolInfo();
            descriptor.SetMolinfo().SetTech(it->second);
        }
        else {
        // x_HandleBadMod
        }
        return true;
    }

    if (s_IsMatch(name, "completeness", "completedness")) {
        auto it = MolInfoToCompleteness.find(mod.second);
        if (it != MolInfoToCompleteness.end()) {
            auto& descriptor = descriptor_cache.SetMolInfo();
            descriptor.SetMolinfo().SetCompleteness(it->second);
        }
        else {
            // x_HandleBad Mod
        }
        return true;
    }

    return false;
}


bool CModApply_Impl::x_AddGBblockMod(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    const auto& value = mod.second;
    if (s_IsMatch(name, "secondary_accession", "secondary_accessions")) {
        auto& genbank = descriptor_cache.SetGBblock().SetGenbank();
        list<CTempString> ranges;
        NStr::Split(value, ",", ranges, NStr::fSplit_Tokenize);
        for (const auto& range : ranges) {
            string s = NStr::TruncateSpaces_Unsafe(range);
            try {
                SSeqIdRange idrange(s);
                for (auto it = idrange.begin(); it != idrange.end(); ++it) {
                    genbank.SetExtra_accessions().push_back(*it);
                }
            }
            catch (CSeqIdException&) {
                genbank.SetExtra_accessions().push_back(s); 
            }
        }
        return true;
    } 
        
    if (s_IsMatch(name, "keyword", "keywords")) {
        auto& genbank = descriptor_cache.SetGBblock().SetGenbank();
        list<string> keywords;
        NStr::Split(value, ",;",  keywords, NStr::fSplit_Tokenize);
        for (const auto& keyword : keywords) {
            genbank.SetKeywords().push_back(keyword);
        }
        return true;
    }

    return false;
}

bool CModApply_Impl::x_AddPubMod(const TMod& mod, CDescriptorCache::SDescrContainer& descr_container)
{
    const auto& name = mod.first;
    if (s_IsMatch(name, "PubMed", "PMID")) {
        auto pmid = NStr::StringToNumeric<int>(mod.second, NStr::fConvErr_NoThrow); 
        if (pmid) {
            // Need to add proper error handling here
            auto pDesc = Ref(new CSeqdesc());
            auto pPub = Ref(new CPub());
            pPub->SetPmid().Set(pmid);
            pDesc->SetPub().SetPub().Set().push_back(move(pPub));
            descr_container.SetDescr().Set().push_back(move(pDesc));
        }
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
            continue;
        }
        // assert - cannot happen!
    }
}


bool CModApply_Impl::x_AddTopology(const TMod& mod, CSeq_inst& seq_inst)
{
    if (!s_IsMatch(mod.first, "topology", "top")) {
        return false;
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

    bool is_mol = s_IsMatch(name, "molecule", "mol");
    bool is_moltype;
    if (!is_mol) {
        is_moltype = s_IsMatch(name, "moltype", "mol_type");
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
        if (s_IsMatch(value, "dna")) {
            seq_inst.SetMol(CSeq_inst::eMol_dna);
        }
        else 
        if (s_IsMatch(value, "rna")) {
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
    if (!s_IsMatch(mod.first, "secondary_accession", "secondary_accessions")) {
        return false;
    }
    list<CTempString> ranges;
    NStr::Split(mod.second, ",", ranges, NStr::fSplit_MergeDelimiters);

    
    for (const auto& range : ranges) {
        string s = NStr::TruncateSpaces_Unsafe(range);
        try {
            SSeqIdRange idrange(s);
            for (auto it = idrange.begin(); it != idrange.end(); ++it) {
                auto pSeqId = it.GetID();
                seq_inst.SetHist().SetReplaces().SetIds().push_back(pSeqId); // Not safe!
            }
        }
        catch (CSeqIdException&) {
           NStr::ReplaceInPlace(s, "ref_seq|", "ref|", 0, 1); // Not safe!
           seq_inst.SetHist().SetReplaces().SetIds().push_back(Ref(new CSeq_id(s))); 
        }
    }

    return true;
}


bool CModApply_Impl::x_CreateGene(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData)
{
    for (const auto& mod : mods) {
        const auto& name = mod.first;
        if (s_IsMatch(name, "gene")) {
            pFeatData->SetGene().SetLocus(mod.second);
        }
        else 
        if (s_IsMatch(name, "allele")) {
            pFeatData->SetGene().SetAllele(mod.second);
        }
        else
        if (s_IsMatch(name, "gene_syn", "gene_synonym")) {
            pFeatData->SetGene().SetSyn().push_back(mod.second);
        }
        else
        if (s_IsMatch(name, "locus_tag")) { 
            pFeatData->SetGene().SetLocus_tag(mod.second);
        }
    }
    return pFeatData.IsInitialized();
}


bool CModApply_Impl::x_CreateProtein(const TMods& mods, CAutoInitRef<CSeqFeatData>& pFeatData) 
{

    for (const auto& mod : mods) {
        const auto& name = mod.first;
        if (s_IsMatch(name, "protein", "prot")) {
            pFeatData->SetProt().SetName().push_back(mod.second);
        } 
        else 
        if (s_IsMatch(name, "prot_desc", "protein_desc")) {
            pFeatData->SetProt().SetDesc(mod.second);
        }
        else 
        if (s_IsMatch(name, "EC_number")) {
            pFeatData->SetProt().SetEc().push_back(mod.second);
        }
        else 
        if (s_IsMatch(name, "activity", "function")) {
            pFeatData->SetProt().SetActivity().push_back(mod.second);
        }
    }
    return pFeatData.IsInitialized();
}


bool CModApply_Impl::x_AddTpaAssemblyMod(const TMod& mod, CDescriptorCache& descriptor_cache)
{
    const auto& name = mod.first;
    if (s_IsMatch(name, "primary", "primary_accessions")) {
        list<string> accession_list;
        NStr::Split(mod.second, ",", accession_list, NStr::fSplit_MergeDelimiters);
      
        auto& user = descriptor_cache.SetTpaAssembly().SetUser();
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

CModApply::CModApply(const multimap<string, string>& mods) : m_pImpl(new CModApply_Impl(mods)) {}

CModApply::~CModApply() = default;


void CModApply::Apply(CBioseq& bioseq)  
{
    m_pImpl->Apply(bioseq);
}

END_SCOPE(edit)
END_SCOPE(objects)
END_NCBI_SCOPE
