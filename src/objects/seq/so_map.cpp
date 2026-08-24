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
#include <util/compile_time.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);

using namespace std::string_literals;

namespace
{
    using TQualValue = string_view;
    using TSoType    = string_view;

    using FEATFUNC = bool(*)(std::string_view, CSeq_feat&);
    using TYPEFUNC = bool(*)(const CSeq_feat&, TSoType&);

    bool s_SoMapFeatureToSoType(const CSeq_feat&, TSoType&);
    bool s_SoMapFeatureMakeGene(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeCds(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeRna(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeMiscFeature(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeMiscRecomb(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeMiscRna(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeNcRna(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeImp(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeProt(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeRegion(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeRegulatory(std::string_view, CSeq_feat&);
    bool s_SoMapFeatureMakeRepeatRegion(std::string_view, CSeq_feat&);

    bool s_SoMapMapGeneric(const CSeq_feat&, TSoType&);
    bool s_SoMapMapGene(const CSeq_feat&, TSoType&);
    bool s_SoMapMapCds(const CSeq_feat&, TSoType&);
    bool s_SoMapMapMiscFeature(const CSeq_feat&, TSoType&);
    bool s_SoMapMapMiscRecomb(const CSeq_feat&, TSoType&);
    bool s_SoMapMapRna(const CSeq_feat&, TSoType&);
    bool s_SoMapMapNcRna(const CSeq_feat&, TSoType&);
    //bool s_SoMapMapOtherRna(const CSeq_feat&, TSoType&);
    bool s_SoMapMapRegion(const CSeq_feat&, TSoType&);
    bool s_SoMapMapRegulatory(const CSeq_feat&, TSoType&);
    bool s_SoMapMapRepeatRegion(const CSeq_feat&, TSoType&);
    bool s_SoMapMapBond(const CSeq_feat&, TSoType&);

//  ----------------------------------------------------------------------------
TQualValue GetUnambiguousNamedQual(
    const CSeq_feat& feature,
    string_view qualName)
//  ----------------------------------------------------------------------------
{
    TQualValue namedQual;
    const auto& quals = feature.GetQual();
    for (const auto& qual: quals) {
        if (!qual->CanGetQual()  ||  !qual->CanGetVal())
            continue;
        if (qualName.compare(qual->GetQual()) != 0)
            continue;
        if (namedQual.empty()) {
            namedQual = qual->GetVal();
            continue;
        }
        if (namedQual.compare(qual->GetVal()) != 0)
            return {};
    }
    return namedQual;
}

//  ----------------------------------------------------------------------------

MAKE_TWOWAY_CONST_MAP(kMapSoIdToType, ct::tagStrNocase, ct::tagStrNocase,
{
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
    {"SO:0001244", "pre_miRNA"},
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
    {"SO:0001877", "lncRNA"},
    {"SO:0001917", "CAGE_cluster"},
    {"SO:0002020", "boundary_element"},
    {"SO:0002072", "sequence_comparison"},
    {"SO:0002087", "pseudogenic_CDS"},
    {"SO:0002094", "non_allelic_homologous_recombination_region"},
    {"SO:0002095",  "scaRNA"},
    {"SO:0002154", "mitotic_recombination_region"},
    {"SO:0002155", "meiotic_recombination_region"},
    {"SO:0002190", "enhancer_blocking_element"},
    {"SO:0002191", "imprinting_control_region"},
    {"SO:0002205", "response_element"},
    {"SO:0002393", "precursor_RNA"},
    {"SO:0005836", "regulatory_region"},
    {"SO:0005850", "primary_binding_site"},
    {"SO:0000000", ""},
});

MAKE_CONST_MAP(kMapFeatFunc, ct::tagStrNocase, FEATFUNC,
{
    {"CAGE_cluster",            s_SoMapFeatureMakeMiscFeature},
    {"CAAT_signal",             s_SoMapFeatureMakeRegulatory},
    {"CDS",                     s_SoMapFeatureMakeCds},
    {"C_gene_segment",          s_SoMapFeatureMakeImp},
    {"DNAsel_hypersensitive_site", s_SoMapFeatureMakeRegulatory},
    {"D_loop",                  s_SoMapFeatureMakeImp},
    {"D_gene_segment",          s_SoMapFeatureMakeImp},
    {"GC_rich_promoter_region", s_SoMapFeatureMakeRegulatory},
    {"J_gene_segment",          s_SoMapFeatureMakeImp},
    {"N_region",                s_SoMapFeatureMakeImp},
    {"pre_miRNA",               s_SoMapFeatureMakeNcRna},
    {"RNase_MRP_RNA",           s_SoMapFeatureMakeNcRna},
    {"RNase_P_RNA",             s_SoMapFeatureMakeNcRna},
    {"scaRNA",                  s_SoMapFeatureMakeNcRna},
    {"SRP_RNA",                 s_SoMapFeatureMakeNcRna},
    {"STS",                     s_SoMapFeatureMakeImp},
    {"S_region",                s_SoMapFeatureMakeImp},
    {"TATA_box",                s_SoMapFeatureMakeRegulatory},
    {"V_gene_segment",          s_SoMapFeatureMakeImp},
    {"V_region",                s_SoMapFeatureMakeImp},
    {"X_element_combinatorical_repeat", s_SoMapFeatureMakeRepeatRegion},
    {"Y_RNA",                   s_SoMapFeatureMakeNcRna},
    {"Y_prime_element",         s_SoMapFeatureMakeRepeatRegion},
    {"antisense_RNA",           s_SoMapFeatureMakeNcRna},
    {"attenuator",              s_SoMapFeatureMakeRegulatory},
    {"autocatalytically_spliced_intron", s_SoMapFeatureMakeNcRna},
    {"binding_site",            s_SoMapFeatureMakeImp},
    {"biological_region",       s_SoMapFeatureMakeRegion},
    {"boundary_element",        s_SoMapFeatureMakeRegulatory},
    {"centromere",              s_SoMapFeatureMakeImp},
    {"centromeric_repeat",      s_SoMapFeatureMakeRepeatRegion},
    {"chromosome_breakpoint",   s_SoMapFeatureMakeMiscRecomb},
    {"conserved_region",        s_SoMapFeatureMakeMiscFeature},
    {"direct_repeat",           s_SoMapFeatureMakeRepeatRegion},
    {"dispersed_repeat",        s_SoMapFeatureMakeRepeatRegion},
    {"enhancer",                s_SoMapFeatureMakeRegulatory},
    {"enhancer_blocking_element", s_SoMapFeatureMakeRegulatory},
    {"epigenetically_modified_region", s_SoMapFeatureMakeRegulatory},
    {"exon",                    s_SoMapFeatureMakeImp},
    {"five_prime_UTR",          s_SoMapFeatureMakeImp},
    {"gap",                     s_SoMapFeatureMakeImp},
    {"gene",                    s_SoMapFeatureMakeGene},
    {"guide_RNA",               s_SoMapFeatureMakeNcRna},
    {"hammerhead_ribozyme",     s_SoMapFeatureMakeNcRna},
    {"iDNA",                    s_SoMapFeatureMakeImp},
    {"immature_peptide_region", s_SoMapFeatureMakeProt},
    {"imprinting_control_region", s_SoMapFeatureMakeRegulatory},
    {"insulator",               s_SoMapFeatureMakeRegulatory},
    {"intron",                  s_SoMapFeatureMakeImp},
    {"inverted_repeat",         s_SoMapFeatureMakeRepeatRegion},
    {"lncRNA",                  s_SoMapFeatureMakeNcRna},
    {"locus_control_region",    s_SoMapFeatureMakeRegulatory},
    {"long_terminal_repeat",    s_SoMapFeatureMakeRepeatRegion},
    {"mRNA",                    s_SoMapFeatureMakeRna},
    {"matrix_attachment_region", s_SoMapFeatureMakeRegulatory},
    {"mature_protein_region",   s_SoMapFeatureMakeImp},
    {"meiotic_recombination_region", s_SoMapFeatureMakeMiscRecomb},
    {"miRNA",                   s_SoMapFeatureMakeNcRna},
    {"microsatellite",          s_SoMapFeatureMakeRepeatRegion},
    {"minisatellite",           s_SoMapFeatureMakeRepeatRegion},
    {"minus_10_signal",         s_SoMapFeatureMakeRegulatory},
    {"minus_35_signal",         s_SoMapFeatureMakeRegulatory},
    {"mitotic_recombination_region", s_SoMapFeatureMakeMiscRecomb},
    {"mobile_genetic_element",  s_SoMapFeatureMakeImp},
    {"modified_DNA_base",       s_SoMapFeatureMakeImp},
    {"ncRNA",                   s_SoMapFeatureMakeNcRna},
    {"nested_repeat",           s_SoMapFeatureMakeRepeatRegion},
    {"non_allelic_homologous_recombination", s_SoMapFeatureMakeMiscRecomb},
    {"non_LTR_retrotransposon_polymeric_tract", s_SoMapFeatureMakeRepeatRegion},
    {"nucleotide_motif",        s_SoMapFeatureMakeMiscFeature},
    {"nucleotide_cleavage_site", s_SoMapFeatureMakeMiscFeature},
    {"nucleotide_site",         s_SoMapFeatureMakeMiscFeature},
    {"operon",                  s_SoMapFeatureMakeImp},
    {"oriT",                    s_SoMapFeatureMakeImp},
    {"origin_of_replication",   s_SoMapFeatureMakeImp},
    {"piRNA",                   s_SoMapFeatureMakeNcRna},
    {"polyA_signal_sequence",   s_SoMapFeatureMakeRegulatory},
    {"polyA_site",              s_SoMapFeatureMakeImp},
    {"precursor_RNA",           s_SoMapFeatureMakeRna},
    {"primary_transcript",      s_SoMapFeatureMakeImp},
    {"primer_binding_site",     s_SoMapFeatureMakeImp},
    {"promoter",                s_SoMapFeatureMakeRegulatory},
    {"protein_binding_site",    s_SoMapFeatureMakeImp},
    {"pseudogene",              s_SoMapFeatureMakeGene},
    {"pseudogenic_exon",        s_SoMapFeatureMakeImp},
    {"pseudogenic_CDS",         s_SoMapFeatureMakeCds},
    {"pseudogenic_rRNA",        s_SoMapFeatureMakeRna},
    {"pseudogenic_tRNA",        s_SoMapFeatureMakeRna},
    {"pseudogenic_transcript",  s_SoMapFeatureMakeMiscRna},
    {"rRNA",                    s_SoMapFeatureMakeRna},
    {"rasiRNA",                 s_SoMapFeatureMakeNcRna},
    {"recoding_stimulatory_region", s_SoMapFeatureMakeRegulatory},
    {"recombination_feature",   s_SoMapFeatureMakeMiscRecomb},
    {"region",                  s_SoMapFeatureMakeImp},
    {"regulatory_region",       s_SoMapFeatureMakeRegulatory},
    {"repeat_instability_region", s_SoMapFeatureMakeMiscFeature},
    {"repeat_region",           s_SoMapFeatureMakeRepeatRegion},
    {"replication_regulatory_region", s_SoMapFeatureMakeRegulatory},
    {"replication_start_site",  s_SoMapFeatureMakeMiscFeature},
    {"response_element",        s_SoMapFeatureMakeRegulatory},
    {"ribosome_entry_site",     s_SoMapFeatureMakeRegulatory},
    {"riboswitch",              s_SoMapFeatureMakeRegulatory},
    {"ribozyme",                s_SoMapFeatureMakeNcRna},
    {"satellite_DNA",           s_SoMapFeatureMakeRepeatRegion},
    {"scRNA",                   s_SoMapFeatureMakeNcRna},
    {"sequence_alteration",     s_SoMapFeatureMakeImp},
    {"sequence_comparison",     s_SoMapFeatureMakeMiscFeature},
    {"sequence_difference",     s_SoMapFeatureMakeImp},
    {"sequence_feature",        s_SoMapFeatureMakeMiscFeature},
    {"sequence_secondary_structure", s_SoMapFeatureMakeImp},
    {"sequence_uncertainty",    s_SoMapFeatureMakeImp},
    {"siRNA",                   s_SoMapFeatureMakeNcRna},
    {"signal_peptide",          s_SoMapFeatureMakeImp},
    {"silencer",                s_SoMapFeatureMakeRegulatory},
    {"snRNA",                   s_SoMapFeatureMakeNcRna},
    {"snoRNA",                  s_SoMapFeatureMakeNcRna},
    {"stem_loop",               s_SoMapFeatureMakeImp},
    {"tRNA",                    s_SoMapFeatureMakeRna},
    {"tandem_repeat",           s_SoMapFeatureMakeRepeatRegion},
    {"telomerase_RNA",          s_SoMapFeatureMakeNcRna},
    {"telomere",                s_SoMapFeatureMakeImp},
    {"telomeric_repeat",        s_SoMapFeatureMakeRepeatRegion},
    {"terminator",              s_SoMapFeatureMakeRegulatory},
    {"tmRNA",                   s_SoMapFeatureMakeRna},
    {"transcript",              s_SoMapFeatureMakeMiscRna},
    {"transcriptional_cis_regulatory_region", s_SoMapFeatureMakeRegulatory},
    {"transcription_start_site", s_SoMapFeatureMakeMiscFeature},
    {"transit_peptide",         s_SoMapFeatureMakeImp},
    {"three_prime_UTR",         s_SoMapFeatureMakeImp},
    {"vault_RNA",               s_SoMapFeatureMakeNcRna},
});


//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeGene(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetGene();
    if (so_type == "pseudogene"sv) {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeRna(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mTypeToRna, ct::tagStrNocase, CRNA_ref::EType, {
        {"mRNA", CRNA_ref::eType_mRNA},
        {"precursor_RNA", CRNA_ref::eType_premsg},
        {"rRNA", CRNA_ref::eType_rRNA},
        {"pseudogenic_rRNA", CRNA_ref::eType_rRNA},
        {"tRNA", CRNA_ref::eType_tRNA},
        {"pseudogenic_tRNA", CRNA_ref::eType_tRNA},
        {"tmRNA", CRNA_ref::eType_tmRNA},
    });
    auto it = mTypeToRna.find(so_type);
    feature.SetData().SetRna().SetType(it->second);
    if(NStr::StartsWith(so_type, "pseudogenic_")) {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeNcRna(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mTypeToClass, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"ncRNA", "other"},
    });
    feature.SetData().SetRna().SetType(CRNA_ref::eType_ncRNA);
    auto normalizedType = so_type;
    auto it = mTypeToClass.find(so_type);
    if (it != mTypeToClass.end()) {
        normalizedType = it->second;
    }
    feature.SetData().SetRna().SetExt().SetGen().SetClass() = normalizedType;
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeCds(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetCdregion();
    if (so_type == "pseudogenic_CDS"sv) {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeProt(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mTypeToProcessed, ct::tagStrNocase, CProt_ref::EProcessed, {
        {"mature_protein_region", CProt_ref::eProcessed_mature},
        {"immature_peptide_region", CProt_ref::eProcessed_preprotein},
    });
    auto cit = mTypeToProcessed.find(so_type);
    if (cit == mTypeToProcessed.end()) {
        return false;
    }
    feature.SetData().SetProt().SetProcessed(cit->second);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeMiscFeature(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapTypeToQual, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"TSS", "transcription_start_site"},
    });
    feature.SetData().SetImp().SetKey("misc_feature");
    if (so_type == "sequence_feature"sv) {
        return true;
    }
    CRef<CGb_qual> feat_class(new CGb_qual);
    feat_class->SetQual("feat_class");
    auto cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        feat_class->SetVal() = so_type;
    }
    else {
        feat_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(feat_class);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeMiscRecomb(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapTypeToQual, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"meiotic_recombination_region", "meiotic"},
        {"mitotic_recombination_region", "mitotic"},
        {"non_allelic_homologous_recombination", "non_allelic_homologous"},
        {"recombination_feature", "other"},
    });
    feature.SetData().SetImp().SetKey("misc_recomb");
    CRef<CGb_qual> recombination_class(new CGb_qual);
    recombination_class->SetQual("recombination_class");
    auto cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        recombination_class->SetVal() = so_type;
    }
    else {
        recombination_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(recombination_class);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeMiscRna(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetImp().SetKey("misc_RNA");
    if (so_type=="pseudogenic_transcript"sv) {
        feature.SetPseudo(true);
    }
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeImp(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapTypeToKey, ct::tagStrNocase, ct::tagStrNocase,
    {
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
    });
    auto cit = mapTypeToKey.find(so_type);
    if (cit == mapTypeToKey.end()) {
        feature.SetData().SetImp().SetKey() = so_type;
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
bool s_SoMapFeatureMakeRegion(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    feature.SetData().SetRegion();
    CRef<CGb_qual> qual(new CGb_qual("SO_type", string(so_type)));
    feature.SetQual().push_back(qual);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeRegulatory(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapTypeToQual, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"DNAsel_hypersensitive_site", "DNase_I_hypersensitive_site"},
        {"GC_rich_promoter_region", "GC_signal"},
        {"boundary_element", "insulator"},
        {"regulatory_region", "other"},
        {"ribosome_entry_site", "ribosome_binding_site"},
    });
    feature.SetData().SetImp().SetKey("regulatory");
    CRef<CGb_qual> regulatory_class(new CGb_qual);
    regulatory_class->SetQual("regulatory_class");
    auto cit = mapTypeToQual.find(so_type);
    if (cit == mapTypeToQual.end()) {
        regulatory_class->SetVal() = so_type;
    }
    else {
        regulatory_class->SetVal(cit->second);
    }
    feature.SetQual().push_back(regulatory_class);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureMakeRepeatRegion(
    std::string_view so_type,
    CSeq_feat& feature)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapTypeToSatellite, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"microsatellite", "microsatellite"},
        {"minisatellite", "minisatellite"},
        {"satellite_DNA", "satellite"},
    });
    MAKE_CONST_MAP(mapTypeToRptType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"tandem_repeat", "tandem"},
        {"inverted_repeat", "inverted"},
        {"direct_repeat", "direct"},
        {"nested_repeat", "nested"},
        {"non_LTR_retrotransposon_polymeric_tract", "non_ltr_retrotransposon_polymeric_tract"},
        {"X_element_combinatorial_repeat", "x_element_combinatorial_repeat"},
        {"Y_prime_element", "y_prime_element"},
        {"repeat_region", "other"},
    });
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
            qual->SetVal() = so_type;
        }
        else {
            qual->SetVal(cit->second);
        }
    }
    feature.SetQual().push_back(qual);
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapFeatureToSoType(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    auto subtype = feature.GetData().GetSubtype();

    switch (subtype) {
        case CSeqFeatData::eSubtype_3UTR:
        case CSeqFeatData::eSubtype_5UTR:
        case CSeqFeatData::eSubtype_assembly_gap:
        case CSeqFeatData::eSubtype_C_region:
        case CSeqFeatData::eSubtype_centromere:
        case CSeqFeatData::eSubtype_conflict:
        case CSeqFeatData::eSubtype_D_loop:
        case CSeqFeatData::eSubtype_D_segment:
        case CSeqFeatData::eSubtype_exon:
        case CSeqFeatData::eSubtype_gap:
        case CSeqFeatData::eSubtype_iDNA:
        case CSeqFeatData::eSubtype_intron:
        case CSeqFeatData::eSubtype_J_segment:
        case CSeqFeatData::eSubtype_LTR:
        case CSeqFeatData::eSubtype_mat_peptide:
        case CSeqFeatData::eSubtype_mat_peptide_aa:
        case CSeqFeatData::eSubtype_misc_binding:
        case CSeqFeatData::eSubtype_misc_difference:
        case CSeqFeatData::eSubtype_misc_structure:
        case CSeqFeatData::eSubtype_mobile_element:
        case CSeqFeatData::eSubtype_modified_base:
        case CSeqFeatData::eSubtype_mRNA:
        case CSeqFeatData::eSubtype_N_region:
        case CSeqFeatData::eSubtype_operon:
        case CSeqFeatData::eSubtype_oriT:
        case CSeqFeatData::eSubtype_otherRNA:
        case CSeqFeatData::eSubtype_polyA_site:
        case CSeqFeatData::eSubtype_preRNA:
        case CSeqFeatData::eSubtype_precursor_RNA:
        case CSeqFeatData::eSubtype_preprotein:
        case CSeqFeatData::eSubtype_prim_transcript:
        case CSeqFeatData::eSubtype_primer_bind:
        case CSeqFeatData::eSubtype_propeptide:
        case CSeqFeatData::eSubtype_prot:
        case CSeqFeatData::eSubtype_protein_bind:
        case CSeqFeatData::eSubtype_rep_origin:
        case CSeqFeatData::eSubtype_S_region:
        case CSeqFeatData::eSubtype_sig_peptide:
        case CSeqFeatData::eSubtype_sig_peptide_aa:
        case CSeqFeatData::eSubtype_source:
        case CSeqFeatData::eSubtype_stem_loop:
        case CSeqFeatData::eSubtype_STS:
        case CSeqFeatData::eSubtype_telomere:
        case CSeqFeatData::eSubtype_tmRNA:
        case CSeqFeatData::eSubtype_transit_peptide:
        case CSeqFeatData::eSubtype_transit_peptide_aa:
        case CSeqFeatData::eSubtype_unsure:
        case CSeqFeatData::eSubtype_V_region:
        case CSeqFeatData::eSubtype_V_segment:
        case CSeqFeatData::eSubtype_variation:
        case CSeqFeatData::eSubtype_attenuator:
        case CSeqFeatData::eSubtype_enhancer:
        case CSeqFeatData::eSubtype_promoter:
        case CSeqFeatData::eSubtype_terminator:
            return s_SoMapMapGeneric(feature, so_type);

        case CSeqFeatData::eSubtype_cdregion:
            return s_SoMapMapCds(feature, so_type);

        case CSeqFeatData::eSubtype_gene:
        case CSeqFeatData::eSubtype_10_signal:
            return s_SoMapMapGene(feature, so_type);

        case CSeqFeatData::eSubtype_misc_feature:
            return s_SoMapMapMiscFeature(feature, so_type);

        case CSeqFeatData::eSubtype_misc_recomb:
            return s_SoMapMapMiscRecomb(feature, so_type);

        case CSeqFeatData::eSubtype_misc_RNA:
        case CSeqFeatData::eSubtype_rRNA:
        case CSeqFeatData::eSubtype_tRNA:
            return s_SoMapMapRna(feature, so_type);

        case CSeqFeatData::eSubtype_ncRNA:
        case CSeqFeatData::eSubtype_snRNA:
        case CSeqFeatData::eSubtype_snoRNA:
            return s_SoMapMapNcRna(feature, so_type);

        case CSeqFeatData::eSubtype_region:
            return s_SoMapMapRegion(feature, so_type);

        case CSeqFeatData::eSubtype_regulatory:
            return s_SoMapMapRegulatory(feature, so_type);

        case CSeqFeatData::eSubtype_repeat_region:
            return s_SoMapMapRepeatRegion(feature, so_type);

        case CSeqFeatData::eSubtype_bond:
            return s_SoMapMapBond(feature, so_type);
        default:
//            // for all not handled enumeration values
            return false;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool s_SoMapMapGeneric(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapSubtypeToSoType, CSeqFeatData::ESubtype, string_view, {
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
        {CSeqFeatData::eSubtype_precursor_RNA, "precursor_RNA"},
        {CSeqFeatData::eSubtype_preRNA, "precursor_RNA"},
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
    });
    auto subtype = feature.GetData().GetSubtype();
    auto cit = mapSubtypeToSoType.find(subtype);
    if (cit != mapSubtypeToSoType.end()) {
        so_type = cit->second;
        return true;
    }
    return false;
}

//  ----------------------------------------------------------------------------
bool s_SoMapMapRegion(
    const CSeq_feat& /*feature*/,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    so_type = "biological_region";
    return true;
}

//  ----------------------------------------------------------------------------
bool s_SoMapMapCds(
    const CSeq_feat& feature,
    TSoType& so_type)
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
bool s_SoMapMapGene(
    const CSeq_feat& feature,
    TSoType& so_type)
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
bool s_SoMapMapRna(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapSubtypeStraight, CSeqFeatData::ESubtype, string_view, {
        {CSeqFeatData::eSubtype_misc_RNA, "transcript"},
        {CSeqFeatData::eSubtype_rRNA, "rRNA"},
        {CSeqFeatData::eSubtype_tRNA, "tRNA"},
    });
    MAKE_CONST_MAP(mapSubtypePseudo, CSeqFeatData::ESubtype, string_view, {
        {CSeqFeatData::eSubtype_misc_RNA, "pseudogenic_transcript"},
        {CSeqFeatData::eSubtype_rRNA, "pseudogenic_rRNA"},
        {CSeqFeatData::eSubtype_tRNA, "pseudogenic_tRNA"},
    });

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
bool s_SoMapMapMiscFeature(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapFeatClassToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"transcription_start_site", "TSS"},
        {"other", "sequence_feature"},
    });
    TQualValue feat_class = GetUnambiguousNamedQual(feature, "feat_class");
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
bool s_SoMapMapMiscRecomb(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapRecombClassToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"meiotic", "meiotic_recombination_region"},
        {"mitotic", "mitotic_recombination_region"},
        {"non_allelic_homologous", "non_allelic_homologous_recombination_region"},
        {"meiotic_recombination", "meiotic_recombination_region"},
        {"mitotic_recombination", "mitotic_recombination_region"},
        {"non_allelic_homologous_recombination", "non_allelic_homologous_recombination_region"},
        {"other", "recombination_feature"},
    });
    TQualValue recomb_class = GetUnambiguousNamedQual(feature, "recombination_class");
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

