/*  $Id:
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
 * Author:  Mati Shomrat
 *
 * File Description:
 *      Implementation of utility classes and functions.
 *
 */
#include <corelib/ncbistd.hpp>
#include <corelib/ncbistr.hpp>
#include "utilities.hpp"

#include <serial/enumvalues.hpp>
#include <serial/serialimpl.hpp>
#include <objects/seqfeat/SeqFeatData.hpp>
#include <objects/seqfeat/Gb_qual.hpp>
#include <objects/objmgr/bioseq_handle.hpp>
#include <objects/objmgr/scope.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <vector>
#include <algorithm>


BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)
BEGIN_SCOPE(validator)

// =============================================================================
//                           Enumeration of GBQual types
// =============================================================================


CGbqualType::EType CGbqualType::GetType(const string& qual)
{
    EType type;
    try {
        type = static_cast<EType>(GetTypeInfo_enum_EType()->FindValue(qual));
    } catch (runtime_error rt) {
        type = CGbqualType::e_Bad;
    }

    return type;
}


CGbqualType::EType CGbqualType::GetType(const CGb_qual& qual)
{
    return GetType(qual.GetQual());
}


const string& CGbqualType::GetString(CGbqualType::EType gbqual)
{
    return GetTypeInfo_enum_EType()->FindName(gbqual, true);
}

        
BEGIN_NAMED_ENUM_IN_INFO("", CGbqualType::, EType, false)
{
    ADD_ENUM_VALUE("bad",               e_Bad);
    ADD_ENUM_VALUE("allele",            e_Allele);
    ADD_ENUM_VALUE("anticodon",         e_Anticodon);
    ADD_ENUM_VALUE("bound_moiety",      e_Bound_moiety);
    ADD_ENUM_VALUE("citation",          e_Citation);
    ADD_ENUM_VALUE("clone",             e_Clone);
    ADD_ENUM_VALUE("codon",             e_Codon);
    ADD_ENUM_VALUE("codon_start",       e_Codon_start);
    ADD_ENUM_VALUE("cons_splice",       e_Cons_splice);
    ADD_ENUM_VALUE("direction",         e_Direction);
    ADD_ENUM_VALUE("EC_number",         e_EC_number);
    ADD_ENUM_VALUE("evidence",          e_Evidence);
    ADD_ENUM_VALUE("exception",         e_Exception);
    ADD_ENUM_VALUE("frequency",         e_Frequency);
    ADD_ENUM_VALUE("function",          e_Function);
    ADD_ENUM_VALUE("gene",              e_Gene);
    ADD_ENUM_VALUE("label",             e_Label);
    ADD_ENUM_VALUE("locus_tag",         e_Locus_tag);
    ADD_ENUM_VALUE("map",               e_Map);
    ADD_ENUM_VALUE("mod_base",          e_Mod_base);
    ADD_ENUM_VALUE("note",              e_Note);
    ADD_ENUM_VALUE("number",            e_Number);
    ADD_ENUM_VALUE("organism",          e_Organism);
    ADD_ENUM_VALUE("partial",           e_Partial);
    ADD_ENUM_VALUE("PCR_conditions",    e_PCR_conditions);
    ADD_ENUM_VALUE("phenotype",         e_Phenotype);
    ADD_ENUM_VALUE("product",           e_Product);
    ADD_ENUM_VALUE("replace",           e_Replace);
    ADD_ENUM_VALUE("rpt_family",        e_Rpt_family);
    ADD_ENUM_VALUE("rpt_type",          e_Rpt_type);
    ADD_ENUM_VALUE("rpt_unit",          e_Rpt_unit);
    ADD_ENUM_VALUE("site",              e_Site);
    ADD_ENUM_VALUE("site_type",         e_Site_type);
    ADD_ENUM_VALUE("standard_name",     e_Standard_name);
    ADD_ENUM_VALUE("transl_except",     e_Transl_except);
    ADD_ENUM_VALUE("transl_table",      e_Transl_table);
    ADD_ENUM_VALUE("translation",       e_Translation);
    ADD_ENUM_VALUE("usedin",            e_Usedin);
}
END_ENUM_INFO


// =============================================================================
//                        Associating Features and GBQuals
// =============================================================================


bool CFeatQualAssoc::IsLegalGbqual
(CSeqFeatData::ESubtype ftype,
 CGbqualType::EType gbqual) 
{
    return Instance()->IsLegal(ftype, gbqual);
}


const CFeatQualAssoc::GBQualTypeVec& CFeatQualAssoc::GetMandatoryGbquals
(CSeqFeatData::ESubtype ftype)
{
    return Instance()->GetMandatoryQuals(ftype);
}


auto_ptr<CFeatQualAssoc> CFeatQualAssoc::sm_Instance;

bool CFeatQualAssoc::IsLegal
(CSeqFeatData::ESubtype ftype, 
 CGbqualType::EType gbqual)
{
    if ( m_LegalGbquals.find(ftype) != m_LegalGbquals.end() ) {
        if ( find( m_LegalGbquals[ftype].begin(), 
                   m_LegalGbquals[ftype].end(), 
                   gbqual) != m_LegalGbquals[ftype].end() ) {
            return true;
        }
    }
    return false;
}


