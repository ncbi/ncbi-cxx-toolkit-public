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
 */
    
#ifndef _MOD_INFO_HPP_
#define _MOD_INFO_HPP_

#include <serial/enumvalues.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seqfeat/BioSource.hpp>
#include <objects/seqfeat/OrgMod.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using TModNameSet = unordered_set<string>;


template<typename TEnum>
static unordered_map<string, TEnum> s_InitModNameToEnumMap(
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
        assert(emplace_result.second);
    }

    for (auto extra_smod_to_enum : extra_enum_names_to_vals) {
        auto emplace_result = smod_enum_map.emplace(extra_smod_to_enum);
        assert(emplace_result.second);
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


static unordered_map<string, CBioSource::TGenome>
s_InitModNameGenomeMap(void)
{
   static const TModNameSet skip_enum_names;
   static const unordered_map<string, CBioSource::TGenome> 
       extra_smod_to_enum_names 
       {{ "mitochondrial", CBioSource::eGenome_mitochondrion },
        { "provirus", CBioSource::eGenome_proviral},
        { "extrachromosomal", CBioSource::eGenome_extrachrom},
        { "insertion sequence", CBioSource::eGenome_insertion_seq}};

   return s_InitModNameToEnumMap<CBioSource::TGenome>(
           *CBioSource::GetTypeInfo_enum_EGenome(),
           skip_enum_names,
           extra_smod_to_enum_names);
}

static unordered_map<string, CBioSource::TOrigin>
s_InitModNameOriginMap(void)
{
    static const TModNameSet skip_enum_names;
    static const unordered_map<string, CBioSource::TOrigin>
        extra_smod_to_enum_names 
        {{ "natural mutant", CBioSource::eOrigin_natmut},
         { "mutant", CBioSource::eOrigin_mut}};

    return s_InitModNameToEnumMap<CBioSource::TOrigin>(
            *CBioSource::GetTypeInfo_enum_EOrigin(),
            skip_enum_names,
            extra_smod_to_enum_names);

}


template<typename T, typename U, typename THash> // Only works if TUmap values are unique
static unordered_map<U,T> s_GetReverseMap(const unordered_map<T,U,THash>& TUmap) 
{
    unordered_map<U,T> UTmap;
    for (const auto& key_val : TUmap) {
        UTmap.emplace(key_val.second, key_val.first);
    }
    return UTmap;
}

static const unordered_map<string, CSeq_inst::EStrand> 
s_StrandStringToEnum = {{"single", CSeq_inst::eStrand_ss},
                        {"double", CSeq_inst::eStrand_ds},
                        {"mixed", CSeq_inst::eStrand_mixed},
                        {"other", CSeq_inst::eStrand_other}};


static const unordered_map<string, CSeq_inst::EMol>
s_MolStringToEnum = {{"dna", CSeq_inst::eMol_dna},
                     {"rna", CSeq_inst::eMol_rna},
                     {"aa", CSeq_inst::eMol_aa},
                     {"na", CSeq_inst::eMol_na},
                    {"other", CSeq_inst::eMol_other}};


static const unordered_map<string, CSeq_inst::ETopology> 
s_TopologyStringToEnum = {{"linear", CSeq_inst::eTopology_linear},
                          {"circular", CSeq_inst::eTopology_circular},
                          {"tandem", CSeq_inst::eTopology_tandem},
                          {"other", CSeq_inst::eTopology_other}};


static const auto s_GenomeStringToEnum = s_InitModNameGenomeMap();

static const auto s_OriginStringToEnum = s_InitModNameOriginMap();

static const auto s_OrgModStringToEnum = s_InitModNameOrgSubtypeMap();
static const auto s_SubSourceStringToEnum = s_InitModNameSubSrcSubtypeMap();


static const 
unordered_map<string, CMolInfo::TBiomol> s_BiomolStringToEnum
= { {"crna",                    CMolInfo::eBiomol_cRNA },   
    {"dna",                     CMolInfo::eBiomol_genomic},   
    {"genomic",                 CMolInfo::eBiomol_genomic},   
    {"genomic-dna",             CMolInfo::eBiomol_genomic},   
    {"genomic-rna",             CMolInfo::eBiomol_genomic},   
    {"mrna",                    CMolInfo::eBiomol_mRNA},   
    {"ncrna",                   CMolInfo::eBiomol_ncRNA},
    {"non-coding-rna",          CMolInfo::eBiomol_ncRNA},   
    {"noncoding-rna",           CMolInfo::eBiomol_ncRNA},   
    {"other-genetic",           CMolInfo::eBiomol_other_genetic}, 
    {"precursor-rna",           CMolInfo::eBiomol_pre_RNA},   
    {"ribosomal-rna",           CMolInfo::eBiomol_rRNA},   
    {"rrna",                    CMolInfo::eBiomol_rRNA},   
    {"transcribed-rna",         CMolInfo::eBiomol_transcribed_RNA},   
    {"transfer-messenger-rna",  CMolInfo::eBiomol_tmRNA},   
    {"tmrna",                   CMolInfo::eBiomol_tmRNA},   
    {"transfer rna",            CMolInfo::eBiomol_tRNA},   
    {"trna",                    CMolInfo::eBiomol_tRNA},   
};


static const 
unordered_map<CMolInfo::TBiomol, CSeq_inst::EMol> s_BiomolEnumToMolEnum
= { { CMolInfo::eBiomol_genomic, CSeq_inst::eMol_dna},

    { CMolInfo::eBiomol_pre_RNA,  CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_mRNA,  CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_rRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_tRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_snRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_scRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_genomic_mRNA, CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_cRNA, CSeq_inst::eMol_rna },
    { CMolInfo::eBiomol_snoRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_transcribed_RNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_ncRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_tmRNA, CSeq_inst::eMol_rna},
    { CMolInfo::eBiomol_peptide, CSeq_inst::eMol_aa},
    { CMolInfo::eBiomol_other_genetic, CSeq_inst::eMol_other},
    { CMolInfo::eBiomol_other, CSeq_inst::eMol_other}
};


static const 
unordered_map<string, CMolInfo::TTech>
s_TechStringToEnum = {
    { "?",                  CMolInfo::eTech_unknown },
    { "barcode",            CMolInfo::eTech_barcode },
    { "both",               CMolInfo::eTech_both },
    { "composite-wgs-htgs", CMolInfo::eTech_composite_wgs_htgs },
    { "concept-trans",      CMolInfo::eTech_concept_trans },
    { "concept-trans-a",    CMolInfo::eTech_concept_trans_a },
    { "derived",            CMolInfo::eTech_derived },
    { "est",                CMolInfo::eTech_est },
    { "fli-cdna",           CMolInfo::eTech_fli_cdna },
    { "genetic-map",        CMolInfo::eTech_genemap },
    { "htc",                CMolInfo::eTech_htc },
    { "htgs-0",             CMolInfo::eTech_htgs_0 },
    { "htgs-1",             CMolInfo::eTech_htgs_1 },
    { "htgs-2",             CMolInfo::eTech_htgs_2 },
    { "htgs-3",             CMolInfo::eTech_htgs_3 },
    { "physical-map",       CMolInfo::eTech_physmap },
    { "seq-pept",           CMolInfo::eTech_seq_pept },
    { "seq-pept-homol",     CMolInfo::eTech_seq_pept_homol },
    { "seq-pept-overlap",   CMolInfo::eTech_seq_pept_overlap },
    { "standard",           CMolInfo::eTech_standard },
    { "sts",                CMolInfo::eTech_sts },
    { "survey",             CMolInfo::eTech_survey },
    { "targeted",           CMolInfo::eTech_targeted },
    { "tsa",                CMolInfo::eTech_tsa },
    { "wgs",                CMolInfo::eTech_wgs }
};


//static const auto s_TechEnumToString  = s_GetReverseMap(s_TechStringToEnum);


static const 
unordered_map<string, CMolInfo::TCompleteness> 
s_CompletenessStringToEnum = {
    { "complete",  CMolInfo::eCompleteness_complete  },
    { "has-left",  CMolInfo::eCompleteness_has_left  },
    { "has-right", CMolInfo::eCompleteness_has_right  },
    { "no-ends",   CMolInfo::eCompleteness_no_ends  },
    { "no-left",   CMolInfo::eCompleteness_no_left  },
    { "no-right",  CMolInfo::eCompleteness_no_right  },
    { "partial",   CMolInfo::eCompleteness_partial  }
};


//static const auto s_CompletenessEnumToString = s_GetReverseMap(s_CompletenessStringToEnum);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_INFO_HPP_