#if 0
//  ----------------------------------------------------------------------------
bool s_SoMapMapOtherRna(
    const CSeq_feat& /*feature*/,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    so_type = "transcript";
    return true;
}
#endif

//  ----------------------------------------------------------------------------
bool s_SoMapMapNcRna(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapNcRnaClassToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"antisense_RNA", "antisense_RNA"},
        {"autocatalytically_spliced_intron", "autocatalytically_spliced_intron"},
        {"guide_RNA", "guide_RNA"},
        {"hammerhead_ribozyme", "hammerhead_ribozyme"},
        {"lncRNA", "lncRNA"},
        {"lnc_RNA", "lncRNA"},
        {"miRNA", "miRNA"},
        {"other", "ncRNA"},
        {"piRNA", "piRNA"},
        {"pre_miRNA", "pre_miRNA"},
        {"rasiRNA", "rasiRNA"},
        {"ribozyme", "ribozyme"},
        {"RNase_MRP_RNA", "RNase_MRP_RNA"},
        {"RNase_P_RNA", "RNase_P_RNA"},
        {"scRNA", "scRNA"},
        {"scaRNA", "scaRNA"},
        {"siRNA", "siRNA"},
        {"snRNA", "snRNA"},
        {"snoRNA", "snoRNA"},
        {"SRP_RNA", "SRP_RNA"},
        {"telomerase_RNA", "telomerase_RNA"},
        {"vault_RNA", "vault_RNA"},
        {"Y_RNA", "Y_RNA"},
    });
    TQualValue ncrna_class = GetUnambiguousNamedQual(feature, "ncRNA_class");
    if (ncrna_class.empty()) {
        if (feature.IsSetData()  &&
                feature.GetData().IsRna()  &&
                feature.GetData().GetRna().IsSetExt()  &&
                feature.GetData().GetRna().GetExt().IsGen()  &&
                feature.GetData().GetRna().GetExt().GetGen().IsSetClass()) {
            ncrna_class = feature.GetData().GetRna().GetExt().GetGen().GetClass();
            if (ncrna_class == "classRNA"sv) {
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
        so_type = "ncRNA";
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
bool s_SoMapMapRegulatory(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapRegulatoryClassToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"DNase_I_hypersensitive_site", "DNaseI_hypersensitive_site"},
        {"GC_signal", "GC_rich_promoter_region"},
        {"enhancer_blocking_element", "enhancer_blocking_element"},
        {"epigenetically_modified_region", "epigenetically_modified_region"},
        {"imprinting_control_region", "imprinting_control_region"},
        {"matrix_attachment_region", "matrix_attachment_site"},
        {"other", "regulatory_region"},
        {"response_element", "response_element"},
        {"ribosome_binding_site", "ribosome_entry_site"},
    });
    TQualValue regulatory_class = GetUnambiguousNamedQual(feature, "regulatory_class");
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
bool s_SoMapMapBond(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapBondTypeToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"disulfide", "disulfide_bond"},
        {"xlink", "cross_link"},
    });
    TQualValue bond_type = GetUnambiguousNamedQual(feature, "bond_type");
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
bool s_SoMapMapRepeatRegion(
    const CSeq_feat& feature,
    TSoType& so_type)