const CFeatQualAssoc::GBQualTypeVec& CFeatQualAssoc::GetMandatoryQuals
(CSeqFeatData::ESubtype ftype)
{
    static GBQualTypeVec empty;

    if ( m_MandatoryGbquals.find(ftype) != m_MandatoryGbquals.end() ) {
        return m_MandatoryGbquals[ftype];
    }

    return empty;
}


CFeatQualAssoc* CFeatQualAssoc::Instance(void)
{
    if ( !sm_Instance.get() ) {
        sm_Instance.reset(new CFeatQualAssoc);
    }
    return sm_Instance.get();
}


CFeatQualAssoc::CFeatQualAssoc(void)
{
    PoplulateLegalGbquals();
    PopulateMandatoryGbquals();
}


#define ASSOCIATE(feat_subtype, gbqual_type) \
    m_LegalGbquals[CSeqFeatData::feat_subtype].push_back(CGbqualType::gbqual_type)

void CFeatQualAssoc::PoplulateLegalGbquals(void)
{
    // gene
    ASSOCIATE( eSubtype_gene, e_Allele );
    ASSOCIATE( eSubtype_gene, e_Function );
    ASSOCIATE( eSubtype_gene, e_Label );
    ASSOCIATE( eSubtype_gene, e_Map );
    ASSOCIATE( eSubtype_gene, e_Phenotype );
    ASSOCIATE( eSubtype_gene, e_Product );
    ASSOCIATE( eSubtype_gene, e_Standard_name );
    ASSOCIATE( eSubtype_gene, e_Usedin );
    
 
    // CDS
    ASSOCIATE( eSubtype_cdregion, e_Allele );
    ASSOCIATE( eSubtype_cdregion, e_Codon );
    ASSOCIATE( eSubtype_cdregion, e_Label );
    ASSOCIATE( eSubtype_cdregion, e_Map );
    ASSOCIATE( eSubtype_cdregion, e_Number );
    ASSOCIATE( eSubtype_cdregion, e_Standard_name );
    ASSOCIATE( eSubtype_cdregion, e_Usedin );

    // prot
    ASSOCIATE( eSubtype_prot, e_Product );

    // preRNA
    ASSOCIATE( eSubtype_preRNA, e_Allele );
    ASSOCIATE( eSubtype_preRNA, e_Function );
    ASSOCIATE( eSubtype_preRNA, e_Label );
    ASSOCIATE( eSubtype_preRNA, e_Map );
    ASSOCIATE( eSubtype_preRNA, e_Product );
    ASSOCIATE( eSubtype_preRNA, e_Standard_name );
    ASSOCIATE( eSubtype_preRNA, e_Usedin );

    // mRNA
    ASSOCIATE( eSubtype_mRNA, e_Allele );
    ASSOCIATE( eSubtype_mRNA, e_Function );
    ASSOCIATE( eSubtype_mRNA, e_Label );
    ASSOCIATE( eSubtype_mRNA, e_Map );
    ASSOCIATE( eSubtype_mRNA, e_Product );
    ASSOCIATE( eSubtype_mRNA, e_Standard_name );
    ASSOCIATE( eSubtype_mRNA, e_Usedin );

    // tRNA
    ASSOCIATE( eSubtype_tRNA, e_Function );
    ASSOCIATE( eSubtype_tRNA, e_Label );
    ASSOCIATE( eSubtype_tRNA, e_Map );
    ASSOCIATE( eSubtype_tRNA, e_Product );
    ASSOCIATE( eSubtype_tRNA, e_Standard_name );
    ASSOCIATE( eSubtype_tRNA, e_Usedin );

    // rRNA
    ASSOCIATE( eSubtype_rRNA, e_Function );
    ASSOCIATE( eSubtype_rRNA, e_Label );
    ASSOCIATE( eSubtype_rRNA, e_Map );
    ASSOCIATE( eSubtype_rRNA, e_Product );
    ASSOCIATE( eSubtype_rRNA, e_Standard_name );
    ASSOCIATE( eSubtype_rRNA, e_Usedin );

    // snRNA
    ASSOCIATE( eSubtype_snRNA, e_Function );
    ASSOCIATE( eSubtype_snRNA, e_Label );
    ASSOCIATE( eSubtype_snRNA, e_Map );
    ASSOCIATE( eSubtype_snRNA, e_Product );
    ASSOCIATE( eSubtype_snRNA, e_Standard_name );
    ASSOCIATE( eSubtype_snRNA, e_Usedin );

    // scRNA
    ASSOCIATE( eSubtype_scRNA, e_Function );
    ASSOCIATE( eSubtype_scRNA, e_Label );
    ASSOCIATE( eSubtype_scRNA, e_Map );
    ASSOCIATE( eSubtype_scRNA, e_Product );
    ASSOCIATE( eSubtype_scRNA, e_Standard_name );
    ASSOCIATE( eSubtype_scRNA, e_Usedin );

    // otherRNA
    ASSOCIATE( eSubtype_otherRNA, e_Function );
    ASSOCIATE( eSubtype_otherRNA, e_Label );
    ASSOCIATE( eSubtype_otherRNA, e_Map );
    ASSOCIATE( eSubtype_otherRNA, e_Product );
    ASSOCIATE( eSubtype_otherRNA, e_Standard_name );
    ASSOCIATE( eSubtype_otherRNA, e_Usedin );

    // attenuator
    ASSOCIATE( eSubtype_attenuator, e_Label );
    ASSOCIATE( eSubtype_attenuator, e_Map );
    ASSOCIATE( eSubtype_attenuator, e_Phenotype );
    ASSOCIATE( eSubtype_attenuator, e_Usedin );

    // C_region
    ASSOCIATE( eSubtype_C_region, e_Label );
    ASSOCIATE( eSubtype_C_region, e_Map );
    ASSOCIATE( eSubtype_C_region, e_Product );
    ASSOCIATE( eSubtype_C_region, e_Standard_name );
    ASSOCIATE( eSubtype_C_region, e_Usedin );

    // CAAT_signal
    ASSOCIATE( eSubtype_CAAT_signal, e_Label );
    ASSOCIATE( eSubtype_CAAT_signal, e_Map );
    ASSOCIATE( eSubtype_CAAT_signal, e_Usedin );

    // Imp_CDS
    ASSOCIATE( eSubtype_Imp_CDS, e_Codon );
    ASSOCIATE( eSubtype_Imp_CDS, e_EC_number );
    ASSOCIATE( eSubtype_Imp_CDS, e_Function );
    ASSOCIATE( eSubtype_Imp_CDS, e_Label );
    ASSOCIATE( eSubtype_Imp_CDS, e_Map );
    ASSOCIATE( eSubtype_Imp_CDS, e_Number );
    ASSOCIATE( eSubtype_Imp_CDS, e_Product );
    ASSOCIATE( eSubtype_Imp_CDS, e_Standard_name );
    ASSOCIATE( eSubtype_Imp_CDS, e_Usedin );

    // conflict
    ASSOCIATE( eSubtype_conflict, e_Label );
    ASSOCIATE( eSubtype_conflict, e_Map );
    ASSOCIATE( eSubtype_conflict, e_Replace );
    ASSOCIATE( eSubtype_conflict, e_Label );

    // D_loop
    ASSOCIATE( eSubtype_D_loop, e_Label );
    ASSOCIATE( eSubtype_D_loop, e_Map );
    ASSOCIATE( eSubtype_D_loop, e_Usedin );

    // D_segment
    ASSOCIATE( eSubtype_D_segment, e_Label );
    ASSOCIATE( eSubtype_D_segment, e_Map );
    ASSOCIATE( eSubtype_D_segment, e_Product );
    ASSOCIATE( eSubtype_D_segment, e_Standard_name );
    ASSOCIATE( eSubtype_D_segment, e_Usedin );

    // enhancer
    ASSOCIATE( eSubtype_enhancer, e_Label );
    ASSOCIATE( eSubtype_enhancer, e_Map );
    ASSOCIATE( eSubtype_enhancer, e_Standard_name );
    ASSOCIATE( eSubtype_enhancer, e_Usedin );

    // exon
    ASSOCIATE( eSubtype_exon, e_Allele );
    ASSOCIATE( eSubtype_exon, e_EC_number );
    ASSOCIATE( eSubtype_exon, e_Function );
    ASSOCIATE( eSubtype_exon, e_Label );
    ASSOCIATE( eSubtype_exon, e_Map );
    ASSOCIATE( eSubtype_exon, e_Number );
    ASSOCIATE( eSubtype_exon, e_Product );
    ASSOCIATE( eSubtype_exon, e_Standard_name );
    ASSOCIATE( eSubtype_exon, e_Usedin );

    // GC_signal
    ASSOCIATE( eSubtype_GC_signal, e_Label );
    ASSOCIATE( eSubtype_GC_signal, e_Map );
    ASSOCIATE( eSubtype_GC_signal, e_Usedin );

    // iDNA
    ASSOCIATE( eSubtype_iDNA, e_Function );
    ASSOCIATE( eSubtype_iDNA, e_Label );
    ASSOCIATE( eSubtype_iDNA, e_Map );
    ASSOCIATE( eSubtype_iDNA, e_Number );
    ASSOCIATE( eSubtype_iDNA, e_Standard_name );
    ASSOCIATE( eSubtype_iDNA, e_Usedin );

    // intron
    ASSOCIATE( eSubtype_intron, e_Allele );
    ASSOCIATE( eSubtype_intron, e_Cons_splice );
    ASSOCIATE( eSubtype_intron, e_Function );
    ASSOCIATE( eSubtype_intron, e_Label );
    ASSOCIATE( eSubtype_intron, e_Map );
    ASSOCIATE( eSubtype_intron, e_Number );
    ASSOCIATE( eSubtype_intron, e_Standard_name );
    ASSOCIATE( eSubtype_intron, e_Usedin );

    // J_segment
    ASSOCIATE( eSubtype_J_segment, e_Label );
    ASSOCIATE( eSubtype_J_segment, e_Map );
    ASSOCIATE( eSubtype_J_segment, e_Product );
    ASSOCIATE( eSubtype_J_segment, e_Standard_name );
    ASSOCIATE( eSubtype_J_segment, e_Usedin );

    // LTR
    ASSOCIATE( eSubtype_LTR, e_Function );
    ASSOCIATE( eSubtype_LTR, e_Label );
    ASSOCIATE( eSubtype_LTR, e_Map );
    ASSOCIATE( eSubtype_LTR, e_Standard_name );
    ASSOCIATE( eSubtype_LTR, e_Usedin );

    // mat_peptide
    ASSOCIATE( eSubtype_mat_peptide, e_EC_number );
    ASSOCIATE( eSubtype_mat_peptide, e_Function );
    ASSOCIATE( eSubtype_mat_peptide, e_Label );
    ASSOCIATE( eSubtype_mat_peptide, e_Map );
    ASSOCIATE( eSubtype_mat_peptide, e_Product );
    ASSOCIATE( eSubtype_mat_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_mat_peptide, e_Usedin );

    // misc_binding
    ASSOCIATE( eSubtype_misc_binding, e_Bound_moiety );
    ASSOCIATE( eSubtype_misc_binding, e_Function );
    ASSOCIATE( eSubtype_misc_binding, e_Label );
    ASSOCIATE( eSubtype_misc_binding, e_Map );
    ASSOCIATE( eSubtype_misc_binding, e_Usedin );

    // misc_difference
    ASSOCIATE( eSubtype_misc_difference, e_Clone );
    ASSOCIATE( eSubtype_misc_difference, e_Label );
    ASSOCIATE( eSubtype_misc_difference, e_Map );
    ASSOCIATE( eSubtype_misc_difference, e_Phenotype );
    ASSOCIATE( eSubtype_misc_difference, e_Replace );
    ASSOCIATE( eSubtype_misc_difference, e_Standard_name );
    ASSOCIATE( eSubtype_misc_difference, e_Usedin );

    // misc_feature
    ASSOCIATE( eSubtype_misc_feature, e_Function );
    ASSOCIATE( eSubtype_misc_feature, e_Label );
    ASSOCIATE( eSubtype_misc_feature, e_Map );
    ASSOCIATE( eSubtype_misc_feature, e_Number );
    ASSOCIATE( eSubtype_misc_feature, e_Phenotype );
    ASSOCIATE( eSubtype_misc_feature, e_Product );
    ASSOCIATE( eSubtype_misc_feature, e_Standard_name );
    ASSOCIATE( eSubtype_misc_feature, e_Usedin );

    // misc_recomb
    ASSOCIATE( eSubtype_misc_recomb, e_Label );
    ASSOCIATE( eSubtype_misc_recomb, e_Map );
    ASSOCIATE( eSubtype_misc_recomb, e_Organism );
    ASSOCIATE( eSubtype_misc_recomb, e_Standard_name );
    ASSOCIATE( eSubtype_misc_recomb, e_Usedin );

    // misc_signal
    ASSOCIATE( eSubtype_misc_signal, e_Function );
    ASSOCIATE( eSubtype_misc_signal, e_Label );
    ASSOCIATE( eSubtype_misc_signal, e_Map );
    ASSOCIATE( eSubtype_misc_signal, e_Phenotype );
    ASSOCIATE( eSubtype_misc_signal, e_Standard_name );
    ASSOCIATE( eSubtype_misc_signal, e_Usedin );

    // misc_structure
    ASSOCIATE( eSubtype_misc_structure, e_Function );
    ASSOCIATE( eSubtype_misc_structure, e_Label );
    ASSOCIATE( eSubtype_misc_structure, e_Map );
    ASSOCIATE( eSubtype_misc_structure, e_Standard_name );
    ASSOCIATE( eSubtype_misc_structure, e_Usedin );

    // modified_base
    ASSOCIATE( eSubtype_misc_structure, e_Frequency );
    ASSOCIATE( eSubtype_misc_structure, e_Label );
    ASSOCIATE( eSubtype_misc_structure, e_Map );
    ASSOCIATE( eSubtype_misc_structure, e_Mod_base );
    ASSOCIATE( eSubtype_misc_structure, e_Usedin );

    // N_region
    ASSOCIATE( eSubtype_N_region, e_Label );
    ASSOCIATE( eSubtype_N_region, e_Map );
    ASSOCIATE( eSubtype_N_region, e_Product );
    ASSOCIATE( eSubtype_N_region, e_Standard_name );
    ASSOCIATE( eSubtype_N_region, e_Usedin );

    // old_sequence
    ASSOCIATE( eSubtype_old_sequence, e_Label );
    ASSOCIATE( eSubtype_old_sequence, e_Map );
    ASSOCIATE( eSubtype_old_sequence, e_Replace );
    ASSOCIATE( eSubtype_old_sequence, e_Usedin );

    // polyA_signal
    ASSOCIATE( eSubtype_polyA_signal, e_Label );
    ASSOCIATE( eSubtype_polyA_signal, e_Map );
    ASSOCIATE( eSubtype_polyA_signal, e_Usedin );

    // polyA_site
    ASSOCIATE( eSubtype_polyA_site, e_Label );
    ASSOCIATE( eSubtype_polyA_site, e_Map );
    ASSOCIATE( eSubtype_polyA_site, e_Usedin );

    // prim_transcript
    ASSOCIATE( eSubtype_prim_transcript, e_Allele );
    ASSOCIATE( eSubtype_prim_transcript, e_Function );
    ASSOCIATE( eSubtype_prim_transcript, e_Label );
    ASSOCIATE( eSubtype_prim_transcript, e_Map );
    ASSOCIATE( eSubtype_prim_transcript, e_Standard_name );
    ASSOCIATE( eSubtype_prim_transcript, e_Usedin );

    // primer_bind
    ASSOCIATE( eSubtype_primer_bind, e_Label );
    ASSOCIATE( eSubtype_primer_bind, e_Map );
    ASSOCIATE( eSubtype_primer_bind, e_PCR_conditions );
    ASSOCIATE( eSubtype_primer_bind, e_Standard_name );
    ASSOCIATE( eSubtype_primer_bind, e_Usedin );

    // promoter
    ASSOCIATE( eSubtype_promoter, e_Function );
    ASSOCIATE( eSubtype_promoter, e_Label );
    ASSOCIATE( eSubtype_promoter, e_Map );
    ASSOCIATE( eSubtype_promoter, e_Phenotype );
    ASSOCIATE( eSubtype_promoter, e_Standard_name );
    ASSOCIATE( eSubtype_promoter, e_Usedin );

    // protein_bind
    ASSOCIATE( eSubtype_promoter, e_Bound_moiety );
    ASSOCIATE( eSubtype_promoter, e_Function );
    ASSOCIATE( eSubtype_promoter, e_Label );
    ASSOCIATE( eSubtype_promoter, e_Map );
    ASSOCIATE( eSubtype_promoter, e_Standard_name );
    ASSOCIATE( eSubtype_promoter, e_Usedin );

    // RBS
    ASSOCIATE( eSubtype_RBS, e_Label );
    ASSOCIATE( eSubtype_RBS, e_Map );
    ASSOCIATE( eSubtype_RBS, e_Standard_name );
    ASSOCIATE( eSubtype_RBS, e_Usedin );

    // repeat_region
    ASSOCIATE( eSubtype_repeat_region, e_Function );
    ASSOCIATE( eSubtype_repeat_region, e_Label );
    ASSOCIATE( eSubtype_repeat_region, e_Map );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_family );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_type );
    ASSOCIATE( eSubtype_repeat_region, e_Rpt_unit );
    ASSOCIATE( eSubtype_repeat_region, e_Standard_name );
    ASSOCIATE( eSubtype_repeat_region, e_Usedin );

    // repeat_unit
    ASSOCIATE( eSubtype_repeat_unit, e_Function );
    ASSOCIATE( eSubtype_repeat_unit, e_Label );
    ASSOCIATE( eSubtype_repeat_unit, e_Map );
    ASSOCIATE( eSubtype_repeat_unit, e_Rpt_family );
    ASSOCIATE( eSubtype_repeat_unit, e_Rpt_type );
    ASSOCIATE( eSubtype_repeat_unit, e_Usedin );

    // rep_origin
    ASSOCIATE( eSubtype_rep_origin, e_Direction );
    ASSOCIATE( eSubtype_rep_origin, e_Label );
    ASSOCIATE( eSubtype_rep_origin, e_Map );
    ASSOCIATE( eSubtype_rep_origin, e_Standard_name );
    ASSOCIATE( eSubtype_rep_origin, e_Usedin );

    // S_region
    ASSOCIATE( eSubtype_repeat_unit, e_Label );
    ASSOCIATE( eSubtype_repeat_unit, e_Map );
    ASSOCIATE( eSubtype_repeat_unit, e_Product );
    ASSOCIATE( eSubtype_repeat_unit, e_Standard_name );
    ASSOCIATE( eSubtype_repeat_unit, e_Usedin );

    // satellite
    ASSOCIATE( eSubtype_satellite, e_Label );
    ASSOCIATE( eSubtype_satellite, e_Map );
    ASSOCIATE( eSubtype_satellite, e_Rpt_family );
    ASSOCIATE( eSubtype_satellite, e_Rpt_type );
    ASSOCIATE( eSubtype_satellite, e_Rpt_unit );
    ASSOCIATE( eSubtype_satellite, e_Standard_name );
    ASSOCIATE( eSubtype_satellite, e_Usedin );

    // sig_peptide
    ASSOCIATE( eSubtype_sig_peptide, e_Function );
    ASSOCIATE( eSubtype_sig_peptide, e_Label );
    ASSOCIATE( eSubtype_sig_peptide, e_Map );
    ASSOCIATE( eSubtype_sig_peptide, e_Product );
    ASSOCIATE( eSubtype_sig_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_sig_peptide, e_Usedin );

    // stem_loop
    ASSOCIATE( eSubtype_stem_loop, e_Function );
    ASSOCIATE( eSubtype_stem_loop, e_Label );
    ASSOCIATE( eSubtype_stem_loop, e_Map );
    ASSOCIATE( eSubtype_stem_loop, e_Standard_name );
    ASSOCIATE( eSubtype_stem_loop, e_Usedin );

    // STS
    ASSOCIATE( eSubtype_STS, e_Label );
    ASSOCIATE( eSubtype_STS, e_Map );
    ASSOCIATE( eSubtype_STS, e_Standard_name );
    ASSOCIATE( eSubtype_STS, e_Usedin );

    // TATA_signal
    ASSOCIATE( eSubtype_TATA_signal, e_Label );
    ASSOCIATE( eSubtype_TATA_signal, e_Map );
    ASSOCIATE( eSubtype_TATA_signal, e_Usedin );

    // terminator
    ASSOCIATE( eSubtype_terminator, e_Label );
    ASSOCIATE( eSubtype_terminator, e_Map );
    ASSOCIATE( eSubtype_terminator, e_Standard_name );
    ASSOCIATE( eSubtype_terminator, e_Usedin );

    // transit_peptide
    ASSOCIATE( eSubtype_transit_peptide, e_Function );
    ASSOCIATE( eSubtype_transit_peptide, e_Label );
    ASSOCIATE( eSubtype_transit_peptide, e_Map );
    ASSOCIATE( eSubtype_transit_peptide, e_Product );
    ASSOCIATE( eSubtype_transit_peptide, e_Standard_name );
    ASSOCIATE( eSubtype_transit_peptide, e_Usedin );

    // unsure
    ASSOCIATE( eSubtype_unsure, e_Label );
    ASSOCIATE( eSubtype_unsure, e_Map );
    ASSOCIATE( eSubtype_unsure, e_Replace );
    ASSOCIATE( eSubtype_unsure, e_Usedin );

    // V_region
    ASSOCIATE( eSubtype_V_region, e_Label );
    ASSOCIATE( eSubtype_V_region, e_Map );
    ASSOCIATE( eSubtype_V_region, e_Product );
    ASSOCIATE( eSubtype_V_region, e_Standard_name );
    ASSOCIATE( eSubtype_V_region, e_Usedin );

    // V_segment
    ASSOCIATE( eSubtype_V_segment, e_Label );
    ASSOCIATE( eSubtype_V_segment, e_Map );
    ASSOCIATE( eSubtype_V_segment, e_Product );
    ASSOCIATE( eSubtype_V_segment, e_Standard_name );
    ASSOCIATE( eSubtype_V_segment, e_Usedin );

    // variation
    ASSOCIATE( eSubtype_variation, e_Allele );
    ASSOCIATE( eSubtype_variation, e_Frequency );
    ASSOCIATE( eSubtype_variation, e_Label );
    ASSOCIATE( eSubtype_variation, e_Map );
    ASSOCIATE( eSubtype_variation, e_Phenotype );
    ASSOCIATE( eSubtype_variation, e_Product );
    ASSOCIATE( eSubtype_variation, e_Replace );
    ASSOCIATE( eSubtype_variation, e_Standard_name );
    ASSOCIATE( eSubtype_variation, e_Usedin );

    // 3clip
    ASSOCIATE( eSubtype_3clip, e_Allele );
    ASSOCIATE( eSubtype_3clip, e_Function );
    ASSOCIATE( eSubtype_3clip, e_Label );
    ASSOCIATE( eSubtype_3clip, e_Map );
    ASSOCIATE( eSubtype_3clip, e_Standard_name );
    ASSOCIATE( eSubtype_3clip, e_Usedin );

    // 3UTR
    ASSOCIATE( eSubtype_3UTR, e_Allele );
    ASSOCIATE( eSubtype_3UTR, e_Function );
    ASSOCIATE( eSubtype_3UTR, e_Label );
    ASSOCIATE( eSubtype_3UTR, e_Map );
    ASSOCIATE( eSubtype_3UTR, e_Standard_name );
    ASSOCIATE( eSubtype_3UTR, e_Usedin );

    // 5clip
    ASSOCIATE( eSubtype_5clip, e_Allele );
    ASSOCIATE( eSubtype_5clip, e_Function );
    ASSOCIATE( eSubtype_5clip, e_Label );
    ASSOCIATE( eSubtype_5clip, e_Map );
    ASSOCIATE( eSubtype_5clip, e_Standard_name );
    ASSOCIATE( eSubtype_5clip, e_Usedin );

    // 5UTR
    ASSOCIATE( eSubtype_5UTR, e_Allele );
    ASSOCIATE( eSubtype_5UTR, e_Function );
    ASSOCIATE( eSubtype_5UTR, e_Label );
    ASSOCIATE( eSubtype_5UTR, e_Map );
    ASSOCIATE( eSubtype_5UTR, e_Standard_name );
    ASSOCIATE( eSubtype_5UTR, e_Usedin );

    // 10_signal
    ASSOCIATE( eSubtype_10_signal, e_Label );
    ASSOCIATE( eSubtype_10_signal, e_Map );
    ASSOCIATE( eSubtype_10_signal, e_Standard_name );
    ASSOCIATE( eSubtype_10_signal, e_Usedin );

    // 35_signal
    ASSOCIATE( eSubtype_35_signal, e_Label );
    ASSOCIATE( eSubtype_35_signal, e_Map );
    ASSOCIATE( eSubtype_35_signal, e_Standard_name );
    ASSOCIATE( eSubtype_35_signal, e_Usedin );

    // region
    ASSOCIATE( eSubtype_region, e_Function );
    ASSOCIATE( eSubtype_region, e_Label );
    ASSOCIATE( eSubtype_region, e_Map );
    ASSOCIATE( eSubtype_region, e_Number );
    ASSOCIATE( eSubtype_region, e_Phenotype );
    ASSOCIATE( eSubtype_region, e_Product );
    ASSOCIATE( eSubtype_region, e_Standard_name );
    ASSOCIATE( eSubtype_region, e_Usedin );

    // mat_peptide_aa
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_mat_peptide_aa, e_Usedin );

    // sig_peptide_aa
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_sig_peptide_aa, e_Usedin );

    // transit_peptide_aa
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Label );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Map );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Product );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Standard_name );
    ASSOCIATE( eSubtype_transit_peptide_aa, e_Usedin );

    // snoRNA
    ASSOCIATE( eSubtype_snoRNA, e_Function );
    ASSOCIATE( eSubtype_snoRNA, e_Label );
    ASSOCIATE( eSubtype_snoRNA, e_Map );
    ASSOCIATE( eSubtype_snoRNA, e_Product );
    ASSOCIATE( eSubtype_snoRNA, e_Standard_name );
    ASSOCIATE( eSubtype_snoRNA, e_Usedin );
}

