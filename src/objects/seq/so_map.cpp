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
#include <objects/seqfeat/SeqFeatData.hpp>
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

//  ----------------------------------------------------------------------------
bool CompareNoCase::operator()(
    const string& lhs,
    const string& rhs) const
//  ----------------------------------------------------------------------------
{
    string::const_iterator pLhs = lhs.begin();
    string::const_iterator pRhs = rhs.begin();
    while (pLhs != lhs.end()  &&  pRhs != rhs.end()  &&  
            tolower(*pLhs) ==  tolower(*pRhs)) {
        ++pLhs;
        ++pRhs;
    }
    if (pLhs == lhs.end()) {
        return (pRhs != rhs.end());
    }
    if (pRhs == rhs.end()) {
        return false;
    }
    return (tolower(*pLhs) < tolower(*pRhs)); 
}

//  ----------------------------------------------------------------------------
string GetUnambiguousNamedQual(
    const CSeq_feat& feature, 
    const string& qualName)
//  ----------------------------------------------------------------------------
{
    string namedQual;
    const auto& quals = feature.GetQual();
    for (const auto& qual: quals) {
        if (!qual->CanGetQual()  ||  !qual->CanGetVal()) {
            continue;
        }
        if (qual->GetQual() != qualName) {
            continue;
        }
        if (namedQual.empty()) {
            namedQual = qual->GetVal();
            continue;
        }
        if (namedQual != qual->GetVal()) {
            return "";
        }
    }
    return namedQual;
}

//  ----------------------------------------------------------------------------
CSoMap::TYPEMAP CSoMap::mMapSoTypeToId;
//  ----------------------------------------------------------------------------

//  ----------------------------------------------------------------------------
CSoMap::TYPEMAP CSoMap::mMapSoIdToType = {
//  ----------------------------------------------------------------------------
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
    {"SO:0000507", "pseudogenic_exon"},
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
    {"SO:0001720", "epigenetically_modified_region"},
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
    {"SO:0002190", "enhancer_blocking_element"},
    {"SO:0002191", "imprinting_control_region"},
    {"SO:0002205", "response_element"},
    {"SO:0005836", "regulatory_region"},
    {"SO:0005850", "primary_binding_site"},

    {"SO:0000000", ""},
    //{"SO:UNKNOWN", "replication_start_site"},
    //{"SO:UNKNOWN", "nucleotide_site"},
    //{"SO:UNKNOWN", "nucleotide_cleavage_site"},
    //{"SO:UNKNOWN", "repeat_instability_region"},
};