//  ----------------------------------------------------------------------------
{
    MAKE_CONST_MAP(mapSatelliteToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
        {"satellite", "satellite_DNA"},
        {"microsatellite", "microsatellite"},
        {"minisatellite", "minisatellite"},
    });
    TQualValue satellite = GetUnambiguousNamedQual(feature, "satellite");
    if (!satellite.empty()) {
        auto cit = mapSatelliteToSoType.find(satellite);
        if (cit == mapSatelliteToSoType.end()) {
            return false;
        }
        so_type = cit->second;
        return true;
    }

    MAKE_CONST_MAP(mapRptTypeToSoType, ct::tagStrNocase, ct::tagStrNocase,
    {
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
        {"centromeric_repeat", "centromeric_repeat"},
        {"long_terminal_repeat", "long_terminal_repeat"},
        {"telomeric_repeat", "telomeric_repeat"},
        {"other", "repeat_region"},
    });
    TQualValue rpt_type = GetUnambiguousNamedQual(feature, "rpt_type");
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

MAKE_CONST_MAP(kMapSoAliases, ct::tagStrNocase, ct::tagStrNocase,
{
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
    {"lnc_RNA", "lncRNA"},
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
    {"pre_RNA", "precursor_RNA"},
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
});

} // end of anonymous namespace