void CFeatQualAssoc::PopulateMandatoryGbquals(void)
{
    // gene feature requires gene gbqual
    m_MandatoryGbquals[CSeqFeatData::eSubtype_gene].push_back(CGbqualType::e_Gene);

    // misc_binding & protein_bind require bound_moiety
    m_MandatoryGbquals[CSeqFeatData::eSubtype_misc_binding].push_back(CGbqualType::e_Bound_moiety);
    m_MandatoryGbquals[CSeqFeatData::eSubtype_protein_bind].push_back(CGbqualType::e_Bound_moiety);

    // modified_base requires mod_base
    m_MandatoryGbquals[CSeqFeatData::eSubtype_modified_base].push_back(CGbqualType::e_Mod_base);
}


// =============================================================================
//                                 Country Names
// =============================================================================


// legal country names
const string CCountries::sm_Countries[] = {
  "Afghanistan",
  "Albania",
  "Algeria",
  "American Samoa",
  "Andorra",
  "Angola",
  "Anguilla",
  "Antarctica",
  "Antigua and Barbuda",
  "Argentina",
  "Armenia",
  "Aruba",
  "Ashmore and Cartier Islands",
  "Australia",
  "Austria",
  "Azerbaijan",
  "Bahamas",
  "Bahrain",
  "Baker Island",
  "Bangladesh",
  "Barbados",
  "Bassas da India",
  "Belarus",
  "Belgium",
  "Belize",
  "Benin",
  "Bermuda",
  "Bhutan",
  "Bolivia",
  "Bosnia and Herzegovina",
  "Botswana",
  "Bouvet Island",
  "Brazil",
  "British Virgin Islands",
  "Brunei",
  "Bulgaria",
  "Burkina Faso",
  "Burundi",
  "Cambodia",
  "Cameroon",
  "Canada",
  "Cape Verde",
  "Cayman Islands",
  "Central African Republic",
  "Chad",
  "Chile",
  "China",
  "Christmas Island",
  "Clipperton Island",
  "Cocos Islands",
  "Colombia",
  "Comoros",
  "Cook Islands",
  "Coral Sea Islands",
  "Costa Rica",
  "Cote d'Ivoire",
  "Croatia",
  "Cuba",
  "Cyprus",
  "Czech Republic",
  "Democratic Republic of the Congo",
  "Denmark",
  "Djibouti",
  "Dominica",
  "Dominican Republic",
  "Ecuador",
  "Egypt",
  "El Salvador",
  "Equatorial Guinea",
  "Eritrea",
  "Estonia",
  "Ethiopia",
  "Europa Island",
  "Falkland Islands (Islas Malvinas)",
  "Faroe Islands",
  "Fiji",
  "Finland",
  "France",
  "French Guiana",
  "French Polynesia",
  "French Southern and Antarctic Lands",
  "Gabon",
  "Gambia",
  "Gaza Strip",
  "Georgia",
  "Germany",
  "Ghana",
  "Gibraltar",
  "Glorioso Islands",
  "Greece",
  "Greenland",
  "Grenada",
  "Guadeloupe",
  "Guam",
  "Guatemala",
  "Guernsey",
  "Guinea",
  "Guinea-Bissau",
  "Guyana",
  "Haiti",
  "Heard Island and McDonald Islands",
  "Honduras",
  "Hong Kong",
  "Howland Island",
  "Hungary",
  "Iceland",
  "India",
  "Indonesia",
  "Iran",
  "Iraq",
  "Ireland",
  "Isle of Man",
  "Israel",
  "Italy",
  "Jamaica",
  "Jan Mayen",
  "Japan",
  "Jarvis Island",
  "Jersey",
  "Johnston Atoll",
  "Jordan",
  "Juan de Nova Island",
  "Kazakhstan",
  "Kenya",
  "Kingman Reef",
  "Kiribati",
  "Kuwait",
  "Kyrgyzstan",
  "Laos",
  "Latvia",
  "Lebanon",
  "Lesotho",
  "Liberia",
  "Libya",
  "Liechtenstein",
  "Lithuania",
  "Luxembourg",
  "Macau",
  "Macedonia",
  "Madagascar",
  "Malawi",
  "Malaysia",
  "Maldives",
  "Mali",
  "Malta",
  "Marshall Islands",
  "Martinique",
  "Mauritania",
  "Mauritius",
  "Mayotte",
  "Mexico",
  "Micronesia",
  "Midway Islands",
  "Moldova",
  "Monaco",
  "Mongolia",
  "Montserrat",
  "Morocco",
  "Mozambique",
  "Myanmar",
  "Namibia",
  "Nauru",
  "Navassa Island",
  "Nepal",
  "Netherlands",
  "Netherlands Antilles",
  "New Caledonia",
  "New Zealand",
  "Nicaragua",
  "Niger",
  "Nigeria",
  "Niue",
  "Norfolk Island",
  "North Korea",
  "Northern Mariana Islands",
  "Norway",
  "Oman",
  "Pakistan",
  "Palau",
  "Palmyra Atoll",
  "Panama",
  "Papua New Guinea",
  "Paracel Islands",
  "Paraguay",
  "Peru",
  "Philippines",
  "Pitcairn Islands",
  "Poland",
  "Portugal",
  "Puerto Rico",
  "Qatar",
  "Republic of the Congo",
  "Reunion",
  "Romania",
  "Russia",
  "Rwanda",
  "Saint Helena",
  "Saint Kitts and Nevis",
  "Saint Lucia",
  "Saint Pierre and Miquelon",
  "Saint Vincent and the Grenadines",
  "Samoa",
  "San Marino",
  "Sao Tome and Principe",
  "Saudi Arabia",
  "Senegal",
  "Seychelles",
  "Sierra Leone",
  "Singapore",
  "Slovakia",
  "Slovenia",
  "Solomon Islands",
  "Somalia",
  "South Africa",
  "South Georgia and the South Sandwich Islands",
  "South Korea",
  "Spain",
  "Spratly Islands",
  "Sri Lanka",
  "Sudan",
  "Suriname",
  "Svalbard",
  "Swaziland",
  "Sweden",
  "Switzerland",
  "Syria",
  "Taiwan",
  "Tajikistan",
  "Tanzania",
  "Thailand",
  "Togo",
  "Tokelau",
  "Tonga",
  "Trinidad and Tobago",
  "Tromelin Island",
  "Tunisia",
  "Turkey",
  "Turkmenistan",
  "Turks and Caicos Islands",
  "Tuvalu",
  "Uganda",
  "Ukraine",
  "United Arab Emirates",
  "United Kingdom",
  "Uruguay",
  "USA",
  "Uzbekistan",
  "Vanuatu",
  "Venezuela",
  "Viet Nam",
  "Virgin Islands",
  "Wake Island",
  "Wallis and Futuna",
  "West Bank",
  "Western Sahara",
  "Yemen",
  "Yugoslavia",
  "Zambia",
  "Zimbabwe"
};


