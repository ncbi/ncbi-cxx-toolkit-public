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
 * Authors:  Frank Ludwig
 *
 * File Description:  Sequence Ontology Type Mapping
 *
 */

#include <ncbi_pch.hpp>
//#include <objects/seq/genbank_type.hpp>
#include <objects/seq/sofa_type.hpp>
#include <objects/seq/sofa_map.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

#define GT( a, b ) CFeatListItem( CSeqFeatData::a, CSeqFeatData::b, "", "" )

CSofaMap::TYPEMAP CSofaMap::mMapSofaTypeToId;

CSofaMap::TYPEMAP CSofaMap::mMapSofaIdToType = {
    {"SO:0000001", "region"},
    {"SO:0000002", "sequece_secondary_structure"},
    {"SO:0000005", "satellite_DNA"},
    {"SO:0000013", "scRNA"},
    {"SO:0000035", "riboswitch"},
    {"SO:0000036", "matrix_attachment_site"},
    {"SO:0000037", "locus_control_region"},
    {"SO:0000104", "polypeptide"},
    {"SO:0000110", "sequence_feature"},
    {"SO:0000139", "ribosome_entry_site"},
    {"SO:0000140", "attenuator"},
    {"SO:0000141", "terminator"},
    {"SO:0000147", "exon"},
    {"SO:0000165", "enhancer"},
    {"SO:0000167", "promoter"},
    {"SO:0000172", "CAAT_signal"},
    {"SO:0000173", "GC_rich_promoter_region"},
    {"SO:0000174", "TATA_box"},
    {"SO:0000175", "minus_10_signal"},
    {"SO:0000176", "minus_35_signal"},
    {"SO:0000178", "operon"},
    {"SO:0000185", "primary_transcript"},
    {"SO:0000188", "intron"},
    {"SO:0000204", "five_prime_UTR"},
    {"SO:0000205", "three_prime_UTR"},
    {"SO:0000234", "mRNA"},
    {"SO:0000252", "rRNA"},
    {"SO:0000253", "tRNA"},
    {"SO:0000274", "snRNA"},
    {"SO:0000275", "snoRNA"},
    {"SO:0000276", "miRNA"},
    {"SO:0000286", "long_terminal_repeat"},
    {"SO:0000289", "microsatellite"},
    {"SO:0000294", "inverted_repeat"},
    {"SO:0000296", "origin_of_replication"},
    {"SO:0000297", "D_loop"},
    {"SO:0000298", "recombination_feature"},
    {"SO:0000305", "modified_DNA_base"},
    {"SO:0000313", "stem_loop"},
    {"SO:0000314", "direct_repeat"},
    {"SO:0000315", "TSS"},
    {"SO:0000316", "CDS"},
    {"SO:0000330", "conserved_region"},
    {"SO:0000331", "STS"},
    {"SO:0000336", "pseudogene"},
    {"SO:0000374", "ribozyme"},
    {"SO:0000380", "hammerhead_ribozyme"},
    {"SO:0000385", "RNase_MRP_RNA"},
    {"SO:0000386", "RNase_P_RNA"},
    {"SO:0000404", "vault_RNA"},
    {"SO:0000405", "Y_RNA"},
    {"SO:0000409", "binding_site"},
    {"SO:0000410", "protein_binding_site"},
    {"SO:0000413", "sequence_difference"},
    {"SO:0000418", "signal_peptide"},
    {"SO:0000419", "mature_protein_region"},
    {"SO:0000433", "non_LTR_retrotransposon_polymeric_tract"},
    {"SO:0000454", "rasiRNA"},
    {"SO:0000458", "D_gene_segment"},
    {"SO:0000466", "V_gene_segment"},
    {"SO:0000470", "J_gene_segment"},
    {"SO:0000478", "C_gene_segment"},
    {"SO:0000516", "pseudogenic_transcript"},
    {"SO:0000551", "polyA_signal_sequence"},
    {"SO:0000553", "polyA_site"},
    {"SO:0000577", "centromere"},
    {"SO:0000584", "tmRNA"},
    {"SO:0000588", "autocatalytically_spliced_intron"},
    {"SO:0000590", "SRP_RNA"},
    {"SO:0000602", "guide_RNA"},
    {"SO:0000624", "telomere"},
    {"SO:0000625", "silencer"},
    {"SO:0000627", "insulator"},
    {"SO:0000644", "antisense_RNA"},
    {"SO:0000646", "siRNA"},
    {"SO:0000655", "ncRNA"},
    {"SO:0000657", "repeat_region"},
    {"SO:0000658", "dispersed_repeat"},
    {"SO:0000673", "transcript"},
    {"SO:0000685", "DNAsel_hypersensitive_site"},
    {"SO:0000704", "gene"},
    {"SO:0000705", "tandem_repeat"},
    {"SO:0000714", "nucleotide_motif"},
    {"SO:0000723", "iDNA"},
    {"SO:0000724", "oriT"},
    {"SO:0000725", "transit_peptide"},
    {"SO:0000730", "gap"},
    {"SO:0000777", "pseudogenic_rRNA"},
    {"SO:0000778", "pseudogenic_tRNA"},
    {"SO:0001021", "chromosome_preakpoint"},
    {"SO:0001035", "piRNA"},
    {"SO:0001037", "mobile_genetic_element"},
    {"SO:0001055", "transcriptional_cis_regulatory_region"},
    {"SO:0001059", "sequence_alteration"},
    {"SO:0001062", "propeptide"},
    {"SO:0001086", "sequence_uncertainty"},
    {"SO:0001087", "cross_link"},
    {"SO:0001088", "disulfide_bond"},
    {"SO:0001268", "recoding_stimulatory_region"},
    {"SO:0001411", "biological_region"},
    {"SO:0001484", "X_element_combinatorical_repeat"},
    {"SO:0001485", "Y_prime_element"},
    {"SO:0001496", "telomeric_repeat"},
    {"SO:0001649", "nested_repeat"},
    {"SO:0001682", "replication_regulatory_region"},
    {"SO:0001797", "centromeric_repeat"},
    {"SO:0001833", "V_region"},
    {"SO:0001835", "N_region"},
    {"SO:0001836", "S_region"},
    {"SO:0001877", "lnc_RNA"},
    {"SO:0001917", "CAGE_cluster"},
    {"SO:0002020", "boundary_element"},
    {"SO:0002072", "sequence_comparison"},
    {"SO:0002087", "pseudogenic_CDS"},
    {"SO:0002094", "non_allelic_homologous_recombination_region"},
    {"SO:0002154", "mitotic_recombination_region"},
    {"SO:0002155", "meiotic_recombination_region"},
    {"SO:0005836", "regulatory_region"},
    {"SO:0005850", "primary_binding_site"},

    {"SO:0000000", ""},
    //{"SO:UNKNOWN", "replication_start_site"},
    //{"SO:UNKNOWN", "nucleotide_site"},
    //{"SO:UNKNOWN", "nucleotide_cleavage_site"},
    //{"SO:UNKNOWN", "repeat_instability_region"},
};

