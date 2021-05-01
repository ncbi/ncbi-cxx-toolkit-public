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

#include <ncbi_pch.hpp>
#include "mod_to_enum.hpp"
#include <serial/enumvalues.hpp>
#include <functional>
#include <unordered_set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

using TModNameSet = unordered_set<string>;


string g_GetNormalizedModVal(const string& unnormalized)
{
    string normalized = unnormalized;
    static const string irrelevant_prefix_suffix_chars = " \t\"\'-_";

    size_t pos = normalized.find_first_not_of(irrelevant_prefix_suffix_chars);
    if (pos != NPOS) {
        normalized.erase(0,pos);
    }

    pos = normalized.find_last_not_of(irrelevant_prefix_suffix_chars);
    if (pos != NPOS) {
        normalized.erase(pos+1);
    }

    normalized.erase(remove_if(normalized.begin(),
                               normalized.end(),
                               [](char c) {
                                return (c=='-' ||
                                        c=='_' ||
                                        c==':' ||
                                        isspace(c)); }),
                     normalized.end());

    NStr::ToLower(normalized);
    return normalized;
}


static string s_NoNormalization(const string& mod_string)
{ return mod_string; };


template<typename TEnum>
static TStringToEnumMap<TEnum> s_InitModStringToEnumMap(
    const CEnumeratedTypeValues& etv,
    const TModNameSet& skip_mod_strings,
    const TStringToEnumMap<TEnum>&  extra_mod_strings_to_enums,
    function<string(const string&)> fNormalizeString  = s_NoNormalization
    )

{
    TModNameSet normalized_skip_set;
    transform(skip_mod_strings.begin(),
              skip_mod_strings.end(),
              inserter(normalized_skip_set, normalized_skip_set.end()),
              fNormalizeString);

    TStringToEnumMap<TEnum> smod_enum_map;
    for (const auto& name_val : etv.GetValues()) {
        const auto&  mod_string = fNormalizeString(name_val.first);
        if (normalized_skip_set.find(mod_string) == normalized_skip_set.end())
        {
            const TEnum& enum_val = static_cast<TEnum>(name_val.second);
            smod_enum_map.emplace(mod_string, enum_val);
        }
    }

    for (auto extra_smod_to_enum : extra_mod_strings_to_enums) {
        smod_enum_map.emplace(
                fNormalizeString(extra_smod_to_enum.first),
                extra_smod_to_enum.second);
    }
    return smod_enum_map;
}


TStringToEnumMap<COrgMod::ESubtype>
g_InitModNameOrgSubtypeMap(void)
{
    static const TModNameSet kDeprecatedOrgSubtypes{
            "dosage", "old-lineage", "old-name"};
    static const TStringToEnumMap<COrgMod::ESubtype>
        extra_smod_to_enum_names
        {{ "subspecies", COrgMod::eSubtype_sub_species},
         {"host",COrgMod::eSubtype_nat_host},
         {"specific-host", COrgMod::eSubtype_nat_host}};
    return s_InitModStringToEnumMap(
        *COrgMod::GetTypeInfo_enum_ESubtype(),
        kDeprecatedOrgSubtypes,
        extra_smod_to_enum_names
    );
}


TStringToEnumMap<CSubSource::ESubtype>
g_InitModNameSubSrcSubtypeMap(void)
{
    // some are skipped because they're handled specially and some are
    // skipped because they're deprecated
    static const TModNameSet skip_enum_names {
        // skip because handled specially elsewhere
        "fwd-primer-seq", "rev-primer-seq",
        "fwd-primer-name", "rev-primer-name",
        // skip because deprecated
        "transposon-name",
        "plastid-name",
        "insertion-seq-name",
    };
    static const TStringToEnumMap<CSubSource::ESubtype>
        extra_smod_to_enum_names
        {{ "sub-clone", CSubSource::eSubtype_subclone },
        { "lat-long",   CSubSource::eSubtype_lat_lon  },
        { "latitude-longitude", CSubSource::eSubtype_lat_lon },
        {  "note",  CSubSource::eSubtype_other  },
        {  "notes", CSubSource::eSubtype_other  }};
    return s_InitModStringToEnumMap(
            *CSubSource::GetTypeInfo_enum_ESubtype(),
            skip_enum_names,
            extra_smod_to_enum_names);
}


TStringToEnumMap<CBioSource::EGenome>
g_InitModNameGenomeMap(void)
{
   static const TModNameSet skip_enum_names;
   static const TStringToEnumMap<CBioSource::EGenome>
       extra_smod_to_enum_names
       {{ "mitochondrial", CBioSource::eGenome_mitochondrion },
        { "provirus", CBioSource::eGenome_proviral},
        { "extrachromosomal", CBioSource::eGenome_extrachrom},
        { "insertion sequence", CBioSource::eGenome_insertion_seq}};

   return s_InitModStringToEnumMap(
           *CBioSource::GetTypeInfo_enum_EGenome(),
           skip_enum_names,
           extra_smod_to_enum_names,
           g_GetNormalizedModVal);
}


TStringToEnumMap<CBioSource::EOrigin>
g_InitModNameOriginMap(void)
{
    static const TModNameSet skip_enum_names;
    static const TStringToEnumMap<CBioSource::EOrigin>
        extra_smod_to_enum_names
        {{ "natural mutant", CBioSource::eOrigin_natmut},
         { "mutant", CBioSource::eOrigin_mut}};

    return s_InitModStringToEnumMap(
            *CBioSource::GetTypeInfo_enum_EOrigin(),
            skip_enum_names,
            extra_smod_to_enum_names,
            g_GetNormalizedModVal);
}


const
TStringToEnumMap<CMolInfo::TBiomol>
g_BiomolStringToEnum
= { {"crna",                    CMolInfo::eBiomol_cRNA },
    {"dna",                     CMolInfo::eBiomol_genomic},
    {"genomic",                 CMolInfo::eBiomol_genomic},
    {"genomicdna",              CMolInfo::eBiomol_genomic},
    {"genomicrna",              CMolInfo::eBiomol_genomic},
    {"mrna",                    CMolInfo::eBiomol_mRNA},
    {"ncrna",                   CMolInfo::eBiomol_ncRNA},
    {"noncodingrna",            CMolInfo::eBiomol_ncRNA},
    {"othergenetic",            CMolInfo::eBiomol_other_genetic},
    {"precursorrna",            CMolInfo::eBiomol_pre_RNA},
    {"ribosomalrna",            CMolInfo::eBiomol_rRNA},
    {"rrna",                    CMolInfo::eBiomol_rRNA},
    {"transcribedrna",          CMolInfo::eBiomol_transcribed_RNA},
    {"transfermessengerrna",    CMolInfo::eBiomol_tmRNA},
    {"tmrna",                   CMolInfo::eBiomol_tmRNA},
    {"transferrna",             CMolInfo::eBiomol_tRNA},
    {"trna",                    CMolInfo::eBiomol_tRNA},
};


const
unordered_map<CMolInfo::TBiomol, CSeq_inst::EMol>
g_BiomolEnumToMolEnum = {
    { CMolInfo::eBiomol_genomic, CSeq_inst::eMol_dna},
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

END_SCOPE(objects)
END_NCBI_SCOPE

