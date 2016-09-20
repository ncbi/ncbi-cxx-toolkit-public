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

CTempString
GetStringOfFeatQual(EFeatureQualifier eFeatureQualifier)
{
#ifdef TYPICAL_FQ
#    error TYPICAL_FQ should not be defined yet
#endif

// Usually, it's the same as the enum name minus the "eSQ_" prefix, but not always.
#define TYPICAL_FQ(x) { eFQ_##x, #x }

    typedef SStaticPair<EFeatureQualifier, const char*> TFeatQualToName;
    static const TFeatQualToName kFeatQualToNames[] = {
        TYPICAL_FQ(none),
        TYPICAL_FQ(allele),
        TYPICAL_FQ(anticodon),
        TYPICAL_FQ(artificial_location),
        TYPICAL_FQ(bond),
        TYPICAL_FQ(bond_type),
        TYPICAL_FQ(bound_moiety),
        TYPICAL_FQ(calculated_mol_wt),
        { eFQ_cds_product, "product" },
        TYPICAL_FQ(citation),
        TYPICAL_FQ(clone),
        TYPICAL_FQ(coded_by),
        TYPICAL_FQ(codon),
        TYPICAL_FQ(codon_start),
        TYPICAL_FQ(compare),
        TYPICAL_FQ(cons_splice),
        TYPICAL_FQ(cyt_map),
        TYPICAL_FQ(db_xref),
        TYPICAL_FQ(derived_from),
        TYPICAL_FQ(direction),
        TYPICAL_FQ(EC_number),
        TYPICAL_FQ(encodes),
        TYPICAL_FQ(estimated_length),
        TYPICAL_FQ(evidence),
        TYPICAL_FQ(experiment),
        TYPICAL_FQ(exception),
        TYPICAL_FQ(exception_note),
        TYPICAL_FQ(figure),
        TYPICAL_FQ(frequency),
        TYPICAL_FQ(function),
        TYPICAL_FQ(gap_type),
        TYPICAL_FQ(gen_map),
        TYPICAL_FQ(gene),
        TYPICAL_FQ(gene_desc),
        { eFQ_gene_allele, "allele" },
        { eFQ_gene_map, "map" },
        TYPICAL_FQ(gene_syn),
        { eFQ_gene_syn_refseq, "synonym" },
        TYPICAL_FQ(gene_note),
        { eFQ_gene_xref, "db_xref" },
        { eFQ_go_component, "GO_component" },
        { eFQ_go_function, "GO_function" },
        { eFQ_go_process, "GO_process" },
        TYPICAL_FQ(heterogen),
        { eFQ_illegal_qual, "illegal" },
        TYPICAL_FQ(inference),
        TYPICAL_FQ(insertion_seq),
        TYPICAL_FQ(label),
        TYPICAL_FQ(linkage_evidence),
        TYPICAL_FQ(locus_tag),
        TYPICAL_FQ(map),
        TYPICAL_FQ(maploc),
        TYPICAL_FQ(mobile_element),
        TYPICAL_FQ(mobile_element_type),
        TYPICAL_FQ(mod_base),
        TYPICAL_FQ(modelev),
        TYPICAL_FQ(mol_wt),
        TYPICAL_FQ(ncRNA_class),
        TYPICAL_FQ(nomenclature),
        TYPICAL_FQ(number),
        TYPICAL_FQ(old_locus_tag),
        TYPICAL_FQ(operon),
        TYPICAL_FQ(organism),
        TYPICAL_FQ(partial),
        TYPICAL_FQ(PCR_conditions),
        TYPICAL_FQ(peptide),
        TYPICAL_FQ(phenotype),
        TYPICAL_FQ(product),
        TYPICAL_FQ(product_quals),
        { eFQ_prot_activity,  "function" },
        TYPICAL_FQ(prot_comment),
        { eFQ_prot_EC_number,  "EC_number" },
        TYPICAL_FQ(prot_note),
        TYPICAL_FQ(prot_method),
        TYPICAL_FQ(prot_conflict),
        TYPICAL_FQ(prot_desc),
        TYPICAL_FQ(prot_missing),
        { eFQ_prot_name, "name" },
        TYPICAL_FQ(prot_names),
        TYPICAL_FQ(protein_id),
        TYPICAL_FQ(pseudo),
        TYPICAL_FQ(pseudogene),
        TYPICAL_FQ(pyrrolysine),
        TYPICAL_FQ(pyrrolysine_note),
        TYPICAL_FQ(rad_map),
        TYPICAL_FQ(region),
        TYPICAL_FQ(region_name),
        TYPICAL_FQ(recombination_class),
        TYPICAL_FQ(regulatory_class),
        TYPICAL_FQ(replace),
        TYPICAL_FQ(ribosomal_slippage),
        TYPICAL_FQ(rpt_family),
        TYPICAL_FQ(rpt_type),
        TYPICAL_FQ(rpt_unit),
        TYPICAL_FQ(rpt_unit_range),
        TYPICAL_FQ(rpt_unit_seq),
        TYPICAL_FQ(rrna_its),
        TYPICAL_FQ(satellite),
        TYPICAL_FQ(sec_str_type),
        TYPICAL_FQ(selenocysteine),
        TYPICAL_FQ(selenocysteine_note),
        TYPICAL_FQ(seqfeat_note),
        TYPICAL_FQ(site),
        TYPICAL_FQ(site_type),
        TYPICAL_FQ(standard_name),
        TYPICAL_FQ(tag_peptide),
        TYPICAL_FQ(trans_splicing),
        TYPICAL_FQ(transcription),
        TYPICAL_FQ(transcript_id),
        { eFQ_transcript_id_note, "tscpt_id_note" },
        TYPICAL_FQ(transl_except),
        TYPICAL_FQ(transl_table),
        TYPICAL_FQ(translation),
        TYPICAL_FQ(transposon),
        TYPICAL_FQ(trna_aa),
        TYPICAL_FQ(trna_codons),
        TYPICAL_FQ(UniProtKB_evidence),
        TYPICAL_FQ(usedin),
        TYPICAL_FQ(xtra_prod_quals)
#undef TYPICAL_FQ
    };
    typedef const CStaticPairArrayMap<EFeatureQualifier, const char*> TFeatQualToNameMap;
    DEFINE_STATIC_ARRAY_MAP(TFeatQualToNameMap, kFeatQualToNameMap, kFeatQualToNames);

    _ASSERT( kFeatQualToNameMap.size() == eFQ_NUM_SOURCE_QUALIFIERS );

    TFeatQualToNameMap::const_iterator find_iter =
        kFeatQualToNameMap.find(eFeatureQualifier);
    if( find_iter != kFeatQualToNameMap.end() ) {
        return find_iter->second;
    }

    return "UNKNOWN_FEAT_QUAL";
}