//  ----------------------------------------------------------------------------
CSoMap::FEATFUNCMAP CSoMap::mMapFeatFunc = {
//  ----------------------------------------------------------------------------
    {"CAGE_cluster",            CSoMap::xFeatureMakeMiscFeature},
    {"CAAT_signal",             CSoMap::xFeatureMakeRegulatory},
    {"CDS",                     CSoMap::xFeatureMakeCds},
    {"C_gene_segment",          CSoMap::xFeatureMakeImp},
    {"DNAsel_hypersensitive_site", CSoMap::xFeatureMakeRegulatory},
    {"D_loop",                  CSoMap::xFeatureMakeImp},
    {"D_gene_segment",          CSoMap::xFeatureMakeImp},
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
    {"X_element_combinatorical_repeat", CSoMap::xFeatureMakeRepeatRegion},
    {"Y_RNA",                   CSoMap::xFeatureMakeNcRna},
    {"Y_prime_element",         CSoMap::xFeatureMakeRepeatRegion},
    {"antisense_RNA",           CSoMap::xFeatureMakeNcRna},
    {"attenuator",              CSoMap::xFeatureMakeRegulatory},
    {"autocatalytically_spliced_intron", CSoMap::xFeatureMakeNcRna},
    {"binding_site",            CSoMap::xFeatureMakeImp},
    {"biological_region",       CSoMap::xFeatureMakeRegion},
    {"boundary_element",        CSoMap::xFeatureMakeRegulatory},
    {"centromere",              CSoMap::xFeatureMakeImp},
    {"centromeric_repeat",      CSoMap::xFeatureMakeRepeatRegion},
    {"chromosome_breakpoint",   CSoMap::xFeatureMakeMiscRecomb},
    {"conserved_region",        CSoMap::xFeatureMakeMiscFeature},
    {"direct_repeat",           CSoMap::xFeatureMakeRepeatRegion},
    {"dispersed_repeat",        CSoMap::xFeatureMakeRepeatRegion},
    {"enhancer",                CSoMap::xFeatureMakeRegulatory},
    {"enhancer_blocking_element", CSoMap::xFeatureMakeRegulatory},
    {"epigenetically_modified_region", CSoMap::xFeatureMakeRegulatory},
    {"exon",                    CSoMap::xFeatureMakeImp},
    {"five_prime_UTR",          CSoMap::xFeatureMakeImp},
    {"gap",                     CSoMap::xFeatureMakeImp},
    {"gene",                    CSoMap::xFeatureMakeGene},
    {"guide_RNA",               CSoMap::xFeatureMakeNcRna},
    {"hammerhead_ribozyme",     CSoMap::xFeatureMakeNcRna},
    {"iDNA",                    CSoMap::xFeatureMakeImp},
    {"immature_peptide_region", CSoMap::xFeatureMakeProt},
    {"imprinting_control_region", CSoMap::xFeatureMakeRegulatory},
    {"insulator",               CSoMap::xFeatureMakeRegulatory},
    {"intron",                  CSoMap::xFeatureMakeImp},
    {"inverted_repeat",         CSoMap::xFeatureMakeRepeatRegion},
    {"lnc_RNA",                 CSoMap::xFeatureMakeNcRna},
    {"locus_control_region",    CSoMap::xFeatureMakeRegulatory},
    {"long_terminal_repeat",    CSoMap::xFeatureMakeRepeatRegion},
    {"mRNA",                    CSoMap::xFeatureMakeRna},
    {"matrix_attachment_region", CSoMap::xFeatureMakeRegulatory},
    {"mature_protein_region",   CSoMap::xFeatureMakeImp},
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
    {"protein_binding_site",    CSoMap::xFeatureMakeImp},
    {"pseudogene",              CSoMap::xFeatureMakeGene},
    {"pseudogenic_exon",        CSoMap::xFeatureMakeImp},
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
    {"response_element",        CSoMap::xFeatureMakeRegulatory},
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
    {"transit_peptide",         CSoMap::xFeatureMakeImp},
    {"three_prime_UTR",         CSoMap::xFeatureMakeImp},
    {"vault_RNA",               CSoMap::xFeatureMakeNcRna},
};


//  ----------------------------------------------------------------------------
bool CSoMap::GetSupportedSoTerms(
    vector<string>& supported_terms)
//  ----------------------------------------------------------------------------
{
    supported_terms.clear();
    for (auto term: CSoMap::mMapFeatFunc) {
        supported_terms.push_back(term.first);
    }
    std::sort(supported_terms.begin(), supported_terms.end());
    return true;
}

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
    CSeq_feat& feature,
    bool invalidToRegion)
