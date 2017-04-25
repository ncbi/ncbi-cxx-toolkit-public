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
#include <objects/seq/so_map.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/seqfeat/RNA_ref.hpp>
#include <objects/seqfeat/RNA_gen.hpp>
#include <objects/seqfeat/Cdregion.hpp>
#include <objects/seqfeat/Imp_feat.hpp>
#include <objects/seqfeat/Prot_ref.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

CSoMap::TYPEMAP CSoMap::mMapSoTypeToId;

CSoMap::TYPEMAP CSoMap::mMapSoIdToType = {
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

CSoMap::FEATFUNCMAP CSoMap::mMapFeatFunc = {
    {"CAGE_cluster",            CSoMap::xFeatureMakeMiscFeature},
    {"CAAT_signal",             CSoMap::xFeatureMakeRegulatory},
    {"CDS",                     CSoMap::xFeatureMakeCds},
    {"C_gene_segment",          CSoMap::xFeatureMakeImp},
    {"DNAsel_hypersensitive_site", CSoMap::xFeatureMakeRegulatory},
    {"D_loop",                  CSoMap::xFeatureMakeImp},
    {"GC_rich_promoter_region", CSoMap::xFeatureMakeRegulatory},
    {"J_gene_segment",          CSoMap::xFeatureMakeImp},
    {"N_region",                CSoMap::xFeatureMakeImp},
    {"RNase_MRP_RNA",           CSoMap::xFeatureMakeNcRna},
    {"RNase_P_RNA",             CSoMap::xFeatureMakeNcRna},
    {"SRP_RNA",                 CSoMap::xFeatureMakeNcRna},
    {"STS",                     CSoMap::xFeatureMakeImp},
    {"S_region",                CSoMap::xFeatureMakeImp},
    {"TATA_box",                CSoMap::xFeatureMakeRegulatory},
    {"V_gene_segment",          CSoMap::xFeatureMakeImp},
    {"V_region",                CSoMap::xFeatureMakeImp},
    {"X_element_combinatorial_repeat", CSoMap::xFeatureMakeRepeatRegion},
    {"Y_RNA",                   CSoMap::xFeatureMakeNcRna},
    {"Y_prime_element",         CSoMap::xFeatureMakeRepeatRegion},
    {"antisense_RNA",           CSoMap::xFeatureMakeNcRna},
    {"attenuator",              CSoMap::xFeatureMakeRegulatory},
    {"autocatalytically_spliced_intron", CSoMap::xFeatureMakeNcRna},
    {"binding_site",            CSoMap::xFeatureMakeImp},
    {"biological_region",       CSoMap::xFeatureMakeMiscFeature},
    {"boundary_element",        CSoMap::xFeatureMakeRegulatory},
    {"centromere",              CSoMap::xFeatureMakeImp},
    {"centromeric_repeat",      CSoMap::xFeatureMakeRepeatRegion},
    {"chromosome_breakpoint",   CSoMap::xFeatureMakeMiscRecomb},
    {"conserved_region",        CSoMap::xFeatureMakeMiscFeature},
    {"direct_repeat",           CSoMap::xFeatureMakeRepeatRegion},
    {"dispersed_repeat",        CSoMap::xFeatureMakeRepeatRegion},
    {"enhancer",                CSoMap::xFeatureMakeRegulatory},
    {"exon",                    CSoMap::xFeatureMakeImp},
    {"five_prime_UTR",          CSoMap::xFeatureMakeImp},
    {"gap",                     CSoMap::xFeatureMakeImp},
    {"gene",                    CSoMap::xFeatureMakeGene},
    {"guide_RNA",               CSoMap::xFeatureMakeNcRna},
    {"hammerhead_ribozyme",     CSoMap::xFeatureMakeNcRna},
    {"iDNA",                    CSoMap::xFeatureMakeImp},
    {"insulator",               CSoMap::xFeatureMakeRegulatory},
    {"intron",                  CSoMap::xFeatureMakeImp},
    {"inverted_repeat",         CSoMap::xFeatureMakeRepeatRegion},
    {"lnc_RNA",                 CSoMap::xFeatureMakeNcRna},
    {"locus_control_region",    CSoMap::xFeatureMakeRegulatory},
    {"long_terminal_repeat",    CSoMap::xFeatureMakeRepeatRegion},
    {"mRNA",                    CSoMap::xFeatureMakeRna},
    {"matrix_attachment_region", CSoMap::xFeatureMakeRegulatory},
    {"mature_protein_region",   CSoMap::xFeatureMakeProt},
    {"meiotic_recombination_region", CSoMap::xFeatureMakeMiscRecomb},
    {"miRNA",                   CSoMap::xFeatureMakeNcRna},
    {"microsatellite",          CSoMap::xFeatureMakeRepeatRegion},
    {"minisatellite",           CSoMap::xFeatureMakeRepeatRegion},
    {"minus_10_signal",         CSoMap::xFeatureMakeRegulatory},
    {"minus_35_signal",         CSoMap::xFeatureMakeRegulatory},
    {"mitotic_recombination_region", CSoMap::xFeatureMakeMiscRecomb},
    {"mobile_genetic_element",  CSoMap::xFeatureMakeImp},
    {"modified_DNA_base",       CSoMap::xFeatureMakeImp},
    {"ncRNA",                   CSoMap::xFeatureMakeNcRna},
    {"nested_repeat",           CSoMap::xFeatureMakeRepeatRegion},
    {"non_allelic_homologous_recombination", CSoMap::xFeatureMakeMiscRecomb},
    {"non_LTR_retrotransposon_polymeric_tract", CSoMap::xFeatureMakeRepeatRegion},
    {"nucleotide_motif",        CSoMap::xFeatureMakeMiscFeature},
    {"nucleotide_cleavage_site", CSoMap::xFeatureMakeMiscFeature},
    {"nucleotide_site",         CSoMap::xFeatureMakeMiscFeature},
    {"operon",                  CSoMap::xFeatureMakeImp},
    {"oriT",                    CSoMap::xFeatureMakeImp},
    {"origin_of_replication",   CSoMap::xFeatureMakeImp},
    {"piRNA",                   CSoMap::xFeatureMakeNcRna},
    {"polyA_signal_sequence",   CSoMap::xFeatureMakeRegulatory},
    {"polyA_site",              CSoMap::xFeatureMakeImp},
    {"primary_transcript",      CSoMap::xFeatureMakeImp},
    {"primer_binding_site",     CSoMap::xFeatureMakeImp},
    {"promoter",                CSoMap::xFeatureMakeRegulatory},
    {"propeptide",              CSoMap::xFeatureMakeProt},
    {"protein_binding_site",    CSoMap::xFeatureMakeImp},
    {"pseudogene",              CSoMap::xFeatureMakeGene},
    {"pseudogenic_CDS",         CSoMap::xFeatureMakeCds},
    {"pseudogenic_rRNA",        CSoMap::xFeatureMakeRna},
    {"pseudogenic_tRNA",        CSoMap::xFeatureMakeRna},
    {"pseudogenic_transcript",  CSoMap::xFeatureMakeMiscRna},
    {"rRNA",                    CSoMap::xFeatureMakeRna},
    {"rasiRNA",                 CSoMap::xFeatureMakeNcRna},
    {"recoding_stimulatory_region", CSoMap::xFeatureMakeRegulatory},
    {"recombination_feature",   CSoMap::xFeatureMakeMiscRecomb},
    {"region",                  CSoMap::xFeatureMakeImp},
    {"regulatory_region",       CSoMap::xFeatureMakeRegulatory},
    {"repeat_instability_region", CSoMap::xFeatureMakeMiscFeature},
    {"repeat_region",           CSoMap::xFeatureMakeRepeatRegion},
    {"replication_regulatory_region", CSoMap::xFeatureMakeRegulatory},
    {"replication_start_site",  CSoMap::xFeatureMakeMiscFeature},
    {"ribosome_entry_site",     CSoMap::xFeatureMakeRegulatory},
    {"riboswitch",              CSoMap::xFeatureMakeRegulatory},
    {"ribozyme",                CSoMap::xFeatureMakeNcRna},
    {"satellite_DNA",           CSoMap::xFeatureMakeRepeatRegion},
    {"scRNA",                   CSoMap::xFeatureMakeNcRna},
    {"sequence_alteration",     CSoMap::xFeatureMakeImp},
    {"sequence_comparison",     CSoMap::xFeatureMakeMiscFeature},
    {"sequence_difference",     CSoMap::xFeatureMakeImp},
    {"sequence_feature",        CSoMap::xFeatureMakeMiscFeature},
    {"sequence_secondary_structure", CSoMap::xFeatureMakeImp},
    {"sequence_uncertainty",    CSoMap::xFeatureMakeImp},
    {"siRNA",                   CSoMap::xFeatureMakeNcRna},
    {"signal_peptide",          CSoMap::xFeatureMakeImp},
    {"silencer",                CSoMap::xFeatureMakeRegulatory},
    {"snRNA",                   CSoMap::xFeatureMakeNcRna},
    {"snoRNA",                  CSoMap::xFeatureMakeNcRna},
    {"stem_loop",               CSoMap::xFeatureMakeImp},
    {"tRNA",                    CSoMap::xFeatureMakeRna},
    {"tandem_repeat",           CSoMap::xFeatureMakeRepeatRegion},
    {"telomerase_RNA",          CSoMap::xFeatureMakeNcRna},
    {"telomere",                CSoMap::xFeatureMakeImp},
    {"telomeric_repeat",        CSoMap::xFeatureMakeRepeatRegion},
    {"terminator",              CSoMap::xFeatureMakeRegulatory},
    {"tmRNA",                   CSoMap::xFeatureMakeRna},
    {"transcript",              CSoMap::xFeatureMakeMiscRna},
    {"transcriptional_cis_regulatory_region", CSoMap::xFeatureMakeRegulatory},
    {"transcription_start_site", CSoMap::xFeatureMakeMiscFeature},
    {"three_prime_UTR",         CSoMap::xFeatureMakeImp},
    {"vault_RNA",               CSoMap::xFeatureMakeNcRna},
};

//  ----------------------------------------------------------------------------
string CSoMap::SoIdToType(
    const string& sofa_id)
//  ----------------------------------------------------------------------------
{
    TYPEENTRY type_it = CSoMap::mMapSoIdToType.find(sofa_id);
    if (type_it == CSoMap::mMapSoIdToType.end()) {
        return "";
    }
    return type_it->second;
}

//  ----------------------------------------------------------------------------
string CSoMap::SoTypeToId(
    const string& so_type)
//  ----------------------------------------------------------------------------
{
    if (CSoMap::mMapSoTypeToId.empty()) {
        for (TYPEENTRY  cit = CSoMap::mMapSoIdToType.begin();
                cit != CSoMap::mMapSoIdToType.end();
                ++cit) {
            CSoMap::mMapSoTypeToId[cit->second] = cit->first;
        }
    }
    TYPEENTRY id_it = mMapSoTypeToId.find(so_type);
    if (id_it == CSoMap::mMapSoTypeToId.end()) {
        return "";
    }
    return id_it->second;
} 

//  ----------------------------------------------------------------------------
bool CSoMap::SoTypeToFeature(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    FEATFUNCENTRY it = mMapFeatFunc.find(so_type);
    if (it != mMapFeatFunc.end()) {
        return (it->second)(so_type, feature);
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeGene(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetGene();
    feature.SetPseudo(so_type=="pseudogene");
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeRna(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, CRNA_ref::EType> mTypeToRna = {
        {"mRNA", CRNA_ref::eType_mRNA},
        {"rRNA", CRNA_ref::eType_rRNA},
        {"pseudogenic_rRNA", CRNA_ref::eType_rRNA},
        {"tRNA", CRNA_ref::eType_tRNA},
        {"pseudogenic_tRNA", CRNA_ref::eType_tRNA},
        {"tmRNA", CRNA_ref::eType_tmRNA},
    };
    map<string, CRNA_ref::EType>::const_iterator it = mTypeToRna.find(so_type);
    feature.SetData().SetRna().SetType(it->second);
    feature.SetPseudo(NStr::StartsWith(so_type, "pseudogenic_"));
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeNcRna(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mTypeToClass = {
        {"ncRNA", "other"},
    };
    feature.SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
    map<string, string>::const_iterator it = mTypeToClass.find(so_type);
    if (it == mTypeToClass.end()) {
        feature.SetData().SetRna().SetExt().SetGen().SetClass(so_type);
    }
    else {
        feature.SetData().SetRna().SetExt().SetGen().SetClass(it->second);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeCds(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetCdregion();
    feature.SetPseudo(so_type=="pseudogenic_CDS");
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeProt(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, CProt_ref::EProcessed> mTypeToProcessed = {
        {"mature_protein_region", CProt_ref::eProcessed_mature},
        {"propeptide", CProt_ref::eProcessed_propeptide},
    };
    map<string, CProt_ref::EProcessed>::const_iterator cit = 
        mTypeToProcessed.find(so_type);
    if (cit == mTypeToProcessed.end()) {
        return false;
    }
    feature.SetData().SetProt().SetProcessed(cit->second);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeMiscFeature(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mapTypeToQual = {
        {"TSS", "transcription_start_site"},
    };
    feature.SetData().SetImp().SetKey("misc_feature");
    if (so_type == "sequence_feature") {
        return true;
    }
    CRef<CGb_qual> feat_class(new CGb_qual);
    feat_class->SetQual("feat_class");
    map<string, string>::const_iterator cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        feat_class->SetVal(so_type);
    }
    else {
        feat_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(feat_class);   
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeMiscRecomb(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mapTypeToQual = {
        {"meiotic_recombination_region", "meiotic"},
        {"mitotic_recombination_region", "mitotic"},
        {"non_allelic_homologous_recombination", "non_allelic_homologous"},
        {"recombination_feature", "other"},
    };
    feature.SetData().SetImp().SetKey("misc_recomb");
    CRef<CGb_qual> recombination_class(new CGb_qual);
    recombination_class->SetQual("recombination_class");
    map<string, string>::const_iterator cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        recombination_class->SetVal(so_type);
    }
    else {
        recombination_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(recombination_class);   
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeMiscRna(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetImp().SetKey("misc_RNA");
    feature.SetPseudo(so_type=="pseudogenic_transcript");
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeImp(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mapTypeToKey = {
        {"C_gene_segment", "C_region"},
        {"D_gene_segment", "D_segment"},
        {"D_loop", "D_loop"},
        {"J_gene_segment", "J_segment"},
        {"V_gene_segment", "V_segment"},
        {"binding_site", "misc_binding"},
        {"five_prime_utr", "5\'UTR"},
        {"long_terminal_repeat", "LTR"},
        {"mobile_genetic_element", "mobile_element"},
        {"modified_DNA_base", "modified_base"},
        {"origin_of_replication", "rep_origin"},
        {"primary_transcript", "prim_transcript"},
        {"primer_binding_site", "primer_bind"},
        {"protein_binding_site", "protein_bind"},
        {"region", "source"},
        {"sequence_alteration", "variation"},
        {"sequence_difference", "misc_difference"},
        {"sequence_secondary_structure", "misc_structure"},
        {"sequence_uncertainty", "unsure"},
        {"signal_peptide", "sig_peptide"},
        {"three_prime_utr", "3\'UTR"},
    };
    map<string, string>::const_iterator cit = mapTypeToKey.find(so_type);
    if (cit == mapTypeToKey.end()) {
        feature.SetData().SetImp().SetKey(so_type);
    }
    else {
        feature.SetData().SetImp().SetKey(cit->second);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeRegion(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetRegion();
    CRef<CGb_qual> qual(new CGb_qual("SO_type", so_type));
    feature.SetQual().push_back(qual);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeRegulatory(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mapTypeToQual = {
        {"DNAsel_hypersensitive_site", "DNase_I_hypersensitive_site"}, 
        {"GC_rich_promoter_region", "GC_signal"},
        {"boundary_element", "insulator"},
        {"regulatory_region", "other"},
        {"ribosome_entry_site", "ribosome_binding_site"},
    };
    feature.SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> regulatory_class(new CGb_qual);
    regulatory_class->SetQual("regulatory_class");
    map<string, string>::const_iterator cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        regulatory_class->SetVal(so_type);
    }
    else {
        regulatory_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(regulatory_class);   
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeRepeatRegion(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, string> mapTypeToQual = {
        {"tandem_repeat", "tandem"},
        {"inverted_repeat", "inverted"},
        {"direct_repeat", "direct"},
        {"nested_repeat", "nested"},
        {"non_LTR_retrotransposon_polymeric_tract", "non_ltr_retrotransposon_polymeric_tract"},
        {"X_element_combinatorial_repeat", "x_element_combinatorial_repeat"},
        {"Y_prime_element", "y_prime_element"},
        {"repeat_region", "other"},
        {"satellite_DNA", "satellite"},
    };
    feature.SetData().SetImp().SetKey("repeat_region");
    CRef<CGb_qual> rpt_type(new CGb_qual);
    rpt_type->SetQual("rpt_type");
    map<string, string>::const_iterator cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        rpt_type->SetVal(so_type);
    }
    else {
        rpt_type->SetVal(cit->second);
    }
    feature.SetQual().push_back(rpt_type);   
    return true;
}
        
//  ----------------------------------------------------------------------------
bool CSoMap::FeatureToSoType(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    if (xSubtypeToSoType(feature, so_type)) {
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xSubtypeToSoType(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    map<CSeqFeatData::ESubtype, string> mapSubtypeToSoType = {
    };
    return false;
}

END_NCBI_SCOPE