//  ----------------------------------------------------------------------------
string CSoMap::ResolveSoAlias(std::string_view alias)
//  ----------------------------------------------------------------------------
{
    auto cit = kMapSoAliases.find(alias);
    if (cit == kMapSoAliases.end()) {
        return string(alias);
    }
    return cit->second;
}
//  ----------------------------------------------------------------------------
bool CSoMap::GetSupportedSoTerms(
    vector<string>& supported_terms)
//  ----------------------------------------------------------------------------
{
    supported_terms.clear();
    supported_terms.reserve(kMapFeatFunc.size());
    for (const auto& term: kMapFeatFunc) {
        supported_terms.push_back(term.first);
    }
    // TODO: do not re-sort case-sensitive
    std::sort(supported_terms.begin(), supported_terms.end());
    return true;
}

//  ----------------------------------------------------------------------------
string CSoMap::SoIdToType(std::string_view sofa_id)
//  ----------------------------------------------------------------------------
{
    auto type_it = kMapSoIdToType.first.find(sofa_id);
    if (type_it == kMapSoIdToType.first.end()) {
        return kEmptyStr;
    }
    return type_it->second;
}

//  ----------------------------------------------------------------------------
string CSoMap::SoTypeToId(std::string_view so_type)
//  ----------------------------------------------------------------------------
{
    auto type_it = kMapSoIdToType.second.find(so_type);
    if (type_it == kMapSoIdToType.second.end()) {
        return kEmptyStr;
    }
    return type_it->second;
}