bool CCountries::IsValid(const string& country)
{
  string name = country;
  size_t pos = country.find(':');
  if ( pos != string::npos ) {
    name = country.substr(0, pos);
  }

  const string *begin = sm_Countries;
  const string *end = &(sm_Countries[sizeof(sm_Countries) / sizeof(string)]);
  if ( find (begin, end, name) == end ) {
    return false;
  }

  return true;
}


// =============================================================================
//                                    Functions
// =============================================================================


bool IsClassInEntry(const CSeq_entry& se, CBioseq_set::EClass clss)
{
    for ( CTypeConstIterator <CBioseq_set> si(se); si; ++si ) {
        if ( si->GetClass() == clss ) {
            return true;
        }
    }
    return false;
}


bool IsDeltaOrFarSeg(const CSeq_loc& loc, CScope* scope)
{
    CBioseq_Handle bsh = scope->GetBioseqHandle(loc);
    const CSeq_entry& se = bsh.GetTopLevelSeqEntry();
    const CBioseq& bsq = bsh.GetBioseq();

    if ( bsq.GetInst().GetRepr() == CSeq_inst::eRepr_delta ) {
        if ( !IsClassInEntry(se, CBioseq_set::eClass_nuc_prot) ) {
            return true;
        }
    }
    if ( bsq.GetInst().GetRepr() == CSeq_inst::eRepr_seg ) {
        if ( !IsClassInEntry(se, CBioseq_set::eClass_parts) ) {
            return true;
        }
    }
    
    return false;
}


END_SCOPE(validator)
END_SCOPE(objects)
END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.2  2002/12/24 16:50:15  shomrat
* Removal of redundant GBQual types
*
* Revision 1.1  2002/12/23 20:13:18  shomrat
* Initial submission after splitting former implementation
*
*
* ===========================================================================
*/