CTempString GetStringOfSourceQual(ESourceQualifier eSourceQualifier)
{
#ifdef TYPICAL_SQ
#    error TYPICAL_SQ should not be defined yet
#endif

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
        TYPICAL_SQ(type_material),
        TYPICAL_SQ(unstructured),
        TYPICAL_SQ(usedin),
        TYPICAL_SQ(variety),
        TYPICAL_SQ(whole_replicon),
        { eSQ_zero_orgmod, "?" },
        { eSQ_one_orgmod, "?" },
        { eSQ_zero_subsrc, "?" }
#undef TYPICAL_SQ
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

ESourceQualifier 
GetSourceQualOfOrgMod(COrgMod::ESubtype eOrgModSubtype)
{
    switch( eOrgModSubtype )
    {
        // In most (but not all) cases, the orgmod subtype 
        // corresponds directly to the sourcequal.  For those
        // easy cases, we use this CASE_ORGMOD macro
#ifdef CASE_ORGMOD
#    error CASE_ORGMOD should NOT be already defined here.
#endif

#define CASE_ORGMOD(x) case COrgMod::eSubtype_##x:  return eSQ_##x;
        CASE_ORGMOD(strain);
        CASE_ORGMOD(substrain);
        CASE_ORGMOD(type);
        CASE_ORGMOD(subtype);
        CASE_ORGMOD(variety);
        CASE_ORGMOD(serotype);
        CASE_ORGMOD(serogroup);
        CASE_ORGMOD(serovar);
        CASE_ORGMOD(cultivar);
        CASE_ORGMOD(pathovar);
        CASE_ORGMOD(chemovar);
        CASE_ORGMOD(biovar);
        CASE_ORGMOD(biotype);
        CASE_ORGMOD(group);
        CASE_ORGMOD(subgroup);
        CASE_ORGMOD(isolate);
        CASE_ORGMOD(common);
        CASE_ORGMOD(acronym);
        CASE_ORGMOD(dosage);
    case COrgMod::eSubtype_nat_host:  return eSQ_spec_or_nat_host;
        CASE_ORGMOD(sub_species);
        CASE_ORGMOD(specimen_voucher);
        CASE_ORGMOD(authority);
        CASE_ORGMOD(forma);
        CASE_ORGMOD(forma_specialis);
        CASE_ORGMOD(ecotype);
        CASE_ORGMOD(culture_collection);
        CASE_ORGMOD(bio_material);
        CASE_ORGMOD(type_material);
        CASE_ORGMOD(synonym);
        CASE_ORGMOD(anamorph);
        CASE_ORGMOD(teleomorph);
        CASE_ORGMOD(breed);
        CASE_ORGMOD(gb_acronym);
        CASE_ORGMOD(gb_anamorph);
        CASE_ORGMOD(gb_synonym);
        CASE_ORGMOD(metagenome_source);
        CASE_ORGMOD(old_lineage);
        CASE_ORGMOD(old_name);
#undef CASE_ORGMOD
    case COrgMod::eSubtype_other:  return eSQ_orgmod_note;
    default:                       return eSQ_none;
    }
}

ESourceQualifier 
GetSourceQualOfSubSource(CSubSource::ESubtype eSubSourceSubtype)
{
    switch( eSubSourceSubtype ) {
        // Generally, there is a direct, obvious correspondence
        // between subsource subtypes and sourcequals.
#ifdef DO_SS
#    error DO_SS should NOT have already been defined here.
#endif

#define DO_SS(x) case CSubSource::eSubtype_##x:  return eSQ_##x;
        DO_SS(chromosome);
        DO_SS(map);
        DO_SS(clone);
        DO_SS(subclone);
        DO_SS(haplotype);
        DO_SS(genotype);
        DO_SS(sex);
        DO_SS(cell_line);
        DO_SS(cell_type);
        DO_SS(tissue_type);
        DO_SS(clone_lib);
        DO_SS(dev_stage);
        DO_SS(frequency);
        DO_SS(germline);
        DO_SS(rearranged);
        DO_SS(lab_host);
        DO_SS(pop_variant);
        DO_SS(tissue_lib);
        DO_SS(plasmid_name);
        DO_SS(transposon_name);
        DO_SS(insertion_seq_name);
        DO_SS(plastid_name);
        DO_SS(country);
        DO_SS(segment);
        DO_SS(endogenous_virus_name);
        DO_SS(transgenic);
        DO_SS(environmental_sample);
        DO_SS(isolation_source);
        DO_SS(lat_lon);
        DO_SS(altitude);
        DO_SS(collection_date);
        DO_SS(collected_by);
        DO_SS(identified_by);
        DO_SS(fwd_primer_seq);
        DO_SS(rev_primer_seq);
        DO_SS(fwd_primer_name);
        DO_SS(rev_primer_name);
        DO_SS(metagenomic);
        DO_SS(mating_type);
        DO_SS(linkage_group);
        DO_SS(haplogroup);
        DO_SS(whole_replicon);
#undef DO_SS
    case CSubSource::eSubtype_other:  return eSQ_subsource_note;
    default:                          return eSQ_none;
    }
}

END_SCOPE(objects)
END_NCBI_SCOPE

