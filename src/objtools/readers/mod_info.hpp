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


template<typename T, typename U, typename THash> // Only works if TUmap values are unique
static unordered_map<U,T> s_GetReverseMap(const unordered_map<T,U,THash>& TUmap) 
{
    unordered_map<U,T> UTmap;
    for (const auto& key_val : TUmap) {
        UTmap.emplace(key_val.second, key_val.first);
    }
    return UTmap;
}


static const unordered_map<CSeq_inst::EStrand, string, hash<underlying_type<CSeq_inst::EStrand>::type>>
s_StrandEnumToString = {{CSeq_inst::eStrand_ss, "single"},
                        {CSeq_inst::eStrand_ds, "double"},
                        {CSeq_inst::eStrand_mixed, "mixed"},
                        {CSeq_inst::eStrand_other, "other"}};

static const auto& s_StrandStringToEnum = s_GetReverseMap(s_StrandEnumToString);


static const unordered_map<CSeq_inst::EMol, string, hash<underlying_type<CSeq_inst::EMol>::type>>
s_MolEnumToString = {{CSeq_inst::eMol_dna, "dna"},
                     {CSeq_inst::eMol_rna, "rna"},
                     {CSeq_inst::eMol_aa, "aa"},
                     {CSeq_inst::eMol_na, "na"},
                    {CSeq_inst::eMol_other, "other"}};

static const auto& s_MolStringToEnum = s_GetReverseMap(s_MolEnumToString);


static const unordered_map<CSeq_inst::ETopology, string, hash<underlying_type<CSeq_inst::ETopology>::type>> 
s_TopologyEnumToString = {{CSeq_inst::eTopology_linear, "linear"},
                          {CSeq_inst::eTopology_circular, "circular"},
                          {CSeq_inst::eTopology_tandem, "tandem"},
                          {CSeq_inst::eTopology_other, "other"}};

static const auto& s_TopologyStringToEnum = s_GetReverseMap(s_TopologyEnumToString);


static unordered_map<CBioSource::EGenome, string, hash<underlying_type<CBioSource::EGenome>::type>> s_GenomeEnumToString 
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


static unordered_map<CBioSource::EOrigin, string, hash<underlying_type<CBioSource::EOrigin>::type>> s_OriginEnumToString
= { {CBioSource::eOrigin_unknown, "unknown"},
    {CBioSource::eOrigin_natural, "natural"},
    {CBioSource::eOrigin_natmut, "natural mutant"},
    {CBioSource::eOrigin_mut, "mutant"},
    {CBioSource::eOrigin_artificial, "artificial"},
    {CBioSource::eOrigin_synthetic, "synthetic"},
    {CBioSource::eOrigin_other, "other"}
};


static const auto s_OrgModStringToEnum = s_InitModNameOrgSubtypeMap();
static const auto s_SubSourceStringToEnum = s_InitModNameSubSrcSubtypeMap();


static const 
unordered_map<string, CMolInfo::TBiomol> s_BiomolStringToEnum
= { {"cRNA",                    CMolInfo::eBiomol_cRNA },   
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
    {"tmRNA",                   CMolInfo::eBiomol_tmRNA},   
    {"Transfer RNA",            CMolInfo::eBiomol_tRNA},   
    {"tRNA",                    CMolInfo::eBiomol_tRNA},   
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
unordered_map<CMolInfo::TBiomol, string> s_BiomolEnumToString =
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
s_TechStringToEnum = {
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


static const auto s_TechEnumToString  = s_GetReverseMap(s_TechStringToEnum);


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


static const auto s_CompletenessEnumToString = s_GetReverseMap(s_CompletenessStringToEnum);


END_SCOPE(objects)
END_NCBI_SCOPE

#endif // _MOD_INFO_HPP_