static string_view s_ResolveSoType(string_view so_type)
{
    auto cit = kMapSoAliases.find(so_type);
    if (cit != kMapSoAliases.end()) {
        return cit->second;
    }
    return so_type;
}


//  ----------------------------------------------------------------------------
bool CSoMap::SoTypeToFeature(
    std::string_view so_type,
    CSeq_feat& feature,
    bool invalidToRegion)
//  ----------------------------------------------------------------------------
{
    string_view resolved_so_type = s_ResolveSoType(so_type);
    auto it = kMapFeatFunc.find(resolved_so_type);
    if (it != kMapFeatFunc.end()) {
        return (it->second)(resolved_so_type, feature);
    }
    if (invalidToRegion) {
        return s_SoMapFeatureMakeRegion(so_type, feature);
    }
    return false;
}


static bool s_IsRnaFeatFunc(FEATFUNC func)
{
    return (func == s_SoMapFeatureMakeRna ||
            func == s_SoMapFeatureMakeMiscRna ||
            func == s_SoMapFeatureMakeNcRna);
}

//  ----------------------------------------------------------------------------
bool CSoMap::SoTypeToRnaFeature(
    std::string_view so_type,
    CSeq_feat&       feature) // Returns false if so_type does not correspond to an RNA feature
//  ----------------------------------------------------------------------------
{
    string_view resolved_so_type = s_ResolveSoType(so_type);
    auto        it               = kMapFeatFunc.find(resolved_so_type);
    if (it != kMapFeatFunc.end()) {
        auto feat_func = it->second;
        if (s_IsRnaFeatFunc(feat_func)) {
        return (feat_func)(resolved_so_type, feature);
        }
    }
    return false;
}


//  ----------------------------------------------------------------------------
bool CSoMap::FeatureToSoType(
    const CSeq_feat& feature,
    string& so_type)
{
    const string& original_type = feature.GetNamedQual("SO_type");
    if (!original_type.empty()) {
        so_type = original_type;
        return true;
    }

    TSoType so_type_sv;
    auto retval = s_SoMapFeatureToSoType(feature, so_type_sv);
    if (retval)
        so_type = so_type_sv;
    return retval;
}



END_NCBI_SCOPE
