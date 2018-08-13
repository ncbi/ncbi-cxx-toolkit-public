
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


using TSModNameSet = set<const char*, CSourceModParser::PKeyCompare>;

// Loads up a map of SMod to subtype
template<typename TEnum,
         typename TSModEnumMap = map<string, TEnum>,
         typename TEnumNameToValMap = map<string, TEnum>>
    TSModEnumMap s_InitSmodToEnumMap(
        const CEnumeratedTypeValues& etv,
        const TSModNameSet& skip_enum_names,
        const TEnumNameToValMap&  extra_enum_names_to_vals)

{
    TSModEnumMap smod_enum_map;
    
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
                "s_InitSModToEnumMap " << enum_name);
        }
    }

    for (auto extra_smod_to_enum : extra_enum_names_to_vals) {
        auto emplace_result = smod_enum_map.emplace(extra_smod_to_enum);
        // emplace must succeed
        if (!emplace_result.second) {
            NCBI_USER_THROW_FMT(
                "s_InitSModToEnumMap " << extra_smod_to_enum.first);
        }
    }
    return smod_enum_map;
}


//using TSModOrgSubtypeMap = map<CSourceModParser::SMod, COrgMod::ESubtype>;
using TSModOrgSubtypeMap = map<string, COrgMod::ESubtype>;

TSModOrgSubtypeMap s_InitSModOrgSubtypeMap(void)
{
    const TSModNameSet kDeprecatedOrgSubtypes{
            "dosage", "old-lineage", "old-name"};
    const map<const char*, COrgMod::ESubtype> extra_smod_to_enum_names {
        { "subspecies",    COrgMod::eSubtype_sub_species },
        { "host",          COrgMod::eSubtype_nat_host    },
        { "specific-host", COrgMod::eSubtype_nat_host    },
    };

    return s_InitSmodToEnumMap<COrgMod::ESubtype>(
        *COrgMod::GetTypeInfo_enum_ESubtype(),
        kDeprecatedOrgSubtypes,
        extra_smod_to_enum_names
    );
}


// The subtype SMods are loaded from the names of the enum
// and they map to ESubtype enum values so we can't just use STATIC_SMOD
//CSafeStatic<TSModOrgSubtypeMap> kModNameOrgSubtypeMap(s_InitSModOrgSubtypeMap, nullptr);

//using TSModSubSrcSubtype = map<CSourceModParser::SMod,CSubSource::ESubtype>;
using TSModSubSrcSubtype = map<string,CSubSource::ESubtype>;

TSModSubSrcSubtype s_InitSModSubSrcSubtypeMap(void)
{
    // some are skipped because they're handled specially and some are
    // skipped because they're deprecated
    TSModNameSet skip_enum_names {
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
    const TSModSubSrcSubtype extra_smod_to_enum_names {
        { "sub-clone",          CSubSource::eSubtype_subclone },
        { "lat-long",           CSubSource::eSubtype_lat_lon  },
        { "latitude-longitude", CSubSource::eSubtype_lat_lon  },
    };
    return s_InitSmodToEnumMap<CSubSource::ESubtype>(
            *CSubSource::GetTypeInfo_enum_ESubtype(),
            skip_enum_names,
            extra_smod_to_enum_names );
}


struct SModInfo {
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

    static TNames seq_inst_mods;
    static TNames annot_mods;
    static TNames orgref_mods; // not org_mods
    static TNames subsource_mods;
    // static TNames org_mods;
};


SModInfo::TNames SModInfo::seq_inst_mods =    
    { "top", "topology", 
    "molecule", "mol", "moltype", "mol_type",
    "strand", 
    "secondary_accession", "secondary_accessions" };

SModInfo::TNames SModInfo::annot_mods = 
    { "gene", "allele", "gene_syn", "gene_synonym", "locus_tag"
      "prot", "protein", "prot_desc", "protein_desc", 
      "EC_number", "activity", "function" };

// What about org_mods?
SModInfo::TNames SModInfo::orgref_mods = 
    { "division", "div", "lineage", "gcode", 
      "mgcode", "pgcode", "taxid"
    };



#endif // __MOD_INFO_HPP__
