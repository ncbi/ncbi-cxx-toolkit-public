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
* Author:  Michael Kornbluh
*                        
* File Description:
*   Simple function(s) to support flat file qual slots, such as
*   enum-to-string function(s).
*
* ===========================================================================
*/

#include <ncbi_pch.hpp>

#include <objtools/format/items/flat_qual_slots.hpp>

#include <util/static_map.hpp>

using namespace std;

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

CTempString GetStringOfSourceQual(ESourceQualifier eSourceQualifier)
{
    // Usually, it's the same as the enum name minus the "eSQ_" prefix, but not always.
    #define TYPICAL_SQ(x) { eSQ_##x, #x }

    typedef SStaticPair<ESourceQualifier, const char*> TSourceQualToName;
    static const TSourceQualToName kSourceQualToNames[] = {
        TYPICAL_SQ(none),
        TYPICAL_SQ(acronym),
        TYPICAL_SQ(altitude),
        TYPICAL_SQ(anamorph),
        TYPICAL_SQ(authority),
        TYPICAL_SQ(bio_material),
        TYPICAL_SQ(biotype),
        TYPICAL_SQ(biovar),
        TYPICAL_SQ(breed),
        TYPICAL_SQ(cell_line),
        TYPICAL_SQ(cell_type),
        TYPICAL_SQ(chemovar),
        TYPICAL_SQ(chromosome),
        TYPICAL_SQ(citation),
        TYPICAL_SQ(clone),
        TYPICAL_SQ(clone_lib),
        TYPICAL_SQ(collected_by),
        TYPICAL_SQ(collection_date),
        TYPICAL_SQ(common),
        TYPICAL_SQ(common_name),
        TYPICAL_SQ(country),
        TYPICAL_SQ(cultivar),
        TYPICAL_SQ(culture_collection),
        TYPICAL_SQ(db_xref),
        { eSQ_org_xref, "db_xref" },
        TYPICAL_SQ(dev_stage),
        TYPICAL_SQ(dosage),
        TYPICAL_SQ(ecotype),
        { eSQ_endogenous_virus_name, "endogenous_virus" },
        TYPICAL_SQ(environmental_sample),
        TYPICAL_SQ(extrachrom),
        TYPICAL_SQ(focus),
        TYPICAL_SQ(forma),
        TYPICAL_SQ(forma_specialis),
        TYPICAL_SQ(frequency),
        TYPICAL_SQ(fwd_primer_name),
        TYPICAL_SQ(fwd_primer_seq),
        TYPICAL_SQ(gb_acronym),
        TYPICAL_SQ(gb_anamorph),
        TYPICAL_SQ(gb_synonym),
        TYPICAL_SQ(genotype),
        TYPICAL_SQ(germline),
        TYPICAL_SQ(group),
        TYPICAL_SQ(haplogroup),
        TYPICAL_SQ(haplotype),
        TYPICAL_SQ(identified_by),
        { eSQ_insertion_seq_name, "insertion_seq" },
        TYPICAL_SQ(isolate),
        TYPICAL_SQ(isolation_source),
        TYPICAL_SQ(lab_host),
        TYPICAL_SQ(label),
        TYPICAL_SQ(lat_lon),
        TYPICAL_SQ(linkage_group),
        TYPICAL_SQ(macronuclear),
        TYPICAL_SQ(mating_type),
        TYPICAL_SQ(map),
        TYPICAL_SQ(metagenome_source),
        TYPICAL_SQ(metagenomic),
        TYPICAL_SQ(mobile_element),
        TYPICAL_SQ(mol_type),
        TYPICAL_SQ(old_lineage),
        TYPICAL_SQ(old_name),
        TYPICAL_SQ(organism),
        TYPICAL_SQ(organelle),
        TYPICAL_SQ(orgmod_note),
        TYPICAL_SQ(pathovar),
        TYPICAL_SQ(PCR_primers),
        TYPICAL_SQ(pcr_primer_note),
        { eSQ_plasmid_name, "plasmid" },
        { eSQ_plastid_name, "plastid" },
        TYPICAL_SQ(pop_variant),
        TYPICAL_SQ(rearranged),
        TYPICAL_SQ(rev_primer_name),
        TYPICAL_SQ(rev_primer_seq),
        TYPICAL_SQ(segment),
        TYPICAL_SQ(seqfeat_note),
        TYPICAL_SQ(sequenced_mol),
        TYPICAL_SQ(serogroup),
        TYPICAL_SQ(serotype),
        TYPICAL_SQ(serovar),
        TYPICAL_SQ(sex),
        { eSQ_spec_or_nat_host, "host" },
        TYPICAL_SQ(specimen_voucher),
        TYPICAL_SQ(strain),
        { eSQ_subclone, "sub_clone" },
        TYPICAL_SQ(subgroup),
        TYPICAL_SQ(sub_species),
        { eSQ_substrain, "sub_strain" },
        TYPICAL_SQ(subtype),
        TYPICAL_SQ(subsource_note),
        TYPICAL_SQ(synonym),
        TYPICAL_SQ(teleomorph),
        TYPICAL_SQ(tissue_lib),
        TYPICAL_SQ(tissue_type),
        TYPICAL_SQ(transgenic),
        { eSQ_transposon_name, "transposon" },
        TYPICAL_SQ(type),
        TYPICAL_SQ(unstructured),
        TYPICAL_SQ(usedin),
        TYPICAL_SQ(variety),
        TYPICAL_SQ(whole_replicon),
        { eSQ_zero_orgmod, "?" },
        { eSQ_one_orgmod, "?" },
        { eSQ_zero_subsrc, "?" }
    };
    typedef const CStaticPairArrayMap<ESourceQualifier, const char*> TSourceQualToNameMap;
    DEFINE_STATIC_ARRAY_MAP(TSourceQualToNameMap, kSourceQualToNameMap, kSourceQualToNames);

    _ASSERT( kSourceQualToNameMap.size() == eSQ_NUM_SOURCE_QUALIFIERS );

    TSourceQualToNameMap::const_iterator find_iter =
        kSourceQualToNameMap.find(eSourceQualifier);
    if( find_iter != kSourceQualToNameMap.end() ) {
        return find_iter->second;
    }

    return "UNKNOWN_SOURCE_QUAL";
}

END_SCOPE(objects)
END_NCBI_SCOPE