//  ----------------------------------------------------------------------------
string CSofaMap::SofaIdToType(
    const string& sofa_id)
//  ----------------------------------------------------------------------------
{
    TYPEENTRY type_it = CSofaMap::mMapSofaIdToType.find(sofa_id);
    if (type_it == CSofaMap::mMapSofaIdToType.end()) {
        return "";
    }
    return type_it->second;
}

//  ----------------------------------------------------------------------------
string CSofaMap::SofaTypeToId(
    const string& sofa_type)
//  ----------------------------------------------------------------------------
{
    if (CSofaMap::mMapSofaTypeToId.empty()) {
        for (TYPEENTRY  cit = CSofaMap::mMapSofaIdToType.begin();
                cit != CSofaMap::mMapSofaIdToType.end();
                ++cit) {
            CSofaMap::mMapSofaTypeToId[cit->second] = cit->first;
        }
    }
    TYPEENTRY id_it = mMapSofaTypeToId.find(sofa_type);
    if (id_it == CSofaMap::mMapSofaTypeToId.end()) {
        return "";
    }
    return id_it->second;
} 


void CSofaMap::x_Init() 
{
    m_default = SofaType( 1, "region" );

    m_Map[ GT( e_Cdregion, eSubtype_cdregion ) ] = SofaType( 316, "CDS" );

    m_Map[ GT( e_Gene, eSubtype_gene ) ]        = SofaType( 704, "gene" );

    m_Map[ GT( e_Rna, eSubtype_mRNA ) ]         = SofaType( 234, "mRNA" );
    m_Map[ GT( e_Rna, eSubtype_ncRNA ) ]        = SofaType( 655, "ncRNA" );
    m_Map[ GT( e_Rna, eSubtype_otherRNA ) ]     = SofaType( 673, "transcript" );
    m_Map[ GT( e_Rna, eSubtype_rRNA ) ]         = SofaType( 252, "rRNA" );
    m_Map[ GT( e_Rna, eSubtype_preRNA ) ]       = SofaType( 185, "primary_transcript" );
    m_Map[ GT( e_Rna, eSubtype_scRNA ) ]        = SofaType( 13,  "scRNA" );
    m_Map[ GT( e_Rna, eSubtype_snRNA ) ]        = SofaType( 274, "snRNA" );
    m_Map[ GT( e_Rna, eSubtype_snoRNA ) ]       = SofaType( 275, "snoRNA" );
    m_Map[ GT( e_Rna, eSubtype_tRNA ) ]         = SofaType( 253, "tRNA" );
    m_Map[ GT( e_Rna, eSubtype_tmRNA ) ]        = SofaType( 584, "tmRNA" );

    m_Map[ GT( e_Imp, eSubtype_10_signal ) ]    = SofaType( 175, "minus_10_signal" );
    m_Map[ GT( e_Imp, eSubtype_35_signal ) ]    = SofaType( 176, "minus_35_signal" );
    m_Map[ GT( e_Imp, eSubtype_3clip ) ]        = SofaType( 557, "three_prime_clip" );
    m_Map[ GT( e_Imp, eSubtype_3UTR ) ]         = SofaType( 205, "three_prime_UTR" );
    m_Map[ GT( e_Imp, eSubtype_5clip ) ]        = SofaType( 555, "five_prime_clip" );
    m_Map[ GT( e_Imp, eSubtype_5UTR ) ]         = SofaType( 204, "five_prime_UTR" );
    m_Map[ GT( e_Imp, eSubtype_C_region ) ]     = SofaType( 478, "C_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_CAAT_signal ) ]  = SofaType( 172, "CAAT_signal" );
    m_Map[ GT( e_Imp, eSubtype_D_loop ) ]       = SofaType( 297, "D_loop" );
    m_Map[ GT( e_Imp, eSubtype_D_segment ) ]    = SofaType( 458, "D_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_exon ) ]         = SofaType( 147, "exon" );
    m_Map[ GT( e_Imp, eSubtype_GC_signal ) ]    = SofaType( 173, "GC_rich_promoter_region" );
    m_Map[ GT( e_Imp, eSubtype_J_segment ) ]    = SofaType( 470, "J_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_LTR ) ]          = SofaType( 286, "long_terminal_repeat" );
    m_Map[ GT( e_Imp, eSubtype_N_region ) ]     = SofaType( 1835, "N_region" );
    m_Map[ GT( e_Imp, eSubtype_RBS ) ]          = SofaType( 139, "ribosome_entry_site" );
    m_Map[ GT( e_Imp, eSubtype_STS ) ]          = SofaType( 331, "STS" );
    m_Map[ GT( e_Imp, eSubtype_S_region ) ]     = SofaType( 1836, "S_region" );
    m_Map[ GT( e_Imp, eSubtype_TATA_signal ) ]  = SofaType( 174, "TATA_box" );
    m_Map[ GT( e_Imp, eSubtype_V_region ) ]     = SofaType( 1833, "V_region");
    m_Map[ GT( e_Imp, eSubtype_assembly_gap ) ] = SofaType( 730, "gap" );
    m_Map[ GT( e_Imp, eSubtype_attenuator)]     = SofaType(140, "attenuator");
    m_Map[ GT( e_Imp, eSubtype_centromere)]     = SofaType( 577, "centromere");
    m_Map[ GT( e_Imp, eSubtype_conflict)]       = SofaType( 1085, "sequence_conflict");
    m_Map[ GT( e_Imp, eSubtype_enhancer)]       = SofaType( 165, "enhancer");
    m_Map[ GT( e_Imp, eSubtype_gap ) ]          = SofaType( 730, "gap" );
    m_Map[ GT( e_Imp, eSubtype_iDNA ) ]         = SofaType( 723, "iDNA" );
    m_Map[ GT( e_Imp, eSubtype_intron ) ]       = SofaType( 188, "intron" );
    m_Map[ GT( e_Imp, eSubtype_mat_peptide ) ]  = SofaType( 419, "mature_peptide" );
    m_Map[ GT( e_Imp, eSubtype_misc_binding ) ] = SofaType( 409, "binding_site" );
    m_Map[ GT( e_Imp, eSubtype_misc_difference) ] = SofaType( 413, "sequence_difference" );
    m_Map[ GT( e_Imp, eSubtype_misc_feature ) ] = SofaType( 110, "sequence_feature" );
    m_Map[ GT( e_Imp, eSubtype_misc_recomb ) ]  = SofaType( 298, "recombination_feature" );
    m_Map[ GT( e_Imp, eSubtype_misc_signal ) ]  = SofaType( 5836, "regulatory_region" );
    m_Map[ GT( e_Imp, eSubtype_misc_structure ) ] = SofaType( 2, "sequence_secondary_structure" );
    m_Map[ GT( e_Imp, eSubtype_mobile_element ) ]= SofaType( 1037, "mobile_genetic_element" );
    m_Map[ GT( e_Imp, eSubtype_modified_base ) ]= SofaType( 305, "modified_DNA_base" );
    m_Map[ GT( e_Imp, eSubtype_operon ) ]       = SofaType( 178, "operon" );
    m_Map[ GT( e_Imp, eSubtype_oriT ) ]         = SofaType( 724, "oriT" );
    m_Map[ GT( e_Imp, eSubtype_polyA_signal ) ] = SofaType( 551, "polyA_signal_sequence" );
    m_Map[ GT( e_Imp, eSubtype_polyA_site ) ]   = SofaType( 553, "polyA_site" );
    m_Map[ GT( e_Imp, eSubtype_preRNA ) ]       = SofaType( 185, "primary_transcript" );
    m_Map[ GT( e_Imp, eSubtype_prim_transcript ) ] = SofaType( 185, "primary_transcript" );
    m_Map[ GT( e_Imp, eSubtype_primer_bind ) ]  = SofaType( 5850, "primer_binding_site" );
    m_Map[ GT( e_Imp, eSubtype_promoter ) ]     = SofaType( 167, "promoter" );
    m_Map[ GT( e_Imp, eSubtype_protein_bind ) ] = SofaType( 410, "protein_binding_site" );
    m_Map[ GT( e_Imp, eSubtype_rep_origin ) ] = SofaType( 296, "origin_of_replication" );
    m_Map[ GT( e_Imp, eSubtype_repeat_region ) ] = SofaType( 657, "repeat_region" );
    m_Map[ GT( e_Imp, eSubtype_repeat_unit ) ]  = SofaType( 726, "repeat_unit" );
    m_Map[ GT( e_Imp, eSubtype_satellite ) ]    = SofaType( 5, "satellite_DNA" );
    m_Map[ GT( e_Imp, eSubtype_sig_peptide ) ]  = SofaType( 418, "signal_peptide" );
    m_Map[ GT( e_Imp, eSubtype_site_ref ) ]       = SofaType( 408, "site" );
//    m_Map[ GT( e_Imp, eSubtype_source ) ]       = SofaType( 2000061, "databank_entry" );
    m_Map[ GT( e_Imp, eSubtype_stem_loop ) ]    = SofaType( 313, "stem_loop" );
    m_Map[ GT( e_Imp, eSubtype_telomere ) ]   = SofaType( 624, "telomere" );
    m_Map[ GT( e_Imp, eSubtype_terminator ) ]   = SofaType( 141, "terminator" );
    m_Map[ GT( e_Imp, eSubtype_transit_peptide ) ] = SofaType( 725, "transit_peptide" );
    m_Map[ GT( e_Imp, eSubtype_unsure ) ] = SofaType( 1086, "sequence_uncertainty" );
    m_Map[ GT( e_Imp, eSubtype_V_segment ) ]    = SofaType( 466, "V_gene_segment" );
    m_Map[ GT( e_Imp, eSubtype_variation ) ]    = SofaType( 1059, "sequence_alteration" );
    m_Map[ GT( e_Imp, eSubtype_virion ) ]    = SofaType( 1041, "viral_sequence" );

    m_Map[ GT( e_Comment, eSubtype_comment ) ]  = SofaType( 700, "remark" );

    m_Map[ GT( e_Prot, eSubtype_mat_peptide_aa ) ]  = SofaType( 419, "mature_protein_region" );
    m_Map[ GT( e_Prot, eSubtype_preprotein ) ]  = SofaType( 1063, "immature_peptide_region" );
    m_Map[ GT( e_Prot, eSubtype_prot ) ]  = SofaType( 358, "polypeptide" );
    m_Map[ GT( e_Prot, eSubtype_sig_peptide_aa ) ]  = SofaType( 418, "signal_peptide" );
    m_Map[ GT( e_Prot, eSubtype_transit_peptide_aa ) ]  = SofaType( 725, "transit_peptide" );

    m_Map[ GT( e_Psec_str, eSubtype_psec_str ) ]  = SofaType( 2, "sequence_secondary_structure" );

    m_Map[ GT( e_Rsite, eSubtype_rsite ) ]  = SofaType( 168, "restriction_enzyme_cut_site" );

    m_Map[ GT( e_Variation, eSubtype_variation_ref ) ]  = SofaType( 1059, "sequence_alteration" );

    //m_Map[ GT( e_Site, eSubtype_site ) ]  = SofaType( 408, "site" );
}


//  ----------------------------------------------------------------------------
string
CSofaMap::FeatureToSofaType(
    const CSeq_feat& feat)
//  ----------------------------------------------------------------------------
{
    const CSeqFeatData& data = feat.GetData();
    CSeqFeatData::ESubtype subtype = data.GetSubtype();
    if (subtype == CSeqFeatData::eSubtype_bond) {
        switch(data.GetBond()) {
        default:
            return MappedName(data.Which(), subtype);
        case CSeqFeatData::eBond_disulfide:
            return "disulfide_bond";
        case CSeqFeatData::eBond_xlink:
            return "cross_link"; 
        }
    }

    if (subtype == CSeqFeatData::eSubtype_regulatory) {
        typedef SStaticPair<const char*, const char*>  REGULATORY_ENTRY;
        static const REGULATORY_ENTRY regulatoryMap_[] = {
            { "attenuator", "attenuator" },
            { "boundary_element", "insulator" },
            { "CAAT_signal", "CAAT_signal" },
            { "enhancer", "enhancer" },
            { "enhancer_blocking_element", "insulator" },
            { "GC_signal", "GC_rich_promoter_region" },
            { "imprinting_control_region", "regulatory_region" },
            { "locus_control_region", "locus_control_region" },
            { "minus_10_signal", "minus_10_signal" },
            { "minus_35_signal", "minus_35_signal" },
            { "other", "regulatory_region" },
            { "polyA_signal_sequence", "polyA_signal_sequence" },
            { "promoter", "promoter" },
            { "response_element", "regulatory_region" },
            { "ribosome_binding_site", "Shine_Dalgarno_sequence" },
            { "riboswitch", "riboswitch" },
            { "silencer", "silencer" },
            { "TATA_box", "TATA_box" },
            { "terminator", "terminator" },
        };
        typedef CStaticArrayMap<string, string, PNocase> REGULATORY_MAP;
        DEFINE_STATIC_ARRAY_MAP_WITH_COPY(REGULATORY_MAP, regulatoryMap, regulatoryMap_);

        string regulatoryClass = feat.GetNamedQual("regulatory_class");
        REGULATORY_MAP::const_iterator cit = regulatoryMap.find(regulatoryClass);
        if (cit != regulatoryMap.end()) {
            return cit->second;
        }
        return MappedName(data.Which(), subtype);
    }

    if (subtype == CSeqFeatData::eSubtype_ncRNA) {
        const CSeqFeatData::TRna& rna = data.GetRna();
        if ( !rna.IsSetExt() ) {
            return MappedName(data.Which(), subtype);
        }
        const CRNA_ref::TExt& ext = rna.GetExt();
        if ( !ext.IsGen()  ||  !ext.GetGen().IsSetClass()) {
            return MappedName(data.Which(), subtype);
        }
        string ncrnaClass = ext.GetGen().GetClass();
    }   
 
    return MappedName(data.Which(), subtype);
}


#undef GT
        
END_NCBI_SCOPE