//  ----------------------------------------------------------------------------
{
    string resolved_so_type = ResolveSoAlias(so_type);
    FEATFUNCENTRY it = mMapFeatFunc.find(resolved_so_type);
    if (it != mMapFeatFunc.end()) {
        return (it->second)(resolved_so_type, feature);
    }
    if (invalidToRegion) {
        return xFeatureMakeRegion(so_type, feature);
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
    if (so_type == "pseudogene") {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeRna(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, CRNA_ref::EType, CompareNoCase> mTypeToRna = {
        {"mRNA", CRNA_ref::eType_mRNA},
        {"rRNA", CRNA_ref::eType_rRNA},
        {"pseudogenic_rRNA", CRNA_ref::eType_rRNA},
        {"tRNA", CRNA_ref::eType_tRNA},
        {"pseudogenic_tRNA", CRNA_ref::eType_tRNA},
        {"tmRNA", CRNA_ref::eType_tmRNA},
    };
    auto it = mTypeToRna.find(so_type);
    feature.SetData().SetRna().SetType(it->second);
    if(NStr::StartsWith(so_type, "pseudogenic_")) {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeNcRna(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mTypeToClass = {
        {"ncRNA", "other"},
    };
    feature.SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
    auto normalizedType = so_type;
    auto it = mTypeToClass.find(so_type);
    if (it != mTypeToClass.end()) {
        normalizedType = it->second;
    }
    feature.SetData().SetRna().SetExt().SetGen().SetClass(normalizedType);
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeCds(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetCdregion();
    if (so_type=="pseudogenic_CDS") {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeProt(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const map<string, CProt_ref::EProcessed, CompareNoCase> 
            mTypeToProcessed = {
        {"mature_protein_region", CProt_ref::eProcessed_mature},
        {"immature_peptide_region", CProt_ref::eProcessed_preprotein},
    };
    auto cit = mTypeToProcessed.find(so_type);
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
    static const TYPEMAP mapTypeToQual = {
        {"TSS", "transcription_start_site"},
    };
    feature.SetData().SetImp().SetKey("misc_feature");
    if (so_type == "sequence_feature") {
        return true;
    }
    CRef<CGb_qual> feat_class(new CGb_qual);
    feat_class->SetQual("feat_class");
    auto cit = mapTypeToQual.find(so_type);
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
    static const TYPEMAP mapTypeToQual = {
        {"meiotic_recombination_region", "meiotic"},
        {"mitotic_recombination_region", "mitotic"},
        {"non_allelic_homologous_recombination", "non_allelic_homologous"},
        {"recombination_feature", "other"},
    };
    feature.SetData().SetImp().SetKey("misc_recomb");
    CRef<CGb_qual> recombination_class(new CGb_qual);
    recombination_class->SetQual("recombination_class");
    auto cit = mapTypeToQual.find(so_type);
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
    if (so_type=="pseudogenic_transcript") {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xFeatureMakeImp(
    const string& so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapTypeToKey = {
        {"C_gene_segment", "C_region"},
        {"D_gene_segment", "D_segment"},
        {"D_loop", "D-loop"},
        {"J_gene_segment", "J_segment"},
        {"V_gene_segment", "V_segment"},
        {"binding_site", "misc_binding"},
        {"five_prime_UTR", "5\'UTR"},
        {"long_terminal_repeat", "LTR"},
        {"mature_protein_region", "mat_peptide"},
        {"mobile_genetic_element", "mobile_element"},
        {"modified_DNA_base", "modified_base"},
        {"origin_of_replication", "rep_origin"},
        {"primary_transcript", "prim_transcript"},
        {"primer_binding_site", "primer_bind"},
        {"protein_binding_site", "protein_bind"},
        {"pseudogenic_exon", "exon"},
        {"region", "source"},
        {"sequence_alteration", "variation"},
        {"sequence_difference", "misc_difference"},
        {"sequence_secondary_structure", "misc_structure"},
        {"sequence_uncertainty", "unsure"},
        {"signal_peptide", "sig_peptide"},
        {"three_prime_UTR", "3\'UTR"},
    };
    auto cit = mapTypeToKey.find(so_type);
    if (cit == mapTypeToKey.end()) {
        feature.SetData().SetImp().SetKey(so_type);
    }
    else {
        feature.SetData().SetImp().SetKey(cit->second);
    }
    if(NStr::StartsWith(so_type, "pseudogenic_")) {
        feature.SetPseudo(true);
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
    static const TYPEMAP mapTypeToQual = {
        {"DNAsel_hypersensitive_site", "DNase_I_hypersensitive_site"}, 
        {"GC_rich_promoter_region", "GC_signal"},
        {"boundary_element", "insulator"},
        {"regulatory_region", "other"},
        {"ribosome_entry_site", "ribosome_binding_site"},
    };
    feature.SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> regulatory_class(new CGb_qual);
    regulatory_class->SetQual("regulatory_class");
    auto cit = mapTypeToQual.find(so_type);
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
    static const TYPEMAP mapTypeToSatellite = {
        {"microsatellite", "microsatellite"},
        {"minisatellite", "minisatellite"},
        {"satellite_DNA", "satellite"},
    };
    static const TYPEMAP mapTypeToRptType = {
        {"tandem_repeat", "tandem"},
        {"inverted_repeat", "inverted"},
        {"direct_repeat", "direct"},
        {"nested_repeat", "nested"},
        {"non_LTR_retrotransposon_polymeric_tract", "non_ltr_retrotransposon_polymeric_tract"},
        {"X_element_combinatorial_repeat", "x_element_combinatorial_repeat"},
        {"Y_prime_element", "y_prime_element"},
        {"repeat_region", "other"},
    };
    feature.SetData().SetImp().SetKey("repeat_region");

    CRef<CGb_qual> qual(new CGb_qual);
    auto cit = mapTypeToSatellite.find(so_type);
    if (cit != mapTypeToSatellite.end()) {
        qual->SetQual("satellite");
        qual->SetVal(cit->second);
    }
    else {
        qual->SetQual("rpt_type");
        cit = mapTypeToRptType.find(so_type);
        if (cit == mapTypeToRptType.end()) {
            qual->SetVal(so_type);
        }
        else {
            qual->SetVal(cit->second);
        }
    }
    feature.SetQual().push_back(qual);   
    return true;
}
        

//  ----------------------------------------------------------------------------
CSoMap::TYPEFUNCMAP CSoMap::mMapTypeFunc = {
//  ----------------------------------------------------------------------------
    {CSeqFeatData::eSubtype_3UTR, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_5UTR, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_assembly_gap, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_C_region, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_cdregion, CSoMap::xMapCds},
    {CSeqFeatData::eSubtype_centromere, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_conflict, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_D_loop, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_D_segment, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_exon, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_gap, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_gene, CSoMap::xMapGene},
    {CSeqFeatData::eSubtype_10_signal, CSoMap::xMapGene},
    {CSeqFeatData::eSubtype_iDNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_intron, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_J_segment, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_LTR, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_mat_peptide, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_mat_peptide_aa, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_misc_binding, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_misc_difference, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_misc_feature, CSoMap::xMapMiscFeature}, 
    {CSeqFeatData::eSubtype_misc_recomb, CSoMap::xMapMiscRecomb},
    {CSeqFeatData::eSubtype_misc_RNA, CSoMap::xMapRna},
    {CSeqFeatData::eSubtype_misc_structure, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_mobile_element, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_modified_base, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_mRNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_N_region, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_ncRNA, CSoMap::xMapNcRna},
    {CSeqFeatData::eSubtype_operon, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_oriT, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_otherRNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_polyA_site, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_preRNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_precursor_RNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_preprotein, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_prim_transcript, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_primer_bind, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_propeptide, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_prot, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_protein_bind, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_region, CSoMap::xMapRegion},
    {CSeqFeatData::eSubtype_regulatory, CSoMap::xMapRegulatory},
    {CSeqFeatData::eSubtype_rep_origin, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_repeat_region, CSoMap::xMapRepeatRegion},
    {CSeqFeatData::eSubtype_rRNA, CSoMap::xMapRna},
    {CSeqFeatData::eSubtype_S_region, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_sig_peptide, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_sig_peptide_aa, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_source, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_stem_loop, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_STS, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_snRNA, CSoMap::xMapNcRna},
    {CSeqFeatData::eSubtype_snoRNA, CSoMap::xMapNcRna},
    {CSeqFeatData::eSubtype_telomere, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_tmRNA, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_transit_peptide, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_transit_peptide_aa, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_tRNA, CSoMap::xMapRna},
    {CSeqFeatData::eSubtype_unsure, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_V_region, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_V_segment, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_variation, CSoMap::xMapGeneric},

    {CSeqFeatData::eSubtype_attenuator, CSoMap::xMapGeneric},
    {CSeqFeatData::eSubtype_enhancer, xMapGeneric},
    {CSeqFeatData::eSubtype_promoter, xMapGeneric},
    {CSeqFeatData::eSubtype_terminator, CSoMap::xMapGeneric},

    {CSeqFeatData::eSubtype_bond, CSoMap::xMapBond},
};

//  ----------------------------------------------------------------------------
bool CSoMap::FeatureToSoType(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    auto original_type = feature.GetNamedQual("SO_type");
    if (!original_type.empty()) {
        so_type = original_type;
        return true;
    }
    auto subtype = feature.GetData().GetSubtype();
    TYPEFUNCENTRY cit = mMapTypeFunc.find(subtype);
    if (cit == mMapTypeFunc.end()) {
        return false;
    }
    return (cit->second)(feature, so_type);
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapGeneric(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const map<CSeqFeatData::ESubtype, string> mapSubtypeToSoType = {
        {CSeqFeatData::eSubtype_3UTR, "three_prime_UTR"},
        {CSeqFeatData::eSubtype_5UTR, "five_prime_UTR"},
        {CSeqFeatData::eSubtype_assembly_gap, "assemply_gap"},
        {CSeqFeatData::eSubtype_C_region, "C_gene_segment"},
        {CSeqFeatData::eSubtype_centromere, "centromere"},
        {CSeqFeatData::eSubtype_conflict, "sequence_conflict"},
        {CSeqFeatData::eSubtype_D_loop, "D_loop"},
        {CSeqFeatData::eSubtype_D_segment, "D_gene_segment"},
        {CSeqFeatData::eSubtype_exon, "exon"},
        {CSeqFeatData::eSubtype_enhancer, "enhancer"},
        {CSeqFeatData::eSubtype_gap, "gap"},
        {CSeqFeatData::eSubtype_iDNA, "iDNA"},
        {CSeqFeatData::eSubtype_intron, "intron"},
        {CSeqFeatData::eSubtype_J_segment, "J_gene_segment"},
        {CSeqFeatData::eSubtype_LTR, "long_terminal_repeat"},
        {CSeqFeatData::eSubtype_mat_peptide, "mature_protein_region"},
        {CSeqFeatData::eSubtype_mat_peptide_aa, "mature_protein_region"},
        {CSeqFeatData::eSubtype_misc_binding, "binding_site"},
        {CSeqFeatData::eSubtype_misc_difference, "sequence_difference"},
        {CSeqFeatData::eSubtype_misc_structure, "sequence_secondary_structure"},
        {CSeqFeatData::eSubtype_mobile_element, "mobile_genetic_element"},
        {CSeqFeatData::eSubtype_modified_base, "modified_DNA_base"},
        {CSeqFeatData::eSubtype_mRNA, "mRNA"},
        {CSeqFeatData::eSubtype_N_region, "N_region"}, 
        {CSeqFeatData::eSubtype_operon, "operon"}, 
        {CSeqFeatData::eSubtype_oriT, "oriT"}, 
        {CSeqFeatData::eSubtype_otherRNA, "transcript"},
        {CSeqFeatData::eSubtype_polyA_site, "polyA_site"}, 
        {CSeqFeatData::eSubtype_precursor_RNA, "primary_transcript"}, 
        {CSeqFeatData::eSubtype_preRNA, "primary_transcript"},
        {CSeqFeatData::eSubtype_preprotein, "immature_peptide_region"},
        {CSeqFeatData::eSubtype_prim_transcript, "primary_transcript"}, 
        {CSeqFeatData::eSubtype_primer_bind, "primer_binding_site"}, 
        {CSeqFeatData::eSubtype_promoter, "promoter"}, 
        {CSeqFeatData::eSubtype_propeptide, "propeptide"}, 
        {CSeqFeatData::eSubtype_prot, "polypeptide"},
        {CSeqFeatData::eSubtype_protein_bind, "protein_binding_site"},
        {CSeqFeatData::eSubtype_rep_origin, "origin_of_replication"},
        {CSeqFeatData::eSubtype_S_region, "S_region"},
        {CSeqFeatData::eSubtype_sig_peptide, "signal_peptide"},
        {CSeqFeatData::eSubtype_sig_peptide_aa, "signal_peptide"},
        {CSeqFeatData::eSubtype_source, "region"},
        {CSeqFeatData::eSubtype_stem_loop, "stem_loop"},
        {CSeqFeatData::eSubtype_STS, "STS"},
        {CSeqFeatData::eSubtype_telomere, "telomere"},
        {CSeqFeatData::eSubtype_terminator, "terminator"},
        {CSeqFeatData::eSubtype_tmRNA, "tmRNA"},
        {CSeqFeatData::eSubtype_transit_peptide, "transit_peptide"},
        {CSeqFeatData::eSubtype_transit_peptide_aa, "transit_peptide"},
        {CSeqFeatData::eSubtype_unsure, "sequence_uncertainty"},
        {CSeqFeatData::eSubtype_V_region, "V_region"},
        {CSeqFeatData::eSubtype_V_segment, "V_gene_segment"},
        {CSeqFeatData::eSubtype_variation, "sequence_alteration"},
        //{CSeqFeatData::eSubtype_attenuator, "attenuator"},
    };
    auto subtype = feature.GetData().GetSubtype();
    auto cit = mapSubtypeToSoType.find(subtype);
    if (cit != mapSubtypeToSoType.end()) {
        so_type = cit->second;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapRegion(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    so_type = "biological_region";
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapCds(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    if (feature.IsSetPseudo()  &&  feature.GetPseudo()) {
        so_type = "pseudogenic_CDS";
        return true;
    }
    for (auto qual: feature.GetQual()) {
        if (qual->GetQual() == "pseudo"  ||  qual->GetQual() == "pseudogene") {
            so_type = "pseudogenic_CDS";
            return true;
        }
    }
    so_type = "CDS";
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapGene(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    if (feature.IsSetPseudo()  &&  feature.GetPseudo()) {
        so_type = "pseudogene";
        return true;
    }
    for (auto qual: feature.GetQual()) {
        if (qual->GetQual() == "pseudo"  ||  qual->GetQual() == "pseudogene") {
            so_type = "pseudogene";
            return true;
        }
    }
    so_type = "gene";
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapRna(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const map<CSeqFeatData::ESubtype, string> mapSubtypeStraight = {
        {CSeqFeatData::eSubtype_misc_RNA, "transcript"},
        {CSeqFeatData::eSubtype_rRNA, "rRNA"},
        {CSeqFeatData::eSubtype_tRNA, "tRNA"},
    };
    static const map<CSeqFeatData::ESubtype, string> mapSubtypePseudo = {
        {CSeqFeatData::eSubtype_misc_RNA, "pseudogenic_transcript"},
        {CSeqFeatData::eSubtype_rRNA, "pseudogenic_rRNA"},
        {CSeqFeatData::eSubtype_tRNA, "pseudogenic_tRNA"},
    };

    auto subtype = feature.GetData().GetSubtype();
    if (feature.IsSetPseudo()  &&  feature.GetPseudo()) {
        auto cit = mapSubtypePseudo.find(subtype);
        if (cit == mapSubtypePseudo.end()) {
            return false;
        }
        so_type = cit->second;
        return true;
    }
    if (feature.IsSetPseudo()  &&  !feature.GetPseudo()) {
        auto cit = mapSubtypeStraight.find(subtype);
        if (cit == mapSubtypeStraight.end()) {
            return false;
        }
        so_type = cit->second;
        return true;
    }

    for (auto qual: feature.GetQual()) {
        if (qual->GetQual() == "pseudo"  ||  qual->GetQual() == "pseudogene") {
            auto cit = mapSubtypePseudo.find(subtype);
            if (cit == mapSubtypePseudo.end()) {
                return false;
            }
            so_type = cit->second;
            return true;
        }
    }
    auto cit = mapSubtypeStraight.find(subtype);
    if (cit == mapSubtypeStraight.end()) {
        return false;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapMiscFeature(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapFeatClassToSoType = {
        {"transcription_start_site", "TSS"},
        {"other", "sequence_feature"},
    };
    string feat_class = GetUnambiguousNamedQual(feature, "feat_class");
    if (feat_class.empty()) {
        so_type = "sequence_feature";
        return true;
    }
    auto cit = mapFeatClassToSoType.find(feat_class);
    if (cit == mapFeatClassToSoType.end()) {
        so_type = feat_class;
        return true;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapMiscRecomb(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapRecombClassToSoType = {
        {"meiotic", "meiotic_recombination_region"},
        {"mitotic", "mitotic_recombination_region"},
        {"non_allelic_homologous", "non_allelic_homologous_recombination_region"},
        {"meiotic_recombination", "meiotic_recombination_region"},
        {"mitotic_recombination", "mitotic_recombination_region"},
        {"non_allelic_homologous_recombination", "non_allelic_homologous_recombination_region"},
        {"other", "recombination_feature"},
    };
    string recomb_class = GetUnambiguousNamedQual(feature, "recombination_class");
    if (recomb_class.empty()) {
        so_type = "recombination_feature";
        return true;
    }
    auto cit = mapRecombClassToSoType.find(recomb_class);
    if (cit == mapRecombClassToSoType.end()) {
        auto validClasses = CSeqFeatData::GetRecombinationClassList();
        auto valid = std::find(validClasses.begin(), validClasses.end(), recomb_class);
        if (valid == validClasses.end()) {
            so_type = "recombination_feature";
        }
        else {
            so_type = recomb_class;
        }
        return true;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapOtherRna(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    so_type = "transcript";
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapNcRna(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapNcRnaClassToSoType = {
        {"antisense_RNA", "antisense_RNA"},
        {"autocatalytically_spliced_intron", "autocatalytically_spliced_intron"},
        {"guide_RNA", "guide_RNA"},
        {"hammerhead_ribozyme", "hammerhead_ribozyme"},
        {"lncRNA", "lnc_RNA"},
        {"miRNA", "miRNA"},
        {"other", "ncRNA"},
        {"piRNA", "piRNA"},
        {"rasiRNA", "rasiRNA"},
        {"ribozyme", "ribozyme"},
        {"RNase_MRP_RNA", "RNase_MRP_RNA"},
        {"RNase_P_RNA", "RNase_P_RNA"},
        {"scRNA", "scRNA"},
        {"siRNA", "siRNA"},
        {"snRNA", "snRNA"},
        {"snoRNA", "snoRNA"},
        {"SRP_RNA", "SRP_RNA"},
        {"telomerase_RNA", "telomerase_RNA"},
        {"vault_RNA", "vault_RNA"},
        {"Y_RNA", "Y_RNA"},
    };
    string ncrna_class = GetUnambiguousNamedQual(feature, "ncRNA_class");
    if (ncrna_class.empty()) {
        if (feature.IsSetData()  &&
                feature.GetData().IsRna()  &&
                feature.GetData().GetRna().IsSetExt()  &&
                feature.GetData().GetRna().GetExt().IsGen()  &&
                feature.GetData().GetRna().GetExt().GetGen().IsSetClass()) {
            ncrna_class = feature.GetData().GetRna().GetExt().GetGen().GetClass();
            if (ncrna_class == "classRNA") {
                ncrna_class = "ncRNA";
            }
        }
    }
    if (ncrna_class.empty()) {
        if (feature.IsSetData()  &&
                feature.GetData().IsRna()  &&
                feature.GetData().GetRna().IsSetType()) {
            auto ncrna_type = feature.GetData().GetRna().GetType();
            ncrna_class = CRNA_ref::GetRnaTypeName(ncrna_type);
        }
    }
    if (ncrna_class.empty()) {
        ncrna_class = "ncRNA";
        return true;
    }
    auto cit = mapNcRnaClassToSoType.find(ncrna_class);
    if (cit == mapNcRnaClassToSoType.end()) {
        so_type = "ncRNA";
        return true;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapRegulatory(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapRegulatoryClassToSoType = {
        {"DNase_I_hypersensitive_site", "DNaseI_hypersensitive_site"},
        {"GC_signal", "GC_rich_promoter_region"},
        {"enhancer_blocking_element", "enhancer_blocking_element"},
        {"epigenetically_modified_region", "epigenetically_modified_region"},
        {"imprinting_control_region", "imprinting_control_region"},
        {"matrix_attachment_region", "matrix_attachment_site"},
        {"other", "regulatory_region"},
        {"response_element", "response_element"},
        {"ribosome_binding_site", "ribosome_entry_site"},
    };

    string regulatory_class = GetUnambiguousNamedQual(feature, "regulatory_class");
    if (regulatory_class.empty()) {
        so_type = "regulatory_region";
        return true;
    }
    auto cit = mapRegulatoryClassToSoType.find(regulatory_class);
    if (cit == mapRegulatoryClassToSoType.end()) {
        auto validClasses = CSeqFeatData::GetRegulatoryClassList();
        auto valid = std::find(
            validClasses.begin(), validClasses.end(), regulatory_class);
        if (valid == validClasses.end()) {
            so_type = "regulatory_region";
        }
        else {
            so_type = regulatory_class;
        }
        return true;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
bool CSoMap::xMapBond(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapBondTypeToSoType = {
        {"disulfide", "disulfide_bond"},
        {"xlink", "cross_link"},
    };
    string bond_type = GetUnambiguousNamedQual(feature, "bond_type");
    if (bond_type.empty()) {
        return false;
    }
    auto cit = mapBondTypeToSoType.find(bond_type);
    if (cit == mapBondTypeToSoType.end()) {
        so_type = bond_type;
        return true;
    }
    so_type = cit->second;
    return true;
}


//  ----------------------------------------------------------------------------
bool CSoMap::xMapRepeatRegion(
    const CSeq_feat& feature,
    string& so_type)
//  ----------------------------------------------------------------------------
{
    static const TYPEMAP mapSatelliteToSoType = {
        {"satellite", "satellite_DNA"},
        {"microsatellite", "microsatellite"},
        {"minisatellite", "minisatellite"},
    };
    string satellite = GetUnambiguousNamedQual(feature, "satellite");
    if (!satellite.empty()) {
        auto cit = mapSatelliteToSoType.find(satellite);
        if (cit == mapSatelliteToSoType.end()) {
            return false;
        }
        so_type = cit->second;
        return true;
    }

    static const TYPEMAP mapRptTypeToSoType = {
        {"tandem", "tandem_repeat"},
        {"inverted", "inverted_repeat"},
        {"flanking", "repeat_region"},
        {"terminal", "repeat_region"},
        {"direct", "direct_repeat"},
        {"dispersed", "dispersed_repeat"},
        {"nested", "nested_repeat"},
        {"non_ltr_retrotransposon_polymeric_tract", "non_LTR_retrotransposon_polymeric_tract"},
        {"x_element_combinatorical_repeat", "X_element_combinatorical_repeat"},
        {"y_prime_element", "Y_prime_element"},
        {"other", "repeat_region"},
    };
    string rpt_type = GetUnambiguousNamedQual(feature, "rpt_type");
    if (rpt_type.empty()) {
        so_type = "repeat_region";
        return true;
    }
    auto cit = mapRptTypeToSoType.find(rpt_type);
    if (cit == mapRptTypeToSoType.end()) {
        so_type = rpt_type;
        return true;
    }
    so_type = cit->second;
    return true;
}

//  ----------------------------------------------------------------------------
CSoMap::SOALIASMAP CSoMap::mMapSoAliases = {
//  ----------------------------------------------------------------------------
    {"-10_signal", "minus_10_signal"},
    {"-35_signal", "minus_35_signal"},
    {"3'UTR", "three_prime_UTR"},
    {"3'clip", "three_prime_clip"},
    {"5'UTR", "five_prime_UTR"},
    {"5'clip", "five_prime_clip"},
    {"C_region", "C_gene_segment"},
    {"D-loop", "D_loop"},
    {"D_segment", "D_gene_segment"},
    {"GC_signal", "GC_rich_promoter_region"},
    {"J_segment", "J_gene_segment"},
    {"LTR", "long_terminal_repeat"},
    {"RBS", "ribosome_entry_site"},
    {"TATA_signal", "TATA_box"},
    {"V_segment", "V_gene_segment"},
    {"assembly_gap", "gap"},
    {"Comment", "remark"},
    {"conflict", "sequence_conflict"},
    {"mat_peptide_nt", "mature_protein_region"},
    {"mat_peptide", "mature_protein_region"},
    {"misc_binding", "binding_site"},
    {"misc_difference", "sequence_difference"},
    {"misc_feature", "sequence_feature"},
    {"misc_recomb", "recombination_feature"},
    {"misc_signal", "regulatory_region"},
    {"misc_structure", "sequence_secondary_structure"},
    {"mobile_element", "mobile_genetic_element"},
    {"modified_base", "modified_DNA_base"},
    {"misc_RNA", "transcript"},
    {"polyA_signal", "polyA_signal_sequence"},
    {"pre_RNA", "primary_transcript"},
    {"precursor_RNA", "primary_transcript"},
    {"proprotein", "immature_peptide_region"},
    {"prim_transcript", "primary_transcript"},
    {"primer_bind", "primer_binding_site"},
    {"Protein", "polypeptide"},
    {"protein_bind", "protein_binding_site"},
    {"SecStr", "sequence_secondary_structure"},
    {"regulatory", "regulatory_region"},
    {"rep_origin", "origin_of_replication"},
    {"Rsite", "restriction_enzyme_cut_site"},
    {"satellite", "satellite_DNA"},
    {"Shine_Dalgarno_sequence", "ribosome_entry_site"},
    {"sig_peptide_nt", "signal_peptide"},
    {"sig_peptide", "signal_peptide"},
    {"Site", "site"},
    {"Site-ref", "site"},
    {"transit_peptide_nt", "transit_peptide"},
    {"unsure", "sequence_uncertainty"},
    {"variation", "sequence_alteration"},
    {"VariationRef", "sequence_alteration"},
    {"virion", "viral_sequence"},
};

//  ----------------------------------------------------------------------------
string CSoMap::ResolveSoAlias(
    const string& alias)
//  ----------------------------------------------------------------------------
{
    ALIASENTRY cit = mMapSoAliases.find(alias);
    if (cit == mMapSoAliases.end()) {
        return alias;
    }
    return cit->second;
}

END_NCBI_SCOPE
