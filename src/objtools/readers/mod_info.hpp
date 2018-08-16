
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
 * Author: Justin Foley
 *
 * File Description:
 *
 */
#ifndef __MOD_INFO_HPP__
#define __MOD_INFO_HPP__

using TModNameSet = set<const char*, CSourceModParser::PKeyCompare>;

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

        if (skip_enum_names.find(enum_name.c_str()) != skip_enum_names.end()) 
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

/*
struct SMods {
    using TModMap = multimap<string, string>;
    using TPair   = pair<string, string>;

    void AddMod(const string& name, const string& value) {
        AddMod(make_pair(name,value));
    }

    void AddMod(const TPair& name_value) {
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
    
    TModMap seq_inst_mods;
    TModMap annot_mods;
    TModMap descr_mods;
};
*/    

#endif // __MOD_INFO_HPP__
